
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// gcc -Wall -O2    myprint.c   -o myprint

int main(int argc, char **argv) 
{
    int i;

    for (i=0; i < 5; i++) {
        sleep(1);
        printf("%d\n",i+1);
    }
	
    exit(0);
}
