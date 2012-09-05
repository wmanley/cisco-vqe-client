/*------------------------------------------------------------------
 * VAM Generic Histogram API
 *
 * June 2006, Anne McCormick
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "../include/utils/vam_hist.h"
#include "vqe_port_macros.h"

/*------------------------- Utility Functions -------------------------------*/

/*
 * Function:    find_bucket
 * Description: This is the comparator function for binary searches
 *              of bucket ranges, called by bsearch()
 * Parameters:  k - key to compare
 *              b - current bucket in binary search comparison
 * Returns:     -1, 0, 1 respectively if key k is less than, equal to,
 *              or greater than the range of bucket b
 */
static int find_bucket(const void *k, const void *b)
{
    int32_t val = *((int32_t *) k);
    vam_hist_bucket_t *bucket = (vam_hist_bucket_t *) b;

    if (val < bucket->lower_bound)
        return -1;

    if (val > bucket->upper_bound)
        return 1;

    return 0;
}


/*---------------------------- Histogram API --------------------------------*/

/*
 * Function:    vam_hist_create_ranges
 * Description: This function creates a histogram given an array of bucket 
 *              ranges and number of ranges desired
 * Parameters:  hist       - histogram struct, allocated by caller
 *              ranges     - array of bucket ranges, sorted lowest to highest
 *              num_ranges - number of bucket ranges desired
 *              title      - histogram title to display
 * Returns:     0 on success, -1 otherwise
 */
int vam_hist_create_ranges(vam_hist_type_t *hist, int32_t *ranges, 
                           uint32_t num_ranges, char *title)
{
    vam_hist_bucket_t *bucket;    
    int i;

    hist->type = VAM_HIST_GENERAL;

    strncpy(hist->title, title, MAX_HIST_TITLE_LEN);
    hist->num_buckets = num_ranges + 1;
    memset(hist->buckets, 0, hist->num_buckets*sizeof(vam_hist_bucket_t));

    for (i = 0; i <= num_ranges; i++)
    {
        bucket = &hist->buckets[i];

        /* Set lower bound */
        if (i > 0)
            bucket->lower_bound = ranges[i-1];
        else
            bucket->lower_bound = INT_MIN;

        /* Set upper bound */
        if (i < num_ranges)
            bucket->upper_bound = ranges[i] - 1;
        else
            bucket->upper_bound = INT_MAX;
    }

    return 0;
}

/* 
 * Function:    vam_hist_create_logarithmic
 * Description: Create a histogram of 33 buckets to plot the
 *              position of the MSBit of the samples.
 * Parameters:  hist       - histogram struct, allocated by caller
 *              title      - histogram title to display
 * Returns:     0 on success, -1 otherwise
 */
static int vam_hist_create_logarithmic(vam_hist_type_t *hist,
                                       char *title)
{
    vam_hist_bucket_t *bucket;    
    int i;
    hist->type = VAM_HIST_LOGARITHMIC;
    hist->base = 0;
    hist->base_offset = 0;
    hist->bsize_exp = 0;
    strncpy(hist->title, title, MAX_HIST_TITLE_LEN);
    hist->num_buckets = VAM_LOG_HIST_NUM_BUCKETS;
    memset(hist->buckets, 0, hist->num_buckets*sizeof(vam_hist_bucket_t));
    hist->buckets[0].lower_bound = INT_MIN;
    hist->buckets[0].upper_bound = -1;

    for (i=1; i<hist->num_buckets; i++) {
        bucket = &hist->buckets[i];  

        /* Set lower bound */
        bucket->lower_bound = hist->buckets[i-1].upper_bound + 1;
        bucket->upper_bound = (1 << (i-1)) - 1;
    }
    return 0;
}

