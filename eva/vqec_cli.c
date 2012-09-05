/**-----------------------------------------------------------------
 * VQE-C Lightweight command line interpreter
 *
 * Copyright (c) 2009-2011 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include "vqec_cli.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <utils/strl.h>
#include <errno.h>

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/* Forward declaration */
static void vqec_cli_display_prompt(struct vqec_cli_def *cli, int sockfd);
static vqec_cli_error_t exec_cmd(struct vqec_cli_def *cli, char *cmd_ptr,
                                 int cmd_ptr_len,
                                 vqec_cli_disp_options_t disp_opt);

/*
 * Safe version of write function 
 */
void cli_write(int fd, const void *buf, size_t count, struct vqec_cli_def *cli)
{
    int written = 0;
    written = write(fd, buf, count);
    if ((written < 0) && (errno == ECONNRESET || errno == EPIPE)) {
        /* need to quit CLI in this case */
        cli->quit = TRUE;
    }
    /* We ignore all other cases */
    return;
}

/*
 * Helper function to convert a string to lower case characters
 *
 * param[in] in_string char pointer to array to be converted to lower case
 * param[out] outputString char pointer to array populated by this function 
 * param[in] len length of characters to be converted
 * @returns void
 */
static void vqec_cli_str_to_lower (char *output_string, char *in_string, 
                                   int len)
{
    int in_len = 0;
    if ((in_string == NULL) || (output_string == NULL)) {
        return;
    }
    while (in_len < len) {
        if (isupper(in_string[in_len])) {
            output_string[in_len] = tolower(in_string[in_len]);
        } else {
            output_string[in_len] = in_string[in_len];
        }
        in_len++;
    }
}

/* 
 * Function to check if a command with the specified mode 
 * and privilege is permitted in the specified CLI state
 *
 * param[in] mode integer specifying mode of the command
 * param[in] privilege integer specifying privilege of the command
 * param[in] cli_state integer specifying state of the CLI
 * @returns vqec_cli_error_t Error code representing success or failure
 */
static vqec_cli_error_t cmd_exec_check (int mode, int privilege, int cli_state)
{
    vqec_cli_error_t retval = VQEC_CLI_ERROR;
    /*
     * Can see only unprivileged commands from base state 
     */
    if (cli_state == VQEC_CLI_STATE_EXEC) {
        /* 
         * Both Privileged & unprivileged, but VQEC_CLI_MODE_EXEC, 
         * VQEC_CLI_MODE_ANY cmds 
         */
        if ((mode == VQEC_CLI_MODE_EXEC) ||
            (mode == VQEC_CLI_MODE_ANY)) {
            return VQEC_CLI_OK;
        } 
    } else if (cli_state == VQEC_CLI_STATE_CONFIG) {
        /*
         * Both Privileged & unprivileged, 
         * but VQEC_CLI_MODE_CONFIG, VQEC_CLI_MODE_ANY cmds 
         */
        if ((mode == VQEC_CLI_MODE_CONFIG) ||
            (mode == VQEC_CLI_MODE_ANY)) {
            return VQEC_CLI_OK;
        }
    } else if (cli_state == VQEC_CLI_STATE_BASE) {
        /* Unprivileged cmds, any mode */
        if (privilege == PRIVILEGE_UNPRIVILEGED) {
            return VQEC_CLI_OK;
        }
    }
    return retval;
}

/* 
 * Function to search the specified command tree for a command, and return
 * the best matching vqec_cli_command node if found
 *
 * param[in] tqh vqec_cmd_tree VQE_TAILQ Head
 * param[in] dest_cmd command string to be searched
 * param[in] cmd_len the number of bytes that will represent the command
 * param[in] state vqec_cli_state_t specifying state of the CLI
 * param[in] result_ptr Pointer to the vqec_cli_command node with the best match
 * @returns vqec_cli_error_t Error code representing success or failure
 */
static vqec_cli_error_t search_node (struct vqec_cmd_tree *tqh, 
                                     char *dest_cmd, int cmd_len,
                                     vqec_cli_state_t state,
                                     struct vqec_cli_command **result_ptr,
                                     struct vqec_cli_def *cli, 
                                     boolean print_help)
{
    int slen;
    vqec_cli_error_t retval = VQEC_CLI_ERROR;
    struct vqec_cli_command *temp_node_ptr;
    *result_ptr = NULL;

    if (VQE_TAILQ_EMPTY(tqh)) return retval;
    if (!dest_cmd) return retval;

    /* Search for unique match in params */
    vqec_cli_str_to_lower(dest_cmd, dest_cmd, cmd_len);
    slen = strlen(dest_cmd);
    if (slen == 0) return retval;

    VQE_TAILQ_FOREACH_REVERSE(temp_node_ptr, tqh, vqec_cmd_tree, 
                              vqec_cli_command_ptrs) {
 
        /* Command is always lowercase (taken care of during registration) */
        if ((strncmp(dest_cmd, temp_node_ptr->command, slen) == 0) &&
            (cmd_exec_check(temp_node_ptr->mode, 
                            temp_node_ptr->privilege,
                            state) == VQEC_CLI_OK)) {
            /* Check for exact match */
            if (strlen(dest_cmd) == strlen(temp_node_ptr->command)) {
                *result_ptr = temp_node_ptr;
                return (VQEC_CLI_OK);
            }
            /* Allow partial match */
            if (retval == VQEC_CLI_ERROR) {
                *result_ptr = temp_node_ptr;     /* partial match */
                retval = VQEC_CLI_OK;
            } else {
                /* duplicate match */
                retval = VQEC_CLI_DUP_OK;
            }
            if (print_help) {
                vqec_cli_print(cli, "  %-20s %s",
                               temp_node_ptr->command,
                               temp_node_ptr->help);
            }
        }
    }
    return (retval);
}

/* 
 * Helper function to insert the node as the child of the parent node 
 *
 * param[in] vqec_cmd_tree_head Pointer to the vqec_cmd_tree
 * param[in] parent Pointer to the vqec_cli_command parent node
 * param[in] child_node Pointer to the vqec_cli_command child node that would
 *           be inserted as the child of the parent
 * @returns void
 */
