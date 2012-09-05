/**-----------------------------------------------------------------
 * VQE-C Lightweight command line interpreter
 *
 * Copyright (c) 2009-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#ifndef __VQEC_CLI__
#define __VQEC_CLI__

#include <stdio.h>
#include <utils/vam_types.h>
#include <utils/queue_plus.h>

#define VQEC_CLI_MAX_ARGS (100)
#define VQEC_CLI_MAX_HISTORY (256)
#define VQEC_CLI_MAX_LINE_SIZE (800)
#define VQEC_CLI_MAXSIZE_PRINTBUF (1200) 

typedef enum vqec_cli_error_ {
    VQEC_CLI_QUIT = -2,
    VQEC_CLI_ERROR = -1, 
    VQEC_CLI_OK = 0,
    VQEC_CLI_DUP_OK = 1,
} vqec_cli_error_t;

#define PRIVILEGE_UNPRIVILEGED  0
#define PRIVILEGE_PRIVILEGED    15

typedef enum vqec_cli_cmd_mode_ {
    VQEC_CLI_MODE_ANY = 0,
    VQEC_CLI_MODE_EXEC, 
    VQEC_CLI_MODE_CONFIG,
} vqec_cli_cmd_mode_t;

typedef enum vqec_cli_state_ {
    VQEC_CLI_STATE_BASE = 0,
    VQEC_CLI_STATE_EXEC,
    VQEC_CLI_STATE_CONFIG,
} vqec_cli_state_t;

typedef enum vqec_cli_disp_options_ {
    VQEC_CLI_DISP_DESC = 0, /* Display description for a command */
    VQEC_CLI_DISP_USAGE,    /* Optionally display usage or list */
    VQEC_CLI_DISP_EXEC,     /* Execute the command */
} vqec_cli_disp_options_t;

/* Declare TailQ Head to the vqec_cli_command structure */
VQE_TAILQ_HEAD(vqec_cmd_tree, vqec_cli_command);

/* Declare the VQE-C CLI structure*/
struct vqec_cli_def {

    /* Pointer to the Command Tree associated with the CLI object */
    struct vqec_cmd_tree vqec_cmd_tree_head;

    /* The banner string visible when logging to the cli */
    char *banner;
   
    /* pointer to the string that would be the prompt of the CLI */ 
    char *promptchar;

    /* Boolean flag indicating whether the prompt is to be displayed */
    boolean showprompt;

    /* Pointer to the substr expression currently valid for the CLI */
    char *substr;

    /* Pointer to the string of the CLI hostname */
    char *hostname;

    /* Array of pointers to strings to store history of commands executed */
    char *hist[VQEC_CLI_MAX_HISTORY];
    
    /* Index of the first element in the history array */
    int hist_first_index;

    /* The callback function for printing */
    void (*print_callback) (struct vqec_cli_def *cli_def, char *string,
                            int string_len);

    /* File Stream associated with the CLI, used for printing */
    FILE *client;

    /* Boolean indicating whether CLI has to be quit */
    boolean quit;

    /* The current state of the VQE-C CLI */
    vqec_cli_state_t state;
    
    /* Current privilege of the VQE-C CLI */
    int privilege;
};

/* Structure of each node in the CLI command tree */
struct vqec_cli_command {
    /* pointer to string that describes the command */
    char *command;
    /* The callback function associated with this node */
    int (*callback) (struct vqec_cli_def *, char *, char **, int);
    int privilege;    /* Privilege of this node/command */
    int mode;         /* The mode in which this command is to be executed*/
    char *help;       /* The help string associated with this node */

    /* TAILQ entry to link vqec_cli_command nodes in a VQE_TAILQ */
    VQE_TAILQ_ENTRY(vqec_cli_command) vqec_cli_command_ptrs;
    /* define a queue header that points to other vqec_cli_command*/
    VQE_TAILQ_HEAD(child_cmd_tree, vqec_cli_command) child_cmd_tree_head;
};

/* 
 * API to set up internal data structures for creating a VQE-C CLI object 
 *
 * @return struct vqec_cli_def Returns the pointer to a new vqec_cli_def 
 *                             VQE-C CLI object if the API was successfully 
 *                             executed, and NULL otherwise.
 */
struct vqec_cli_def *vqec_cli_init(void);

/* 
 * API to free memory used by VQE-C CLI
 * param[in] cli Pointer to the vqec_cli_def structure.
 * @return void
 */
void vqec_cli_deinit (struct vqec_cli_def *cli);

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
                        int privilege, int mode, char *help);

/* 
 * API to start the main loop of the VQE-C CLI. Must be called with the fd
 * of a socket open for bidirectional communication. 
 * The function handles the telnet negotiation and authentication and returns
 * only when the server or client disconnect.  
 *
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] sockfd Socket to be used for Telnet.
 * @return int Error code representing successful execution of the command
 */
vqec_cli_error_t vqec_cli_loop(struct vqec_cli_def *cli, int sockfd);


/* 
 * API to set the greeting for VQE-C CLI that telnet clients see when they 
 * connect and is seen as part of the CLI-prompt 
 * 
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] banner_string Pointer to a string that would be used as the banner.
 * @returns void
 */
void vqec_cli_set_banner(struct vqec_cli_def *cli, char *banner_string);

/* 
 * API to set the greeting for VQE-C CLI that telnet clients see when they 
 * connect. 
 *
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] hostname Pointer to a string that would be used as the first 
 *                    part of the CLI prompt
 * @returns void
 */
void vqec_cli_set_hostname(struct vqec_cli_def *cli, char *hostname);

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
                                              int));

/* 
 * API to print specified string using VQE-C CLIs print callback function
 *
 * param[in] cli Pointer to vqec_cli_def structure.
 * param[in] format printf() style format string and variable number of 
 *                  arguments. 
 * @returns void
 */
void vqec_cli_print(struct vqec_cli_def *cli, char *format, ... );



#endif
