/*
 * Copyright (c) 2007, 2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains the implementation of a (rate, burst) token bucket
 * policer.
 */

#include "vqe_token_bucket.h"
#include <log/libdebug.h>
#include "limits.h"


/*
 * TB_DEBUG controls debug output for token bucket APIs.
 *
 * This is only controllable via compile time switch, not CLI.
 * It uses printf() so that it doesn't rely on buginf(),
 * which is not included in the unit test image.
 */
#if TB_DEBUG
#define TB_DEBUG_STR_LEN 1024
#include "vam_time.h"
#endif /* TB_DEBUG */

/*
 * tb_retval_to_str()
 *
 * Converts a token bucket return value to a descriptive string.
 *
 * Parameters:
 *  @param[in]  retval  - return value to be translated
 *
 * @return  pointer to a descriptive string 
 */
const char *
tb_retval_to_str (tb_retval_t retval)
{
    switch (retval) {
    case TB_RETVAL_TB_INVALID:
        return "Invalid token bucket supplied";
    case TB_RETVAL_INTERNAL_ERROR:
        return "Internal error, operation failed";
    case TB_RETVAL_RATE_INVALID:
        return "Invalid token bucket rate supplied";
    case TB_RETVAL_BURST_INVALID:
        return "Invalid token bucket burst supplied";        
    case TB_RETVAL_QUANTUM_INVALID:
        return "Invalid token bucket quantum supplied";
    case TB_RETVAL_INSUFFICIENT_TOKENS:
        return "Insufficient tokens in bucket for request";
    case TB_RETVAL_OK:
        return "Token bucket request succeeded";
    default:
        return "Unknown token bucket error code";
    }
}

/*
 * tb_init_common()
 *
 * Initializes a token bucket.  Requires all parameters to be supplied.
 * See function header for tb_init() for details.
 */
tb_retval_t
tb_init_common (token_bucket_info_t  *tb,
                uint32_t             rate,
                uint32_t             burst,
                uint32_t             quantum_size,
                abs_time_t           curr_time)
{
    tb_retval_t retval = TB_RETVAL_OK;

    if (!tb) {
        retval = TB_RETVAL_TB_INVALID;
        goto done;
    }
    if (rate > TB_RATE_MAX) {
        retval = TB_RETVAL_RATE_INVALID;
        goto done;
    }
    if (burst > TB_BURST_MAX) {
        retval = TB_RETVAL_BURST_INVALID;
        goto done;
    }
    if (!quantum_size || (quantum_size > TB_QUANTUM_MAX)) {
        retval = TB_RETVAL_QUANTUM_INVALID;
        goto done;
    }        

/* initializing the token bucket properties */

    tb->rate = rate;
    tb->quantum_size = quantum_size;
    tb->burst = burst;

/* initializing the token bucket state */

    if (rate) {
#define MICROSECONDS_PER_SECOND 1000000
        tb->replenish_period = (uint32_t)quantum_size *
            MICROSECONDS_PER_SECOND / rate;
    } else {
        /* tb->replenish_period is not used when rate = 0 */
        tb->replenish_period = 0; 
    }
    
    if (TIME_CMP_A(eq, curr_time, ABS_TIME_0)) {
        curr_time = get_sys_time();
    }    
    tb->last_token_replenish_time = curr_time;   

    tb->tokens_in_bucket = burst;

done:
    return (retval);
}


/*
 * tb_init_simple()
 *
 * Initializes a token bucket object.  The bucket is initially filled
 * with tokens.  This is identical to tb_init(), but takes in fewer
 * parameters.  (Default values are used for those parameters not accepted.)
 *
 * Params:
 * @param[in] tb                      Token bucket to be initialized
 * @param[in] rate                    Token bucket refill rate (tokens/second)
 *                                     valid range:  0 <= x <= TB_RATE_MAX
 * @param[in] burst                   Token bucket burst (a.k.a. bucket depth,
 *                                     or the max number of tokens it can hold)
 *                                     [valid range:  0 <= x <= TB_BURST_MAX]
 *
 * @return:
 *   TB_RETVAL_OK
 *   TB_RETVAL_TB_INVALID
 *   TB_RETVAL_RATE_INVALID
 *   TB_RETVAL_BURST_INVALID
 *   TB_RETVAL_INTERNAL_ERROR
 */
