/*
 * PostgreSQL extension for linear algebra calculations
 *
 * contrib/pg_array_linalg/pg_array_linalg.c
 *
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "access/tupmacs.h"
#include <math.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

typedef struct {
	double **matrix;
	int rows;
	int columns;
} pg_matrix;

#define ERROR_NCM 1
#define ERROR_CNTM 2
#define OK 0

static int pg_is_invalid(const pg_matrix *A) {
	return (A == NULL || A->matrix == NULL || A->rows <= 0 || A->columns <= 0);
}

static int pg_create_matrix(int rows, int columns, pg_matrix *result) {
	int i, j;
	if (rows <= 0 || columns <= 0 || result == NULL) {
		return ERROR_NCM;
	}
	result->rows = rows;
	result->columns = columns;
	result->matrix = (double **)palloc(rows * sizeof(double *));
	if (result->matrix == NULL) {
		return ERROR_NCM;
	}
	for (i = 0; i < rows; i++) {
		result->matrix[i] = (double *)palloc0(columns * sizeof(double));
		if (result->matrix[i] == NULL) {
			result->rows = i;
			for (j = 0; j < i; j++) {
				pfree(result->matrix[j]);
			}
			pfree(result->matrix);
			return ERROR_NCM;
		}
	}
	return OK;
}

static void pg_remove_matrix(pg_matrix *A) {
	int i;
	if (A->matrix != NULL) {
		for (i = 0; i < A->rows; i++) {
			if (A->matrix[i] != NULL) {
				pfree(A->matrix[i]);
			}
		}
		pfree(A->matrix);
		A->rows = 0;
		A->columns = 0;
		A->matrix = NULL;
	}
}

static int array_to_matrix(ArrayType *array, pg_matrix *result) {
	int ndims = ARR_NDIM(array);
	int *dims = ARR_DIMS(array);
	Oid element_type = ARR_ELEMTYPE(array);
	int16 typlen;
	bool typbyval;
	char typalign;
	int rows;
	int columns;
	Datum *elems;
	bool *nulls;
	int nelems;
	int i;

	get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

	if (ndims == 0) {
		return ERROR_NCM;
	}
	if (ndims > 2) {
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("array must be 1 or 2 dimensional")));
	}

	rows = (ndims == 1) ? 1 : dims[0];
	columns = (ndims == 1) ? dims[0] : dims[1];

	if (pg_create_matrix(rows, columns, result) != OK) {
		return ERROR_NCM;
	}

	deconstruct_array(array, element_type, typlen, typbyval, typalign,
					  &elems, &nulls, &nelems);

	for (i = 0; i < nelems; i++) {
		int row, col;
		double val = 0.0;

		if (nulls[i]) {
			ereport(ERROR,
					(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
					 errmsg("array elements cannot be NULL")));
		}

		/* Поддерживаем конвертацию разных типов данных в double */
		if (element_type == FLOAT8OID) {
			val = DatumGetFloat8(elems[i]);
		} else if (element_type == INT4OID) {
			val = (double) DatumGetInt32(elems[i]);
		} else if (element_type == INT8OID) {
			val = (double) DatumGetInt64(elems[i]);
		} else if (element_type == FLOAT4OID) {
			val = (double) DatumGetFloat4(elems[i]);
		} else if (element_type == NUMERICOID) {
			val = DatumGetFloat8(DirectFunctionCall1(numeric_float8, elems[i]));
		} else {
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("unsupported array element type for linear algebra")));
		}

		row = (ndims == 1) ? 0 : i / columns;
		col = (ndims == 1) ? i : i % columns;
		result->matrix[row][col] = val;
	}

	pfree(elems);
	pfree(nulls);
	return OK;
}

