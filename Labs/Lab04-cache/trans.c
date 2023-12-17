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
    int a0, a1, a2, a3, a4, a5, a6, a7;
    min = M>N?N:M;
    min = 8*((int)(min/8));
    //Divide matrix into 8x8 block.
    for(i = 0; i < min; i += 8){
        for(j = 0; j < min; j += 8){
            
            //Firstly, focus on the top half of a 8x8 block in A.
            for(k = j; k < j + 4; k++){
                a0 = A[k][i];
                a1 = A[k][i+1];
                a2 = A[k][i+2];
                a3 = A[k][i+3];
                a4 = A[k][i+4];
                a5 = A[k][i+5];
                a6 = A[k][i+6];
                a7 = A[k][i+7];
                
                //Place the top-left correctly.
                B[i][k] = a0;
                B[i+1][k] = a1;
                B[i+2][k] = a2;
                B[i+3][k] = a3;
                
                //Place the top-right temporarily - use B as a 'cache'.
                B[i][k+4] = a4;
                B[i+1][k+4] = a5;
                B[i+2][k+4] = a6;
                B[i+3][k+4] = a7;
            }
        
            //con't.
            for(k = i; k < i + 4; k++){
                //Read temporary guys.
                a0 = B[k][j+4];
                a1 = B[k][j+5];
                a2 = B[k][j+6];
                a3 = B[k][j+7];
                
                //Read left-bottom of A's 8x8 block.
                a4 = A[j+4][k];
                a5 = A[j+5][k];
                a6 = A[j+6][k];
                a7 = A[j+7][k];
                
                //Place them correctly.
                B[k][j+4] = a4;
                B[k][j+5] = a5;
                B[k][j+6] = a6;
                B[k][j+7] = a7;
                B[k+4][j] = a0;
                B[k+4][j+1] = a1;
                B[k+4][j+2] = a2;
                B[k+4][j+3] = a3;
            }
            
            //Handle the remaining guys of that 8x8 block, 
            //just swapping them by diagonal.
            for(k=i+4;k<i+8;k++){
                a0 = A[j+4][k];
                a1 = A[j+5][k];
                a2 = A[j+6][k];
                a3 = A[j+7][k];
                B[k][j+4] = a0;
                B[k][j+5] = a1;
                B[k][j+6] = a2;
                B[k][j+7] = a3;
            }
        }
    }
    
    //If this is not a square matrix, do simple swap for the remaining part.
    k=0;
    if (M!=N) {
        for(i = 0; i < min; i ++)
            for(j = min; j < M; j ++){
                k = A[i][j];
                B[j][i] = k;
            }
        for (i = min; i<N; i++) {
            for (j=0; j<M;j++) {
                k = A[i][j];
                B[j][i] = k;
            }
        }
    }
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

