/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii, jj, x, y;
    if (M == 32 && N == 32) {
        for (i=0; i < M; i+=8) {
            ii = i + 8;
            ii = ii > M ? M : ii; // can be omitted
            for (j=0; j < N; j+=8) {
                jj = j + 8;
                jj = jj > N ? N : jj; // can be omitted
                if (i == j) { 
                    /*  
                        因为A和B两个数组基地址的特殊性：每8个int映射到同一个cache set的同一个cache line
                        导致对角线位置存在较多的conflict miss,需要特殊处理
                    */
                    for (x=i; x < ii; x++) {
                        for (y=j; y < x; y++) {
                            B[y][x] = A[x][y];
                        }
                        for (y=jj-1; y >= x; y--) {
                            B[y][x] = A[x][y];
                        }
                        /*==Equivalent Loop==*/
                        // for (y=j; y < jj; y++) {
                        //     if (x == y) {
                        //         continue;
                        //     }
                        //     B[y][x] = A[x][y];
                        // }
                        // B[x][x] = A[x][x];
                    }
                } else {
                    for (x=i; x < ii; x++) {
                        for (y=j; y < jj; y++) {
                            B[y][x] = A[x][y];
                        }
                    }
                }
            }
        }
    }

    
}

// 00 04 08 0c 10 
// 0x20

//  1  2  3  4  5  6
//  7  8  9 10 11 12
// 13 14 15 16 17 18
// 19 20 21 22 23 24

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