/*
 * Function:    vam_hist_create
 * Description: This function creates a histogram given a fixed bucket
 *              size, base, and number of buckets desired, or a
 *              histogram with logarithmic ranges.
 * Parameters:  hist       - histogram struct, allocated by caller
 *              fixed_size - fixed bucket size (must be power of two)
 *              num_fixed  - number of fixed buckets desired
 *              base       - starting point for fixed buckets
 *              title      - histogram title to display
 *              logarithmic - plot the log2 of the values 
 *                          (fixed_size, num_fixed, base ignored)
 *                            
 * Returns:     0 on success, -1 otherwise
 */
int vam_hist_create(vam_hist_type_t *hist, uint32_t fixed_size, 
                    uint32_t num_fixed, int32_t base, char *title,
                    boolean logarithmic)
{
    vam_hist_bucket_t *bucket;    
    int i;

    if (logarithmic) {
        return vam_hist_create_logarithmic(hist,title);
    }

    if ((fixed_size & (fixed_size - 1)) != 0)
    {
        VQE_SET_ERRNO(EINVAL);
        perror("Bucket size must be power of 2" );
        return -1;
    }
    
    if ((base%fixed_size) != 0)
    {
        VQE_SET_ERRNO(EINVAL);
        perror("Base must be evenly divisible by fixed_size");
        return -1;
    }
    
    /* Set up optimized bucket search parameters */
    hist->type = VAM_HIST_FIXED;
    hist->base = base;
    hist->base_offset = -1 * (base/(int32_t)fixed_size) + 1;
    hist->bsize_exp = 0;
    if (fixed_size == 1)
        hist->bsize_exp = 0;
    else
        while ((1 << ++hist->bsize_exp) < fixed_size);

    strncpy(hist->title, title, MAX_HIST_TITLE_LEN);
    hist->num_buckets = num_fixed + 1;
    memset(hist->buckets, 0, hist->num_buckets*sizeof(vam_hist_bucket_t));

    /* Set up buckets */
    hist->buckets[0].lower_bound = INT_MIN;
    hist->buckets[0].upper_bound = base - 1;
    for (i = 1; i <= num_fixed; i++)
    {
        bucket = &hist->buckets[i];

        /* Set lower bound */
        bucket->lower_bound = hist->buckets[i-1].upper_bound + 1;

        /* Set upper bound */
        if (i < num_fixed)
            bucket->upper_bound = bucket->lower_bound + fixed_size - 1;
        else
            bucket->upper_bound = INT_MAX;
    }

    return 0;
}

/*
 * Function:    vam_hist_add
 * Description: Given a numeric value, this function will bump up the
 *              hits count for the corresponding bucket in the histogram
 * Parameters:  hist - histogram
 *              val  - value to add to histogram.  Will be capped
 *              at +/- 32 bit maximum.
 * Returns:     0 on success, -1 otherwise
 */
int vam_hist_add(vam_hist_type_t *hist, int64_t val64)
{
    int32_t val;
    vam_hist_bucket_t *bucket = NULL;

    /* Truncate the value if it does not fit in 32 bit range */
    if (val64 <= INT_MIN)
        val = INT_MIN;
    else if (val64 >= INT_MAX)
        val = INT_MAX;
    else
        val = val64;

    switch (hist->type) {

    case VAM_HIST_GENERAL:
        if (val < hist->buckets[0].upper_bound)
            bucket = &hist->buckets[0];
        else if (val > hist->buckets[hist->num_buckets-1].lower_bound)
            bucket = &hist->buckets[hist->num_buckets - 1];
        else
        {
            /* Do binary search to find bucket */
            bucket = bsearch(&val, hist->buckets, hist->num_buckets, 
                             sizeof(vam_hist_bucket_t), find_bucket);
            
        }

#if 0
        CONSOLE_PRINTF("Adding val %d to bucket (%d/%d)\n", val,
               bucket->lower_bound, 
               bucket->upper_bound);
#endif
        break;

    case VAM_HIST_FIXED:
    {
        int32_t index = 0;

        if (val < hist->base)
            index = 0;
        else
        {
            index = (val >> hist->bsize_exp) + hist->base_offset;
            index = (index <= (hist->num_buckets - 1)) ? 
                index : hist->num_buckets - 1;
        }
        bucket = &hist->buckets[index];

#if 0
        printf("Adding val %d to bucket (%d/%d)\n", val,
               hist->buckets[index].lower_bound, 
               hist->buckets[index].upper_bound);
#endif
        break;
    }

    case VAM_HIST_LOGARITHMIC:
    {
        int32_t index;

        if (val < 0) {
            index = 0;
        } else {
            index = 1;
            while (val) {
                val >>= 1;
                index++;
            }
        }
        if (index < VAM_LOG_HIST_NUM_BUCKETS)
            bucket = &hist->buckets[index];
        break;
    }

    default:
        break;
    }
            
    if (bucket)
        bucket->hits++;
    
    return 0;
}

