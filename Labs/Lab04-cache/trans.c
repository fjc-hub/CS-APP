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
void transpose_submit(int M, int N, int A[N][M], int B[M][N]){
    int i, j, k,min;
    int r0, r1, r2, r3, r4, r5, r6, r7; // values stored in register
    min = M>N?N:M;
    min = 8*((int)(min/8));
    //Divide matrix into 8x8 block.
    for(i = 0; i < min; i += 8){
        for(j = 0; j < min; j += 8){
            // Move A[i..i+3][j..j+7], load a cache line once
            for (k = i; k < i+4; k++) {
                // load a cache-line data of A into 8 register batchly
                r0 = A[k][j];
                r1 = A[k][j+1];
                r2 = A[k][j+2];
                r3 = A[k][j+3];
                r4 = A[k][j+4];
                r5 = A[k][j+5];
                r6 = A[k][j+6];
                r7 = A[k][j+7];

                // store into B[j..j+3][k]
                B[j][k] = r0;
                B[j+1][k] = r1;
                B[j+2][k] = r2;
                B[j+3][k] = r3;

                // store into B[j..j+3][k+4] temporarily to avoid conflict miss, as temporary cache
                B[j][k+4] = r4;
                B[j+1][k+4] = r5;
                B[j+2][k+4] = r6;
                B[j+3][k+4] = r7;
            }

            // Move B[j..j+3][i+4..i+7] to B[j+4..j+7][i..i+3], 1*4 elements once
            for (k = j; k < j+4; k++) {
                // load temporary cache into registers batchly
                r0 = B[k][i+4];
                r1 = B[k][i+5];
                r2 = B[k][i+6];
                r3 = B[k][i+7];

                // load A[k+4][j..j+3]
                r4 = A[i+4][k];
                r5 = A[i+5][k];
                r6 = A[i+6][k];
                r7 = A[i+7][k];

                // store from registers, pay attention to sequence of part.1 and part.2
                // part.1
                B[k][i+4] = r4;
                B[k][i+5] = r5;
                B[k][i+6] = r6;
                B[k][i+7] = r7;
                // part.2  put part.2 after part.1; 
                //  because you will operate next 4x4 block of B (B[j+4..j+7][i+4..i+7])
                B[k+4][i] = r0;
                B[k+4][i+1] = r1;
                B[k+4][i+2] = r2;
                B[k+4][i+3] = r3;
                
                // part.1  if execute part.1 after part.2 will case next 4x4 block-move more misses
                // B[k][i+4] = r4;
                // B[k][i+5] = r5;
                // B[k][i+6] = r6;
                // B[k][i+7] = r7;
            }

            // Move A[i+4..i+7][j+4..j+7] method-1
            // for (k = i+4; k < i+8; k++) {
            //     for (r0 = j+4; r0 < j+8; r0++) {
            //         if (k != r0) {
            //             B[r0][k] = A[k][r0];
            //         }
            //     }
            //     B[k][k] = A[k][k]; // move the diagonal element last
            // }

            // Move A[i+4..i+7][j+4..j+7] method-2 (with less misses than method-1 when the 4x4 blocks are in diagonal)
            for (k = i+4; k < i+8; k++) {
                r0 = A[k][j+4];
                r1 = A[k][j+5];
                r2 = A[k][j+6];
                r3 = A[k][j+7];
                B[j+4][k] = r0;
                B[j+5][k] = r1;
                B[j+6][k] = r2;
                B[j+7][k] = r3;
            }
        }
    }
    
    //If this is not a square matrix, do simple swap for the remaining part.
    
}

char transpose_submit_first_version_desc[] = "My First Version";
void transpose_submit_first_version(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii, jj, x, y, bIntSize;
    int tmp0, tmp1, tmp2, tmp3;
    if (M == 32 && N == 32) {
        bIntSize = 8;
        for (i=0; i < N; i+=bIntSize) {
            ii = i + bIntSize;
            ii = ii > N ? N : ii; // can be omitted
            for (j=0; j < M; j+=bIntSize) {
                jj = j + bIntSize;
                jj = jj > M ? M : jj; // can be omitted
                if (i == j) { 
                    /*  
                        因为A和B两个数组基地址的特殊性：每多个int映射到同一个cache set的同一个cache line
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
    } else if (M == 64 && N == 64) {
        bIntSize = 4;
        for (i=0; i < N; i+=bIntSize) {
            ii = i + bIntSize;
            ii = ii > N ? N : ii; // can be omitted
            for (j=0; j < M; j+=bIntSize) {
                jj = j + bIntSize;
                jj = jj > M ? M : jj; // can be omitted
                if (i == j) {
                    for (x=i; x < ii; x++) {
                        for (y=j; y < jj; y++) {
                            if (x == y) {
                                continue;
                            }
                            B[y][x] = A[x][y];
                        }
                        B[x][x] = A[x][x];
                    }
                } else if (i - j == 4 || j - i == 4) {
                    // stay into register
                    tmp1 = A[i+2][j];
                    tmp2 = A[i+2][j+1];
                    tmp3 = A[i+1][j];
                    
                    // move A[i+0..3][j+3]
                    tmp0 = A[i+3][j+3];
                    B[j+3][i] = A[i][j+3];
                    B[j+3][i+1] = A[i+1][j+3];
                    B[j+3][i+2] = A[i+2][j+3];
                    B[j+3][i+3] = tmp0;

                    // move A[i+0..2][j+2]
                    tmp0 = A[i+2][j+2]; 
                    B[j+2][i] = A[i][j+2];
                    B[j+2][i+1] = A[i+1][j+2];
                    B[j+2][i+2] = tmp0;

                    // move A[i+0..1][j+1]
                    tmp0 = A[i+1][j+1];
                    B[j+1][i] = A[i][j+1];
                    B[j+1][i+1] = tmp0;

                    // move A[i][j]
                    B[j][i] = A[i][j];

                    // move left

                    // acquire from register
                    B[j][i+1] = tmp3;
                    B[j][i+2] = tmp1;
                    B[j+1][i+2] = tmp2;

                    // move A[i+3][]
                    B[j][i+3] = A[i+3][j];
                    B[j+1][i+3] = A[i+3][j+1];
                    B[j+2][i+3] = A[i+3][j+2];

                } else {
                    for (x=i; x < ii; x++) {
                        for (y=j; y < jj; y++) {
                            B[y][x] = A[x][y];
                        }
                    }
                }
            }
        }
    
    } else if (M == 61 && N == 67) {
                
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

    registerTransFunction(transpose_submit_first_version, transpose_submit_first_version_desc); 

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

