#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

/**
 * IDEA: potrei mantenere ogni deleted chunk all'interno della struttura,
 *  aggiungendo un campo `jump_to` ad ogni elemento eliminato.
 *      PRO: velocizzare la delete, e di molto (no memcpy e niente shift)
 *      CON: pochissimo più overhead e complicattezza nelle change e print;
            con tante delete la complessità potrebbe essere eccessivamente alta?
            In caso ci fossero N operazioni di delete atomiche e consecutive ogni change/print/undo/redo costa O(N) goddammit
 *      UNCHANGED: consumo di memoria rimane lo stesso (in realtà aumenta l'uso di 4/8 byte per ogni linea),
            stessa complessità spaziale e temporale
*/

// TODO: fix duplicating pointers!

#define INPUT_MAX_LENGTH 1025
#define CAPACITY_CONST 10000
//#define max(x,y) (x > y ? x : y)
//#define DEBUG 1

// commands in enum (BOTTOM is an auxiliary command that represents the bottom of the stack)
enum cmd_type {CHANGE, DELETE, PRINT, UNDO, REDO, QUIT, BOTTOM};

typedef struct {
    enum cmd_type type;
    long int args[2];
}cmd;

typedef struct txt_line_s {
    char* content;
    struct txt_line_s* right;
    struct txt_line_s* left;
}txt_line_t;

typedef struct lines_stack_s{
    long int index;
    long int size;
    long int capacity;
    txt_line_t** lines;
}lines_stack_t;

typedef struct cmd_stack_node_s{
    cmd* command;
    struct cmd_stack_node_s* next;
}cmd_stack_node_t;

typedef struct cmd_head_s {
    cmd_stack_node_t* head;
    long int size;
}cmd_head_t;

typedef struct del_list_node_s{
    lines_stack_t* linesStack;
    struct del_list_node_s* next;
}del_list_node_t;


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
        if(linesStack->lines[i]->right != NULL) {
            txt_line_t* tmp = linesStack->lines[i];
            printf("%s <- %s -> ", (tmp->left!=NULL)?tmp->left->content:"(null)", tmp->content);
            tmp = tmp->right;
            while(tmp != NULL) {
                printf("\t%s; ", tmp->content);
                tmp = tmp->right;
            }
            printf("\n");
        } else {
            printf("%s\n", linesStack->lines[i]->content);
        }
    }
    printf("----------------\n");
}

void debug_print_cmd_stack(cmd_head_t* cmdHead) {
    cmd_stack_node_t* tmp = cmdHead->head;
    printf("Commands: size = %ld\n", cmdHead->size);
    while(tmp->command->type != BOTTOM) {
        debug_print_cmd(tmp->command);
        tmp = tmp->next;
    }
}

/**
 * Pushes a command inside the cmdHead stack.
 * @param cmdHead the command stack head.
 * @param command the command to be pushed.
 */
void push_cmd(cmd_head_t** cmdHead, cmd* command) {
    cmd_stack_node_t* tmp = (cmd_stack_node_t *)malloc(sizeof(cmd_stack_node_t));
    tmp->command = command;
    tmp->next = (*cmdHead)->head;
    (*cmdHead)->head = tmp;
    (*cmdHead)->size++;
}

/**
 * Moves the first command from a stack head to another
 * @param from
 * @param to
 */
void move_cmd(cmd_head_t* from, cmd_head_t* to) {
    cmd_stack_node_t* tmp = from->head;
    from->head = from->head->next;
    from->size--;
    tmp->next = to->head;
    to->head = tmp;
    to->size++;
}

/**
 * Deletes all nodes into the undo command stack: used after a permanent undo/redo.
 * @param cmd the undo command structure *content* (not the head)
 */
void clear_undo_stack(cmd_stack_node_t** cmd) {
    cmd_stack_node_t* curr = *cmd;
    cmd_stack_node_t* next;

    while(curr->command->type != BOTTOM) {
        next = curr->next;
        free(curr->command);
        curr->command = NULL;
        free(curr);
        curr = NULL;
        curr = next;
    }
    *cmd = curr;
}

