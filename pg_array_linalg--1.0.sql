/* contrib/pg_array_linalg/pg_array_linalg--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_array_linalg" to load this file. \quit

-- array_dot: scalar product of two vectors
CREATE FUNCTION array_dot(anyarray, anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_dot'
LANGUAGE C
IMMUTABLE
STRICT
PARALLEL SAFE;

-- array_matmul: matrix multiplication
CREATE FUNCTION array_matmul(anyarray, anyarray)
RETURNS anyarray
AS 'MODULE_PATHNAME', 'array_matmul'
LANGUAGE C
IMMUTABLE
STRICT
PARALLEL SAFE;

-- array_transpose: matrix transpose
CREATE FUNCTION array_transpose(anyarray)
RETURNS anyarray
AS 'MODULE_PATHNAME', 'array_transpose'
LANGUAGE C
IMMUTABLE
STRICT
PARALLEL SAFE;

-- array_det: determinant of a square matrix
CREATE FUNCTION array_det(anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_det'
LANGUAGE C
IMMUTABLE
STRICT
PARALLEL SAFE;

-- array_inv: inverse matrix
CREATE FUNCTION array_inv(anyarray)
RETURNS anyarray
AS 'MODULE_PATHNAME', 'array_inv'
LANGUAGE C
IMMUTABLE
STRICT
PARALLEL SAFE;

-- array_solve: solve linear system A * x = b
CREATE FUNCTION array_solve(anyarray, anyarray)
RETURNS anyarray
AS 'MODULE_PATHNAME', 'array_solve'
LANGUAGE C
IMMUTABLE
STRICT
PARALLEL SAFE;
