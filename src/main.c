
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "lexer.h"
#include "utils.h"
#include "string.h"
#include "parser.h"

#include "stdlib/functions.h"
#include "stdlib/classes.h"

#include "vm.h"

#define print_help() \
{ \
    printf("usage: %s <file_name> [-a] [-b] [-h]\n", argv[0]); \
    printf("help:\n"); \
    printf("\t<file_name> : file with code to execute\n"); \
    printf("\t-a          : dump the abstract syntax tree\n"); \
    printf("\t-b          : dump the generated bytecodes\n"); \
    printf("\t-h          : not execute the program\n"); \
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    bool print_ast = false;
    bool print_bytecode = false;
    bool execute_program = true;

    // parse flags
    int index = 2;
    while (index < argc) {
        const char* current_arg = argv[index];
        if (strlen(current_arg) != 2) {
            print_help();
            return 1;
        }

        if (current_arg[0] != '-') {
            print_help();
            return 1;
        }

        switch (current_arg[1]) {
            case 'a':
                print_ast = true;
                break;

            case 'b':
                print_bytecode = true;
                break;

            case 'h':
                execute_program = false;
                break;
            default:
                print_help();
                return 1;
        }

        ++index;
    }

    const char * file_name = argv[1];
    FILE* f = fopen(file_name, "r");
    if (!f) {
        printf("%s not existent\n", file_name);
        return 1;
    }

    char text[2048];
    size_t size = fread(text, sizeof(char), sizeof(text), f);
    fclose(f);

    text[size] = '\0';

    struct token* tokens = NULL;
    uint32_t n_tokens;
    int res = tokenize(text, strlen(text), &tokens, &n_tokens);
    if (res != 0) {
        ERROR("failed to tokenize");
        return 1;
    }

    struct parser parser = {
        .text = text,
        .tokens = tokens,
        .n_tokens = n_tokens
    };
    struct node* ast = NULL;

    res = parse(&parser, &ast);
    if (res != 0) {
        ERROR("failed to parse");

        const struct token* token = &parser.tokens[parser.current_index];
        ERROR("at line %d", token->line);
        print_program_error(parser.text, token->index);

        return 1;
    }

    if (print_ast) {
        dump_ast(ast, 0);
        puts("");
    }

    struct compiler_data cd = {};
    struct binary_data d = {.n_constants_bytes = sizeof(int32_t)};
    uint32_t current_stack_index = 0;

    add_builtin_functions(&cd);
    add_builtin_classes(&cd);

    if (compile(&cd, ast, &d, &current_stack_index, 0, -1, NULL) != 0) {
        ERROR("failed to evaluate");
        return 1;
    }

    uint8_t bytecode[4096];
    uint32_t n_bytecodes = 0;

    memcpy(bytecode, d.constants_bytes, d.n_constants_bytes);
    n_bytecodes += d.n_constants_bytes;

    uint32_t start_address = n_bytecodes;

    // program start address
    memcpy(bytecode + n_bytecodes, d.program_bytes, d.n_program_bytes);
    n_bytecodes += d.n_program_bytes;

    if (print_bytecode) {
        disassembly(bytecode, n_bytecodes, start_address);
        puts("");
    }

    if (execute_program) {
        struct vm vm = {
            .bytes = bytecode,
            .n_bytes = n_bytecodes,
            .start_address = start_address,
            .builtin_functions = cd.functions,
            .builtin_classes = cd.classes
        };

        if (execute(&vm) != 0) {
            ERROR("failed to execute");
            return 1;
        }
    }


    for (uint32_t i = 0; i < n_tokens; ++i) {
        free(tokens[i].value);
    }

    free(tokens);

    if (ast)
        node_free(ast);

    return res;
}