tb_retval_t
tb_init_simple (token_bucket_info_t  *tb,
                uint32_t             rate,
                uint32_t             burst)
{
    tb_retval_t retval;
    
    retval = tb_init_common(tb, rate, burst, TB_QUANTUM_DEFAULT, ABS_TIME_0);

    switch (retval) {
    case TB_RETVAL_OK:
    case TB_RETVAL_TB_INVALID:
    case TB_RETVAL_RATE_INVALID:
    case TB_RETVAL_BURST_INVALID:
        break;
    default:
        retval = TB_RETVAL_INTERNAL_ERROR;
    }

#if TB_DEBUG
    /* Not sure how best to preserve this... */
    char *dbg_buf, dbg_tmp1[TB_DEBUG_STR_LEN], dbg_tmp2[TB_DEBUG_STR_LEN];
    char dbg_time1[TIME_STRING_BUFF_SIZE];
    snprintf(dbg_tmp2, TB_DEBUG_STR_LEN,
             "tb_init_simple("
             "tb=%p, rate=%u (tokens/s), burst=%u (tokens)): %s\r\n",
             tb,
             rate,
             burst,
             tb_retval_to_str(retval));
    if (retval == TB_RETVAL_OK) {
        snprintf(dbg_tmp1, TB_DEBUG_STR_LEN, "%s"
                 "   rate:                                 %u (tokens/s)\r\n"
                 "   burst:                                %u (tokens)\r\n"
                 "   quantum:                              %u (tokens)\r\n"
                 "   replenish_period:                     %u (us)\r\n"
                 "   last_token_replenish_time:            %s\r\n"
                 "   (initial) tokens_in_bucket:           %u (tokens)\r\n",
                 dbg_tmp2,
                 tb->rate,
                 tb->burst,
                 tb->quantum_size,
                 tb->replenish_period,
                 abs_time_to_str(tb->last_token_replenish_time,
                                 dbg_time1, TIME_STRING_BUFF_SIZE),
                 tb->tokens_in_bucket);
        dbg_buf = dbg_tmp2;
    } else {
        dbg_buf = dbg_tmp1;
    }
    printf(dbg_buf);
#endif /* TB_DEBUG */

    return (retval);
}

/*
 * tb_init()
 *
 * Initializes a token bucket object.  The bucket is initially filled
 * with tokens.
 *
 * Params:
 * @param[in] tb                      Token bucket to be initialized
 * @param[in] rate                    Token bucket refill rate (tokens/second)
 *                                     valid range:  0 <= x <= TB_RATE_MAX
 * @param[in] burst                   Token bucket burst (a.k.a. bucket depth,
 *                                     or the max number of tokens it can hold)
 *                                     [valid range:  0 <= x <= TB_BURST_MAX]
 * @param[in] quantum_size            Tokens are credited to the bucket in
 *                                     batches of "n" at a time, where this
 *                                     "n" value is called a quantum.      
 *                                     I.e. if quantum_size=2, then a token
 *                                     credit will only occur for even amounts
 *                                     of tokens at a time.
 *                                     [valid range:  1 <= x <= TB_QUANTUM_MAX]
 * @param[in] curr_time               Current time if known, or
 *                                     ABS_TIME_0 if not already known
 *
 * @return:
 *   TB_RETVAL_OK
 *   TB_RETVAL_TB_INVALID
 *   TB_RETVAL_RATE_INVALID
 *   TB_RETVAL_BURST_INVALID
 *   TB_RETVAL_QUANTUM_INVALID
 */
