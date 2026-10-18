CREATE EXTENSION pg_array_linalg;

SELECT array_dot(ARRAY[1,2,3], ARRAY[4,5,6]) AS result;
SELECT array_dot(ARRAY[-1, 2, -3], ARRAY[4, -5, 6]) AS result;
SELECT array_dot(ARRAY[1.5, 2.5], ARRAY[2.0, 4.0]) AS result;
SELECT array_dot(NULL, ARRAY[1,2,3]) AS result;
SELECT array_dot(ARRAY[1,2,3], NULL) AS result;
SELECT array_dot(ARRAY[1,2,3], ARRAY[4,5]) AS result;
SELECT array_dot(ARRAY[[1,2],[3,4]], ARRAY[[5,6],[7,8]]) AS result;

SELECT array_matmul(ARRAY[[1,2],[3,4]], ARRAY[[5,6],[7,8]]) AS result;
SELECT array_matmul(ARRAY[[1,2,3],[4,5,6]], ARRAY[[7,8],[9,10],[11,12]]) AS result;
SELECT array_matmul(ARRAY[[1,0],[0,1]], ARRAY[[5,6],[7,8]]) AS result;
SELECT array_matmul(NULL, ARRAY[[1,2],[3,4]]) AS result;
SELECT array_matmul(ARRAY[[1,2],[3,4]], NULL) AS result;
SELECT array_matmul(ARRAY[[1,2],[3,4]], ARRAY[[1,2,3],[4,5,6],[7,8,9]]) AS result;

SELECT array_transpose(ARRAY[[1,2,3],[4,5,6]]) AS result;
SELECT array_transpose(ARRAY[[1,2],[3,4]]) AS result;
SELECT array_transpose(ARRAY[1,2,3]) AS result;
SELECT array_transpose(NULL) AS result;

SELECT array_det(ARRAY[[5]]) AS result;
SELECT array_det(ARRAY[[1,2],[3,4]]) AS result;
SELECT array_det(ARRAY[[1,2,3],[4,5,6],[7,8,9]]) AS result;
SELECT array_det(ARRAY[[1,0],[0,1]]) AS result;
SELECT array_det(NULL) AS result;
SELECT array_det(ARRAY[[1,2,3],[4,5,6]]) AS result;

SELECT array_inv(ARRAY[[2,0],[0,2]]) AS result;
SELECT array_inv(ARRAY[[1,0],[0,1]]) AS result;
SELECT array_inv(ARRAY[[4,7],[2,6]]) AS result;
SELECT array_matmul(ARRAY[[2,0],[0,2]], array_inv(ARRAY[[2,0],[0,2]])) AS result;
SELECT array_inv(NULL) AS result;
SELECT array_inv(ARRAY[[1,2],[2,4]]) AS result;
SELECT array_inv(ARRAY[[1,2,3],[4,5,6]]) AS result;

SELECT array_solve(ARRAY[[2,1],[1,1]], ARRAY[5,3]) AS result;
SELECT array_solve(ARRAY[[1,0],[0,1]], ARRAY[5,7]) AS result;
SELECT array_solve(ARRAY[[2,0],[0,3]], ARRAY[4,9]) AS result;
SELECT array_solve(NULL, ARRAY[1,2]) AS result;
SELECT array_solve(ARRAY[[1,2],[3,4]], NULL) AS result;
SELECT array_solve(ARRAY[[1,2,3],[4,5,6]], ARRAY[1,2]) AS result;
SELECT array_solve(ARRAY[[1,2],[3,4]], ARRAY[1,2,3]) AS result;
SELECT array_solve(ARRAY[[1,2],[2,4]], ARRAY[1,2]) AS result;

DROP EXTENSION pg_array_linalg;
