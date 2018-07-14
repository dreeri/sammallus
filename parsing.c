#include <editline/readline.h>
#include "mpc.h"

typedef struct lisp_value {
    int type;
    long number;
    char* error;
    char* symbol;
    int count;
    struct lisp_value** cell;
} lisp_value;

enum { VALUE_NUMBER, VALUE_ERROR, VALUE_SYMBOL, VALUE_S_EXPRESSION };

lisp_value* lisp_value_number(long x) {
    lisp_value* value = malloc(sizeof(lisp_value));
    value->type = VALUE_NUMBER;
    value->number = x;
    return value;
}

lisp_value* lisp_value_error(char* message) {
    lisp_value* value = malloc(sizeof(lisp_value));
    value->type = VALUE_ERROR;
    value->error = malloc(strlen(message) + 1);
    strcpy(value->error, message);
    return value;
}

lisp_value* lisp_value_symbol(char* symbol) {
    lisp_value* value = malloc(sizeof(lisp_value));
    value->type = VALUE_SYMBOL;
    value->symbol = malloc(strlen(symbol) + 1);
    strcpy(value->symbol, symbol);
    return value;
}

lisp_value* lisp_value_s_expression(void) {
    lisp_value* value = malloc(sizeof(lisp_value));
    value->type = VALUE_S_EXPRESSION;
    value->count = 0;
    value->cell = NULL;
    return value;
}

void lisp_value_delete(lisp_value* value) {
    switch (value->type) {
        case VALUE_NUMBER:
            break;
        case VALUE_ERROR:
            free(value->error);
            break;
        case VALUE_SYMBOL:
            free(value->symbol);
            break;
        case VALUE_S_EXPRESSION:
            for(int i = 0; i < value->count; i++) {
                lisp_value_delete(value->cell[i]);
            }
            free(value->cell);
            break;
    }
    free(value);
}

lisp_value* lisp_value_read_number(mpc_ast_t* tree) {
    errno = 0;
    long x = strtol(tree->contents, NULL, 10);
    return errno != ERANGE ?
        lisp_value_number(x) : lisp_value_error("Invalid number. Internal datatype is a long int, so stay below signed 2^32 range.\n");
}

lisp_value* lisp_value_add(lisp_value* value, lisp_value* child) {
    value->count++;
    value->cell = realloc(value->cell, sizeof(lisp_value*) * value->count);
    value->cell[value->count - 1] = child;
    return value;
}

lisp_value* lisp_value_read(mpc_ast_t* tree) {
    if(strstr(tree->tag, "number")) {
        return lisp_value_read_number(tree);
    }
    if(strstr(tree->tag, "symbol")) {
        return lisp_value_symbol(tree->contents);
    }

    lisp_value* expressions = NULL;
    if(strcmp(tree->tag, ">") == 0) {
        expressions = lisp_value_s_expression();
    }
    if(strstr(tree->tag, "s_expression")) {
        expressions = lisp_value_s_expression();
    }

    for(int i = 0; i < tree->children_num; i++) {
        if(strcmp(tree->children[i]->contents, "(") == 0) {
            continue;
        }
        if(strcmp(tree->children[i]->contents, ")") == 0) {
            continue;
        }
        if(strcmp(tree->children[i]->tag, "regex") == 0) {
            continue;
        }
        expressions = lisp_value_add(expressions, lisp_value_read(tree->children[i]));
    }

    return expressions;
}

//lisp_value_print forward declaration
void lisp_value_print(lisp_value* value);