/**
 * Pushes a line into the lines structure
 * @param line The line object
 * @param text the text (already alloc'd) to push
 */
void push_line(txt_line_t** line, char* text) {
    txt_line_t* tmp;
    tmp = (txt_line_t *)calloc(1, sizeof(txt_line_t));
    tmp->content = text;
    tmp->right = *line;
    if(*line != NULL) {
        tmp->left = (*line)->left;
        if((*line)->left != NULL) {
            (*line)->left->right = tmp;
        }
        (*line)->left = tmp;
        if((*line)->content == NULL) {
            free(*line);
            *line = NULL;
            tmp->right = NULL;
        }
    } else {
        tmp->left = NULL;
    }
    *line = tmp;
}

/**
 * Moves a line (from, to) properly.
 * @param linesStack
 * @param i_from
 * @param i_to
 */
void move_line(lines_stack_t* linesStack, long int i_from, long int i_to) {
    linesStack->lines[i_to]->content = linesStack->lines[i_from]->content;
    linesStack->lines[i_to]->left = linesStack->lines[i_from]->left;
    linesStack->lines[i_to]->right = linesStack->lines[i_from]->right;
    if(linesStack->lines[i_to]->right != NULL) {
        linesStack->lines[i_to]->right->left = linesStack->lines[i_to];
    }
    if(linesStack->lines[i_to]->left != NULL) {
        linesStack->lines[i_to]->left->right = linesStack->lines[i_to];
    }
}
/**
 * Handles change. For each line inputted after 'c' command it pushes the line in place.
 * If a line is occupied, the old line gets pushed to the right, and the new line is pointed by the structure.
 * Note that the left pointer remains untouched after this operation.
 * @param linesStack the main lines structure
 * @param command the change command object
 */
void handle_change(lines_stack_t* linesStack, cmd *command) {
    char* input;
    char buff[INPUT_MAX_LENGTH];

    // if a resize is necessary... do it.
    if(command->args[1] > linesStack->capacity) {
        linesStack->lines = (txt_line_t **)realloc(linesStack->lines, (command->args[1] + CAPACITY_CONST) * sizeof(txt_line_t*));
        for(long int i = linesStack->capacity; i < command->args[1] + CAPACITY_CONST; i++) {
            linesStack->lines[i] = NULL;
        }
        linesStack->capacity = command->args[1] + CAPACITY_CONST;
    }
    if(command->args[1] > linesStack->size) linesStack->size = command->args[1];
    // get input lines
    for(long int i = command->args[0] - 1; i <= command->args[1] - 1; i++) {
        fgets(buff, INPUT_MAX_LENGTH, stdin);
        input = (char *)malloc((strlen(buff) + 1) * sizeof(char));
        strcpy(input, buff);
        push_line(&(linesStack->lines[i]), input);
    }
    // .\n
    getchar_unlocked();
    getchar_unlocked();
}

/**
 * Handles print. Iterates through the lines structure, if the line has null content it prints '.\n',
 *  else prints the line's content.
 * @param linesStack the lines structure
 * @param from command parameter
 * @param to command parameter
 */
void handle_print(lines_stack_t* linesStack, long int from, long int to) {
    long int delta = to - from;
    int count = 0;
    long int x = from - 1;
    while(count < delta + 1) {
        if(x >= linesStack->size || x < 0 || linesStack->lines[x]->content == NULL) {
            fputs(".\n", stdout);
            count++;
        } else {
            fputs(linesStack->lines[x]->content, stdout);
            count++;
            x++;
        }
    }
}

/**
 * Handles delete. It deletes a chunk of lines and pushes it to the delete stack structure.
 * if delta <=0: create a dummy delete to ignore in case of undo
 * else:
 *      create a new delete stack node;
 *      transfer the chunk into the node;
 *      shift the lines structure (fill the gap);
 * @param linesStack
 * @param delStack
 * @param from
 * @param to
 */