tb_retval_t
tb_init (token_bucket_info_t  *tb,
         uint32_t             rate,
         uint32_t             burst,
         uint32_t             quantum_size,
         abs_time_t           curr_time)
{
    tb_retval_t retval;
    
    retval = tb_init_common(tb, rate, burst, quantum_size, curr_time);
    
#if TB_DEBUG
    char *dbg_buf, dbg_tmp1[TB_DEBUG_STR_LEN], dbg_tmp2[TB_DEBUG_STR_LEN];
    char dbg_time1[TIME_STRING_BUFF_SIZE];
    snprintf(dbg_tmp2, TB_DEBUG_STR_LEN,
             "tb_init(tb=%p, rate=%u (tokens/s), burst=%u (tokens), "
             "quantum_size=%u (tokens), curr_time=%s): %s\r\n",
             tb,
             rate,
             burst,
             quantum_size,
             abs_time_to_str(curr_time, dbg_time1, TIME_STRING_BUFF_SIZE),
             tb_retval_to_str(retval));
    if (retval == TB_RETVAL_OK) {
        snprintf(dbg_tmp1, TB_DEBUG_STR_LEN, "%s "
                 "   rate:                                 %u (tokens/s)\r\n"
                 "   burst:                                %u (tokens)\r\n"
                 "   quantum:                              %u (tokens)\r\n"
                 "   replenish_period:                     %u (us)\r\n"
                 "   last_token_replenish_time:            %s\r\n"
                 "   (initial) tokens_in_bucket:           %u (tokens)\r\n",
                 dbg_tmp2,
                 tb->rate,
                 tb->burst,
                 tb->quantum_size,
                 tb->replenish_period,
                 abs_time_to_str(tb->last_token_replenish_time,
                                 dbg_time1, TIME_STRING_BUFF_SIZE),
                 tb->tokens_in_bucket);
        dbg_buf = dbg_tmp1;
    } else {
        dbg_buf = dbg_tmp2;
    }
    printf(dbg_buf);
#endif /* TB_DEBUG */

    return (retval);
}

/*
 * tb_credit_tokens()
 *
 * Credits a token bucket for any tokens earned since the last credit,
 * and retrieves the amount of tokens currently in the updated bucket.
 *
 * If the supplied curr_time value is ABS_TIME_0, the current time will
 * be determined internally by this function for updating the token bucket.
 * Otherwise, the supplied curr_time value will be used.
 *
 * This function is used primarily for updating/reading the amount of
 * tokens in a token bucket.  To drain tokens, use the "drain_tokens()"
 * function.
 *
 * Params:
 *  @param[in]  tb           Token bucket to be credited/queried
 *  @param[in]  curr_time    Current time if known, or
 *                           ABS_TIME_0 if not already known
 *  @param[out] curr_tokens  Total amount of tokens in the bucket following
 *                            the credit (if any)
 * @return:
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 *  TB_RETVAL_INTERNAL_ERROR
 */