static ArrayType *matrix_to_array(pg_matrix *A, Oid element_type) {
	int ndims = (A->rows == 1 || A->columns == 1) ? 1 : 2;
	int dims[2];
	int lbs[2];
	int nelems;
	Datum *elems;
	bool *nulls;
	int i, j;
	int16 typlen;
	bool typbyval;
	char typalign;
	ArrayType *result_arr;

	if (ndims == 1) {
		dims[0] = (A->rows == 1) ? A->columns : A->rows;
		lbs[0] = 1;
	} else {
		dims[0] = A->rows;
		dims[1] = A->columns;
		lbs[0] = 1;
		lbs[1] = 1;
	}

	nelems = A->rows * A->columns;
	elems = (Datum *)palloc(nelems * sizeof(Datum));
	nulls = (bool *)palloc0(nelems * sizeof(bool));

	for (i = 0; i < A->rows; i++) {
		for (j = 0; j < A->columns; j++) {
			double val = A->matrix[i][j];
			Datum elem_datum;

			if (element_type == FLOAT8OID) {
				elem_datum = Float8GetDatum(val);
			} else if (element_type == INT4OID) {
				elem_datum = Int32GetDatum((int32) round(val));
			} else if (element_type == INT8OID) {
				elem_datum = Int64GetDatum((int64) round(val));
			} else if (element_type == FLOAT4OID) {
				elem_datum = Float4GetDatum((float4) val);
			} else if (element_type == NUMERICOID) {
				elem_datum = DirectFunctionCall1(float8_numeric, Float8GetDatum(val));
			} else {
				elem_datum = Float8GetDatum(val);
			}
			elems[i * A->columns + j] = elem_datum;
		}
	}

	get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

	result_arr = construct_md_array(elems, nulls, ndims, dims, lbs,
									element_type, typlen, typbyval, typalign);

	pfree(elems);
	pfree(nulls);
	return result_arr;
}

static int pg_mult_matrix(pg_matrix *A, pg_matrix *B, pg_matrix *result) {
	int status;
	int i, j, k;

	if (pg_is_invalid(A) || pg_is_invalid(B) || result == NULL) {
		return ERROR_NCM;
	}
	if (A->columns != B->rows) {
		return ERROR_CNTM;
	}
	status = pg_create_matrix(A->rows, B->columns, result);
	if (status == OK) {
		for (i = 0; i < A->rows; i++) {
			for (j = 0; j < B->columns; j++) {
				result->matrix[i][j] = 0.0;
				for (k = 0; k < A->columns; k++) {
					result->matrix[i][j] += A->matrix[i][k] * B->matrix[k][j];
				}
			}
		}
	}
	return status;
}

static int pg_transpose(pg_matrix *A, pg_matrix *result) {
	int status;
	int i, j;

	if (pg_is_invalid(A) || result == NULL) {
		return ERROR_NCM;
	}
	status = pg_create_matrix(A->columns, A->rows, result);
	if (status == OK) {
		for (i = 0; i < A->rows; i++) {
			for (j = 0; j < A->columns; j++) {
				result->matrix[j][i] = A->matrix[i][j];
			}
		}
	}
	return status;
}

static void pg_get_minor(int row, int col, pg_matrix *A, pg_matrix *result) {
	int r_row = 0;
	int i;

	for (i = 0; i < A->rows; i++) {
		if (i != row) {
			int r_col = 0;
			int j;
			for (j = 0; j < A->columns; j++) {
				if (j != col) {
					result->matrix[r_row][r_col] = A->matrix[i][j];
					r_col++;
				}
			}
			r_row++;
		}
	}
}

static double pg_det_recursive(pg_matrix *A) {
	if (A->rows == 1) {
		return A->matrix[0][0];
	} else if (A->rows == 2) {
		return A->matrix[0][0] * A->matrix[1][1] - A->matrix[0][1] * A->matrix[1][0];
	} else {
		double det = 0.0;
		int sign = 1;
		int j;
		for (j = 0; j < A->columns; j++) {
			pg_matrix minor;
			if (pg_create_matrix(A->rows - 1, A->columns - 1, &minor) == OK) {
				pg_get_minor(0, j, A, &minor);
				det += (double)sign * A->matrix[0][j] * pg_det_recursive(&minor);
				pg_remove_matrix(&minor);
			}
			sign = -sign;
		}
		return det;
	}
}

static int pg_determinant(pg_matrix *A, double *result) {
	if (pg_is_invalid(A) || result == NULL) {
		return ERROR_NCM;
	}
	if (A->rows != A->columns) {
		return ERROR_CNTM;
	}
	*result = pg_det_recursive(A);
	return OK;
}