void handle_delete(lines_stack_t* linesStack, del_list_node_t** delStack, long int* from, long int* to) {
    del_list_node_t* new_tmp;
    if(*to >= linesStack->size) *to = linesStack->size;
    register long int delta = *to - *from + 1;
    if(delta <= 0 || linesStack->size == 0) {
        new_tmp = (del_list_node_t*) malloc(sizeof(del_list_node_t));
        new_tmp->next = *delStack;
        new_tmp->linesStack = (lines_stack_t*) malloc(sizeof(lines_stack_t));
        new_tmp->linesStack->index = -1;
        new_tmp->linesStack->lines = NULL;
        new_tmp->linesStack->size = -1;
        new_tmp->linesStack->capacity = -1;
        *delStack = new_tmp;
        return;
    }
    new_tmp = (del_list_node_t*) malloc(sizeof(del_list_node_t));
    new_tmp->next = *delStack;
    new_tmp->linesStack = (lines_stack_t*) malloc(sizeof(lines_stack_t));
    new_tmp->linesStack->index = *from;
    new_tmp->linesStack->lines = (txt_line_t**) calloc(delta, sizeof(txt_line_t*));
    new_tmp->linesStack->size = delta;
    new_tmp->linesStack->capacity = delta;
    // copy lines to delete into the delete node
    for(long int i = 0; i < delta; i++) {
        new_tmp->linesStack->lines[i] = (txt_line_t *)calloc(1, sizeof(txt_line_t));
        new_tmp->linesStack->lines[i]->left = linesStack->lines[i + *from - 1]->left;
        new_tmp->linesStack->lines[i]->right = linesStack->lines[i + *from - 1]->right;
        new_tmp->linesStack->lines[i]->content = linesStack->lines[i + *from - 1]->content;
    }
    //memcpy(new_tmp->linesStack->lines, linesStack->lines + (from - 1), delta * sizeof(txt_line_t*));
    *delStack = new_tmp;

    // shift linesStack (fill the gap)
    for(long int i = *from - 1; i + delta <= linesStack->size; i++) {
        if(linesStack->lines[i + delta] == NULL) {
            linesStack->lines[i]->content = NULL;
            linesStack->lines[i]->right = NULL;
            linesStack->lines[i]->left = NULL;
            continue;
        }
        move_line(linesStack, i + delta, i);
        linesStack->lines[i + delta]->content = NULL;
        linesStack->lines[i + delta]->right = NULL;
        linesStack->lines[i + delta]->left = NULL;
    }
    if(*to >= linesStack->size) {
        for(long int i = *from - 1; i < linesStack->size; i++) {
            linesStack->lines[i]->content = NULL;
            linesStack->lines[i]->right = NULL;
            linesStack->lines[i]->left = NULL;
        }
    } else {
        for(long int i = linesStack->size - delta; i < linesStack->size; i++) {
            linesStack->lines[i]->content = NULL;
            linesStack->lines[i]->right = NULL;
            linesStack->lines[i]->left = NULL;
        }
    }
    // resize stack
    linesStack->size = linesStack->size - delta;
    // lets not actually resize it, to save time.
    /*
    linesStack->lines = (txt_line_t **) realloc(linesStack->lines, (linesStack->size - delta + CAPACITY_CONST) * sizeof(txt_line_t*));
    linesStack->capacity = linesStack->size + CAPACITY_CONST;
     */
}

/**
 * Slides a text line to the right: used for redo of a change.
 * @param line
 */
void slide_right_line(txt_line_t** line) {
    txt_line_t* tmp = *line;
    (*line) = (*line)->left;
    (*line)->right = tmp;
    if((*line)->right->content == NULL) {
        free((*line)->right);
        (*line)->right = NULL;
    }
}

/**
 * Slides a text line to the left: used for undo of a change.
 * @param line
 */
void slide_left_line(txt_line_t** line) {
	if((*line)->right == NULL) {
		(*line)->right = (txt_line_t *) calloc(1, sizeof(txt_line_t));
	}
    txt_line_t* tmp = *line;
    (*line) = (*line)->right;
    (*line)->left = tmp;
}