/* Refer to the header for tb_credit_tokens() */
static tb_retval_t
tb_credit_tokens_internal (token_bucket_info_t *tb,
                           abs_time_t curr_time,
                           uint32_t *curr_tokens)
{
    uint64_t delta_time =0;         /* in micro seconds */
    uint64_t elapsed_replenish_periods =0;
    uint64_t new_total_tokens = 0;
    uint32_t tokens_in_bucket = 0;
    abs_time_t last_token_replenish_time;
    tb_retval_t retval = TB_RETVAL_OK;
    boolean use_bucket_max = FALSE;

    if (!tb) {
        retval = TB_RETVAL_TB_INVALID;
        goto done;
    }

    if (!tb->replenish_period) {     
        goto done;    /*  retval = TB_RETVAL_OK */
    }

    /*
     * Basic algorithm:
     *   - compute the time delta between the last time the token
     *     bucket was credited, and now
     *   - compute how many "replenish periods" have passed, where
     *      a replenish period is the amount of time to earn a single
     *      quantum of tokens
     *   - credit the bucket by the amount of 
     *      (# of passed replenish periods) * (tokens per quantum)
     *   - advance the bucket's last_token_replenish_time by the (whole)
     *      number of replenish periods that have passed
     */
    tokens_in_bucket = tb->tokens_in_bucket;
    last_token_replenish_time = tb->last_token_replenish_time;
    if (TIME_CMP_A(eq, curr_time, ABS_TIME_0)) {
        curr_time = get_sys_time();
    }
    delta_time = 
        TIME_GET_R(usec, TIME_SUB_A_A(curr_time,
                                      tb->last_token_replenish_time));
    elapsed_replenish_periods = delta_time / tb->replenish_period;
    /*
     * Only update the token bucket (token count and last update timestamp)
     * if one or more replenish periods have passed
     */
    if (elapsed_replenish_periods) {
        /*
         * Add the newly earned tokens to the bucket.  But be careful not
         * overflow the bucket or any local variables on the way to doing this.
         */
        if (elapsed_replenish_periods >= UINT_MAX) {
            use_bucket_max = TRUE;
        } else {
            /* computation below shouldn't overflow uint64_t */
            new_total_tokens =
                elapsed_replenish_periods * tb->quantum_size +
                tb->tokens_in_bucket;
            if (new_total_tokens > tb->burst) {
                use_bucket_max = TRUE;
            }
        }
        /* Cap to the bucket's burst value (max) if needed */
        tb->tokens_in_bucket = (use_bucket_max ? tb->burst : new_total_tokens);

        /*
         * Update the last token replenish time.
         *
         * If previous token calculations would have overflowed the bucket,
         * then we use the current timestamp.
         *
         * Otherwise, use a timestamp in the past corresponding to the
         * time at which the last full token is granted.  This helps 
         * prevent loss of tokens due trunctation of fractional 
         * elapsed_replenish_periods.
         */
        tb->last_token_replenish_time = 
            (use_bucket_max ?
             curr_time : 
             TIME_ADD_A_R(tb->last_token_replenish_time,
                          TIME_MK_R(usec,
                                    elapsed_replenish_periods * 
                                    tb->replenish_period))
                );
    }

done:
    if (curr_tokens) {
        if (retval == TB_RETVAL_OK) {
            *curr_tokens = tb->tokens_in_bucket;
        } else {
            *curr_tokens = 0;
        }
    }

#if TB_DEBUG
    /* Not sure how best to preserve this... */
    char *dbg_buf, dbg_tmp1[TB_DEBUG_STR_LEN], dbg_tmp2[TB_DEBUG_STR_LEN];
    char dbg_time1[TIME_STRING_BUFF_SIZE], dbg_time2[TIME_STRING_BUFF_SIZE];
    snprintf(dbg_tmp2, TB_DEBUG_STR_LEN,
             "tb_credit_tokens_internal("
             "tb=%p, curr_time=%s, curr_tokens=%p): %s\r\n",
             tb,
             abs_time_to_str(curr_time, dbg_time1, TIME_STRING_BUFF_SIZE),
             curr_tokens,
             tb_retval_to_str(retval));
    if (retval == TB_RETVAL_OK) {
        snprintf(dbg_tmp1, TB_DEBUG_STR_LEN, "%s "
                 "   rate:                                 %u (tokens/s)\r\n"
                 "   burst:                                %u (tokens)\r\n"
                 "   quantum:                              %u (tokens)\r\n"
                 "   replenish_period:                     %u (us)\r\n"
                 "   (incoming) last_token_replenish_time: %s\r\n"
                 "   (incoming) tokens_in_bucket:          %u (tokens)\r\n"
                 "   delta_time:                           %llu (us)\r\n"
                 "   elapsed_replenish_periods:            %llu\r\n"
                 "   new token credits:                    %u (tokens)\r\n"
                 "   (updated) last_token_replenish_time:  %s\r\n"
                 "   (updated) tokens_in_bucket:           %u (tokens)\r\n",
                 dbg_tmp2,
                 tb->rate,
                 tb->burst,
                 tb->quantum_size,
                 tb->replenish_period,
                 abs_time_to_str(last_token_replenish_time, 
                                 dbg_time1, TIME_STRING_BUFF_SIZE),
                 tokens_in_bucket,
                 delta_time,
                 elapsed_replenish_periods,
                 (uint32_t)(tb->tokens_in_bucket - tokens_in_bucket),
                 abs_time_to_str(tb->last_token_replenish_time, 
                                 dbg_time2, TIME_STRING_BUFF_SIZE),
                 tb->tokens_in_bucket);
        dbg_buf = dbg_tmp1;
    } else {
        dbg_buf = dbg_tmp2;
    }
    printf(dbg_buf);
#endif /* TB_DEBUG */

    return (retval);
}

