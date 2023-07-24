#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include "stdlib.h"
#include "utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

static int get_variable(const char* name, struct evaluator* e, int32_t* index) {
    uint32_t i = e->n_locals;

    while (i > 0) {
        --i;

        if (strcmp(name, e->locals[i].name) == 0) {
            *index = e->locals[i].stack_index;
            return 0;
        }
    }

    return 1;
}

static void add_variable(const char* var_name, uint32_t scope, uint32_t index, struct evaluator* e) {
    e->locals[e->n_locals++] = (struct var) {
        .name = var_name,
        .scope = scope,
        .stack_index = index
    };
}

static uint32_t pop_variables(uint32_t scope, struct evaluator* e) {
    uint32_t count = 0;

    while (e->n_locals > 0 && e->locals[e->n_locals - 1].scope > scope) {
        if (e->locals[e->n_locals - 1].stack_index >= 0)
            count += 1;

        --(e->n_locals);
    }

    return count;
}

static uint32_t get_var_count(uint32_t scope, struct evaluator* e) {
    uint32_t count = 0;
    uint32_t n = e->n_locals;

    while (n > 0 && e->locals[n - 1].scope > scope) {
        if (e->locals[n - 1].stack_index >= 0)
            count += 1;

        --n;
    }

    return count;
}

static struct function* get_function(const char* function_name, struct evaluator* e) {
    for (uint i = 0; i < e->n_functions; ++i) {
        if (strcmp(e->functions[i].name, function_name) == 0) {
            return &e->functions[i];
        }
    }

    return NULL;
}

static int32_t add_constant(struct binary_data* data, const struct object* o) {
    uint32_t constant_address = data->n_constants_bytes;

    *(int32_t*)(&data->constants_bytes[data->n_constants_bytes]) = o->type;
    data->n_constants_bytes += sizeof(int32_t);

    if (o->type == OBJ_NUMBER) {
        *(int32_t*)(&data->constants_bytes[data->n_constants_bytes]) = o->int_value;
        data->n_constants_bytes += sizeof(int32_t);
        return constant_address;
    }

    if (o->type == OBJ_STRING) {
        strcpy((char*)&data->constants_bytes[data->n_constants_bytes], o->str_value);
        data->n_constants_bytes += strlen(o->str_value);

        data->constants_bytes[data->n_constants_bytes] = '\0';
        ++data->n_constants_bytes;

        return constant_address;
    }

    return -1;
}

static void add_function(const char* function_name, uint32_t n_parameters, uint32_t index, struct evaluator* e) {
    e->functions[e->n_functions++] = (struct function) {
        .type = USER,
        .name = function_name,
        .n_parameters = n_parameters,
        .index = index
    };
}


#define add_instruction(code) \
{ \
    bytes[(*n_bytes)++] = code; \
}

#define add_number(num) \
{ \
    int32_t int_value = (int32_t)(num); \
    memcpy(&bytes[*n_bytes], &int_value, sizeof(int_value)); \
    (*n_bytes) += sizeof(num); \
}

#define increment_index() \
    ++(*current_stack_index)

#define create_placeholder() \
    *n_bytes; (*n_bytes) += sizeof(uint32_t)

#define patch_placeholder(placeholder) \
    { \
        memcpy(&bytes[placeholder], n_bytes, sizeof(*n_bytes)); \
    } \

int initialize_evaluator(struct evaluator* e) {
    e->functions[e->n_functions++] = (struct function){.type = BUILT_IN, .name = "print",        .n_parameters = 1, .index = 0};
    e->functions[e->n_functions++] = (struct function){.type = BUILT_IN, .name = "input_number", .n_parameters = 1, .index = 1};
    e->functions[e->n_functions++] = (struct function){.type = BUILT_IN, .name = "input_string", .n_parameters = 1, .index = 2};

    return 0;
};

