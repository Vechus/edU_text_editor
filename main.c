#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#define INPUT_MAX_LENGTH 1024
#define INIT_LINES_SIZE 2
#define DEBUG 1

// commands in enum (BOTTOM is an auxiliary command that represents the bottom of the stack)
enum cmd_type {CHANGE, DELETE, PRINT, UNDO, REDO, QUIT, BOTTOM};

typedef struct {
    enum cmd_type type;
    unsigned long int args[2];
}cmd;

typedef struct txt_line_s {
    char* content;
    struct txt_line_s* next;
}txt_line_t;

typedef struct lines_stack_s{
    unsigned int index;
    unsigned long int size;
    txt_line_t** lines;
}lines_stack_t;

typedef struct cmd_stack_node_s{
    cmd* command;
    struct cmd_stack_node_s* next;
}cmd_stack_node_t;

typedef struct del_list_node_s{
    lines_stack_t* linesStack;
    struct del_list_node_s* next;
}del_list_node_t;


/*int read_int() {
    char c = getchar_unlocked();
    while(c<'0' || c>'9') c = getchar_unlocked();
    int ret = 0;
    while(c>='0' && c<='9') {
        ret = 10 * ret + c - 48;
        c = getchar_unlocked();
    }
    return ret;
}*/

void debug_print_cmd(cmd* c) {
    enum cmd_type t = c->type;
    if(t==CHANGE||t==DELETE||t==PRINT) {
        printf("CMD::%d: %lu, %lu\n", t, c->args[0], c->args[1]);
    } else if(t!=QUIT) {
        printf("CMD::%d: %lu\n", t, c->args[0]);
    } else {
        printf("QUIT\n");
    }
}

void debug_print_lines_stack(lines_stack_t* linesStack) {
    printf("----------------\n");
    for(int i = 0; i < linesStack->size; i++) {
        if(linesStack->lines[i] == NULL) continue;
        if(linesStack->lines[i]->next != NULL) {
            txt_line_t* tmp = linesStack->lines[i];
            printf("%s -> ", tmp->content);
            tmp = tmp->next;
            while(tmp != NULL) {
                printf("\t%s; ", tmp->content);
                tmp = tmp->next;
            }
            printf("\n");
        } else {
            printf("%s\n", linesStack->lines[i]->content);
        }
    }
    printf("----------------\n");
}

void debug_print_cmd_stack(cmd_stack_node_t* cmdStack) {
    cmd_stack_node_t* tmp = cmdStack;
    printf("Commands: \n");
    while(tmp->command->type != BOTTOM) {
        debug_print_cmd(tmp->command);
        tmp = tmp->next;
    }
}

void push_cmd(cmd_stack_node_t** cmdStack, cmd* command) {
    cmd_stack_node_t* tmp = (cmd_stack_node_t *)malloc(sizeof(cmd_stack_node_t));
    tmp->command = command;
    tmp->next = *cmdStack;
    *cmdStack = tmp;
}

void push_line(txt_line_t** stack, char* text) {
    txt_line_t* tmp;
    tmp = (txt_line_t *)malloc(sizeof(txt_line_t));
    tmp->content = (char *)malloc(strlen(text) * sizeof(char));
    strcpy(tmp->content, text);
    tmp->next = *stack;
    *stack = tmp;
}

void handle_change(lines_stack_t* linesStack, cmd *command) {
    char* input;
    char buff[INPUT_MAX_LENGTH];

    if(command->args[1] > linesStack->size) {
        linesStack->lines = (txt_line_t **)realloc(linesStack->lines, (command->args[1] + 1) * sizeof(txt_line_t*));
        linesStack->size = command->args[1];
    }

    for(unsigned long int i = command->args[0] - 1; i <= command->args[1] - 1; i++) {
        if(linesStack->lines[i] == NULL) {
            linesStack->lines[i] = (txt_line_t *)malloc(sizeof(txt_line_t));
        }
        fgets(buff, INPUT_MAX_LENGTH, stdin);
        input = (char *)malloc(strlen(buff) * sizeof(char));
        strcpy(input, buff);
        push_line(&(linesStack->lines[i]), input);
    }
    // .\n
    getchar_unlocked();
    getchar_unlocked();
}

void handle_print(lines_stack_t* linesStack, unsigned long int from, unsigned long int to) {
    unsigned long int delta = to - from;
    int count = 0;
    unsigned long int x = from - 1;
    while(count < delta) {
        if(x > linesStack->size - 1) {
            fputs(".\n", stdout);
            count++;
        } else {
            fputs(linesStack->lines[x]->content, stdout);
            count++;
            x++;
        }
    }
}

