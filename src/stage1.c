/*
 * The MIT License (MIT)
 * Copyright (c) 2016 Erik K. Nyquist
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "regexvm_err.h"
#include "regexvm_common.h"
#include "stack.h"

#define ISOP(x) \
((x == ONE || x == ZERO || x == ONEZERO || x == ALT) ? 1 : 0)

/* pointers for the start & end of text in
 * input string for the current token */
extern char *lp1;
extern char *lpn;

enum {STATE_START, STATE_CHARC};

/* set_op functions:
 * a bunch of convenience functions for populating
 * inst_t types for all instructions */
static void set_op_char (inst_t *inst, char c)
{
    inst->op = OP_CHAR;
    inst->c = c;
    inst->ccs = NULL;
    inst->x = inst->y = 0;
}

static void set_op_class (inst_t *inst, char *ccs)
{
    inst->op = OP_CLASS;
    inst->c = 0;
    inst->ccs = ccs;
    inst->x = inst->y = 0;
}

static void set_op_any (inst_t *inst)
{
    inst->op = OP_ANY;
    inst->c = 0;
    inst->ccs = NULL;
    inst->x = inst->y = 0;
}

static void set_op_branch (inst_t *inst, int x, int y)
{
    inst->op = OP_BRANCH;
    inst->c = 0;
    inst->ccs = NULL;
    inst->x = x;
    inst->y = y;
}

static void set_op_jmp (inst_t *inst, int x)
{
    inst->op = OP_JMP;
    inst->c = 0;
    inst->y = 0;
    inst->x = x;
    inst->ccs = NULL;
}

static void set_op_match (inst_t *inst)
{
    inst->op = OP_MATCH;
    inst->c = 0;
    inst->ccs = NULL;
    inst->x = inst->y = 0;
}

/* create_inst: allocate space for an inst_t, populate it
   with the data in 'inst' and return a pointer to it */
static inst_t *create_inst (inst_t *inst)
{
    inst_t *new;

    if ((new = malloc(sizeof(inst_t))) == NULL)
        return NULL;

    memset(new, 0, sizeof(inst_t));

    new->op = inst->op;
    new->c = inst->c;
    new->x = inst->x;
    new->y = inst->y;
    new->ccs = inst->ccs;

    return new;
}

static stackitem_t *stack_add_inst_head (stack_t *stack, inst_t *inst)
{
    return stack_add_head(stack, (void *) create_inst(inst));
}

static stackitem_t *stack_add_inst_tail (stack_t *stack, inst_t *inst)
{
    return stack_add_tail(stack, (void *) create_inst(inst));
}

/* stack_cat_from_item: append items from a stack, starting from
 * stack item 'i', until stack item 'stop' is reached, onto stack1. */
static void stack_cat_from_item (stack_t *stack1, stackitem_t *stop,
                                 stackitem_t *i)
{
    while (1) {
        stack_point_new_head(stack1, i);
        if (i == stop) break;
        i = i->previous;
    }
}

/* when an alternation operator "|" is seen, it leaves a 'jmp'
 * instructuction with an unset pointer. This pointer marks the end of
 * that alternation, and attach_dangling_alt is called to set this
 * pointer when 1) another "| alt. token is seen, 2) a right-paren.
 * token is seen, or 3) the end of the input string is reached. */
static void attach_dangling_alt (context_t *cp)
{
    inst_t *inst;

    if (cp->target->dangling_alt == NULL) {
        return;
    } else {
        inst = (inst_t *) cp->target->dangling_alt->data;
        inst->x = (cp->target->size - cp->target->dsize) + 1;

        cp->target->dangling_alt = NULL;
    }
}

static void stack_reset (stack_t *stack)
{
    if (stack != NULL) {
        stack->size = 0;
        stack->head = NULL;
        stack->tail = NULL;
    }
}

static void inst_stack_cleanup (void *data)
{
    inst_t *inst;

    inst = (inst_t *) data;

    if (inst->ccs != NULL)
        free(inst->ccs);

    free(inst);
}

static void stack_stack_cleanup (void *data)
{
    stack_t *stack;

    stack = (stack_t *) data;
    stack_free(stack, inst_stack_cleanup);
}

/* process_op: process operator 'tok' against a buffer of
 * literals 'buf', where the first operand is 'operand', and
 * append the generated instructions to 'target' */
