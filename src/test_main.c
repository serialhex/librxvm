#include <stdio.h>
#include <stdlib.h>
#include "regexvm.h"

int main (int argc, char *argv[])
{
    int ret;
    regexvm_t compiled;

    if (argc != 3) {
        printf("Usage: %s <regex> <input>\n", argv[0]);
        exit(1);
    }

    if ((ret = regexvm_compile(&compiled, argv[1])) < 0) {
        regexvm_print_err(ret);
        exit(ret);
    }

    printf("compiled size: %d\n", sizeof(inst_t) * compiled.size);

    if (regexvm_match(&compiled, argv[2])) {
        printf("Match!\n");
    } else {
        printf("No match.\n");
    }

    regexvm_free(&compiled);
    return 0;
}