static void insert_node (struct vqec_cmd_tree *vqec_cmd_tree_head,
                         struct vqec_cli_command *parent,
                         struct vqec_cli_command *child_node) 
{
    if (!parent) {
        /* No parent indicates that this is a node in the first level */
        if (VQE_TAILQ_EMPTY(vqec_cmd_tree_head)) {
            VQE_TAILQ_INIT(vqec_cmd_tree_head);
        }
        VQE_TAILQ_INSERT_HEAD(vqec_cmd_tree_head, 
                          child_node, vqec_cli_command_ptrs);
    } else {
        /* Find the TQH of the parent's child queue */
        if (VQE_TAILQ_EMPTY(&parent->child_cmd_tree_head)) {
            VQE_TAILQ_INIT(&(parent->child_cmd_tree_head));
        }
        VQE_TAILQ_INSERT_HEAD(&parent->child_cmd_tree_head, 
                          child_node, vqec_cli_command_ptrs);
    }
}

/* Callback function for history command */
UT_STATIC int vqec_cli_history_func(struct vqec_cli_def *cli, 
                                    char *command, char *argv[], int argc) 
{
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    int j = 0;
    if (!cli) {
        return VQEC_CLI_ERROR;
    }
    if (argc == 0) {
        vqec_cli_print(cli, "Command History:");
        for (i=cli->hist_first_index; 
             i < (cli->hist_first_index + VQEC_CLI_MAX_HISTORY); i++) {
            if (cli->hist[i % VQEC_CLI_MAX_HISTORY]) {
                vqec_cli_print(cli, "%3d. %s", j++, 
                               cli->hist[(i)%VQEC_CLI_MAX_HISTORY]);
            }
        }
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 
            }
            vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        }
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/* Callback function for help to display the commands available for this mode */
UT_STATIC int vqec_cli_help_func(struct vqec_cli_def *cli, 
                                 char *command, char *argv[], int argc)
{
    char *cmd;
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    vqec_cli_error_t retval;
    cmd = malloc(1 * sizeof (char)); /* Allocate in the heap */
    if (!cmd) {
        return VQEC_CLI_ERROR;
    }
    cmd[0] = '\0';
    if (!cli) {
        free(cmd);
        return VQEC_CLI_ERROR;
    }

    if (argc == 0) {
        retval = exec_cmd(cli, cmd, 0, VQEC_CLI_DISP_DESC);
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 
            }
            vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        }
        free(cmd);
        return VQEC_CLI_ERROR;
    }
    free(cmd);
    return VQEC_CLI_OK;
}


/* Callback function to Quit the CLI */
UT_STATIC int vqec_cli_quit_func(struct vqec_cli_def *cli, 
                                 char *command, char *argv[], int argc)
{
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    if (!cli) {
        return VQEC_CLI_ERROR;
    }
    if (argc == 0) {
        cli->quit = TRUE;
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 
            }
        }
        vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/* Callback function to Exit the mode */
UT_STATIC int vqec_cli_exit_func(struct vqec_cli_def *cli, 
                                 char *command, char *argv[], int argc)
{
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    if (!cli) {
        return VQEC_CLI_ERROR;
    }
    if (argc == 0) {
        if (cli->state == VQEC_CLI_STATE_CONFIG) {
            cli->state = VQEC_CLI_STATE_EXEC;
        } else {
            cli->quit =  TRUE;
        }
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 
            }
        }
        vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/* 
 * Callback funciton to enter 'Enable mode'. 
 * This turns on privileged commands. 
 */
UT_STATIC int vqec_cli_enable_func(struct vqec_cli_def *cli, 
                                   char *command, char *argv[], int argc)
{
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    if (!cli) {
        return VQEC_CLI_ERROR;
    }
    if (argc == 0) {
        /* to take the user from VQEC_CLI_STATE_BASE to VQEC_CLI_STATE_EXEC */
        if (cli->state == VQEC_CLI_STATE_BASE) {
            cli->privilege = PRIVILEGE_PRIVILEGED;
            cli->state = VQEC_CLI_STATE_EXEC;
        }
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 

            }
        }
        vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/* callback function for the 'disable' command */
UT_STATIC int vqec_cli_disable_func(struct vqec_cli_def *cli, char *command, 
                                    char *argv[], int argc)
{
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    if (!cli) {
        return VQEC_CLI_ERROR;
    }
    if (argc == 0) {
        if (cli->state == VQEC_CLI_STATE_EXEC) {
            cli->state = VQEC_CLI_STATE_BASE;
            cli->privilege = PRIVILEGE_UNPRIVILEGED;
        }
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 
            }
        }
        vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/* callback function for the 'configure terminal' command */
