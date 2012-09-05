/*------------------------------------------------------------------
 * VAM Generic Histogram API
 *
 * June 2006, Anne McCormick
 *
 * Copyright (c) 2006-2011 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VAM_HIST_H__
#define __VAM_HIST_H__

#include "vam_types.h"


/*-------------------------- Global Definitions  ----------------------------*/

#define MAX_HIST_TITLE_LEN   128


/*----------------------- API Structure Definitions -------------------------*/

#pragma pack(4)

/*
 * vam_hist_bucket_t 
 * 
 * Range and count for a single bucket.  PUBLIC, as it is returned
 * through the API.  Also used internally, though that may change.
 */
typedef struct vam_hist_bucket_
{
    int32_t lower_bound;              /* Lower bound for bucket */
    int32_t upper_bound;              /* Upper bound for bucket */

    uint32_t hits;                    /* Number of hits for this bucket */

} vam_hist_bucket_t;

typedef enum vam_hist_class_
{
    VAM_HIST_GENERAL = 0, /* any bucket ranges okay */
    VAM_HIST_FIXED,       /* Fixed sized ranges */
    VAM_HIST_LOGARITHMIC  /* logarithmic ranges */
} vam_hist_class_t; 

#define VAM_HIST_STRUCT_DECL(_struct_name, _num_bucket_spaces)                          \
/* vam_hist_type_t */                                                                   \
/* A histogram.    */                                                                   \
/* PRIVATE.  ACCESS ONLY THROUGH API.*/                                                 \
struct _struct_name                                                                     \
{                                                                                       \
    char title[MAX_HIST_TITLE_LEN];    /* Histogram title to display */                 \
    int num_buckets;                   /* Number of buckets */                          \
                                                                                        \
    /* Following fields are for optimized bucket search */                              \
    vam_hist_class_t type;             /* type of histogram */                          \
    int32_t base;                      /* Base of bucket ranges (fixed) */              \
    int32_t base_offset;               /* Number of buckets at index 0 */               \
    int32_t bsize_exp;                 /* Power of 2 exponent for bucket size */        \
    vam_hist_bucket_t buckets[_num_bucket_spaces];        /* Bucket array */            \
                                                                                        \
}

typedef VAM_HIST_STRUCT_DECL(vam_hist_type_, 0) vam_hist_type_t;

#pragma pack()

/*---------------------------- API Prototypes -------------------------------*/

/* Number of ranges in a logarithmic histogram.  here are the ranges:
 * [-inf - -1], [0, 0], [1, 1], [2,3], [4, 7], ....
 */
#define VAM_LOG_HIST_NUM_BUCKETS 33 
/* Get size of memory chunk required for a histogram with n buckets. */
static inline uint32_t vam_hist_calc_size(uint32_t num_buckets, 
                                          boolean logarithmic)
{
    uint32_t actual_buckets = logarithmic ? VAM_LOG_HIST_NUM_BUCKETS : num_buckets;
    return sizeof(vam_hist_type_t) + sizeof(vam_hist_bucket_t)*actual_buckets;
}

/* Accessor functions */
static inline const char * vam_hist_get_title(vam_hist_type_t * hist)
{
    return hist->title;
}

static inline int vam_hist_get_num_buckets(vam_hist_type_t * hist)
{
    return hist->num_buckets;
}

static inline void vam_hist_get_bucket(vam_hist_type_t * hist, uint32_t bucket_num,
                                       vam_hist_bucket_t * bucket)
{
    if (bucket_num < hist->num_buckets)
        *bucket = hist->buckets[bucket_num];
}

/* Create histogram with ranges.
 * 
 * WARNING: Uses (num_ranges + 1) buckets.
 */
extern int vam_hist_create_ranges(vam_hist_type_t *hist, int32_t *ranges, 
                                  uint32_t num_ranges, char *title);

/* Create histogram with fixed-size buckets.
 *
 * WARNING: Uses (num_fixed + 1) buckets. 
 */
extern int vam_hist_create(vam_hist_type_t *hist, uint32_t fixed_size, 
                           uint32_t num_fixed, int32_t base, char *title,
                           boolean logarithmic);


/* Add value to histogram */
extern int vam_hist_add(vam_hist_type_t *hist, int64_t val);

/* Copy a histogram */
extern void vam_hist_copy(vam_hist_type_t * target, uint32_t target_max_buckets,
                          vam_hist_type_t * src);

/* Merge two histograms (bucket size/number of buckets must be the same ) */
extern int vam_hist_merge(vam_hist_type_t * target, vam_hist_type_t * src);

/* Clear a histogram */
void vam_hist_clear(vam_hist_type_t *hist);

/* Display histogram data */
extern void vam_hist_display(vam_hist_type_t *hist);
extern void vam_hist_display_combine_zero_hit_ranges(vam_hist_type_t *hist);
extern void vam_hist_display_nonzero_hits(vam_hist_type_t *hist);
extern int vam_hist_publish_nonzero_hits(vam_hist_type_t *hist, 
                                         char *bufp, int len);

#endif /* __VAM_HIST_H__ */
