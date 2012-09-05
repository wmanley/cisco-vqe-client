/*------------------------------------------------------------------
 * VAM Common stuff for IPC
 *
 * June 2006, Josh Gahm
 *
 * Copyright (c) 2006-2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

#include "utils/vam_ipc.h"
#include "utils/vam_types.h"

key_t vam_ipc_get_key(const char * ipcf, int proj_id)
{
    char * malloced = NULL;
    key_t result = -1;

    if (! ipcf) {
        struct passwd * pwent = getpwuid(getuid());
        if (!pwent) {
            fprintf(stderr, "Could not get pw entry\n");
            goto bail;
        }

        const char * homedir = pwent->pw_dir; 
        malloced = malloc(strlen(homedir) + strlen(VAMIPC_KEY_FILE_DEF) + 2);
        if (! malloced)
            goto bail;
        sprintf(malloced, "%s/%s",homedir,VAMIPC_KEY_FILE_DEF);
        ipcf = malloced;
    }

    FILE * f = NULL;

    /* Make sure the ftok file exists or create it */
    if ((f = fopen(ipcf,"r"))
        || (f = fopen(ipcf, "w+"))) {
        ASSERT(fclose(f) == 0, "Could not close file");
    } else {
        fprintf(stderr, "Could not open %s\n",ipcf);
        goto bail;
    } 

    result = ftok(ipcf, proj_id);

 bail: 
    if (malloced)
        free(malloced);
    return result;
}