/*
 * tb_credit_tokens()
 *
 * Credits a token bucket for any tokens earned since the last credit,
 * and retrieves the amount of tokens currently in the updated bucket.
 *
 * If the supplied curr_time value is ABS_TIME_0, the current time will
 * be determined internally by this function for updating the token bucket.
 * Otherwise, the supplied curr_time value will be used.
 *
 * This function is used primarily for updating/reading the amount of
 * tokens in a token bucket.  To drain tokens, use the "drain_tokens()"
 * function.
 *
 * Params:
 *  @param[in]  tb           Token bucket to be credited/queried
 *  @param[in]  curr_time    Current time if known, or
 *                           ABS_TIME_0 if not already known
 *  @param[out] curr_tokens  Total amount of tokens in the bucket following
 *                            the credit (if any)
 * @return:
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 *  TB_RETVAL_INTERNAL_ERROR
 */
tb_retval_t
tb_credit_tokens (token_bucket_info_t *tb,
                  abs_time_t curr_time,
                  uint32_t *curr_tokens)
{
#if TB_DEBUG
    printf("tb_credit_tokens():\r\n");
#endif /* TB_DEBUG */

    return (tb_credit_tokens_internal(tb, curr_time, curr_tokens));
}


/*
 * tb_drain_tokens()
 *
 * Takes in a number of tokens needed, and drains them from the token bucket.
 * Note that this function does NOT credit the token bucket first with
 * tokens that have accumulated since the last token bucket update.
 * Callers which desire the token bucket be updated first should call
 * tb_credit_tokens() prior to calling this function.
 *
 * If there are insufficient tokens in the bucket, an error is 
 * returned, and the token bucket remains unmodified.
 *
 * Params:
 *  @param[in] tb                Token bucket to be credited/queried
 *  @param[in] number_of_tokens  Number of tokens to be drained, if possible.
 *
 * @return:
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 *  TB_RETVAL_INSUFFICIENT_TOKENS
 */
tb_retval_t
tb_drain_tokens (token_bucket_info_t *tb,
                 uint32_t number_of_tokens)
{
    uint32_t tokens_in_bucket = 0;
    tb_retval_t retval = TB_RETVAL_OK;

    if (!tb) {
        retval = TB_RETVAL_TB_INVALID;
        goto done;
    }

    if (number_of_tokens > tb->tokens_in_bucket) {
        retval = TB_RETVAL_INSUFFICIENT_TOKENS;
        goto done;
    }

    tokens_in_bucket = tb->tokens_in_bucket;
    tb->tokens_in_bucket -= number_of_tokens;

done:

#ifdef TB_DEBUG
    {
        /* Not sure how best to preserve this... */
        char *dbg_buf, dbg_tmp1[TB_DEBUG_STR_LEN], dbg_tmp2[TB_DEBUG_STR_LEN];
        char dbg_time1[TIME_STRING_BUFF_SIZE];
        snprintf(dbg_tmp2, TB_DEBUG_STR_LEN,
                 "tb_drain_tokens(tb=%p, number_of_tokens=%u (tokens)): "
                 "%s\r\n",
                 tb,
                 number_of_tokens,
                 tb_retval_to_str(retval));
        if (retval == TB_RETVAL_OK || 
            retval == TB_RETVAL_INSUFFICIENT_TOKENS) {
            snprintf(dbg_tmp1, TB_DEBUG_STR_LEN, "%s"
                     "   rate:                               %u (tokens/s)\r\n"
                     "   burst:                              %u (tokens)\r\n"
                     "   quantum:                            %u (tokens)\r\n"
                     "   replenish_period:                   %u (us)\r\n"
                     "   last_token_replenish_time:          %s\r\n"
                     "   (incoming) tokens_in_bucket:        %u (tokens)\r\n"
                     "   tokens_drained:                     %u (tokens)\r\n"
                     "   (updated) tokens_in_bucket:         %u (tokens)\r\n",
                     dbg_tmp2,
                     tb->rate,
                     tb->burst,
                     tb->quantum_size,
                     tb->replenish_period,
                     abs_time_to_str(tb->last_token_replenish_time, 
                                     dbg_time1, TIME_STRING_BUFF_SIZE),
                     tokens_in_bucket,
                     number_of_tokens,
                     tb->tokens_in_bucket);
            dbg_buf = dbg_tmp1;
        } else {
            dbg_buf = dbg_tmp2;
        }
        printf(dbg_buf);
    }
#endif /* TB_DEBUG */

    return (retval);
}