void handle_delete(lines_stack_t* linesStack, del_list_node_t* delStack, unsigned long int from, unsigned long int to) {
    del_list_node_t* new_tmp;
    register unsigned long int delta = (to < linesStack->size ? to : linesStack->size) - from + 1;
    new_tmp = (del_list_node_t*) malloc(sizeof(del_list_node_t));
    new_tmp->next = delStack;
    new_tmp->linesStack = (lines_stack_t*) malloc(sizeof(lines_stack_t));
    new_tmp->linesStack->index = from;
    new_tmp->linesStack->lines = (txt_line_t**) malloc((delta) * sizeof(txt_line_t*));
    new_tmp->linesStack->size = delta;
    // copy sub array into the new array
    memcpy(new_tmp->linesStack->lines, linesStack->lines + (from - 1), delta * sizeof(txt_line_t*));
    delStack = new_tmp;
    // resize linesStack
    for(unsigned long int i = from - 1; i + delta < linesStack->size; i++) {
        linesStack->lines[i] = linesStack->lines[i + delta];
    }
    linesStack->lines = (txt_line_t **) realloc(linesStack->lines, (linesStack->size - delta + 1) * sizeof(txt_line_t*));
    linesStack->size = linesStack->size - delta;
#ifdef DEBUG
    debug_print_lines_stack(new_tmp->linesStack);
    debug_print_lines_stack(linesStack);
#endif
}

cmd* parse_cmd() {
    char c, t;
    int arg1 = 0, arg2 = 0;
    cmd *ret = (cmd*) malloc(sizeof(cmd));
    bool a1 = false;
    t = getchar_unlocked();
    int i = 0;

    //while(c<'0' || c>'9') c = getchar_unlocked();
    c = getchar_unlocked();
    while(c>='0' && c<='9') {
        if(a1 == false) arg1 = 10 * arg1 + c - 48;
        else arg2 = 10 * arg2 + c - 48;
        c = getchar_unlocked();
        if(c == '\n') break;
        else if(c == ',') {
            a1 = true;
            c = getchar_unlocked();
        }
    }
    switch (t) {
        case 'q':
            ret->type = QUIT;
            break;
        case 'u':
            ret->type = UNDO;
            //ret->args = (unsigned long int*) malloc(sizeof(int));
            ret->args[0] = arg1;
            break;
        case 'r':
            ret->type = REDO;
            //ret->args = (unsigned long int*) malloc(sizeof(int));
            ret->args[0] = arg1;
            break;
        case 'c':
            ret->type = CHANGE;
            //ret->args = (unsigned long int*) malloc(2 * sizeof(int));
            ret->args[0] = arg1;
            ret->args[1] = arg2;
            break;
        case 'p':
            ret->type = PRINT;
            //ret->args = (unsigned long int*) malloc(2 * sizeof(int));
            ret->args[0] = arg1;
            ret->args[1] = arg2;
            break;
        case 'd':
            ret->type = DELETE;
            //ret->args = (unsigned long int*) malloc(2 * sizeof(int));
            ret->args[0] = arg1;
            ret->args[1] = arg2;
            break;
        default:
            puts("\nInvalid command format.\n");
            break;
    }
    return ret;
}

int main() {
    cmd* command;
    char* dot_ptr = ".";
    char* ptr;
    // all lines with history
    lines_stack_t* linesStack;
    del_list_node_t* delList = NULL;
    // command history
    cmd_stack_node_t* cmdStack;

    cmdStack = (cmd_stack_node_t *)malloc(sizeof(cmd_stack_node_t));
    cmdStack->command = (cmd*) malloc(sizeof(cmd));
    cmdStack->command->type = BOTTOM;
    cmdStack->next = NULL;

    linesStack = (lines_stack_t *)malloc(sizeof(lines_stack_t));
    linesStack->index = 0;
    linesStack->lines = (txt_line_t **)malloc(INIT_LINES_SIZE * sizeof(txt_line_t*));
    linesStack->size = INIT_LINES_SIZE;

    #ifdef DEBUG
        printf("DEBUG\n");
    #endif

    command = parse_cmd();
    //cmdStack->command = command;
    while(command->type != QUIT) {
        switch (command->type) {
            case CHANGE:
                handle_change(linesStack, command);
                break;
            case PRINT:
                handle_print(linesStack, command->args[0], command->args[1]);
                break;
            case DELETE:
                handle_delete(linesStack, delList, command->args[0], command->args[1]);
                break;
            default:
                break;
        }
        push_cmd(&cmdStack, command);
        command = parse_cmd();
    }
    #ifdef DEBUG
        debug_print_cmd_stack(cmdStack);
        debug_print_lines_stack(linesStack);
    #endif
    return 0;
}
