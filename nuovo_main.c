/**
 * WELCOME TO MY API PROJECT LMAO
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define INPUT_MAX_LENGTH 1025
#define CAPACITY_CONST 500
#define INIT_SNAP_LEN 1000
#define INCREASE_CONST 500
#define INIT_CMD_LEN 1000
#define INIT_INDEXES_LEN 1000

enum cmd_type {CHANGE, DELETE, PRINT, UNDO, REDO, QUIT, BOTTOM};

typedef struct snapshot_s {
    char **lines;
    int size;
    int capacity;
    int index;
}snapshot_t;

typedef struct command_s {
    int arg1;
    int arg2;
    char **content_lines;
}command_t;

typedef struct command_wrap_s {
    int size;
    int capacity;
    command_t **commands;
}command_wrap_t;

typedef struct {
    enum cmd_type type;
    int args[2];
}cmd;

typedef struct int_array_s {
    int size;
    int capacity;
    int *array;
}int_array_t;

void copy_editor(snapshot_t *editor, snapshot_t *dest) {
    dest->size = editor->size;
    dest->capacity = dest->size + CAPACITY_CONST;
    dest->lines = (char **) malloc((dest->size + CAPACITY_CONST) * sizeof(char *));
    for(int i = 0; i < dest->size; i++) {
        dest->lines[i] = editor->lines[i];
    }
    for(int i = dest->size; i < dest->capacity; i++) {
        dest->lines[i] = NULL;
    }
}

/**
 * Handles print. Iterates through the lines structure, if the line has null content it prints '.\n',
 *  else prints the line's content.
 * @param editor
 * @param arg1
 * @param arg2
 */
void handle_print(snapshot_t *editor, int arg1, int arg2) {
    int delta = arg2 - arg1;
    int count = 0;
    int x = arg1 - 1;
    while(count < delta + 1) {
        if(x >= editor->size || x < 0) {
            fputs(".\n", stdout);
            count++;
        } else {
            fputs(editor->lines[x], stdout);
            count++;
            x++;
        }
    }
}

void handle_change(snapshot_t *editor, command_t *command) {
    char *input;
    char buff[INPUT_MAX_LENGTH];
    int arg2 = command->arg2;
    int arg1 = command->arg1;

    if(arg2 > editor->capacity) {
        editor->lines = (char **) realloc(editor->lines, (arg2 + CAPACITY_CONST) * sizeof(char *));
        for(int i = editor->capacity; i < arg2 + CAPACITY_CONST; i++) {
            editor->lines[i] = NULL;
        }
        editor->capacity = arg2 + CAPACITY_CONST;
    }
    if(arg2 > editor->size) editor->size = arg2;
    // alloc content lines
    command->content_lines = (char **) malloc((arg2 - arg1 + 1) * sizeof(char *));
    for(int i = arg1 - 1; i <= arg2 - 1; i++) {
        fgets(buff, INPUT_MAX_LENGTH, stdin);
        input = (char *)malloc((strlen(buff) + 1) * sizeof(char));
        strcpy(input, buff);
        editor->lines[i] = input;
        command->content_lines[i - arg1 + 1] = input;
    }
    // .\n
    getchar_unlocked();
    getchar_unlocked();
}

void handle_delete(snapshot_t *editor, snapshot_t **snapshot, int snap_size, int arg1, int arg2) {
    int from, to;
    if(arg1 <= 0) {
        from = 1;
    } else {
        from = arg1;
    }
    if(arg2 > editor->size) {
        to = editor->size;
    } else {
        to = arg2;
    }
    int delta = to - from + 1;
    if(delta <= 0) {
        // invalid delete, copy whole editor
        copy_editor(editor, snapshot[snap_size]);
        return;
    }
    snapshot[snap_size]->lines = (char **) malloc((editor->size - delta) * sizeof(char *));
    // copy first part
    for(int i = 0; i < from - 1; i++) {
        snapshot[snap_size]->lines[i] = editor->lines[i];
    }
    for(int i = from - 1; i + delta < editor->size; i++) {
        if(editor->lines[i + delta] == NULL) {
            editor->lines[i] = NULL;
            continue;
        }
        // shifted line
        editor->lines[i] = editor->lines[i + delta];
        editor->lines[i + delta] = NULL;
        // copy shifted line
        snapshot[snap_size]->lines[i] = editor->lines[i];
    }
    if(to == editor->size) {
        for(int i = from - 1; i < editor->size; i++) {
            editor->lines[i] = NULL;
        }
    } else {
        for(int i = editor->size - delta; i < editor->size; i++) {
            editor->lines[i] = NULL;
        }
    }
    editor->size -= delta;
    snapshot[snap_size]->size = editor->size;
    snapshot[snap_size]->capacity = editor->size;
}