static int process_op (context_t *cp)
{
    stackitem_t *i;
    stackitem_t *x;
    stackitem_t *y;
    stack_t *cur;
    unsigned int size;
    inst_t inst;

    if (cp->buf == NULL || cp->buf->head == NULL) {
        if (cp->tok == ALT) {
            cur = (stack_t *) cp->parens->head->data;
            size = cur->size;
            i = NULL;
        } else {
            return RVM_BADOP;
        }
    } else {
        size = cp->buf->size;
        i = cp->buf->tail;

        /* Add all literals from current buffer onto output, until
         * the start of the operands is reached. */
        while (i != cp->operand) {
            stack_point_new_head(cp->target, i);
            i = i->previous;
            size--;
        }
    }

    x = NULL;
    y = NULL;

    /* Generate instructions for operator & operand(s) */
    switch (cp->tok) {
        case ONE:
            /* x = current position MINUS size of operand buf
             * y = current position PLUS 1 */
            set_op_branch(&inst, -(size), 1);

            stack_cat_from_item(cp->target, cp->buf->head, i);
            if (stack_add_inst_head(cp->target, &inst) == NULL)
                return RVM_EMEM;
        break;
        case ZERO:
            /* x = current position PLUS size of operand buf PLUS 2
             * y = current position PLUS 1 */
            set_op_branch(&inst, size + 2, 1);

            x = stack_add_inst_head(cp->target, &inst);
            stack_cat_from_item(cp->target, cp->buf->head, i);
            set_op_branch(&inst, 1, -(size));
            y = stack_add_inst_head(cp->target, &inst);

            if (x == NULL || y == NULL)
                return RVM_EMEM;
        break;
        case ONEZERO:
            /* x = current position PLUS 1
             * y = current position PLUS size of operand buf PLUS 1 */
            set_op_branch(&inst, 1, size + 1);

            if (stack_add_inst_head(cp->target, &inst) == NULL)
                return RVM_EMEM;

            stack_cat_from_item(cp->target, cp->buf->head, i);
        break;
        case ALT:
            if (i != NULL)
                stack_cat_from_item(cp->target, cp->buf->head, i);

            attach_dangling_alt(cp);

            /* x = current position PLUS 1
             * y = current position PLUS size of target buf PLUS 2 */
            set_op_branch(&inst, 1, cp->target->size + 2);

            x = stack_add_inst_tail(cp->target, &inst);
            set_op_jmp(&inst, 0);
            cp->target->dangling_alt = stack_add_inst_head(cp->target, &inst);
            cp->target->dsize = cp->target->size;

            if (x == NULL || cp->target->dangling_alt == NULL)
                return RVM_EMEM;

        break;
    }

    stack_reset(cp->buf);
    if (cp->lasttok == RPAREN) {
        free(cp->buf);
        stack_free_head(cp->parens);
    }

    cp->buf = (stack_t *) cp->parens->tail->data;

    return 0;
}

/* stage1_main_state: main logic for adding incoming literals to a stack
 * (multiple stacks, if parenthesis groups are used) so that process_op()
 * can operate on them, should any operators be seen. */
static int stage1_main_state (context_t *cp, int *state)
{
    stack_t *new;
    stack_t *cur;
    stack_t *base;
    inst_t inst;

    if (ISOP(cp->tok)) {
        return process_op(cp);
    }

    /* in the event that a parenthesis group is closed, and the following
     * token is not an operator, the buffer for the just-closed group needs
     * to be flushed into cp->target (otherwise done by process_op()) */
    if (cp->lasttok == RPAREN && cp->buf->size > 0) {
        stack_cat(cp->target, cp->buf);
        free(cp->buf);
        stack_free_head(cp->parens);
        cp->buf = (stack_t *) cp->parens->tail->data;

    }

    base = (stack_t *) cp->parens->tail->data;

    switch (cp->tok) {
        case LITERAL:
            set_op_char(&inst, *lp1);
            cp->operand = stack_add_inst_head(cp->buf, &inst);
        break;
        case ANY:
            set_op_any(&inst);
            cp->operand = stack_add_inst_head(cp->buf, &inst);
        break;
        case CHARC_OPEN:
            *state = STATE_CHARC;

            if ((cp->ccs = malloc(CHARC_BLOCK_SIZE)) == NULL)
                return RVM_EMEM;

            cp->cspace = CHARC_BLOCK_SIZE;
        break;
        case LPAREN:

            stack_cat(cp->target, base);
            stack_reset(base);


            if ((new = create_stack()) == NULL)
                    return RVM_EMEM;

            stack_add_head(cp->parens, (void *) new);
            cp->target = new;
        break;
        case RPAREN:
            if (cp->parens->head == cp->parens->tail) {
                return RVM_BADPAREN;

            } else {
                stack_cat(cp->target, base);
                attach_dangling_alt(cp);

                stack_reset(base);

                cur = (stack_t *) cp->parens->head->data;
                cp->operand = cur->tail;
                cp->buf = cur;
                cp->target = (cp->parens->head->next == cp->parens->tail) ?
                    cp->prog : (stack_t *) cp->parens->head->next->data;
            }
        break;
        case CHARC_CLOSE:
            return RVM_BADCLASS;
        break;
    }

    return 0;
}

static int charc_enlarge (context_t *cp, size_t size)
{
    char *new;

    new = realloc(cp->ccs, cp->cspace + size);

    if (new == NULL) {
        return RVM_EMEM;
    }

    cp->ccs = new;
    cp->cspace += size;
    return 0;
}

