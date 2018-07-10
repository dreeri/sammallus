#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include "mpc.h"

typedef struct {
    int type;
    long number;
    int error;
} lisp_value;

enum { VALUE_NUMBER, VALUE_ERROR };

enum { ERROR_DIVIDE_ZERO, ERROR_BAD_OPERATOR, ERROR_BAD_NUMBER };

lisp_value lisp_value_number(long x) {
    lisp_value value;
    value.type = VALUE_NUMBER;
    value.number = x;
    return value;
}

lisp_value lisp_value_error(int x) {
    lisp_value value;
    value.type = VALUE_ERROR;
    value.error = x;
    return value;
}

void lisp_value_print(lisp_value value) {
    switch (value.type) {
        case VALUE_NUMBER:
            printf("%li\n", value.number);
            break;
        case VALUE_ERROR:
            if(value.error == ERROR_DIVIDE_ZERO) {
                printf("Error: Division By Zero.\n");
            }
            if(value.error == ERROR_BAD_OPERATOR) {
                printf("Error: Invalid Operator.\n");
            }
            if(value.error == ERROR_BAD_NUMBER) {
                printf("Error: Invalid Number.\n");
            }
            break;
    }
}

lisp_value eval_op(lisp_value x, char* op, lisp_value y) {
    if(x.type == VALUE_ERROR) {
        return x;
    }
    if(y.type == VALUE_ERROR) {
        return y;
    }

    if(strcmp(op, "+") == 0) {
        return lisp_value_number(x.number + y.number);
    }
    if(strcmp(op, "-") == 0) {
        return lisp_value_number(x.number - y.number);
    }
    if(strcmp(op, "*") == 0) {
        return lisp_value_number(x.number * y.number);
    }
    if(strcmp(op, "/") == 0) {
        return y.number == 0
                ? lisp_value_error(ERROR_DIVIDE_ZERO)
                : lisp_value_number(x.number / y.number);
    }
    return lisp_value_error(ERROR_BAD_OPERATOR);
}

lisp_value eval(mpc_ast_t* t) {
    if(strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lisp_value_number(x) : lisp_value_error(ERROR_BAD_NUMBER);
    }

    char* op = t->children[1]->contents;
    lisp_value x = eval(t->children[2]);
    int i = 3;
    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        " \
        number : /-?[0-9]+/; \
        operator : '+' | '-' | '*' | '/'; \
        expr : <number> | '(' <operator> <expr>+ ')'; \
        lispy : /^/ <operator> <expr>+ /$/; \
        ",
        Number, Operator, Expr, Lispy);

    puts("Lispy Version 0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {
        char* input = readline("lispy> ");
        add_history(input);

        mpc_result_t result;
        if(mpc_parse("<stdin>", input, Lispy, &result)) {
            lisp_value evalued_result = eval(result.output);
            lisp_value_print(evalued_result);
            mpc_ast_delete(result.output);
        } else {
            mpc_err_print(result.error);
            mpc_err_delete(result.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