/**
 * Parses commands
 * @return
 */
cmd* parse_cmd() {
    char c;
    int arg1 = 0, arg2 = 0;
    cmd *ret = (cmd*) malloc(sizeof(cmd));
    bool a1 = false;
    int i = 0;

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
    getchar_unlocked(); // '\n'
    switch (c) {
        case 'q':
            ret->type = QUIT;
            break;
        case 'u':
            ret->type = UNDO;
            ret->args[0] = arg1;
            break;
        case 'r':
            ret->type = REDO;
            ret->args[0] = arg1;
            break;
        case 'c':
            ret->type = CHANGE;
            ret->args[0] = arg1;
            ret->args[1] = arg2;
            break;
        case 'p':
            ret->type = PRINT;
            ret->args[0] = arg1;
            ret->args[1] = arg2;
            break;
        case 'd':
            ret->type = DELETE;
            ret->args[0] = arg1;
            ret->args[1] = arg2;
            break;
        default:
            puts("\nInvalid command format.\n");
            putc(c, stdout);
            break;
    }
    return ret;
}

int main() {
    // how do i know its size? AH YES! indexes array
    snapshot_t **snapshots = (snapshot_t**) malloc(INIT_SNAP_LEN * sizeof(snapshot_t*));
    for(int i = 0; i < INIT_SNAP_LEN; i++) {
        snapshots[i] = (snapshot_t *) malloc(sizeof(snapshot_t));
    }
    int snap_capacity = INIT_SNAP_LEN;
    int snap_size = 0;

    snapshots[0]->index = 0;
    snapshots[0]->size = 0;
    snapshots[0]->capacity = CAPACITY_CONST;
    snapshots[0]->lines = (char **) malloc(CAPACITY_CONST * sizeof(char *));

    command_wrap_t *commandWrap = (command_wrap_t *) malloc(sizeof(command_wrap_t));
    commandWrap->commands = (command_t**) malloc(INIT_CMD_LEN * sizeof(command_t*));
    commandWrap->size = 0;
    commandWrap->capacity = INIT_CMD_LEN;
    for(int i = 0; i < INIT_CMD_LEN; i++) {
        commandWrap->commands[i] = (command_t *) malloc(sizeof(command_t));
    }

    int_array_t *snap_indexes = (int_array_t *) malloc(sizeof(int_array_t));
    snap_indexes->array = (int *) malloc(INIT_INDEXES_LEN * sizeof(int));
    snap_indexes->size = 0;
    snap_indexes->capacity = INIT_INDEXES_LEN;
    snap_indexes->array[0] = 0;

    snapshot_t *editor = (snapshot_t *) malloc(sizeof(snapshot_t));
    editor->lines = (char **) calloc(CAPACITY_CONST, sizeof(char *));
    editor->index = 0;
    editor->size = 0;
    editor->capacity = CAPACITY_CONST;

    cmd* curr_cmd;
    int undo_count = 0;
    int redo_count = 0;
    int command_counter = 0;
    int executed_undos = 0;

    curr_cmd = parse_cmd();
    while(curr_cmd->type != QUIT) {
        switch (curr_cmd->type) {
            case CHANGE:
                command_counter++;
                commandWrap->commands[commandWrap->size]->arg1 = curr_cmd->args[0];
                commandWrap->commands[commandWrap->size]->arg2 = curr_cmd->args[1];
                
                /*if(undo_count > redo_count) {
                    // permanent undo
                    handle_perm_undo(linesStack, &delList, &cmdHead, &undoHead, undo_count - redo_count);
                } else if(redo_count > 0 && undo_count < redo_count) {
                    // permanent redo
                    handle_temp_redo(linesStack, &delList, cmdHead, undoHead, redo_count - undo_count);
                    make_permanent_undo(linesStack, &undoHead);
                }
                if(undoHead->size > 0) make_permanent_undo(linesStack, &undoHead);*/
                undo_count = 0;
                redo_count = 0;
                handle_change(editor, commandWrap->commands[commandWrap->size]);
                commandWrap->size++;
                // resize commandWrap if needed
                if(commandWrap->size >= commandWrap->capacity) {
                    commandWrap->commands = (command_t **) realloc(commandWrap->commands,
                                                                   (commandWrap->size + INIT_CMD_LEN) * sizeof(command_t *));
                    for(int i = commandWrap->size; i < commandWrap->size + INIT_CMD_LEN; i++) {
                        commandWrap->commands[i] = (command_t *) malloc(sizeof(command_t));
                    }
                    commandWrap->capacity = commandWrap->size + INIT_CMD_LEN;
                }
                break;
            case PRINT:
                // first handle undos/redos (both functions end in case the curr_cmd stack / undo stack gets depleted)
                /*if(undo_count > redo_count) {
                    handle_temp_undo(linesStack, &delList, cmdHead, undoHead, undo_count - redo_count);
                } else if(redo_count > 0 && undo_count < redo_count) {
                    handle_temp_redo(linesStack, &delList, cmdHead, undoHead, redo_count - undo_count);
                }*/
                undo_count = 0;
                redo_count = 0;
                handle_print(editor, curr_cmd->args[0], curr_cmd->args[1]);
                break;
            case DELETE:
                command_counter++;
                snap_size++;
                // resize snapshot structure if needed
                if(snap_size >= snap_capacity) {
                    snapshots = (snapshot_t **) realloc(snapshots, (snap_size + INCREASE_CONST) * sizeof(snapshot_t *));
                    snap_capacity = snap_size + INCREASE_CONST;
                    for(int i = snap_size; i < snap_capacity; i++) {
                        snapshots[i] = (snapshot_t *) malloc(sizeof(snapshot_t));
                    }
                }
                snap_indexes->size++;
                // resize indexes structure if needed
                if(snap_indexes->size >= snap_indexes->capacity) {
                    snap_indexes->array = (int *) realloc(snap_indexes->array, (snap_indexes->size + INCREASE_CONST) * sizeof(int));
                    snap_indexes->capacity = snap_indexes->size + INCREASE_CONST;
                }
                snap_indexes->array[snap_indexes->size] = command_counter;
                snapshots[snap_size]->index = command_counter;
                handle_delete(editor, snapshots, snap_size, curr_cmd->args[0], curr_cmd->args[1]);

                /*
                if(undo_count > redo_count) {
                    // permanent undo
                    handle_perm_undo(linesStack, &delList, &cmdHead, &undoHead, undo_count - redo_count);
                } else if(redo_count > 0 && undo_count < redo_count) {
                    // permanent redo
                    handle_temp_redo(linesStack, &delList, cmdHead, undoHead, redo_count - undo_count);
                    make_permanent_undo(linesStack, &undoHead);
                }
                if(undoHead->size > 0) make_permanent_undo(linesStack, &undoHead);
                undo_count = 0;
                redo_count = 0;
                handle_delete(linesStack, &delList, &curr_cmd->args[0] + (curr_cmd->args[0] <= 0 ? 1 : 0), &curr_cmd->args[1]);
                if(delList->linesStack->index == -1) curr_cmd->args[1] = -1;
                push_cmd(&cmdHead, curr_cmd);*/
                break;
            case UNDO:
                undo_count += curr_cmd->args[0];
                // cap undo value
                if(undo_count > command_counter + redo_count)
                    undo_count = command_counter + redo_count;
                break;
            case REDO:
                redo_count += curr_cmd->args[0];
                // cap redo value
                if(redo_count > undo_count + executed_undos)
                    redo_count = undo_count + executed_undos;
                break;
            default:
                break;
        }
        free(curr_cmd);
        curr_cmd = NULL;
        curr_cmd = parse_cmd();
    }
}
