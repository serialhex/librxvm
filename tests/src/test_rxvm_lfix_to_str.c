#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rxvm.h"
#include "test_common.h"
#include "lfix_to_str.h"

static int tests;

static void log_trs (char *msg, const char *func)
{
    fprintf(trsfp, ":test-result: %s %s #%d\n", msg, func, tests);
}

void verify_rxvm_lfix_to_str (char *regex, char *lfix, const char *modname)
{
    char *ltemp;
    char *msg;
    int ret;
    rxvm_t compiled;

    if ((ret = rxvm_compile(&compiled, regex)) < 0) {
        fprintf(logfp, "Failed to compile expression %s\n", regex);
        log_trs("FAIL", modname);
        return;
    }

    ltemp = lfix_to_str(regex, &compiled);

    if ((ltemp == NULL && lfix != NULL) || ltemp && strcmp(ltemp, lfix) != 0) {
        fprintf(logfp, "\nFail: Incorrect longest-fixed-string "
                       "generated by lfix_to_str, for expression:\n%s\n\n",
                       regex);
        fprintf(logfp, "expected:\n\t%s\n\n", lfix);
        fprintf(logfp, "seen:\n\t%s\n\n", (ltemp) ? ltemp : "(NULL)");
        msg = "FAIL";
    } else {
        msg = "PASS";
    }

    if (ltemp != NULL) {
        free(ltemp);
    }

    rxvm_free(&compiled);
    log_trs(msg, modname);
    printf("%s: %s #%i\n", msg, modname, ++tests);
}

void test_rxvm_lfix_to_str (void)
{
    tests = 0;

    verify_rxvm_lfix_to_str("AaBb+", "AaBb", __func__);
    verify_rxvm_lfix_to_str("ab+c", "ab", __func__);
    verify_rxvm_lfix_to_str("abc?d", "ab", __func__);
    verify_rxvm_lfix_to_str("ab*c", NULL, __func__);

    verify_rxvm_lfix_to_str("abcd{,5}c", "abc", __func__);
    verify_rxvm_lfix_to_str("abcd{0,}c", "abc", __func__);
    verify_rxvm_lfix_to_str("abcd{1,}c", "abcd", __func__);
    verify_rxvm_lfix_to_str("ab{123,}", "ab", __func__);
    verify_rxvm_lfix_to_str("ab{1234,}123", "123", __func__);

    verify_rxvm_lfix_to_str("abc(defg)", "abc", __func__);
    verify_rxvm_lfix_to_str("abc(defg)hijk", "hijk", __func__);

    verify_rxvm_lfix_to_str("abc(d|e)fghi", "fghi", __func__);
    verify_rxvm_lfix_to_str("abc(de)hij|k", NULL, __func__);
    verify_rxvm_lfix_to_str("aaaaa|bbbbb", NULL, __func__);
    verify_rxvm_lfix_to_str("aaa|bbb|ccc|ddd", NULL, __func__);
    verify_rxvm_lfix_to_str("aaa", "aaa", __func__);
    verify_rxvm_lfix_to_str("aa\\*", "aa*", __func__);
    verify_rxvm_lfix_to_str("aaa\\*b\\?b", "aaa*b?b", __func__);

    verify_rxvm_lfix_to_str("abc(def)+ijkl", "ijkl", __func__);
    verify_rxvm_lfix_to_str("abc(def)+ijkl*", "abc", __func__);
    verify_rxvm_lfix_to_str("abc(def)+ijklm*", "ijkl", __func__);
    verify_rxvm_lfix_to_str("abcc(def)+ijkl", "abcc", __func__);

    verify_rxvm_lfix_to_str("abc(d|(e?|f|g+)|h)ijkl", "ijkl", __func__);
    verify_rxvm_lfix_to_str("abc(d|(e?|f|g+)|h)ijkl*", "abc", __func__);
    verify_rxvm_lfix_to_str("abc(d|(e?|f|g+)|h)ijklm*", "ijkl", __func__);
    verify_rxvm_lfix_to_str("abcc(d|(e?|f|g+)|h)ijkl", "abcc", __func__);

    verify_rxvm_lfix_to_str("abcd*efghi", "efghi", __func__);
    verify_rxvm_lfix_to_str("abcd\nefghi", "abcd", __func__);
    verify_rxvm_lfix_to_str("abcd[ \t\n]efghi", "abcd", __func__);
    verify_rxvm_lfix_to_str("abcd[ \t]efghi", "efghi", __func__);
    verify_rxvm_lfix_to_str("abcd(ef\n)?efghi", "abcd", __func__);
    verify_rxvm_lfix_to_str("abcc(d|(e?|f|g+)|h)ijklm", "ijklm", __func__);
    verify_rxvm_lfix_to_str("abcc(d|(e?|f\n|g+)|h)ijklm", "abcc", __func__);

    verify_rxvm_lfix_to_str("ab\\*\nefghi", "ab*", __func__);
    verify_rxvm_lfix_to_str("ab\\*\n(e|fg)hi", "ab*", __func__);
    verify_rxvm_lfix_to_str("ab\\*\\+\\?\nefghi", "ab*+?", __func__);
    verify_rxvm_lfix_to_str("abc(def)*ghi\\+", "ghi+", __func__);

    verify_rxvm_lfix_to_str("abc[defgh]", "abc", __func__);
    verify_rxvm_lfix_to_str("abc[defgh]ijkl", "ijkl", __func__);
    verify_rxvm_lfix_to_str("abc[d\\]fgh]ijkl", "ijkl", __func__);
    verify_rxvm_lfix_to_str("abc[d\\]fgh]ij\\kl", "ijkl", __func__);
}