int evaluate(struct node* ast, uint8_t* bytes, uint32_t* n_bytes, struct binary_data* data, uint32_t* current_stack_index, struct evaluator* e) {
    if (ast == NULL) {
        return 0;
    }

    switch (ast->type) {
        case NODE_NUMBER:
            {
                add_instruction(PUSH);

                int32_t constant_address = add_constant(data, &(struct object){.type = OBJ_NUMBER, .int_value = *(int32_t*)ast->token->value});
                add_number(constant_address);

                return 0;
            }
        case NODE_STRING:
            {
                add_instruction(PUSH);

                int32_t constant_address = add_constant(data, &(struct object){.type = OBJ_STRING, .str_value = ast->token->value});
                add_number(constant_address);

                return 0;
            }
        case NODE_VAR:
            {
                int32_t position;
                CHECK(get_variable(ast->token->value, e, &position), "failed to get variable");

                add_instruction(DUP);
                add_number(position);

                return 0;
            }

        case NODE_BINARY_OP:
            CHECK(evaluate(ast->right, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate binary operation");
            CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e),  "failed to evaluate binary operation");

            switch (ast->token->code) {
                case TOK_ADD:
                    add_instruction(ADD);
                    break;

                case TOK_MIN:
                    add_instruction(MIN);
                    break;

                case TOK_MUL:
                    add_instruction(MUL);
                    break;

                case TOK_DIV:
                    add_instruction(DIV);
                    break;

                case TOK_LES:
                    add_instruction(LES);
                    break;

                case TOK_LEQ:
                    add_instruction(LEQ);
                    break;

                case TOK_GRE:
                    add_instruction(GRE);
                    break;

                case TOK_GRQ:
                    add_instruction(GRQ);
                    break;

                case TOK_DEQ:
                    add_instruction(DEQ);
                    break;

                case TOK_NEQ:
                    add_instruction(NEQ);
                    break;

                case TOK_AND:
                    add_instruction(AND);
                    break;

                case TOK_OR:
                    add_instruction(OR);
                    break;

                default:
                    return 1;
            }

            return 0;

        case NODE_IF:
            {
                CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate expression in if statement");

                add_instruction(JMP_NOT);
                uint32_t placeholder_true = create_placeholder();

                // true
                CHECK(evaluate(ast->right->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate \"true\" side in if statement");

                uint32_t n_cleaned = pop_variables(ast->scope, e);
                for (uint32_t i = 0; i < n_cleaned; ++i) {
                    add_instruction(POP);
                }

                uint32_t placeholder_false;
                if (ast->right->right) {
                    add_instruction(JMP);
                    placeholder_false = create_placeholder();
                }

                // fill placeholder
                patch_placeholder(placeholder_true);

                // false
                CHECK(evaluate(ast->right->right, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate \"false\" side in if statement");

                if (ast->right->right) {
                    n_cleaned = pop_variables(ast->scope, e);
                    for (uint32_t i = 0; i < n_cleaned; ++i) {
                        add_instruction(POP);
                    }

                    patch_placeholder(placeholder_false);
                }

                return 0;
            }

        case NODE_WHILE:
            {
                uint32_t start_index = *n_bytes;

                CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate expression in while statement");

                add_instruction(JMP_NOT);
                uint32_t placeholder_false = create_placeholder();

                // body
                CHECK(evaluate(ast->right->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate while statement body");

                uint32_t n_cleaned = pop_variables(ast->scope, e);
                for (uint32_t i = 0; i < n_cleaned; ++i) {
                    add_instruction(POP);
                }

                add_instruction(JMP);
                add_number(start_index);

                // fill placeholder
                patch_placeholder(placeholder_false);

                return 0;
            }

        case NODE_ASSIGN:
            {
                const char* var_name = ast->left->token->value;

                int32_t position;
                int res = get_variable(var_name, e, &position);
                if (res != 0) {
                    add_variable(var_name, ast->scope, *current_stack_index, e);
                    increment_index();

                    CHECK(evaluate(ast->right, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate initialization value");

                    return 0;
                }

                CHECK(evaluate(ast->right, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate assignment value");

                add_instruction(CHANGE);
                add_number(position);

                return 0;
            }
        case NODE_STATEMENT:
            {
                CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate current statement");
                CHECK(evaluate(ast->right, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate next statement");

                return 0;
            }

        case NODE_NOT:
            {
                CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate not statement");

                add_instruction(NOT);

                return 0;
            }

        case NODE_FUNCTION:
            {
                uint32_t n_parameters = 0;

                struct node* parameter = ast->left;
                while (parameter) {
                    add_variable(parameter->token->value, ast->scope + 1, -1 - n_parameters, e);
                    parameter = parameter->left;
                    ++n_parameters;
                }

                add_instruction(JMP);
                uint32_t placeholder = create_placeholder();

                const char* fun_name = ast->token->value;
                add_function(fun_name, n_parameters, *n_bytes, e);

                // the first 2 are old base and return address
                uint32_t new_stack_index = 2;
                CHECK(evaluate(ast->right, bytes, n_bytes, data, &new_stack_index,  e), "failed to evaluate function body");

                uint32_t n_cleaned = pop_variables(ast->scope, e);
                for (uint32_t i = 0; i < n_cleaned; ++i) {
                    add_instruction(POP);
                }

                // TODO: double ret in case of return
                add_instruction(RET);

                patch_placeholder(placeholder);

                return 0;
            }

        case NODE_FUNCTION_CALL:
            {
                const char* function_name = ast->token->value;
                struct function* f = get_function(function_name, e);
                if (!f) {
                    ERROR("function not definited: %s", function_name);
                    return 1;
                }

                struct node* arguments = ast->left;
                struct node* argument = arguments;

                uint32_t n_arguments = 0;
                struct node* arg_list[256];

                while (argument) {
                    arg_list[n_arguments++] = argument->left;
                    argument = argument->right;
                }

                for (int32_t i = n_arguments - 1; i >= 0; --i) {
                    CHECK(evaluate(arg_list[i], bytes, n_bytes, data, current_stack_index, e), "failed to evaluate function: %s argument", function_name);
                }

                add_instruction(f->type == USER ? CALL : CALL_BUILTIN);
                add_number(f->index);

                for (uint32_t i = 0; i < n_arguments; ++i) {
                    add_instruction(POP);
                }

                add_instruction(DUP_ABS);
                add_number(RETURN_INDEX);

                return 0;
            }

        case NODE_RETURN:
            {
                CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate return value");

                add_instruction(CHANGE_ABS);
                add_number(RETURN_INDEX);

                uint32_t n_cleaned = get_var_count(0, e);
                for (uint32_t i = 0; i < n_cleaned; ++i) {
                    add_instruction(POP);
                }

                add_instruction(RET);

                return 0;
            }
        case NODE_EXP_STATEMENT:
            {
                CHECK(evaluate(ast->left, bytes, n_bytes, data, current_stack_index, e), "failed to evaluate return value");
                add_instruction(POP);

                return 0;
            }

        default:
            return 0;
    }

    return 0;
}

#define program_counter \
    vm->program_counter

#define stack_base \
    vm->stack_base

#define stack_size \
    vm->stack_size

#define return_value \
    vm->stack[RETURN_INDEX]

#define read_value(value_type) \
    (*((value_type*)(vm->bytes + program_counter + 1)))

#define read_value_increment(value_type) \
    (*((value_type*)(vm->bytes + program_counter + 1))); program_counter += sizeof(value_type)


void push_number(struct vm* vm, int32_t number) {
    vm->stack[stack_size++] = (struct object){.type = OBJ_NUMBER, .int_value = number};
}

void push_string(struct vm* vm, char* string) {
    vm->stack[stack_size++] = (struct object){.type = OBJ_STRING, .str_value = string};
}

void push_bool(struct vm* vm, bool value) {
    vm->stack[stack_size++] = (struct object){.type = OBJ_BOOL, .bool_value = value};
}

void push_constant(struct vm* vm, int32_t address) {
    int type = *((int32_t*)&vm->bytes[address]);
    address += sizeof(int32_t);

    void* value;
    if (type == OBJ_NUMBER) {
        int32_t number = *((int32_t*)&vm->bytes[address]);
        push_number(vm, number);
        return;
    }

    if (type == OBJ_STRING) {
        value = &vm->bytes[address];
        push_string(vm, value);
        return;
    }
}

#define push(o) \
    vm->stack[stack_size++] = o

#define pop() \
    (&vm->stack[--stack_size])

#define peek(n) \
    (&vm->stack[stack_size - 1 - n])

int32_t pop_number(struct vm* vm) {
    struct object* obj = (struct object*)pop();
    return obj->int_value;
}

const char* pop_string(struct vm* vm) {
    struct object* obj = (struct object*)pop();
    return obj->str_value;
}

bool pop_bool(struct vm* vm) {
    struct object* obj = (struct object*)pop();
    return obj->bool_value;
}

typedef struct object (*builtin_fun)(struct vm* vm);

static struct object print_object(struct vm* vm) {
    struct object* o = peek(0);

    if (o->type == OBJ_NUMBER) {
        printf("%d\n", o->int_value);
    } else if (o->type == OBJ_STRING) {
        printf("%s\n", o->str_value);
    }

    return (struct object){};
}

static struct object input_number(struct vm* vm) {
    struct object* o = (struct object*)peek(0);

    const char* input_text = o->str_value;
    printf("%s", input_text);

    int32_t number;
    scanf("%d", &number);

    puts("");

    return (struct object){.type = OBJ_NUMBER, .int_value = number};
};

static struct object input_string(struct vm* vm) {
    struct object* o = (struct object*)peek(0);

    const char* input_text = o->str_value;
    printf("%s", input_text);

    char* string = malloc(200);
    scanf("%s", string);

    puts("");

    return (struct object){.type = OBJ_STRING, .str_value = string};
}

static builtin_fun builtin_functions[] = {
    print_object,
    input_number,
    input_string
};

void add_strings(struct vm* vm, struct object* value1, struct object* value2) {
    char* new_string = malloc(500);
    uint32_t n_new_string = 0;

    if (value1->type == OBJ_NUMBER) {
        n_new_string += sprintf(new_string, "%d", value1->int_value);
    } else {
        strcpy(new_string, value1->str_value);
        n_new_string += strlen(value1->str_value);
    }

    if (value2->type == OBJ_NUMBER) {
        n_new_string += sprintf(new_string + n_new_string, "%d", value2->int_value);
    } else {
        strcpy(new_string + n_new_string, value2->str_value);
        n_new_string += strlen(value2->str_value);
    }

    push_string(vm, new_string);
}

int execute(struct vm* vm) {
    // 0 - return value

    stack_size = 1;
    stack_base = 1;

    int32_t start_address = *(int32_t*)vm->bytes;
    program_counter = start_address;

    while (!vm->halt && program_counter < vm->n_bytes) {
        switch (vm->bytes[program_counter]) {
            case PUSH:
                {
                    int32_t constant = read_value_increment(int32_t);
                    push_constant(vm, constant);
                }
                break;

            case POP:
                --stack_size;
                break;

            case ADD:
                {
                    struct object* value1 = pop();
                    struct object* value2 = pop();

                    if (value1->type == OBJ_STRING || value2->type == OBJ_STRING) {
                        add_strings(vm, value1, value2);
                        break;
                    }

                    int32_t number1 = (int32_t)(size_t)value1->int_value;
                    int32_t number2 = (int32_t)(size_t)value2->int_value;

                    push_number(vm, number1 + number2);
                }
                break;

            case MIN:
                {
                    uint32_t number1 = pop_number(vm);
                    uint32_t number2 = pop_number(vm);
                    push_number(vm, number1 - number2);
                }
                break;

            case MUL:
                {
                    uint32_t number1 = pop_number(vm);
                    uint32_t number2 = pop_number(vm);
                    push_number(vm, number1 * number2);
                }
                break;

            case DIV:
                {
                    uint32_t number1 = pop_number(vm);
                    uint32_t number2 = pop_number(vm);
                    push_number(vm, number1 / number2);
                }
                break;

            case NOT:
                {
                    uint32_t exp = pop_bool(vm);
                    if (exp) {
                        push_bool(vm, false);
                        break;
                    }

                    push_bool(vm, true);
                }
                break;

            case DEQ:
                {
                    uint32_t exp1 = pop_number(vm);
                    uint32_t exp2 = pop_number(vm);
                    push_bool(vm, exp1 == exp2);
                }
                break;

            case NEQ:
                {
                    uint32_t exp1 = pop_number(vm);
                    uint32_t exp2 = pop_number(vm);
                    push_bool(vm, exp1 != exp2);
                }
                break;

            case GRE:
                {
                    uint32_t exp1 = pop_number(vm);
                    uint32_t exp2 = pop_number(vm);
                    push_bool(vm, exp1 > exp2);
                }
                break;

            case GRQ:
                {
                    uint32_t exp1 = pop_number(vm);
                    uint32_t exp2 = pop_number(vm);
                    push_bool(vm, exp1 >= exp2);
                }
                break;

            case LES:
                {
                    uint32_t exp1 = pop_number(vm);
                    uint32_t exp2 = pop_number(vm);
                    push_bool(vm, exp1 < exp2);
                }
                break;

            case LEQ:
                {
                    bool exp1 = pop_number(vm);
                    bool exp2 = pop_number(vm);
                    push_bool(vm, exp1 <= exp2);
                }
                break;

            case AND:
                {
                    bool exp1 = pop_bool(vm);
                    bool exp2 = pop_bool(vm);
                    push_bool(vm, exp1 && exp2);
                }
                break;

            case OR:
                {
                    bool exp1 = pop_bool(vm);
                    bool exp2 = pop_bool(vm);
                    push_bool(vm, exp1 || exp2);
                }
                break;

            case DUP:
                {
                    int32_t index = read_value_increment(int32_t);
                    push(vm->stack[stack_base + index]);
                }
                break;
            case DUP_ABS:
                {
                    int32_t index = read_value_increment(int32_t);
                    push(vm->stack[index]);
                }
                break;
            case CHANGE:
                {
                    int32_t index = read_value_increment(int32_t);
                    vm->stack[stack_base + index] = *pop();
                }
                break;
            case CHANGE_ABS:
                {
                    int32_t index = read_value_increment(int32_t);
                    vm->stack[index] = *pop();
                }
                break;
            case JMP_NOT:
                {
                    int32_t index = read_value_increment(int32_t);

                    uint32_t condition = pop_number(vm);
                    if (!condition) {
                        program_counter = start_address + index - 1;
                    }
                }
                break;

            case JMP:
                {
                    int32_t index = read_value(int32_t);
                    program_counter = start_address + index - 1;
                }
                break;

            case CALL:
                {
                    uint32_t old_base = stack_base;
                    stack_base = stack_size;

                    int32_t index = read_value_increment(int32_t);
                    push_number(vm, program_counter + 1);

                    program_counter = start_address + index - 1;

                    push_number(vm, old_base);
                }
                break;

            case CALL_BUILTIN:
                {
                    int32_t index = read_value_increment(int32_t);
                    return_value = builtin_functions[index](vm);
                }
                break;
            case RET:
                {
                    stack_base = pop_number(vm);
                    int32_t index = pop_number(vm);

                    program_counter = index - 1;
                }
                break;
        }

        ++program_counter;
    }

    return 0;
}

