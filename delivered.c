#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define INPUT_MAX_LENGTH 1025
#define CAPACITY_CONST 100
#define INIT_SNAP_LEN 1000
#define INCREASE_CONST 100
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

/**
 * create a new snapshot with editor content
 * @param editor the main editor (not null)
 * @param dest where to copy the editor (not null)
 */
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
 * copies dest content into the main editor object
 * @param editor (not null)
 * @param dest (not null)
 */
void pass_to_snapshot(snapshot_t *editor, snapshot_t *dest) {
    int size = dest->size;
    if(editor->capacity < dest->capacity) {
        editor->lines = (char **) realloc(editor->lines, (dest->capacity + CAPACITY_CONST) * sizeof(char *));
        for(int i = size; i < dest->capacity + CAPACITY_CONST; i++) {
            editor->lines[i] = NULL;
        }
        editor->capacity = dest->capacity + CAPACITY_CONST;
    }
    editor->size = size;
    for(int i = 0; i < size; i++) {
        editor->lines[i] = dest->lines[i];
    }
    editor->index = dest->index;
}

/**
 * Gets the target snapshot to return to.
 * @param snapshots (not null)
 * @param snap_size
 * @param target_command
 * @return the index of the snapshot to return to
 */
int backward_search_snapshot(snapshot_t **snapshots, int snap_size, int target_command) {
    if(target_command >= snapshots[snap_size]->index) return snap_size;
    for(int i = snap_size; i > 0; i--) {
        if(target_command >= snapshots[i - 1]->index && target_command < snapshots[i]->index) {
            return i - 1;
        }
    }
    return 0;
}

/**
 * Handles print. Iterates through the lines structure, if the line has null content it prints '.\n',
 *  else prints the line's content.
 * @param editor (not null)
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

/**
 * Handle a change command. Put all new lines where they belong inside the editor.
 * @param editor (not null)
 * @param command (not null)
 */
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

/**
 * Redo a change. Puts all the lines that have been inserted by the command back where they belong.
 * @param editor (not null)
 * @param command (not null)
 */
void redo_change(snapshot_t *editor, command_t *command) {
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

    for(int i = arg1 - 1; i <= arg2 - 1; i++) {
        editor->lines[i] = command->content_lines[i - arg1 + 1];
    }
}

/**
 * Handle delete.
 *      * create a new snapshot and copy the editor' content
 *      * put the new snapshot into the main structure
 * @param editor (not null)
 * @param snapshot (not null)
 * @param snap_size
 * @param arg1
 * @param arg2
 */
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
 * Handle undo.
 * @param snapshots (not null)
 * @param editor (not null)
 * @param commandWrap (not null)
 * @param undo_count
 * @param redo_count
 * @param snap_size the amount of snapshots alloc'd in the main structure
 * @param command_counter
 * @param executed_undos the amount of temporary executed undos in the past
 * @param curr_snap the index of the closest snapshot
 */
void handle_undo(snapshot_t **snapshots, snapshot_t *editor,  command_wrap_t *commandWrap, int undo_count, int redo_count, int snap_size, int *command_counter, int *executed_undos, int *curr_snap) {
    // find the right snapshot to jump back to
    int target;
    if(undo_count - redo_count >= *command_counter)
        target = 0;
    else
        target = backward_search_snapshot(snapshots, snap_size, *command_counter - (undo_count - redo_count));
    // copy snapshot into editor
    pass_to_snapshot(editor, snapshots[target]);
    *curr_snap = target;
    // shift back to the right command (command counter)
    *command_counter -= undo_count - redo_count;
    // execute changes until counter reaches command_counter - (undo_count - redo_count)
    for(int i = snapshots[target]->index - target; i < *command_counter - target; i++) {
        redo_change(editor, commandWrap->commands[i]);
    }
    *executed_undos += undo_count - redo_count;
}

/**
 * Handle redo.
 * @param snapshots (not null)
 * @param editor (not null)
 * @param commandWrap (not null)
 * @param steps amount of steps to redo
 * @param snap_size the amount of snapshots alloc'd in the main structure
 * @param command_counter
 * @param curr_snap the index of the closest snapshot
 */
void handle_redo(snapshot_t **snapshots, snapshot_t *editor,  command_wrap_t *commandWrap, int steps, int snap_size, int *command_counter, int *curr_snap) {
    // find right snapshot to jump forward to (if needed)
    int target = backward_search_snapshot(snapshots, snap_size, *command_counter + (steps));
    if(target != *curr_snap) {
        *curr_snap = target;
        // (if needed) copy new snapshot into editor
        pass_to_snapshot(editor, snapshots[target]);
    }
    *command_counter += steps;
    // execute changes until command_counter - (redo_count - undo_count) is reached
    for(int i = snapshots[target]->index - target; i < *command_counter - target; i++) {
        redo_change(editor, commandWrap->commands[i]);
    }
}

/**
 * Make a change or a delete permanent by deleting all current history.
 * @param snapshots  (not null)
 * @param commandWrap  (not null)
 * @param curr_snap
 * @param snap_size
 * @param curr_change index of the last usable change
 */