static int pg_calc_complements(pg_matrix *A, pg_matrix *result) {
	int status;

	if (pg_is_invalid(A) || result == NULL) {
		return ERROR_NCM;
	}
	if (A->rows != A->columns) {
		return ERROR_CNTM;
	}
	status = pg_create_matrix(A->rows, A->columns, result);
	if (status == OK) {
		if (A->rows == 1) {
			result->matrix[0][0] = 1.0;
		} else {
			int i, j;
			for (i = 0; i < A->rows; i++) {
				for (j = 0; j < A->columns; j++) {
					pg_matrix minor;
					if (pg_create_matrix(A->rows - 1, A->columns - 1, &minor) == OK) {
						double det;
						double sign;
						pg_get_minor(i, j, A, &minor);
						det = pg_det_recursive(&minor);
						sign = ((i + j) % 2 == 0) ? 1.0 : -1.0;
						result->matrix[i][j] = sign * det;
						pg_remove_matrix(&minor);
					}
				}
			}
		}
	}
	return status;
}

static int pg_mult_number(pg_matrix *A, double number, pg_matrix *result) {
	int i, j;

	if (pg_is_invalid(A) || result == NULL) {
		return ERROR_NCM;
	}
	if (pg_create_matrix(A->rows, A->columns, result) == ERROR_NCM) {
		return ERROR_NCM;
	}
	for (i = 0; i < A->rows; i++) {
		for (j = 0; j < A->columns; j++) {
			result->matrix[i][j] = A->matrix[i][j] * number;
		}
	}
	return OK;
}

static int pg_inverse_matrix(pg_matrix *A, pg_matrix *result) {
	double det;
	pg_matrix complements;
	pg_matrix complements_transposed;
	int status;

	if (pg_is_invalid(A) || result == NULL) {
		return ERROR_NCM;
	}
	if (A->rows != A->columns) {
		return ERROR_CNTM;
	}

	det = pg_det_recursive(A);
	if (fabs(det) < 1e-7) {
		return ERROR_CNTM;
	}

	if (A->rows == 1) {
		if (pg_create_matrix(1, 1, result) == OK) {
			result->matrix[0][0] = 1.0 / det;
			return OK;
		}
		return ERROR_NCM;
	}

	status = pg_calc_complements(A, &complements);
	if (status == OK) {
		status = pg_transpose(&complements, &complements_transposed);
		pg_remove_matrix(&complements);
	}
	if (status == OK) {
		status = pg_mult_number(&complements_transposed, 1.0 / det, result);
		pg_remove_matrix(&complements_transposed);
	}
	return status;
}

PG_FUNCTION_INFO_V1(array_dot);

Datum
array_dot(PG_FUNCTION_ARGS)
{
	ArrayType *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *array2 = PG_GETARG_ARRAYTYPE_P(1);
	pg_matrix A, B;
	double result = 0.0;
	int i;

	if (array_to_matrix(array1, &A) != OK) {
		PG_RETURN_NULL();
	}
	if (array_to_matrix(array2, &B) != OK) {
		pg_remove_matrix(&A);
		PG_RETURN_NULL();
	}

	if (A.rows != 1 || B.rows != 1) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("both arguments must be 1D arrays")));
	}

	if (A.columns != B.columns) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("vectors must have the same length")));
	}

	for (i = 0; i < A.columns; i++) {
		result += A.matrix[0][i] * B.matrix[0][i];
	}

	pg_remove_matrix(&A);
	pg_remove_matrix(&B);

	PG_RETURN_DATUM(DirectFunctionCall1(float8_numeric, Float8GetDatum(result)));
}

PG_FUNCTION_INFO_V1(array_matmul);