/**
 * Deletes all left nodes to the left: used after a permanent undo/redo to all involved lines.
 * @param line
 */
void clear_left_list(txt_line_t* line) {
    txt_line_t* curr = line->left;
    txt_line_t* next;
    /*
    while(curr != NULL) {
        next = curr->left;
        free(curr->content);
        free(curr);
        curr = next;
    }*/
    line->left = NULL;
}

/**
 * Undos a change: slides all involved lines to the left.
 * @param linesStack the lines structure
 * @param cmd the command to be undo'd
 */
void undo_change(lines_stack_t* linesStack, cmd_stack_node_t** cmd) {
    // slide left all the lines
    for(long int i = (*cmd)->command->args[0] - 1; i <= (*cmd)->command->args[1] - 1; i++) {
        slide_left_line(&linesStack->lines[i]);
    }
}

/**
 * Redos a change: slides all involved lines to the right.
 * @param linesStack the lines structure
 * @param cmd the command to be redo'd
 */
void redo_change(lines_stack_t* linesStack, cmd_stack_node_t** cmd) {
    // slide right all the lines
    for(long int i = (*cmd)->command->args[0] - 1; i <= (*cmd)->command->args[1] - 1; i++) {
        slide_right_line(&linesStack->lines[i]);
    }
}

/**
 * Undoes a delete operation.
 *      if the delete is a dummy one: ignore it;
 *      if the lines structure needs to be resized: do it (Note: not necessary because the delete does not shrink back the structure);
 *      shift the lines, creating a gap to insert the deleted lines to;
 *      insert the lines into the gap;
 *      pop stack node;
 * @param linesStack
 * @param delNode
 */
void undo_delete(lines_stack_t* linesStack, del_list_node_t** delNode) {
    if((*delNode)->linesStack->index == -1) {
        del_list_node_t* tmp = (*delNode)->next;
        free(*delNode);
        (*delNode) = tmp;
        return;
    }
    if(linesStack->capacity < (*delNode)->linesStack->size + linesStack->size) {
        // re-alloc linesStack
        linesStack->capacity = linesStack->size + (*delNode)->linesStack->size + CAPACITY_CONST;
        linesStack->lines = (txt_line_t **) realloc(linesStack->lines, (linesStack->capacity) * sizeof(txt_line_t*));
        for(long int i = linesStack->capacity - (*delNode)->linesStack->size; i < linesStack->capacity; i++) {
            linesStack->lines[i] = NULL;
        }
    }

    // shift all elements forward
    linesStack->size += (*delNode)->linesStack->size;
    // shift
    for(long int i = linesStack->size - 1; i - (*delNode)->linesStack->size >= (*delNode)->linesStack->index - 1; i--) {
        if(linesStack->lines[i] == NULL){
            // theoretically this if is useless AF
            linesStack->lines[i] = (txt_line_t *)calloc(1, sizeof(txt_line_t));
        }
        move_line(linesStack, i - (*delNode)->linesStack->size, i);
    }
    // re insert deleted lines
    int k = 0;
    for (long int i = (*delNode)->linesStack->index - 1; i < (*delNode)->linesStack->index + (*delNode)->linesStack->size - 1; i++) {
        linesStack->lines[i]->content = (*delNode)->linesStack->lines[k]->content;
        linesStack->lines[i]->right = (*delNode)->linesStack->lines[k]->right;
        linesStack->lines[i]->left = (*delNode)->linesStack->lines[k]->left;
        free((*delNode)->linesStack->lines[k]);
        k++;
    }

    // pop delNode List element
    del_list_node_t* next = (*delNode)->next;
    free(*delNode);
    (*delNode) = next;
}

/**
 * @deprecated
 * Pops a command from a command stack and returns it.
 * @param cmdHead
 * @return
 */
cmd_stack_node_t* pop_cmd(cmd_head_t** cmdHead) {
    cmd_stack_node_t* tmp = (*cmdHead)->head;
    (*cmdHead)->head = (*cmdHead)->head->next;
    (*cmdHead)->size--;
    return tmp;
}

/**
 * Pops a command from a command stack and frees it.
 * @param cmdHead
 */
