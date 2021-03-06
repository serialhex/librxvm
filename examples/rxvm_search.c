#include <stdio.h>
#include <stdlib.h>
#include "librxvm/rxvm.h"

void print_substring (char *start, char *end)
{
    while (*start) {
        printf("%c", *start);
        if (start == end) break;
        ++start;
    }
    printf("\n");
}

int main (int argc, char *argv[])
{
    char *start;
    char *end;
    int ret;
    char *input;
    rxvm_t compiled;

    if (argc != 3) {
        printf("Usage: %s <regex> <input>\n", argv[0]);
        return 1;
    }

    input = argv[2];
    ret = 0;

    /* Compile the expression */
    if ((ret = rxvm_compile(&compiled, argv[1])) < 0) {
#ifndef NOEXTRAS
        rxvm_print_err(ret);
#endif
        return ret;
    }

    /* Look for occurrences of expression in the input string */
    while (rxvm_search(&compiled, input, &start, &end, RXVM_MULTILINE)) {
        printf("Found match: ");
        print_substring(start, end);

        /* Reset input pointer to after the end of the last match */
        input = end + 1;
    }

    printf("Done.\n");
    rxvm_free(&compiled);
    return 0;
}