Datum
array_matmul(PG_FUNCTION_ARGS)
{
	ArrayType *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *array2 = PG_GETARG_ARRAYTYPE_P(1);
	pg_matrix A, B, result;
	Oid element_type = ARR_ELEMTYPE(array1);
	ArrayType *result_array;

	if (array_to_matrix(array1, &A) != OK) {
		PG_RETURN_NULL();
	}
	if (array_to_matrix(array2, &B) != OK) {
		pg_remove_matrix(&A);
		PG_RETURN_NULL();
	}

	if (pg_mult_matrix(&A, &B, &result) != OK) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("matrix dimensions are incompatible for multiplication")));
	}

	result_array = matrix_to_array(&result, element_type);

	pg_remove_matrix(&A);
	pg_remove_matrix(&B);
	pg_remove_matrix(&result);

	PG_RETURN_ARRAYTYPE_P(result_array);
}

PG_FUNCTION_INFO_V1(array_transpose);

Datum
array_transpose(PG_FUNCTION_ARGS)
{
	ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
	pg_matrix A, result;
	Oid element_type = ARR_ELEMTYPE(array);
	ArrayType *result_array;

	if (array_to_matrix(array, &A) != OK) {
		PG_RETURN_NULL();
	}

	if (pg_transpose(&A, &result) != OK) {
		pg_remove_matrix(&A);
		PG_RETURN_NULL();
	}

	result_array = matrix_to_array(&result, element_type);

	pg_remove_matrix(&A);
	pg_remove_matrix(&result);

	PG_RETURN_ARRAYTYPE_P(result_array);
}

PG_FUNCTION_INFO_V1(array_det);

Datum
array_det(PG_FUNCTION_ARGS)
{
	ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
	pg_matrix A;
	double det;

	if (array_to_matrix(array, &A) != OK) {
		PG_RETURN_NULL();
	}

	if (pg_determinant(&A, &det) != OK) {
		pg_remove_matrix(&A);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("matrix must be square")));
	}

	pg_remove_matrix(&A);

	PG_RETURN_DATUM(DirectFunctionCall1(float8_numeric, Float8GetDatum(det)));
}

PG_FUNCTION_INFO_V1(array_inv);

Datum
array_inv(PG_FUNCTION_ARGS)
{
	ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
	pg_matrix A, result;
	Oid element_type = FLOAT8OID;
	ArrayType *result_array;

	if (array_to_matrix(array, &A) != OK) {
		PG_RETURN_NULL();
	}

	if (pg_inverse_matrix(&A, &result) != OK) {
		pg_remove_matrix(&A);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("matrix is singular or not square")));
	}

	result_array = matrix_to_array(&result, element_type);

	pg_remove_matrix(&A);
	pg_remove_matrix(&result);

	PG_RETURN_ARRAYTYPE_P(result_array);
}

PG_FUNCTION_INFO_V1(array_solve);

Datum
array_solve(PG_FUNCTION_ARGS)
{
	ArrayType *arrayA = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arrayB = PG_GETARG_ARRAYTYPE_P(1);
	pg_matrix A, B, B_col, A_inv, result;
	Oid element_type = FLOAT8OID;
	ArrayType *result_array;

	if (array_to_matrix(arrayA, &A) != OK) {
		PG_RETURN_NULL();
	}
	if (array_to_matrix(arrayB, &B) != OK) {
		pg_remove_matrix(&A);
		PG_RETURN_NULL();
	}

	if (B.rows != 1) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("second argument must be a 1D array")));
	}

	if (A.rows != A.columns || A.columns != B.columns) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("matrix dimensions are incompatible")));
	}

	if (pg_inverse_matrix(&A, &A_inv) != OK) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("matrix is singular or not square")));
	}

	if (pg_transpose(&B, &B_col) != OK) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		pg_remove_matrix(&A_inv);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("vector transposition failed")));
	}

	if (pg_mult_matrix(&A_inv, &B_col, &result) != OK) {
		pg_remove_matrix(&A);
		pg_remove_matrix(&B);
		pg_remove_matrix(&B_col);
		pg_remove_matrix(&A_inv);
		ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("matrix multiplication failed")));
	}

	result_array = matrix_to_array(&result, element_type);

	pg_remove_matrix(&A);
	pg_remove_matrix(&B);
	pg_remove_matrix(&B_col);
	pg_remove_matrix(&A_inv);
	pg_remove_matrix(&result);

	PG_RETURN_ARRAYTYPE_P(result_array);
}
