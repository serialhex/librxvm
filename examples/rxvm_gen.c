#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "librxvm/rxvm.h"

char *rgx;
char *input;

int main (int argc, char *argv[])
{
    char *gen;
    int ret;
    rxvm_t compiled;

    ret = 0;
    if (argc != 2) {
        printf("Usage: %s <regex>\n", argv[0]);
        exit(1);
    }

    srand(time(NULL));

    /* Compile the expression */
    if ((ret = rxvm_compile(&compiled, argv[1])) < 0) {
        rxvm_print_err(ret);
        exit(ret);
    }

    gen = rxvm_gen(&compiled, NULL);
    printf("%s\n", gen);
    free(gen);

    rxvm_free(&compiled);
    return ret;
}