void pop_cmd_free(cmd_head_t** cmdHead) {
    cmd_stack_node_t* tmp = (*cmdHead)->head;
    (*cmdHead)->head = (*cmdHead)->head->next;
    (*cmdHead)->size--;
    free(tmp->command);
    free(tmp);
}

/**
 * Handles a temporary undo.
 * Iterates through the command stack, calls the right undo functions and moves the command from cmdHead to undoHead.
 * @param linesStack
 * @param delStack
 * @param cmdHead
 * @param undoHead
 * @param steps number of undo steps
 */
void handle_temp_undo(lines_stack_t* linesStack, del_list_node_t** delStack, cmd_head_t* cmdHead, cmd_head_t* undoHead, long int steps) {
    long int count = 0;

    while(cmdHead->head->command->type != BOTTOM && count < steps) {
        // stack the commands to undo and undo them
        switch(cmdHead->head->command->type) {
        	case CHANGE:
        		undo_change(linesStack, &cmdHead->head);
        		break;
        	case DELETE:
        		// re insert deleted chunk
        		undo_delete(linesStack, delStack);
        		break;
            default:
                break;
        }
        // move command from cmdHead to undoHead
        move_cmd(cmdHead, undoHead);
        count++;
    }
}

void make_permanent_undo(lines_stack_t* linesStack, cmd_head_t** undoHead) {
    long int old_size = linesStack->size;
    for(long int i = 0; i < old_size; i++) {
        clear_left_list(linesStack->lines[i]);
        if(linesStack->lines[i]->content == NULL)
            linesStack->size--;
    }
    clear_undo_stack(&(*undoHead)->head);
    (*undoHead)->size = 0;
    /*
    while((*undoHead)->head->command->type != BOTTOM) {
        if((*undoHead)->head->command->type == CHANGE) {
            for(long int i = (*undoHead)->head->command->args[0] - 1; i < (*undoHead)->head->command->args[1]; i++) {
                clear_left_list(linesStack->lines[i]);
            }
        }
        pop_cmd_free(undoHead);
    }*/
}

/**
 * Handles a permanent undo.
 * Iterates through the command stack, calls the right undo functions, pops command from the cmdHead and then purges the undo stack.
 * @param linesStack
 * @param delStack
 * @param cmdHead
 * @param undoHead
 * @param steps
 */
void handle_perm_undo(lines_stack_t* linesStack, del_list_node_t** delStack, cmd_head_t** cmdHead, cmd_head_t** undoHead, long int steps) {
    long int count = 0;

    // first, make sure the previously executed temporary undo are permanent
    if((*undoHead)->size > 0)
        make_permanent_undo(linesStack, undoHead);

    while((*cmdHead)->head->command->type != BOTTOM && count < steps) {
        // stack the commands to undo and undo them
        switch((*cmdHead)->head->command->type) {
        	case CHANGE:
        		undo_change(linesStack, &(*cmdHead)->head);
                // purge left tail
                for(long int i = (*cmdHead)->head->command->args[0] - 1; i < (*cmdHead)->head->command->args[1]; i++) {
                    clear_left_list(linesStack->lines[i]);
                }
                if(linesStack->lines[(*cmdHead)->head->command->args[0] - 1]->content == NULL) {
                    linesStack->size -= (*cmdHead)->head->command->args[1] - (*cmdHead)->head->command->args[0] + 1;
                }
        		break;
        	case DELETE:
        		undo_delete(linesStack, delStack);
        		break;
            default:
                break;
        }
        // pop from cmdHead, no need to push it to undoHead
        pop_cmd_free(cmdHead);
        count++;
    }
}

/**
 * Handles a temporary redo.
 * Calls the right redo functions and moves the commands from undoHead to cmdHead
 * @param linesStack
 * @param delStack
 * @param cmdHead
 * @param undoHead
 * @param steps
 */
