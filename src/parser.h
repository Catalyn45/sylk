#ifndef PARSER_H_
#define PARSER_H_

#include "lexer.h"
#include "ast.h"

enum node_type {
    NODE_NUMBER = 0,
    NODE_STRING = 1,
    NODE_BINARY_OP = 2,
    NODE_IF = 3,
    NODE_WHILE = 4,
    NODE_ASSIGN = 5,
    NODE_STATEMENT = 6,
    NODE_VAR = 7,
    NODE_FUNCTION_CALL = 8,
    NODE_NOT = 9,
    NODE_FUNCTION = 10,
    NODE_DECISION = 11,
    NODE_PARAMETER = 12,
    NODE_ARGUMENT = 13,
    NODE_RETURN = 14,
    NODE_EXP_STATEMENT = 15,
    NODE_BOOL = 16,
    NODE_MEMBER = 17,
    NODE_INDEX = 18,
    NODE_DECLARATION = 19,
    NODE_METHOD = 20,
    NODE_CLASS = 21
};

enum NODE_FLAGS {
    LVALUE   = (0x1 << 0),
    CALLABLE = (0x1 << 1)
};

struct parser {
    const char* text;
    uint32_t current_index;

    const struct token*  tokens;
    uint32_t n_tokens;

    uint32_t current_scope;
};

int parse(struct parser* parser, struct node** root );

#endif