void lisp_value_expression_print(lisp_value* value, char open, char close) {
    putchar(open);
    for(int i = 0; i < value->count; i++) {
        lisp_value_print(value->cell[i]);
        if(i != (value->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lisp_value_print(lisp_value* value) {
    switch (value->type) {
        case VALUE_NUMBER:
            printf("%li", value->number);
            break;
        case VALUE_ERROR:
            printf("Error: %s", value->error);
            break;
        case VALUE_SYMBOL:
            printf("%s", value->symbol);
            break;
        case VALUE_S_EXPRESSION:
            lisp_value_expression_print(value, '(', ')');
            break;
    }
}

void lisp_value_print_line(lisp_value* value) {
    lisp_value_print(value);
    putchar('\n');
}

lisp_value* lisp_value_pop(lisp_value* value, int i) {
    lisp_value* child = value->cell[i];
    memmove(&value->cell[i], &value->cell[i + 1], sizeof(lisp_value*) * (value->count - i - 1));
    value->count--;
    value->cell = realloc(value->cell, sizeof(lisp_value*) * value->count);
    return child;
}

lisp_value* lisp_value_take(lisp_value* value, int i) {
    lisp_value* popped_value = lisp_value_pop(value, i);
    lisp_value_delete(value);
    return popped_value;
}

lisp_value* builtin_operator(lisp_value* value, char* operator) {
    for(int i = 0; i < value->count; i++) {
        if(value->cell[i]->type != VALUE_NUMBER) {
            lisp_value_delete(value);
            return lisp_value_error("Cannot operate on non-numbers.");
        }
    }

    lisp_value* first_element = lisp_value_pop(value, 0);

    if((strcmp(operator, "-") == 0) && value->count == 0) {
        first_element->number = -first_element->number;
    }

    while(value->count > 0) {
        lisp_value* next_element = lisp_value_pop(value, 0);
        if(strcmp(operator, "+") == 0) {
            first_element->number = first_element->number + next_element->number;
        }
        if(strcmp(operator, "-") == 0) {
            first_element->number = first_element->number - next_element->number;
        }
        if(strcmp(operator, "*") == 0) {
            first_element->number = first_element->number * next_element->number;
        }
        if(strcmp(operator, "/") == 0) {
            if(next_element->number == 0) {
                lisp_value_delete(first_element);
                lisp_value_delete(next_element);
                first_element = lisp_value_error("Division by zero.");
                break;
            }
            first_element->number = first_element->number / next_element->number;
        }
    }
    lisp_value_delete(value);
    return first_element;
}

//lisp_value_eval forward decleration
lisp_value* lisp_value_eval(lisp_value* value);

lisp_value* lisp_value_evaluate_s_expression(lisp_value* value) {
    for(int i = 0; i < value->count; i++) {
        value->cell[i] = lisp_value_eval(value->cell[i]);
    }

    for(int i = 0; i < value->count; i++) {
        if(value->cell[i]->type == VALUE_ERROR) {
            return lisp_value_take(value, i);
        }
    }

    if(value->count == 0) {
        return value;
    }

    if(value->count == 1) {
        return lisp_value_take(value, 0);
    }

    lisp_value* first = lisp_value_pop(value, 0);
    if(first->type != VALUE_SYMBOL) {
        lisp_value_delete(first);
        lisp_value_delete(value);
        return lisp_value_error("S-expression does not start with a symbol.");
    }

    lisp_value* result = builtin_operator(value, first->symbol);
    lisp_value_delete(first);
    return result;
}

lisp_value* lisp_value_eval(lisp_value* value) {
    if(value->type == VALUE_S_EXPRESSION) {
        return lisp_value_evaluate_s_expression(value);
    }
    return value;
}

int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* S_Expression = mpc_new("s_expression");
    mpc_parser_t* Expression = mpc_new("expression");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        " \
        number : /-?[0-9]+/; \
        symbol : '+' | '-' | '*' | '/'; \
        s_expression : '(' <expression>* ')'; \
        expression : <number> | <symbol> | <s_expression>; \
        lispy : /^/ <expression>* /$/; \
        ",
        Number, Symbol, S_Expression, Expression, Lispy);

    puts("Sammallus Version 0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {
        char* input = readline("lispy> ");
        add_history(input);

        mpc_result_t result;
        if(mpc_parse("<stdin>", input, Lispy, &result)) {
            lisp_value* evalued_result = lisp_value_eval(lisp_value_read(result.output));
            lisp_value_print_line(evalued_result);
            lisp_value_delete(evalued_result);
            mpc_ast_delete(result.output);
        } else {
            mpc_err_print(result.error);
            mpc_err_delete(result.error);
        }

        free(input);
    }

    mpc_cleanup(5, Number, Symbol, S_Expression, Expression, Lispy);

    return 0;
}