void handle_temp_redo(lines_stack_t* linesStack, del_list_node_t** delStack, cmd_head_t* cmdHead, cmd_head_t* undoHead, long int steps) {
    long int count = 0;

    while(undoHead->head->command->type != BOTTOM && count < steps) {
        // stack the commands to undo and undo them
        switch(undoHead->head->command->type) {
        	case CHANGE:
        		redo_change(linesStack, &undoHead->head);
        		break;
        	case DELETE:
        		// re-perform deletion
        		handle_delete(linesStack, delStack, &undoHead->head->command->args[0], &undoHead->head->command->args[1]);
        		break;
            default:
                break;
        }
        // move from undoHead to cmdHead
        move_cmd(undoHead, cmdHead);
        count++;
    }
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
#ifdef DEBUG
    int dbg_delete_count = 0;
#endif
    cmd* command;
    long int undo_count = 0;
    long int redo_count = 0;
    // all lines with history
    lines_stack_t* linesStack;
    del_list_node_t* delList = NULL;
    // command history
    cmd_head_t* cmdHead;
    cmd_head_t* undoHead;

    cmdHead = (cmd_head_t *)malloc(sizeof(cmd_head_t));
    cmdHead->size = 0;
    undoHead = (cmd_head_t *)malloc(sizeof(cmd_head_t));
    undoHead->size = 0;

    cmdHead->head = (cmd_stack_node_t *)malloc(sizeof(cmd_stack_node_t));
    cmdHead->head->command = (cmd*) malloc(sizeof(cmd));
    cmdHead->head->command->type = BOTTOM;
    cmdHead->head->next = NULL;

    undoHead->head = (cmd_stack_node_t *)malloc(sizeof(cmd_stack_node_t));
    undoHead->head->command = (cmd*) malloc(sizeof(cmd));
    undoHead->head->command->type = BOTTOM;
    undoHead->head->next = NULL;

    linesStack = (lines_stack_t *)malloc(sizeof(lines_stack_t));
    linesStack->index = 0;
    linesStack->lines = (txt_line_t **)calloc(CAPACITY_CONST, sizeof(txt_line_t*));
    linesStack->size = 0;
    linesStack->capacity = CAPACITY_CONST;

    long int command_count = 0;

    #ifdef DEBUG
        printf("WARNING: DEBUG MODE\n");
    #endif

    command = parse_cmd();
    while(command->type != QUIT) {
        command_count++;
        switch (command->type) {
            case CHANGE:
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
                handle_change(linesStack, command);
                push_cmd(&cmdHead, command);
                break;
            case PRINT:
            	// first handle undos/redos (both functions end in case the command stack / undo stack gets depleted)
            	if(undo_count > redo_count) {
                	handle_temp_undo(linesStack, &delList, cmdHead, undoHead, undo_count - redo_count);
            	} else if(redo_count > 0 && undo_count < redo_count) {
                	handle_temp_redo(linesStack, &delList, cmdHead, undoHead, redo_count - undo_count);
                }
                undo_count = 0;
                redo_count = 0;
                handle_print(linesStack, command->args[0], command->args[1]);
                break;
            case DELETE:
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
                handle_delete(linesStack, &delList, &command->args[0] + (command->args[0] <= 0 ? 1 : 0), &command->args[1]);
                if(delList->linesStack->index == -1) command->args[1] = -1;
                push_cmd(&cmdHead, command);
                break;
            case UNDO:
                undo_count += command->args[0];
                // cap undo value
                if(undo_count > cmdHead->size + redo_count)
                    undo_count = cmdHead->size + redo_count;
                break;
            case REDO:
                redo_count += command->args[0];
                // cap redo value
                if(redo_count > undo_count + undoHead->size)
                    redo_count = undo_count + undoHead->size;
                break;
            default:
                break;
        }
        command = parse_cmd();
    }
    #ifdef DEBUG
        printf("EQ_UN:: %ld equivalent undo; \n", undo_count - redo_count);
        printf("----------\nCommand stack: \n");
        debug_print_cmd_stack(cmdHead);
        printf("----------\nUndo stack: \n");
        debug_print_cmd_stack(undoHead);
        debug_print_lines_stack(linesStack);
    #endif
    return 0;
}
