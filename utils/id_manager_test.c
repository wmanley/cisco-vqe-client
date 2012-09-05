/*------------------------------------------------------------------
 * ID Manager Unit Test for VAM
 *
 * August 2006, Anne McCormick
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/id_manager.h"

static char str1[] = "VAM is great!!";
static char str2[] = "IPTV rocks!";

int main(int argc, char * argv[])
{
    char * ptr1, * ptr2;
    idmgr_id_t id1, id2;
    id_table_key_t table_key;
    id_mgr_ret ret;
    boolean randomize = FALSE;

    printf("ID Manager Unit Test\n");

    while (1)
    {
        /* First malloc some memory to put in id table */
        ptr1 = malloc(strlen(str1));
        strcpy(ptr1, str1);
        ptr2 = malloc(strlen(str2));
        strcpy(ptr2, str2);
        
        printf("\n\nInitial setup (random ids %s):\n\tptr1 (%p) = %s\n\tptr2 "
               "(%p) = %s\n",
               (randomize == TRUE) ? "enabled" : "disabled",
               ptr1, ptr1, ptr2, ptr2);
        
        table_key = id_create_new_table(10, 5);

        if (!randomize)
            id_table_set_no_randomize(table_key);
        
        /* Put ptr1 and ptr2 in the id table */
        id1 = id_get(ptr1, table_key);
        id2 = id_get(ptr2, table_key);
        
        printf("Put pointers into table:\n\tptr1 -> id1 = 0x%lx\n\tptr2 -> "
               "id2 = 0x%lx\n", 
               id1, id2);
        
        /* Nothing up my sleeve ... */
        ptr1 = ptr2 = NULL;

        printf("Retrieve pointers/strings:\n\tid1 ptr (%p) = %s\n\tid2 ptr "
               "(%p) = %s\n",
               id_to_ptr(id1, &ret, table_key), 
               (char *) id_to_ptr(id1, &ret, table_key), 
               id_to_ptr(id2, &ret, table_key), 
               (char *) id_to_ptr(id2, &ret, table_key));
        
        /* Now test id removal */
        ptr2 = (char *) id_to_ptr(id2, &ret, table_key);
        id_delete(id2, table_key);
        printf("Remove id2 from table, try to retrieve:\n\tid2 (%p) = %s\n",
               id_to_ptr(id2, &ret, table_key),
               (char *) id_to_ptr(id2, &ret, table_key));

        /* Try a few id reservations */
        uint ret = ID_MGR_RET_ERROR;
        id2 = 4;
        if ((ret = id_reserve(ptr2, table_key, id2) == ID_MGR_RET_OK))
        {
            printf("Reserving id 0x%lx (%p) = %s\n",
                   id2, id_to_ptr(id2, &ret, table_key),
                   (char *) id_to_ptr(id2, &ret, table_key));
            ptr2 = (char *) id_to_ptr(id2, &ret, table_key);
            id_delete(id2, table_key);
        }
        else
            printf("Failed to reserve id 0x%lx!! status %d\n",
                   id2, ret);

        /* Try to reserve a used id */
        if (id_reserve(ptr2, table_key, id1) == ID_IN_USE)
            printf("Tried to reserve used id, failed appropriately\n");
        else
            printf("Tried to reserve used id, succeeded?!\n");

        /* Cleanup */
        ptr1 = (char *) id_to_ptr(id1, &ret, table_key);
        id_delete(id1, table_key);
        free(ptr1);
        free(ptr2);
        printf("Destroy id table\n");
        id_destroy_table(table_key);

        /* Now do the whole thing again with randomized ids enabled */
        if (!randomize)
        {
            randomize = TRUE;
            continue;
        }
        break;
    }


    return 0;
}