static int expand_char_range (context_t *cp)
{
    char rhi;
    char rlo;
    int err;
    size_t size;
    size_t delta;

    /* Figure out which character is highest */
    if (*lp1 > *(lp1 + 2)) {
        rhi = *lp1;
        rlo = *(lp1 + 2);
    } else {
        rhi = *(lp1 + 2);
        rlo = *lp1;
    }

    /* No. of characters in range */
    size = (size_t) rhi - rlo;

    /* Enlarge char. class buffer if needed */
    if ((cp->clen + size + 1) > cp->cspace) {
        delta = (cp->clen + size + 1) - cp->cspace;
        if ((err = charc_enlarge(cp, delta)) < 0)
            return err;
    }

    /* Expand range */
    while (rlo <= rhi) {
        cp->ccs[cp->clen++] = rlo++;
    }

    return 0;
}

/* stage1_charc_state: called repeatedly when the character class open token
 * "[" is seen, consuming tokens until the closing character "]" is seen.
 * Generates a single "class" instruction which is added to the shared
 * buffer parens[0]. */
static int stage1_charc_state (context_t *cp, int *state)
{
    int ret;
    inst_t inst;

    /* Enlarge char. class buffer if it is full */
    if ((cp->clen + 1) >= cp->cspace) {
        if ((ret = charc_enlarge(cp, CHARC_BLOCK_SIZE)) < 0)
            return ret;
    } else {
        ret = 0;
    }

    switch (cp->tok) {
        case LITERAL:
            cp->ccs[cp->clen++] = *lp1;
        break;
        case CHAR_RANGE:
            ret = expand_char_range(cp);
        break;
        case CHARC_CLOSE:
            cp->ccs[cp->clen] = '\0';
            set_op_class(&inst, cp->ccs);
            cp->operand =
                stack_add_inst_head((stack_t *) cp->parens->tail->data, &inst);

            cp->ccs = NULL;
            cp->clen = 0;
            *state = STATE_START;
        break;
        default:
            ret = RVM_EINVAL;
    }

    return ret;
}

static void stage1_cleanup (context_t *cp)
{
    free((stack_t *) cp->parens->tail->data);
    free(cp->parens->tail);
    free(cp->parens);
}

static void stage1_err_cleanup (context_t *cp)
{
    if (cp->ccs)
        free(cp->ccs);

    stack_free(cp->parens, stack_stack_cleanup);
    stack_free(cp->prog, inst_stack_cleanup);
}

static int stage1_init (context_t *cp, stack_t **ret)
{
    stack_t *base;

    memset(cp, 0, sizeof(context_t));

    if ((cp->parens = create_stack()) == NULL)
        return RVM_EMEM;

    if ((*ret = create_stack()) == NULL) {
        free(cp->parens);
        return RVM_EMEM;
    }

    if ((base = create_stack()) == NULL) {
        free(cp->parens);
        free(*ret);
        return RVM_EMEM;
    }

    cp->prog = *ret;
    stack_add_head(cp->parens, (void *) base);
    cp->buf = base;
    cp->target = cp->prog;
    return 0;
}

/* Stage 1 compiler: takes the input expression string,
 * runs it through the lexer and generates an IR for stage 2. */
int stage1 (char *input, stack_t **ret)
{
    int state;
    int err;

    context_t *cp;
    context_t context;
    inst_t inst;

    cp = &context;

    if ((err = stage1_init(cp, ret)) < 0)
        return err;

    state = STATE_START;
    lex_init();

    /* get the next token */
    while ((cp->tok = lex(&input)) != END) {
        if (cp->tok < 0) {
            stage1_err_cleanup(cp);
            return cp->tok;
        }

        switch (state) {
            case STATE_START:
                /* if it's not a character class, then all the
                 * metacharacters apply-- run the main state. */
                if ((err = stage1_main_state(cp, &state)) < 0) {
                    stage1_err_cleanup(cp);
                    return err;
                }

            break;
            case STATE_CHARC:
                /* inside character class-- run character class state */
                if ((err = stage1_charc_state(cp, &state)) < 0) {
                    stage1_err_cleanup(cp);
                    return err;
                }

            break;
        }
        cp->lasttok = cp->tok;
    }

    if (cp->lasttok == RPAREN && cp->buf->size > 0) {
        stack_cat(cp->prog, cp->buf);
        free(cp->buf);
        stack_free_head(cp->parens);
    }

    if (state == STATE_CHARC) {
        stage1_err_cleanup(cp);
        return RVM_ECLASS;
    } else if (cp->parens->head != cp->parens->tail) {
        stage1_err_cleanup(cp);
        return RVM_EPAREN;
    }

    /* End of input-- anything left in the buffer
     * can be appended to the output. */
    stack_cat(cp->prog, (stack_t *) cp->parens->tail->data);
    attach_dangling_alt(cp);

    /* Add the match instruction */
    set_op_match(&inst);
    stack_add_inst_head(cp->prog, &inst);

    stage1_cleanup(cp);
    return 0;
}