/*
 * tb_conform()
 *
 * Credits a token bucket for any tokens earned since the last credit,
 * and check if the requested number of tokens conforms to the amount 
 * of tokens currently in the updated bucket.
 *
 * If the supplied curr_time value is ABS_TIME_0, the current time will
 * be determined internally by this function for updating the token bucket.
 * Otherwise, the supplied curr_time value will be used. This is to provide
 * flexibility to the calling applications when the callers already have 
 * up-to-date system time.
 *
 * Note this function is used to check for conformability, and it does not 
 * drain the tokens. To drain tokens, use the "drain_tokens()" function.
 *
 * Params:
 *  @param[in]  tb           Token bucket to be credited/queried
 *  @param[in]  curr_time    Current time if known, or
 *                           ABS_TIME_0 if not already known
 *  @param[in]  number_of_tokens   Number of tokens to check for conformability
 *                           
 * @return::
 *  TB_RETVAL_OK
 *  TB_RETVAL_INSUFFICIENT_TOKENS
 *  TB_RETVAL_TB_INVALID
 *  TB_RETVAL_INTERNAL_ERROR
 */
tb_retval_t
tb_conform (token_bucket_info_t *tb,
            abs_time_t curr_time,
            uint32_t number_of_tokens)
{
    tb_retval_t retval;
    uint32_t curr_tokens;

#if TB_DEBUG
    printf("tb_conform():\r\n");
#endif /* TB_DEBUG */

    retval = tb_credit_tokens_internal(tb, curr_time, &curr_tokens);
    if (retval != TB_RETVAL_OK) {
        goto done;
    }

    if (number_of_tokens > curr_tokens) {
        retval = TB_RETVAL_INSUFFICIENT_TOKENS;
    } else {
        retval = TB_RETVAL_OK;
    }

done:

#ifdef TB_DEBUG
    {
        /* Not sure how best to preserve this... */
        char *dbg_buf, dbg_tmp2[TB_DEBUG_STR_LEN];
        char dbg_time1[TIME_STRING_BUFF_SIZE];
        snprintf(dbg_tmp2, TB_DEBUG_STR_LEN,
                 "tb_conform("
                 "tb=%p, curr_time=%s, number_of_tokens=%u): %s\r\n"
                 "   In token bucket: available tokens=%u, requested=%u\r\n",
                 tb,
                 abs_time_to_str(curr_time, dbg_time1, TIME_STRING_BUFF_SIZE),
                 number_of_tokens,
                 tb_retval_to_str(retval),
                 curr_tokens,
                 number_of_tokens);
        dbg_buf = dbg_tmp2;

        printf(dbg_buf);
    }
#endif /* TB_DEBUG */

    return (retval);
}


/*
 * tb_get_properties()
 *
 * Params:
 *  @param[in]   tb                    Token bucket object
 *  @param[out]  rate                  Token bucket's rate value [optional]
 *  @param[out]  burst                 Token bucket's burst value [optional]
 *  @param[out]  quantum_size          Token bucket's quantum size [optional]
 *  @param[out]  tokens_in_bucket      Number of tokens currently in bucket 
 *                                     [optional]
 *
 * @return::
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 */
inline tb_retval_t
tb_get_properties (const token_bucket_info_t  *tb,
                   uint32_t                   *rate,
                   uint32_t                   *burst,
                   uint32_t                   *quantum_size,
                   uint32_t                   *tokens_in_bucket)
{
    if (!tb) {
        return (TB_RETVAL_TB_INVALID);
    }
    
    if (rate) {
        *rate = tb->rate;
    }
    if (burst) {
        *burst = tb->burst;
    }
    if (quantum_size) {
        *quantum_size = tb->quantum_size;
    }
    if (tokens_in_bucket) {
        *tokens_in_bucket = tb->tokens_in_bucket;
    }

    return (TB_RETVAL_OK);
}

