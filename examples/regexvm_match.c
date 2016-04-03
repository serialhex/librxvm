#include <stdio.h>
#include <stdlib.h>
#include "regexvm.h"

char *rgx;
char *input;

static void regexvm_print_err (int err);

int main (int argc, char *argv[])
{
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
    if (regexvm_match(&compiled, argv[2], 0)) {
        printf("Match!\n");
    } else {
        printf("No match.\n");
        ret = 1;
    }

    regexvm_free(&compiled);
    return ret;
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
        case RVM_BADPAREN:
            msg = "Unexpected parenthesis group closing character";
        break;
        case RVM_EPAREN:
            msg = "Unterminated parenthesis group";
        break;
        case RVM_ECLASS:
            msg = "Unterminated character class";
        break;
        case RVM_ETRAIL:
            msg = "Trailing escape character";
        break;
        case RVM_EMEM:
            msg = "Failed to allocate memory";
        break;
        case RVM_ENEST:
            msg = "Too many nested parenthesis groups";
        break;
        case RVM_ECLASSLEN:
            msg = "Too many elements in character class";
        break;
        case RVM_EINVAL:
            msg = "Unrecognised symbol";
        break;
        default:
            msg = "Unrecognised error code";
    }

    printf("Error %d: %s\n", err, msg);
}