/*
 * Function:    vam_hist_display 
 * Description: This function displays the current number of hits for all
 *              configured buckets in the given histogram
 * Parameters:  hist - histogram to display
 * Returns:     none
 */
void vam_hist_display(vam_hist_type_t *hist)
{
    vam_hist_bucket_t *bucket;
    int b;

    CONSOLE_PRINTF("\n");
    CONSOLE_PRINTF("Histogram of %s:\n", hist->title); 
    for (b = 0; b < hist->num_buckets; b++)
    {
        bucket = &hist->buckets[b];

        CONSOLE_PRINTF("%11d - %10d    [ %10u  ]\n", 
               bucket->lower_bound, bucket->upper_bound, bucket->hits);
    }
}

/*
 * Function:    vam_hist_display_combine_zero_hit_ranges 
 * Description: This function displays the current number of hits for each
 *              configured bucket in the given histogram. Note that this
 *              version of histogram display will combine ranges at the 
 *              beginning and the end of the histogram that have zero hits.
 * Parameters:  hist - histogram to display
 * Returns:     none
 */
void vam_hist_display_combine_zero_hit_ranges(vam_hist_type_t *hist)
{
    vam_hist_bucket_t *bucket;
    int b, first_nonzero = 0, last_nonzero = 0;

    CONSOLE_PRINTF("\n");
    CONSOLE_PRINTF("Histogram of %s:\n", hist->title); 
    for (b = 0; b < hist->num_buckets; b++)
    {
        if (hist->buckets[b].hits > 0)
        {
            first_nonzero = b;
            break;
        }
    }

    for (b = hist->num_buckets - 1; b >= 0; b--)
    {
        if (hist->buckets[b].hits > 0)
        {
            last_nonzero = b;
            break;
        }
    }

    if (first_nonzero > 0)
    {
        CONSOLE_PRINTF("%11d - %10d    [ %10u  ]\n", 
               hist->buckets[0].lower_bound, 
               hist->buckets[first_nonzero - 1].upper_bound, 
               hist->buckets[first_nonzero - 1].hits);
    }
    for (b = first_nonzero; b <= last_nonzero; b++)
    {
        bucket = &hist->buckets[b];

        CONSOLE_PRINTF("%11d - %10d    [ %10u  ]\n", 
               bucket->lower_bound, bucket->upper_bound, bucket->hits);
    }
    if (last_nonzero < hist->num_buckets - 1)
    {
        CONSOLE_PRINTF("%11d - %10d    [ %10u  ]\n", 
               hist->buckets[last_nonzero + 1].lower_bound, 
               hist->buckets[hist->num_buckets - 1].upper_bound, 
               hist->buckets[hist->num_buckets - 1].hits);
    }
}

/*
 * Function:    vam_hist_display_nonzero_hits
 * Description: This function prints to a buffer the current number of hits for
 *              each nonzero configured bucket in the given histogram. Note that
 *              ranges with zero hits will not be displayed.
 * Parameters:  hist - histogram to display
 * Returns:     none
 */
