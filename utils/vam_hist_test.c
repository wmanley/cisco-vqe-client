/*------------------------------------------------------------------
 * VAM Histogram API test program
 *
 * June 2006, Anne McCormick
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include "../include/utils/vam_hist.h"

#define NUM_FIXED_RANGES 10


/*
 * Needed for use of CONSOLE_PRINTF() by vam_hist.c functions.
 */
void
vqes_syslog_cli(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	printf(fmt, ap);
	va_end(ap);
}



int main(int argc, const char * argv[])
{
    vam_hist_type_t * r_hist, *f_hist;
    int ret, i;
    int test1_ranges[] = {-10,-5,0,5,10}, test1_num_ranges = 5;
    int test1_input[] = {15,1,-50,-1,-8}, test1_num_input = 5;
    int test2_input[] = {5,-50,67,-45,-66}, test2_num_input = 5;
    int test4_input[] = {-10,-1,35,0,11}, test4_num_input = 5;

    /* 
     * Test 1: Creation of histogram with ranges 
     */

    printf("/*****************************************************************/\n");
    printf("Test 1: Creation of histogram with ranges\n");
    printf("    Ranges: { ");
    for (i = 0; i < test1_num_ranges; i++)
        printf("%d ", test1_ranges[i]);
    printf(" }\n");
    printf("     Input: { ");
    for (i = 0; i < test1_num_input; i++)
        printf("%d ", test1_input[i]);
    printf(" }\n");

    r_hist = malloc(vam_hist_calc_size(test1_num_ranges+1,FALSE));
    ret = vam_hist_create_ranges(r_hist, test1_ranges, test1_num_ranges, "Range Test");
    if (ret != 0)
    {
        printf("%s: vam_hist_create_ranges() returned error: %d\n", 
               argv[0], ret);
        goto bail;
    }

    for (i = 0; i < test1_num_input; i++)
    {
        ret = vam_hist_add(r_hist, test1_input[i]);
        if (ret != 0) 
        {
            printf("%s: vam_hist_add() returned error: %d\n", argv[0], ret);    
            goto bail;
        }
    }
    vam_hist_display(r_hist);
    free(r_hist);


    /* 
     * Test 2: Creation of histogram with fixed sizes
     */

    printf("/*****************************************************************/\n");
    printf("Test 2: Creation of histogram with fixed sizes\n");
    printf("     Fixed Sizes: size %d num_ranges %d base %d\n", 16, 
           NUM_FIXED_RANGES, -64);
    printf("     Input: { ");
    for (i = 0; i < test2_num_input; i++)
        printf("%d ", test2_input[i]);
    printf(" }\n");

    f_hist = malloc(vam_hist_calc_size(NUM_FIXED_RANGES + 1,FALSE));
    ret = vam_hist_create(f_hist, 16, NUM_FIXED_RANGES, -64, "Fixed Test",FALSE);
    if (ret != 0)
    {
        printf("%s: vam_hist_create_fixed() returned error: %d\n", 
               argv[0], ret);
        goto bail;
    }

    for (i = 0; i < test2_num_input; i++)
    {
        ret = vam_hist_add(r_hist, test2_input[i]);
        if (ret != 0) 
        {
            printf("%s: vam_hist_add() returned error: %d\n", argv[0], ret);    
            goto bail;
        }
    }
    vam_hist_display(f_hist);

    /*
     * Test 3: Histogram copy
     */

    printf("/*****************************************************************/\n");
    printf("Test 3: Histogram copy\n");
    uint32_t f_hist_num_buckets = vam_hist_get_num_buckets(f_hist);
    vam_hist_type_t * f_copy = malloc(vam_hist_calc_size(f_hist_num_buckets,FALSE));
    vam_hist_copy(f_copy, f_hist_num_buckets, f_hist);
    printf("Original histogram:\n");
    vam_hist_display(f_hist);
    printf("Copied histogram:\n");
    vam_hist_display(f_copy);
    free(f_hist);
    free(f_copy);


    /* 
     * Test 4: Histogram display
     */

    printf("/*****************************************************************/\n");
    printf("Test 4: Histogram display\n");
    printf("     Fixed Sizes: size %d num_ranges %d base %d\n", 16, 
           NUM_FIXED_RANGES, -64);
    printf("     Input: { ");
    for (i = 0; i < test4_num_input; i++)
        printf("%d ", test4_input[i]);
    printf(" }\n");

    f_hist = malloc(vam_hist_calc_size(NUM_FIXED_RANGES + 1,FALSE));
    ret = vam_hist_create(f_hist, 16, NUM_FIXED_RANGES, -64, "Fixed Test",FALSE);
    if (ret != 0)
    {
        printf("%s: vam_hist_create_fixed() returned error: %d\n", 
               argv[0], ret);
        goto bail;
    }

    for (i = 0; i < test4_num_input; i++)
    {
        ret = vam_hist_add(r_hist, test4_input[i]);
        if (ret != 0) 
        {
            printf("%s: vam_hist_add() returned error: %d\n", argv[0], ret);    
            goto bail;
        }
    }

    printf("Full histogram display:\n");
    vam_hist_display(f_hist);        
    printf("Combined zero hit ranges display:\n");
    vam_hist_display_combine_zero_hit_ranges(f_hist); 
    printf("Nonzero hits display:\n");
    vam_hist_display_nonzero_hits(f_hist); 


    /* 
     * Test 5: Histogram clear
     */
    vam_hist_clear(f_hist);
    free(f_hist);

bail:    
    return 0;
}

    
           

    
                      
    
