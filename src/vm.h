#ifndef VM_H_
#define VM_H_

#include "parser.h"
#include <stdbool.h>
#include <stddef.h>

enum instructions {
    PUSH       = 0,
    PUSH_TRUE  = 1,
    PUSH_FALSE = 2,
    POP        = 3,
    ADD        = 4,
    MIN        = 5,
    MUL        = 6,
    DIV        = 7,
    NOT        = 8,
    DEQ        = 9,
    NEQ        = 10,
    GRE        = 11,
    GRQ        = 12,
    LES        = 13,
    LEQ        = 14,
    AND        = 15,
    OR         = 16,
    DUP        = 17,
    DUP_LOC    = 18,
    DUP_REG    = 19,
    CHANGE     = 20,
    CHANGE_REG = 21,
    CHANGE_LOC = 22,
    JMP_NOT    = 23,
    JMP        = 24,
    CALL       = 25,
    RET        = 26,
    PUSH_NUM   = 27,
    GET_FIELD  = 28,
    SET_FIELD  = 29,
    RET_INS    = 30
};

#define push(o) \
    vm->stack[vm->stack_size++] = o

#define pop() \
    (&vm->stack[--vm->stack_size])

#define peek(n) \
    (&vm->stack[vm->stack_size - 1 - n])

struct var {
    const char* name;
    void* value;
    uint32_t scope;
    int32_t stack_index;
};

struct vm;
typedef struct object (*builtin_fun)(struct vm* vm);
typedef struct object (*builtin_method)(struct object self, struct vm* vm);

struct function {
    const char* name;
    uint32_t n_parameters;
    uint32_t index;
    builtin_fun fun;
};

struct method {
    const char* name;
    builtin_method method;
};

struct class_ {
    const char* name;
    uint32_t index;

    const char* members[20];
    uint32_t n_members;

    struct method methods[20];
    uint32_t n_methods;
};

enum object_type {
    OBJ_NUMBER   = 0,
    OBJ_STRING   = 1,
    OBJ_BOOL     = 2,
    OBJ_FUNCTION = 3,
    OBJ_METHOD   = 4,
    OBJ_INSTANCE = 5,
    OBJ_CLASS    = 6,
    OBJ_USER     = 7
};

enum implementation_type {
    USER     = 0,
    BUILT_IN = 1
};

struct object_function {
    int32_t type;

    int32_t index;
    int32_t n_parameters;
};

struct object {
    int32_t type;

    union {
        int32_t int_value;
        const char* str_value;
        bool bool_value;
        struct object_function* function_value;
        struct object_method* method_value;
        struct object_instance* instance_value;
        struct object_class*    class_value;
        void* user_value;
    };
};

struct object_method {
    int32_t type;

    int32_t index;
    int32_t n_parameters;

    struct object context;
};

struct object_instance {
    int32_t type;

    union {
        struct object_class* class_index;
        struct class_* buintin_index;
    };

    struct object members[256];
};

struct pair {
    const char* name;
    int32_t index;
    uint32_t n_parameters;
};

struct object_class {
    int32_t type;
    uint32_t index;

    const char* name;

    const char* members[10];
    uint32_t n_members;

    struct pair methods[10];
    uint32_t n_methods;
};

struct evaluator {
    struct function functions[1024];
    uint32_t n_functions;

    struct class_ classes[256];
    uint32_t n_classes;

    struct var locals[1024];
    uint32_t n_locals;
};

struct binary_data {
    uint8_t constants_bytes[2048];
    uint32_t n_constants_bytes;

    uint8_t classes[2048];
    uint32_t n_classes;

    uint8_t program_bytes[2048];
    uint32_t n_program_bytes;
};

int add_builtin_functions(struct evaluator* e);
int add_builtin_classes(struct evaluator* e);
int evaluate(struct evaluator* e, struct node* ast, struct binary_data* data, uint32_t* current_stack_index, uint32_t function_scope, int32_t current_scope);

struct vm {
    struct function* builtin_functions;
    struct class_* builtin_classes;

    uint32_t globals[2048];
    struct object stack[2048];

    uint8_t* bytes;
    uint32_t n_bytes;

    uint32_t start_address;

    uint32_t stack_size;
    uint32_t stack_base;
    uint32_t program_counter;
    struct object registers[10];

    bool halt;
};

int execute(struct vm* vm);

#endif