void vam_hist_display_nonzero_hits(vam_hist_type_t *hist)
{
    vam_hist_bucket_t *bucket;
    int b;

    CONSOLE_PRINTF("\n");
    CONSOLE_PRINTF("Histogram of %s:\n", hist->title); 
    for (b = 0; b < hist->num_buckets; b++)
    {
        bucket = &hist->buckets[b];

        if (bucket->hits > 0)
        {
            CONSOLE_PRINTF("%11d - %10d    [ %10u  ]\n", 
                   bucket->lower_bound, bucket->upper_bound, bucket->hits);
        }
    }
}

/*
 * Function:    vam_hist_publish_nonzero_hits
 * Description: This function displays the current number of hits for each
 *              nonzero configured bucket in the given histogram. Note that
 *              ranges with zero hits will not be displayed.
 * Parameters:  hist - histogram to display
 * Returns:     none
 */
int vam_hist_publish_nonzero_hits (vam_hist_type_t *hist, 
                                   char *bufp, int len)
{
    vam_hist_bucket_t *bucket;
    int b, wr, cum = 0;

    wr = snprintf(bufp, len, "\nHistogram of %s:\n", hist->title); 
    if (wr >= len) {
        cum += len;
        return (cum);
    } 
    cum += wr;
    bufp += wr;
    len -= wr;
    for (b = 0; b < hist->num_buckets; b++)
    {
        bucket = &hist->buckets[b];

        if (bucket->hits > 0)
        {
            wr = snprintf(bufp, len, "%11d - %10d    [ %10u  ]\n", 
                          bucket->lower_bound, 
                          bucket->upper_bound, bucket->hits);
            if (wr >= len) {
                cum += len;
                break;
            } 
            cum += wr;
            bufp += wr;
            len -= wr;
        }
    }

    return (cum);
}

/*
 * Function:    vam_hist_copy
 * Description: This function copies a histogram
 * Parameters:  target             - target histogram, allocated by caller
 *              target_max_buckets - max buckets in target histogram
 *              src                - source histogram to copy from
 * Returns:     0 on success, -1 otherwise
 */
void vam_hist_copy(vam_hist_type_t * target, uint32_t target_max_buckets,
                   vam_hist_type_t * src)
{
    uint32_t num_buckets_to_copy;

    if (src->num_buckets < target_max_buckets)
        num_buckets_to_copy = src->num_buckets;
    else 
        num_buckets_to_copy = target_max_buckets;

    /*$$$ we copy the name here which is fairly ugly */
    memcpy(target, src, vam_hist_calc_size(num_buckets_to_copy,FALSE));
    target->num_buckets = num_buckets_to_copy;
}

/*
 * Function:    vam_hist_merge
 * Description: This function merges two histograms.
 * Parameters:  target - target histogram to merge into, allocated by caller
 *              src    - source histogram to merge from
 * Returns:     0 on success, -1 otherwise
 */
int
vam_hist_merge(vam_hist_type_t * target, vam_hist_type_t * src) 
{
    vam_hist_bucket_t * src_bucket;
    vam_hist_bucket_t * target_bucket;
    int i;
    if ((src->type != target->type) ||
        (src->num_buckets != target->num_buckets) ||
        (src->base != target->base) ||
        (src->base_offset != target->base_offset) ||
        (src->bsize_exp != target->bsize_exp))
        return -1;

    for (i = 0; i < src->num_buckets; i++) 
    {
        src_bucket = &src->buckets[i];
        target_bucket = &target->buckets[i];
        
        target_bucket->hits += src_bucket->hits;
    }
    return 0;
}

/*
 * Function:    vam_hist_clear
 * Description: This function clears the current number of hits for all
 *              configured buckets in the given histogram.
 * Parameters:  hist - histogram to display
 * Returns:     none
 */
void
vam_hist_clear (vam_hist_type_t *hist)
{
    int b;

    if (!hist) {
        return;
    }
    for (b = 0; b < hist->num_buckets; b++) {
        hist->buckets[b].hits = 0;
    }
}