UT_STATIC int vqec_cli_conf_terminal_func(struct vqec_cli_def *cli, 
                                          char *command, char *argv[], int argc)
{
    char temp_cmd[VQEC_CLI_MAX_LINE_SIZE];
    int i;
    if (!cli) {
        return VQEC_CLI_ERROR;
    }
    if (argc == 0) {
        if ((cli->state == VQEC_CLI_STATE_EXEC) && (argc == 0)) {
            cli->state = VQEC_CLI_STATE_CONFIG;
        }
    } else if ((argc == 1) && (argv[0][0] == '?')) {
        /* Do nothing */
    } else {
        if (command) {
            temp_cmd[0] = '\0';
            strncat(temp_cmd, command, strlen(command));
            for (i=0; i<argc; i++) {
                strncat(temp_cmd, " ", 1);
                strncat(temp_cmd, argv[i], strlen(argv[i])); 
            }
        }
        vqec_cli_print(cli, "Invalid command \"%s\"", temp_cmd);
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/* 
 * API to set up internal data structures for creating a VQE-C CLI object 
 *
 * @return struct vqec_cli_def Returns the pointer to a new vqec_cli_def 
 *                             VQE-C CLI object if the API was successfully 
 *                             executed, and NULL otherwise.
 */
struct vqec_cli_def *vqec_cli_init(void) 
{
    /* Create a CLI-object */
    struct vqec_cli_def *cli;
    struct vqec_cli_command *c;
    int i;
    if (!(cli = calloc(sizeof(struct vqec_cli_def), 1))) {
        return (NULL);
    }
    /* Initialize the TQH to the cmd tree */
    VQE_TAILQ_INIT(&cli->vqec_cmd_tree_head);

    /* Also register commands such as follows */
    vqec_cli_register_command(cli, 0, "help", vqec_cli_help_func, 
                              PRIVILEGE_UNPRIVILEGED,
                              VQEC_CLI_MODE_ANY, "Show available commands");

    vqec_cli_register_command(cli, 0, "history", vqec_cli_history_func,
                              PRIVILEGE_UNPRIVILEGED,
                              VQEC_CLI_MODE_ANY, "Show command history");

    vqec_cli_register_command(cli, 0, "quit", vqec_cli_quit_func, 
                              PRIVILEGE_UNPRIVILEGED,
                              VQEC_CLI_MODE_ANY, "Disconnect");

    vqec_cli_register_command(cli, 0, "logout", vqec_cli_quit_func, 
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_ANY, 
                              "Disconnect");

    vqec_cli_register_command(cli, 0, "exit", vqec_cli_exit_func, 
                              PRIVILEGE_UNPRIVILEGED,
                              VQEC_CLI_MODE_ANY, "Exit from current mode");

    vqec_cli_register_command(cli, 0, "enable", vqec_cli_enable_func, 
                              PRIVILEGE_UNPRIVILEGED,
                              VQEC_CLI_MODE_EXEC, 
                              "Turn on privileged commands");

    vqec_cli_register_command(cli, 0, "disable", vqec_cli_disable_func,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_EXEC, 
                              "Turn off privileged commands");

    c = vqec_cli_register_command(cli, 0, "configure", 0, PRIVILEGE_PRIVILEGED,
                                  VQEC_CLI_MODE_EXEC, 
                                  "Enter configuration mode");

    vqec_cli_register_command(cli, c, "terminal", vqec_cli_conf_terminal_func,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Configure from the terminal");

    cli->privilege = PRIVILEGE_UNPRIVILEGED;
    cli->state = VQEC_CLI_STATE_BASE;
    cli->quit = 0; 
    cli->substr = NULL;
    /* Initialize the history array to NULL */
    for (i=0; i<VQEC_CLI_MAX_HISTORY; i++) {
        cli->hist[i] = NULL;
    }
    cli->hist_first_index = 0;
    return cli;
}

/* 
 * Helper function to recusively delete a command tree's nodes
 *
 * param[in] vqec_cmd_tree_head Pointer to the vqec_cmd_tree
 * @returns void
 */
static void delete_tree(struct vqec_cmd_tree *tqh) 
{
    struct vqec_cmd_tree *temp_tqh;
    struct child_cmd_tree *child_tqh;
    struct vqec_cli_command *temp_node_ptr;

    if (tqh) {
        VQE_TAILQ_FOREACH_REVERSE(temp_node_ptr, 
                              tqh, 
                              vqec_cmd_tree, 
                              vqec_cli_command_ptrs) {
            
            child_tqh = (struct child_cmd_tree *)
                        &temp_node_ptr->child_cmd_tree_head;
            temp_tqh = (struct vqec_cmd_tree *) child_tqh;
            free(temp_node_ptr->command);
            free(temp_node_ptr->help);
            if (!VQE_TAILQ_EMPTY(temp_tqh)) {
               delete_tree(temp_tqh);    
            }
        }
    }
}

/* 
 * API to free memory used by VQE-C CLI
 *
 * param[in] cli Pointer to the vqec_cli_def structure.
 * @return void 
 */
void vqec_cli_deinit (struct vqec_cli_def *cli)
{
    struct vqec_cmd_tree *th;
    if (cli) {
        free(cli->banner);
        free(cli->hostname);
        th = (struct vqec_cmd_tree *) &cli->vqec_cmd_tree_head;
        if (!VQE_TAILQ_EMPTY(th)) {
            delete_tree(th);    
        }
    }
}

/* 
 * API to register a new command with the VQE-C CLI
 *
 * param[in] cli Pointer to the vqec_cli_def structure.
 * param[in] parent Pointer to vqec_cli_command structure, which is 
 *                  the parent node of whom this command is to be a child, 
 *                  in the CLI command tree.
 * param[in] command char * - the command string 
 * param[in] callback Function pointer to the command handler.
 * param[in] privilege Integer representing the privilege of this command node
 * param[in] mode Integer representing the mode of this command node
 * param[in] help Pointer to a character string that would be displayed as help.
 * @return vqec_cli_command* Pointer to a vqec_cli_command structure, pointing 
 *                           to the newly created command tree node.
 */
struct vqec_cli_command *vqec_cli_register_command (struct vqec_cli_def *cli, 
                        struct vqec_cli_command *parent, 
                        char *command,
                        int (*callback)(struct vqec_cli_def *cli, char *, 
                                        char **, int),
                        int privilege, int mode, char *help)
{
    /* 
     * Create a new command node and insert into the cli_command parent's 
     * tree
     */
    struct vqec_cli_command *node;
    node = (struct vqec_cli_command *) 
           calloc(1, sizeof(struct vqec_cli_command));
    if (!node)  {
        return NULL;
    }
    node->callback = callback;
    if (command) {
        node->command = strdup(command);
        /* Convert command to lowercase */
        if (node->command) {
            vqec_cli_str_to_lower(node->command, node->command, 
                                  strlen(node->command));
        }
    } else {
        node->command = NULL;
    }
    node->mode = mode;
    node->privilege = privilege;
    if (help) {
        node->help = strdup(help);
    } else {
        node->help = NULL;
    }
    /* Initialise Node's child TailQ */
    VQE_TAILQ_INIT(&node->child_cmd_tree_head);
    
    /* Now insert this node into the parent's child_tailq */
    insert_node(&cli->vqec_cmd_tree_head, parent, node); 

    return (node);
}

/* 
 * Helper function to list all the children for a specified node 
 *
 * params[in] tqh VQE_TAILQ Head to the vqec_cmd_tree structure
 *                whose children have to be enumerated
 * params[in] cli pointer to the VQE-C CLI structure to be used for printing
 * @returns void
 */
static void print_list (struct vqec_cmd_tree *tqh, 
                        struct vqec_cli_def *cli)
{
    struct vqec_cli_command *temp_node_ptr;
    /* 
     * Reached end of command, no callback but
     * a child exists for this node.
     * List the elements in the child's queue.
     */
    if (tqh && cli) {
        VQE_TAILQ_FOREACH_REVERSE(temp_node_ptr, 
                                  tqh, 
                                  vqec_cmd_tree, 
                                  vqec_cli_command_ptrs) {
            if (cmd_exec_check(temp_node_ptr->mode, 
                               temp_node_ptr->privilege,
                               cli->state) == VQEC_CLI_OK) {
                vqec_cli_print(cli, "  %-20s %s",
                               temp_node_ptr->command,
                               temp_node_ptr->help);
            }
        }
    }
}

/*
 * Helper function to remove quotes from a string
 * The string may or may not be null terminated. This function will not
 * insert an NULL character at the end of the in_string array. 
 *
 * param[in] in_string pointer to char array that is to be de-quoted
 * param[in] len integer specifying the characters to be considered
 * @returns void
 */
static void remove_quotes (char *in_string, int len)
{
    if (in_string == NULL) {
        return;
    }
    int in_len = 0;
    int out_len = 0;
    while (in_len < len) {
        if (in_string[in_len] != '\"' &&
            in_string[in_len] != '\'') {
            in_string[out_len++] = in_string[in_len];
        }
        if (in_string[in_len] == '\0') {
            break;
        }
        in_len++;
    }
}

/* 
 * Execute the callback for the command specified.
 *
 * params[in] cli pointer to the vqec_cli_def object
 * params[in] cmd_ptr character pointer to a null terminated command string
 * params[in] cmd_ptr_len command length in the cmd_ptr array (excluding null)
 * params[in] disp_opt vqec_cli_disp_opt object representing type of display
 *                     expected from this function
 * @returns int Error code describing the success of the function
 */
static vqec_cli_error_t exec_cmd (struct vqec_cli_def *cli, 
                                  char *cmd_ptr,
                                  int cmd_ptr_len,
                                  vqec_cli_disp_options_t disp_opt)
{
    int cmdlen, i, j, k;
    vqec_cli_error_t retval;
    struct vqec_cli_command *cli_command_node = NULL;

    struct vqec_cmd_tree *tqh;
    struct child_cmd_tree *child_tqh;
    char *token;
    char *argv[VQEC_CLI_MAX_ARGS];
    char *new_token;
    int argc, ws;
    int max_argc = 0;
    /* Buffer to be used by strtok_r */
    char *save_ptr;
    boolean disp_opt_change;
    char *temp_buf;

    /* Boolean flag for keeping track of whether a character was seen */
    boolean char_flag = 0; 
    char cmd[VQEC_CLI_MAX_LINE_SIZE];
    char cmd_temp[VQEC_CLI_MAX_LINE_SIZE];

    /* 
     * We extract one token at a time, and then do a search in the 
     * CLI command tree 
     */
    if (cli && cmd_ptr) {
        /* Copy the cmd_ptr into a temp_buf as we may modify the cmd array */
        temp_buf = (char *) malloc(sizeof(char) * VQEC_CLI_MAX_LINE_SIZE);
        if (!temp_buf) {
            return VQEC_CLI_ERROR;
        }
        memcpy(temp_buf, cmd_ptr, VQEC_CLI_MAX_LINE_SIZE);
        cmd_ptr = temp_buf;
             
        /* 
         * If the command has a pipe, ensure that there is space before and 
         * after the pipe symbol, if found.
         */
        for (i=0; i <= cmd_ptr_len; i++) {
            /* cmd_ptr[cmd_ptr_len] is the last non-null character */
            if (cmd_ptr[i] == '|') {
                cmd_ptr_len += 2;
                if (cmd_ptr_len >= VQEC_CLI_MAX_LINE_SIZE - 2) {
                    cmd_ptr_len = VQEC_CLI_MAX_LINE_SIZE - 2;
                }
                /* Shift the command two to the right */
                for (j=cmd_ptr_len; j > (i+2); j--) { 
                    cmd_ptr[j] = cmd_ptr[j-2];
                } 
                cmd_ptr[i] = ' '; 
                if (i+1 <= VQEC_CLI_MAX_LINE_SIZE - 2) {
                    cmd_ptr[i+1] = '|';
                }
                if (i+2 <= VQEC_CLI_MAX_LINE_SIZE - 2) {
                    cmd_ptr[i+2] = ' '; /* Space */
                }
                i += 2;
            }
        }
        cmd_ptr[i] = '\0'; /* terminate the array with a NULL */

        j = 0;
        ws = 0;
        /* Remove all occurrences of multiple whitespace */
        for (i=0; i <= cmd_ptr_len; i++) {
            if (cmd_ptr[i] == ' ') {
                if (ws == 0 && (char_flag == 1)) {
                    cmd[j++] = ' ';
                }
                ws++;
            } else if (cmd_ptr[i] == '\0') {
                if (ws > 0 && (char_flag == 1)) {
                    /* One space was inserted into cmd */
                    cmd[j-1] = '\0';
                }
            } else {
                char_flag = 1;
                cmd[j++] = cmd_ptr[i];
                ws = 0;
            }
        }
        cmd[j] ='\0';
        tqh = (struct vqec_cmd_tree *) &cli->vqec_cmd_tree_head;
        cmdlen = strlen(cmd);
        if (cmdlen == 0) {
            if (disp_opt == VQEC_CLI_DISP_DESC) {
                print_list(tqh, cli);
            } 
            free(temp_buf);
            return VQEC_CLI_OK;
        }
        if (cmdlen >= VQEC_CLI_MAX_LINE_SIZE) {
            free(temp_buf);
            return VQEC_CLI_ERROR;
        }
           
        /* we will now tokenize the cmd */
        argc = 0;
        token = strtok_r(cmd, " ", &save_ptr);
        if (token == NULL) {
            free(temp_buf);
            return VQEC_CLI_OK;
        }
        cmd_temp[0] = '\0';
        while (token != NULL) {
            /* find the command with this token */
            retval = search_node(tqh, token, strlen(token),
                                 cli->state, &cli_command_node, cli, FALSE);
            if (retval == VQEC_CLI_OK) {
                /* Node found */
                if (cli_command_node->command) {
                    if (cmd_temp[0] != '\0') {
                        strncat(cmd_temp, " ", 1);
                    }
                    cmdlen = VQEC_CLI_MAX_LINE_SIZE - strlen(cmd_temp) - 1;
                    if (cmdlen > strlen(cli_command_node->command)) {
                        cmdlen = strlen(cli_command_node->command);
                    } 
                    strncat(cmd_temp, 
                            cli_command_node->command,
                            cmdlen);
                    cmd_temp[VQEC_CLI_MAX_LINE_SIZE - 1] = '\0';
                } 
                child_tqh = (struct child_cmd_tree *)
                            &cli_command_node->child_cmd_tree_head;
                tqh = (struct vqec_cmd_tree *) child_tqh;
                /* call the callback function associated with this node */
                if (cli_command_node->callback != NULL) {
                    token = strtok_r(NULL, " ", &save_ptr);
                    /* if token is not null, it means that the rest of the 
                     * tokens are meant to be arguments, and thus we should
                     * change the disp_opt to VQEC_CLI_DISP_USAGE if it was
                     * VQEC_CLI_DISP_DESC, and have the "?" as the last token
                     */
                    disp_opt_change = FALSE;
                    if (token != NULL && disp_opt == VQEC_CLI_DISP_DESC) {
                        disp_opt = VQEC_CLI_DISP_USAGE;
                        disp_opt_change = TRUE;
                    }
                    if (disp_opt != VQEC_CLI_DISP_DESC) {
                        /* 
                         * Tokenize the rest of the string and pass it as
                         * arguments to the callback function 
                         */
                        if (token) {
                            if (argc < VQEC_CLI_MAX_ARGS - 1) {
                                argv[argc++] = strdup(token);
                            }
                        }
                        while (token != NULL) {
                            token = strtok_r(NULL, " ", &save_ptr);
                            if (token) {
                                if (argc < VQEC_CLI_MAX_ARGS - 1) {
                                    argv[argc++] = strdup(token);
                                }
                            }
                        }

                        /* Manually add a "?" token for the special case */
                        if (disp_opt_change) {
                            if (argc < VQEC_CLI_MAX_ARGS - 1) {
                                argv[argc++] = strdup("?");
                            }
                        }

                        max_argc = argc;
                        /* check for filtering */
                        for (k=0 ; k < argc ; k++) {
                            if (strncmp(argv[k], "|", strlen(argv[k])) == 0) {
                                if ((k+1 < argc)) {
                                    vqec_cli_str_to_lower(argv[k+1],
                                                          argv[k+1],
                                                          strlen(argv[k+1]));
                                    if ((strncmp(argv[k+1], 
                                                 "include", 
                                                 strlen(argv[k+1])) == 0) ||
                                        (strncmp(argv[k+1], 
                                                 "grep", 
                                                 strlen(argv[k+1])) == 0)) {
                                        if (k+2 >= argc) {
                                            argc = k+1;
                                            break;
                                        } else {
                                            /* argv[k+2] is the substring
                                             * we store the string here
                                             * and use it during printing
                                             */
                                            cli->substr = strdup(argv[k+2]);
                                            if (cli->substr) {
                                                remove_quotes(cli->substr,
                                                             strlen(cli->substr)
                                                              + 1);
                                            }
                                            argc = k;
                                        }
                                    } else {
                                        /* input keyword not "include/grep" */
                                        argc = k+1;
                                        break;
                                    }
                                } else {
                                    argc = k+1;
                                }
                                break;
                            }
                        }
                        retval = cli_command_node->callback(cli, cmd_temp, 
                                                            argv, 
                                                            argc);
                        /* Clean up argv allocations */
                        for (k = 0; k < max_argc; k++) {
                            free(argv[k]);
                        }

                        if (cli->substr) {
                            free(cli->substr);
                            cli->substr =  NULL;
                        }
                        free(temp_buf);
                        return retval;
                    } else {
                        if (token == NULL) {
                            /* Print the description for this node */
                            vqec_cli_print(cli, "  %-20s %s",
                                           cli_command_node->command,
                                           cli_command_node->help);
                        }
                        free(temp_buf);
                        return VQEC_CLI_OK;
                    }
                } else {
                    /* No Callback for this node */
                    token = strtok_r(NULL, " ", &save_ptr);
                    if (token == NULL || (strncmp(token, "?", 1) == 0)) {
                        if (!VQE_TAILQ_EMPTY(tqh)) {
                            if (disp_opt == VQEC_CLI_DISP_USAGE) {
                                print_list(tqh, cli);
                            } else if (disp_opt == VQEC_CLI_DISP_DESC) {
                                vqec_cli_print(cli, "  %-20s %s",
                                               cli_command_node->command,
                                               cli_command_node->help);
                                free(temp_buf);
                                return (VQEC_CLI_OK);
                            } else {
                                /* no callback, cmd end, but children */
                                vqec_cli_print(cli, "Incomplete command");
                            }
                        }
                        free(temp_buf);
                        return (VQEC_CLI_ERROR);
                    }
                }
            } else {
                /*
                 * return code VQEC_CLI_ERROR, VQEC_CLI_DUP_OK
                 * Unique Node does not exist . Return
                 */
                new_token = strtok_r(NULL, " ", &save_ptr);
                if ((new_token == NULL) || 
                    (strncmp(new_token, "?", 1) == 0)) {
                    /* OK we have reached the end */
                    if (retval == VQEC_CLI_DUP_OK) {
                        if (disp_opt != VQEC_CLI_DISP_EXEC) {
                            /* 
                             * Iterate through the commands and print 
                             * the partial matches 
                             */
                            retval = search_node(tqh, token, strlen(token),
                                                 cli->state, 
                                                 &cli_command_node,
                                                 cli, TRUE);
                            free(temp_buf);
                            return (VQEC_CLI_OK);
                        } else {
                            vqec_cli_print(cli, 
                                           "Invalid command (Unidentified "
                                           "token \"%s\")", 
                                           token);
                        }
                        free(temp_buf);
                        return (VQEC_CLI_ERROR);
                    }
                }
                vqec_cli_print(cli, "Invalid command (Unidentified token " 
                               "\"%s\")", token);
                free(temp_buf);
                return (VQEC_CLI_ERROR);
            }
        }
        free(temp_buf);
        return VQEC_CLI_OK;
    } else {
        return VQEC_CLI_ERROR;
    }
}

static void vqec_cli_display_prompt (struct vqec_cli_def *cli, int sockfd)
{  
    if (cli && sockfd) {
        if (cli->hostname) {
            cli_write(sockfd, cli->hostname, strlen(cli->hostname), cli);
            if (cli->quit) {
                return;
            }
        }
        switch(cli->state) {
            case VQEC_CLI_STATE_BASE:
                cli->promptchar = ">";
                break;
            case VQEC_CLI_STATE_EXEC:
                cli->promptchar = "#";
                break;
            case VQEC_CLI_STATE_CONFIG:
                cli->promptchar = "(config)#";
                break;
        }
        cli_write(sockfd, cli->promptchar, strlen(cli->promptchar), cli);
        if (cli->quit) {
            return;
        }
        cli_write(sockfd, " ", 1, cli);
    }
}

/* helper function to detect presence of non-whitespace character in the cmd */
static int has_non_space(char *cmd)
{
    int i;
    if (!cmd || (cmd[0] == '\0')) { 
        return 0;
    } else {
        for (i = 0 ; i < strlen(cmd); i++) {
            if (cmd[i] != ' ') {
                return 1;
            }
        }
    }
    return 0;
}

/* Helper function to move the cursor to the start of line */
static void move_disp_to_start_of_line_func (int cursor, int sockfd, 
                                             struct vqec_cli_def *cli)
{
    int i;
    for (i=0 ; i < cursor; i++) {
        cli_write(sockfd, "\b", 1, cli);
        if (cli->quit) {
            return;
        }
    }
}

/* Helper function to move the cursor to the end of line */
static void move_disp_to_end_of_line_func (int cursor, int sockfd,
                                           char *cmd, struct vqec_cli_def 
                                           *cli)
{
    cli_write(sockfd, &cmd[cursor], strlen(&cmd[cursor]), cli);
}

/* 
 * API to start the main loop of the VQE-C CLI. Must be called with the FD 
 * of a socket open for bidirectional communication. The function handles 
 * the telnet negotiation and authentication. 
 * Returns only when the server or client disconnect.  
 *
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] sockfd Socket to be used for Telnet.
 * @return int Error code representing successful execution of the command
 */
#define CTRL(c) (c - '@')
vqec_cli_error_t vqec_cli_loop (struct vqec_cli_def *cli, int sockfd)
{
    /* Negotiate the telnet to send across one character at a time */
    char *negotiate =
        "\xFF\xFB\x03"
        "\xFF\xFD\x03"
        "\xFF\xFB\x01"
        "\xFF\xFD\x01";
    fd_set fdset;
    unsigned char c;
    int i;
    vqec_cli_error_t retval;

    /* State information variables */
    char cmd[VQEC_CLI_MAX_LINE_SIZE];
    int cursor = 0;
    int cmd_limit = 0;
    boolean esc = FALSE;
    boolean esc_bracket = FALSE;
    boolean esc_bracket_1 = FALSE;
    boolean esc_bracket_3 = FALSE;
    boolean esc_bracket_4 = FALSE;
    boolean telnet_option_flag = FALSE;
    int hcount = 0;
    int hindex = 0;
    int disp_index = 0;
    boolean down_key = FALSE;
    boolean showprompt = TRUE;
    vqec_cli_disp_options_t disp_opt;
    struct timeval tv;

    if (!cli || !sockfd) {
        return VQEC_CLI_ERROR;
    }

    cli_write(sockfd, negotiate, strlen(negotiate), cli);
    if (cli->quit) {
        return VQEC_CLI_ERROR;
    }
    for (i=0; i < strlen(negotiate); i++) {
        /* Flush the echo */
        read(sockfd, &c, 1);
    }

    /* 
     * Here we also do the job of associating the file stream cli->client
     * to the sockfd file. This is the file stream to which fprintf can
     * be used, which is used by cli->print
     */
    if (!(cli->client = fdopen(sockfd, "w+"))) {
        return VQEC_CLI_ERROR;
    }
    setbuf(cli->client, 0);
     

    /* Now send the VQE-C CLI Banner */
    cli_write(sockfd, cli->banner, strlen(cli->banner), cli);
    if (cli->quit) { 
        goto end_cli_loop;
    }
    cli_write(sockfd, "\n\r\n\r", 4, cli);
    if (cli->quit) { 
        goto end_cli_loop;
    }

    while (1) {
        if (cli->quit) {
            break;
        }
	if (showprompt) {
            vqec_cli_display_prompt(cli, sockfd);
            showprompt = 0;
            /* we are beginning a new line, so clear earlier line's state */
            cursor = 0;
            cmd_limit = 0;
            bzero(cmd, VQEC_CLI_MAX_LINE_SIZE);
        }
        
        /* clear the file descriptor set */
        FD_ZERO(&fdset);
        /* Add the socket file descriptor to fdset */
        FD_SET(sockfd, &fdset);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select(sockfd+1, &fdset, NULL, NULL, &tv) < 0) {
            /* Error Condition */
            break;
        }

        /* Read will block until there is something to read */
        if ((read(sockfd, &c, 1)) > 0)
        {
            switch (c) {
                case (0) : 
                    break;
                case (0xff) :
                    /* Is a telnet option */
                    telnet_option_flag = TRUE;
                    break;
                case (0x7e): /* case '~' */
                    /* 
                     * We take no action for tilde because it is not clear
                     * how to interpret it, i.e. on some terminals
                     * Esc+[+1 = Home key, but on some other terminals
                     * Esc+[+1+~ is the Home key.
                     * If we decide to interpret Esc+[+1+~ as the Home
                     * key, on some terminals, what could have been expected
                     * was the effect of "home" and then the ~ character.
                     * On other terminals only the "home" action is 
                     * expected for that prefix. Thus, we decide that tilde
                     * character does not have any much use on the CLI and 
                     * drop it, the prefix (without the tilde) 
                     * deciding the intent (e.g. home key in this example)
                     */
                    break;
                case '\n': 
                    /* case for newline */
                    esc = FALSE;
                    break;
                case '?':
                    if (cmd_limit == (VQEC_CLI_MAX_LINE_SIZE - 1)) {
                        break;
                    }
                    /* call the help callback */
                    if (cursor != cmd_limit) {
                        goto character_case;
                    } 
                    cmd[cmd_limit] = '\0';
                    cli_write(sockfd, "\n\r\n\r", 4, cli);
                    if (cli->quit) {
                        goto end_cli_loop;
                    }
                    esc = FALSE;
                    disp_opt = VQEC_CLI_DISP_DESC;
                    /* 
                     * if the cmd so far has any non-space characters
                     * AND, the last char is a space, then what is 
                     * desired is list/usage, else what is desired is desc
                     */ 
                    if (has_non_space(cmd)) {
                        if (cmd[cmd_limit - 1] == ' ') {
                            if (cmd_limit + 2 < VQEC_CLI_MAX_LINE_SIZE) {
                                disp_opt = VQEC_CLI_DISP_USAGE;
                                strlcpy(&cmd[cmd_limit] , "?\0", 2);
                                cmd_limit += 2;
                                cursor += 2;
                            }
                        }
                    }
                    /* Now parse and try to execute the command */
                    retval = exec_cmd(cli, cmd, cmd_limit, disp_opt);
                    if (disp_opt == VQEC_CLI_DISP_USAGE) {
                        cmd_limit -= 2;
                        cursor -= 2;
                        cmd[cmd_limit] = '\0';
                    }
                    /* Now display the CLI prompt, and the cmd array */
                    cli_write(sockfd, "\n\r", 2, cli);
                    if (cli->quit) {
                        goto end_cli_loop;
                    }
                    vqec_cli_display_prompt(cli, sockfd);
                    cli_write(sockfd, cmd, strlen(cmd), cli);
                    if (cli->quit) {
                        goto end_cli_loop;
                    }
                    break;
                case '\r':
                    /* Case for return key */
                    showprompt = TRUE;
                    /* at this point we have the command in cmd */
                    cli_write(sockfd, "\n\r\n\r", 4, cli);
                    if (cli->quit) {
                        goto end_cli_loop;
                    }
                    esc = FALSE;
                    cmd[cmd_limit] = '\0';
                    if (cmd_limit != 0) {
                        /* Put this command into cli->history */
                        if (hcount == VQEC_CLI_MAX_HISTORY) {
                            /* History cache is full, flush one element */
                            free(cli->hist[hindex]);
                            cli->hist_first_index = 
                                (hindex + 1) % VQEC_CLI_MAX_HISTORY;
                        } else {
                            hcount++;
                        }
                        cli->hist[hindex] = strdup(cmd);
                        hindex = (hindex + 1) % VQEC_CLI_MAX_HISTORY;
                        disp_index = hindex;
                        /* Now parse and try to execute the command */
                        retval = exec_cmd(cli, cmd, 
                                          cmd_limit, VQEC_CLI_DISP_EXEC);
                        cli_write(sockfd, "\n\r", 2, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                    }
                    break;
                case (27): 
                    /* Escape character */
                    esc = TRUE;
                    break;
                case 'B':
                    /* down key */
                    if (esc_bracket != 1) {
                        esc = FALSE;
                        break;
                    } 
                    /* else just fall through */
                    down_key = TRUE;
                case 'A':
                    esc = 0;
                    if (esc_bracket == TRUE) {
                        /* up key */
                        cursor = 0;
                        cmd_limit = 0;
                        /* '\b' takes the cursor back by 1 character */
                        for (i=0; i < strlen(cmd); i++) {
                            cli_write(sockfd, "\b \b", 3, cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                        }
                        cli_write(sockfd, "\r", 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }

                        vqec_cli_display_prompt(cli, sockfd);
                        if (hcount > 0) {
                            if (!down_key) {
                                disp_index = (disp_index - 1 + hcount) 
                                             % hcount;
                            } else {
                                disp_index = (disp_index + 1) 
                                             % hcount;
                            }
                            strlcpy(cmd, cli->hist[disp_index], 
                                    strlen(cli->hist[disp_index]) + 1);
                            cursor = strlen(cli->hist[disp_index]);
                            cmd_limit = cursor;
                            cli_write(sockfd, cmd, strlen(cmd), cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                        }
                        esc_bracket = FALSE;
                    } else {
                        down_key = FALSE;
                        goto character_case;
                    }
                    down_key = FALSE;
                    break;
                case 'C':
                    esc = FALSE;
                    if (esc_bracket == TRUE) {
                        /* 
                         * Right arrow key. Write a single character
                         * which moves the cursor one to the right
                         */ 
                        if (cursor < cmd_limit) {
                            cli_write(sockfd, &cmd[cursor], 1, cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                        }
                        cursor++;
                        if (cursor > cmd_limit) {
                            cursor = cmd_limit;
                        }
                        esc_bracket = FALSE;
                    } else {
                        goto character_case;
                    }
                    break;
                case 'D': 
                    esc = FALSE;
                    if (esc_bracket == TRUE) {
                        /* left */
                        cursor--;
                        if (cursor < 0) {
                            cursor = 0;
                        } else {
                            cli_write(sockfd, "\b", 1, cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                        }
                        esc_bracket = FALSE;
                    } else {
                        goto character_case;
                    }
                   break;
                /* Start of line */
                case CTRL('A'):
                    move_disp_to_start_of_line_func(cursor, sockfd, cli);
                    if (cli->quit) {
                        goto end_cli_loop;
                    }
                    esc = FALSE;
                    cursor = 0;
                    break;
                case CTRL('E'):
                    move_disp_to_end_of_line_func(cursor, sockfd, cmd, cli);
                    if (cli->quit) {
                        goto end_cli_loop;
                    }
                    cursor = cmd_limit;
                    break;
                case CTRL('C'):
                    /* Ctrl-C pressed */
                    cli->quit = TRUE;
                    break;
                case CTRL('L'):
                    /* Clear line */
                    for (i=cursor;i>0;i--) {
                        cli_write(sockfd, "\b", 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                    }
                    for (i=0; i < cmd_limit; i++) {
                        cli_write(sockfd, " ", 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                    }
                    for (i=cmd_limit;i>0;i--) {
                        cli_write(sockfd, "\b", 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                    }
                    cursor = 0;
                    cmd_limit = 0;
                    esc = FALSE;
                    break;
                case (0x7f):
                case CTRL('H'):
                    /* backspace */
                    cursor --;
                    if (cursor < 0) {
                        cursor = 0;
                    } else {
                        cli_write(sockfd, "\b", 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                        /* move elements in array one to the left */
                        for (i=cursor; i < cmd_limit; i++) {
                            cmd[i] = cmd[i+1];
                        }

                        cmd_limit--;
                        if (cmd_limit < 0) {
                            cmd_limit = 0;
                        }
                        cmd[cmd_limit] = '\0';
                        cli_write(sockfd, &cmd[cursor], strlen(&cmd[cursor]), 
                                  cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                        cli_write(sockfd, " ", 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                        /* again move back the cursor */
                        for (i=0; i <= strlen(&cmd[cursor]); i++) {
                            cli_write(sockfd, "\b", 1, cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                        }
                    }

                    esc = FALSE;
                    break;
                case 'H':
                case '1':
                    if (esc_bracket == TRUE) {
                        /* 
                         * Home key is Esc + [ + 1 as well as 
                         * 'Esc + [ + 1 + ~, depending on terminal 
                         */
                        esc_bracket_1 = TRUE;
                        esc_bracket = FALSE;
                        goto special_chars; 
                    } else {
                        goto character_case;
                    }
                case '3':
                    if (esc_bracket == TRUE) {
                        /* Delete Key prefix */
                        esc_bracket_3 = TRUE;
                        esc_bracket = FALSE;
                        goto special_chars; 
                    } else {
                        goto character_case;
                    }
                case 'F':
                case '4':
                    if (esc_bracket == TRUE) {
                        /* End key */
                        esc_bracket_4 = TRUE;
                        esc_bracket = FALSE;
                        /* Fall through to special_chars */
                    } else {
                        goto character_case;
                    }
special_chars:
                    if (esc_bracket_1 == TRUE) {
                        esc_bracket_1 = FALSE;
                        move_disp_to_start_of_line_func(cursor, sockfd, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                        esc = FALSE;
                        cursor = 0;
                    } else if (esc_bracket_4 == TRUE) {
                        esc_bracket_4 = FALSE;
                        move_disp_to_end_of_line_func(cursor, sockfd, cmd, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                        esc =  FALSE;
                        cursor = cmd_limit;
                    } else if (esc_bracket_3 == TRUE) {
                        if (cursor != cmd_limit) {
                            /* Delete key */
                            for (i=cursor; i < cmd_limit; i++) {
                                cmd[i] = cmd[i+1];
                            }
                            cmd[cmd_limit] = '\0';
                            cli_write(sockfd, &cmd[cursor], 
                                      strlen(&cmd[cursor]), cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                            cli_write(sockfd, " ", 1, cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }

                            /* again move back the cursor */
                            for (i=0; i <= strlen(&cmd[cursor]); i++) {
                                cli_write(sockfd, "\b", 1, cli);
                                if (cli->quit) {
                                    goto end_cli_loop;
                                }
                            }
                            cmd_limit--;
                            if (cmd_limit < 0) {
                                cmd_limit = 0;
                            }
                        }
                        esc_bracket_3 = FALSE;
                        esc = FALSE;
                        break;
                    } 
                    break;
                case 0x09:
                    /* Tab character received */
                    esc = FALSE;
                    break;
                case '[':
                    if (esc == 1) {
                        esc_bracket = TRUE;
                        esc = FALSE;
                        break;
                    } /* else fall through */
                default :
character_case:  
                    if (telnet_option_flag) {
                        if (c >= 251 && c <= 254) { 
                            /* 
                             * Will, wont, do, dont codes. 
                             * Another code follow up expected.
                             * We ignore all telnet codes.
                             */
                            break;
                        }
                        telnet_option_flag = FALSE;
                        break;
                    } 
                    esc = FALSE;
                    /* Check if the character pressed was a 'Ctrl(char)' */
                    if (c < 32) {
                        break;
                    } 
                    /* a character was pressed */
                    if (cmd_limit == (VQEC_CLI_MAX_LINE_SIZE - 1)) {
                        break;
                    }
                    if (cursor == cmd_limit) {
                        /* append to end of line */
                        cmd[cursor] = c;
                        cmd[cursor+1] = '\0';
                        cli_write(sockfd, &c, 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                    } else if (cursor < cmd_limit) {
                        /* modify the command buffer */
                        for (i=cmd_limit+1; i>=(cursor+1); i--) {
                            cmd[i] = cmd[i-1];
                        }
                        cmd[cursor] = c;
                        cli_write(sockfd, &cmd[cursor],
                                  cmd_limit - cursor + 1, cli);
                        if (cli->quit) {
                            goto end_cli_loop;
                        }
                        /* Now set the cursor back to where it was */
                        for (i=0; i < (cmd_limit-cursor); i++) {
                            cli_write(sockfd, "\b", 1, cli);
                            if (cli->quit) {
                                goto end_cli_loop;
                            }
                        }
                    }
                    cursor++; 
                    cmd_limit++;
                    break;
            }
        } else {
            break;
        }
    }
end_cli_loop:
    /* 
     * Breaking out of the CLI loop, thus cleaning off the allocated variables
     */
    for (i=0; i < hcount; i++) {
        free(cli->hist[i]);
    }
    free(cli->substr);
    fclose(cli->client); 
    return 0;
}

/* 
 * API to set the greeting for VQE-C CLI that telnet clients see when they 
 * connect
 * 
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] banner_string Pointer to a string that would be used as the banner.
 * @returns void
 */
void vqec_cli_set_banner(struct vqec_cli_def *cli, char *banner_string)
{
    if (banner_string && cli) {
        cli->banner = strdup(banner_string);
    }
}

/* 
 * API to set the hostname for VQE-C CLI that telnet clients see when they 
 * connect and in the CLI-Prompt thereafter
 * 
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] hostname Pointer to a string that would be used as the first 
 *                    part of the CLI prompt
 * @returns void
 */
void vqec_cli_set_hostname(struct vqec_cli_def *cli, char *hostname)
{
    if (hostname && cli)  {
        cli->hostname = strdup(hostname);
    }
}

/* 
 * API to set the callback function for printing to the CLI 
 *
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] callback Function Pointer to a function that would be used to 
 *                             print characters to registered vqec_cli client 
 *                             (File Pointer)
 * @returns void
 */
void vqec_cli_print_callback(struct vqec_cli_def *cli, 
                             void (*callback)(struct vqec_cli_def *, char *,
                                              int))
{
    if (callback && cli) {
        cli->print_callback = callback;
    }
}

/* 
 * API to print specified string using VQE-C CLIs print callback function
 *
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] format printf() style format string and variable number of 
 *                  arguments. 
 * @returns void
 */
void vqec_cli_print(struct vqec_cli_def *cli, char *format, ... )
{
    char *print_buf;
    int retval;
    char *tok_ptr;
    char *save_ptr;
    va_list arg_ptr; /* typedef for pointer to list of arguments */
    print_buf = (char *) malloc(sizeof(char) * VQEC_CLI_MAXSIZE_PRINTBUF);
    if (!print_buf) {
        return;
    }
    va_start(arg_ptr, format); /* 
                                * set arg_ptr to location after 'format'
                                * on the stack frame
                                */
    /* 
     * Based on the input format string, use the arg_ptr to create a string
     * in a buffer, that could later be written to the file stream.
     * Instead of working on the internals of how printf deals with formatting,
     * we instead used the vsnprintf that does this job for us
     */
    retval = vsnprintf(print_buf, VQEC_CLI_MAXSIZE_PRINTBUF, format, arg_ptr);
    /* Check if the print_buf is acceptable for the filter */
    if (cli && cli->print_callback) {
        if (cli->substr) {
            if (strstr(print_buf, cli->substr)) {
                /* 
                 * Check if there are '\n's in the buffer, in which
                 * case we need to print only those lines in which the substring
                 * exists and ignore all other lines
                 */
                tok_ptr = strtok_r(print_buf, "\n", &save_ptr);
                if (tok_ptr) {
                    while (tok_ptr != NULL) {
                        if (strstr(tok_ptr, cli->substr)) {
                            cli->print_callback(cli, tok_ptr, 
                                                VQEC_CLI_MAXSIZE_PRINTBUF);
                        }
                        tok_ptr = strtok_r(NULL, "\n", &save_ptr);
                    }        
                } else {
                    cli->print_callback(cli, print_buf, 
                                        VQEC_CLI_MAXSIZE_PRINTBUF);
                }
            }
        } else {
            /* No substring specified */
            cli->print_callback(cli, print_buf, VQEC_CLI_MAXSIZE_PRINTBUF);
        }
    }
    va_end(arg_ptr); /* set the arg_ptr to NULL */
    free(print_buf);
}
