#include <stdio.h>
#include <stdlib.h>
#include "regexvm.h"

static void print_substring (char *start, char  *end);
static void regexvm_print_err (int err);

int main (int argc, char *argv[])
{
    char *start;
    char *end;
    int ret;
    regexvm_t compiled;

    ret = 0;
    if (argc != 3) {
        printf("Usage: %s <regex> <input>\n", argv[0]);
        exit(1);
    }

    /* Compile the expression */
    if ((ret = regexvm_compile(&compiled, argv[1])) < 0) {
        regexvm_print_err(ret);
        exit(ret);
    }

    /* print the compiled expression (VM instructions) */
    regexvm_print(&compiled);

    /* Check if input string matches expression */
    if (regexvm_search(&compiled, argv[2], &start, &end, 0)) {
        printf("Match!\n");
        printf("Found matching substring:\n");
        print_substring(start, end);
    } else {
        printf("No match.\n");
        ret = 1;
    }

    regexvm_free(&compiled);
    return ret;
}

void print_substring (char *start, char *end)
{
    while (start != end) {
        printf("%c", *start);
        ++start;
    }
    printf("\n");
}

void regexvm_print_err (int err)
{
    const char *msg;

    switch (err) {
        case RVM_BADOP:
            msg = "Operator used incorrectly";
        break;
        case RVM_BADCLASS:
            msg = "Unexpected character class closing character";
        break;
        case RVM_BADREP:
            msg = "Unexpected closing repetition character";
        break;
        case RVM_BADPAREN:
            msg = "Unexpected parenthesis group closing character";
        break;
        case RVM_EPAREN:
            msg = "Unterminated parenthesis group";
        break;
        case RVM_ECLASS:
            msg = "Unterminated character class";
        break;
        case RVM_EREP:
            msg = "Missing repetition closing character";
        break;
        case RVM_MREP:
            msg = "Empty repetition";
        break;
        case RVM_ETRAIL:
            msg = "Trailing escape character";
        break;
        case RVM_EMEM:
            msg = "Failed to allocate memory";
        break;
        case RVM_EINVAL:
            msg = "Invalid symbol";
        break;
        default:
            msg = "Unrecognised error code";
    }

    printf("Error %d: %s\n", err, msg);
}