void make_permanent(snapshot_t **snapshots, command_wrap_t *commandWrap, int curr_snap, int snap_size, int curr_change) {
    // delete all snapshots with index > curr_snap
    for(int i = curr_snap + 1; i <= snap_size; i++) {
        snapshots[i]->index = 0;
        snapshots[i]->size = 0;
        free(snapshots[i]->lines);
        snapshots[i]->lines = NULL;
        snapshots[i]->capacity = 0;
    }
    // delete all commands with index > curr_change
    for(int i = curr_change; i < commandWrap->size; i++) {
        for(int j = 0; j < commandWrap->commands[i]->arg2 - commandWrap->commands[i]->arg1 + 1; j++) {
            free(commandWrap->commands[i]->content_lines[j]);
        }
        free(commandWrap->commands[i]->content_lines);
        commandWrap->commands[i]->content_lines = NULL;
        commandWrap->commands[i]->arg1 = 0;
        commandWrap->commands[i]->arg2 = 0;
    }
    commandWrap->size = curr_change;
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

    /*int_array_t *snap_indexes = (int_array_t *) malloc(sizeof(int_array_t));
    snap_indexes->array = (int *) malloc(INIT_INDEXES_LEN * sizeof(int));
    snap_indexes->size = 0;
    snap_indexes->capacity = INIT_INDEXES_LEN;
    snap_indexes->array[0] = 0;*/

    snapshot_t *editor = (snapshot_t *) malloc(sizeof(snapshot_t));
    editor->lines = (char **) calloc(CAPACITY_CONST, sizeof(char *));
    editor->index = 0;
    editor->size = 0;
    editor->capacity = CAPACITY_CONST;

    cmd* curr_cmd;
    int undo_count = 0;
    int curr_snap = 0;
    int redo_count = 0;
    int command_counter = 0;
    int executed_undos = 0;
    int tot = 0;

    curr_cmd = parse_cmd();
    while(curr_cmd->type != QUIT) {
        tot++;
        switch (curr_cmd->type) {
            case CHANGE:
                if(undo_count > redo_count) {
                    // permanent undo
                    handle_undo(snapshots, editor, commandWrap, undo_count, redo_count, snap_size, &command_counter, &executed_undos, &curr_snap);
                    make_permanent(snapshots, commandWrap, curr_snap, snap_size, command_counter - curr_snap);
                    snap_size = curr_snap;
                } else if(redo_count > 0 && undo_count < redo_count) {
                    // permanent redo
                    handle_redo(snapshots, editor, commandWrap, redo_count - undo_count, snap_size, &command_counter, &curr_snap);
                    make_permanent(snapshots, commandWrap, curr_snap, snap_size, command_counter - curr_snap);
                    snap_size = curr_snap;
                } else if(executed_undos > 0) {
                    make_permanent(snapshots, commandWrap, curr_snap, snap_size, command_counter - curr_snap);
                    snap_size = curr_snap;
                }
                executed_undos = 0;
                undo_count = 0;
                redo_count = 0;
                command_counter++;
                commandWrap->commands[commandWrap->size]->arg1 = curr_cmd->args[0];
                commandWrap->commands[commandWrap->size]->arg2 = curr_cmd->args[1];
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
                // handle undos/redos
                if(undo_count > redo_count) {
                    handle_undo(snapshots, editor, commandWrap, undo_count, redo_count, snap_size, &command_counter, &executed_undos, &curr_snap);
                } else if(redo_count > 0 && undo_count < redo_count) {
                    handle_redo(snapshots, editor, commandWrap, redo_count - undo_count, snap_size, &command_counter, &curr_snap);
                    // shift command counter
                    executed_undos -= redo_count - undo_count;
                }
                undo_count = 0;
                redo_count = 0;
                handle_print(editor, curr_cmd->args[0], curr_cmd->args[1]);
                break;
            case DELETE:
                if(undo_count > redo_count) {
                    // permanent undo
                    handle_undo(snapshots, editor, commandWrap, undo_count, redo_count, snap_size, &command_counter, &executed_undos, &curr_snap);
                    make_permanent(snapshots, commandWrap, curr_snap, snap_size, command_counter - curr_snap);
                    snap_size = curr_snap;
                } else if(redo_count > 0 && undo_count < redo_count) {
                    // permanent redo
                    handle_redo(snapshots, editor, commandWrap, redo_count - undo_count, snap_size, &command_counter, &curr_snap);
                    make_permanent(snapshots, commandWrap, curr_snap, snap_size, command_counter - curr_snap);
                    snap_size = curr_snap;
                } else if(executed_undos > 0) {
                    make_permanent(snapshots, commandWrap, curr_snap, snap_size, command_counter - curr_snap);
                    snap_size = curr_snap;
                }
                command_counter++;
                snap_size++;
                executed_undos = 0;
                undo_count = 0;
                redo_count = 0;
                // resize snapshot structure if needed
                if(snap_size >= snap_capacity) {
                    snapshots = (snapshot_t **) realloc(snapshots, (snap_size + INCREASE_CONST) * sizeof(snapshot_t *));
                    snap_capacity = snap_size + INCREASE_CONST;
                    for(int i = snap_size; i < snap_capacity; i++) {
                        snapshots[i] = (snapshot_t *) malloc(sizeof(snapshot_t));
                    }
                }
                /*snap_indexes->size++;
                // resize indexes structure if needed
                if(snap_indexes->size >= snap_indexes->capacity) {
                    snap_indexes->array = (int *) realloc(snap_indexes->array, (snap_indexes->size + INCREASE_CONST) * sizeof(int));
                    snap_indexes->capacity = snap_indexes->size + INCREASE_CONST;
                }
                snap_indexes->array[snap_indexes->size] = command_counter;*/
                snapshots[snap_size]->index = command_counter;
                curr_snap = snap_size;
                handle_delete(editor, snapshots, snap_size, curr_cmd->args[0], curr_cmd->args[1]);
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
