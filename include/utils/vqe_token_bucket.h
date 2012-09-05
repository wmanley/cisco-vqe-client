/*
 * Copyright (c) 2007, 2009-2012 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains APIs for functions implementing a simple (rate, burst)
 * token bucket policer.
 */

#ifndef __VQE_TOKEN_BUCKET_H__
#define __VQE_TOKEN_BUCKET_H__


#include <inttypes.h>
#include "vam_types.h"
#include "vam_time.h"

/*
 * Token bucket property limits
 *
 * If changing these, be sure to verify the implementation can accommodate
 * the new values (e.g. it gives reasonable accuracy despite truncations
 * in computations).
 */
#define TB_RATE_MAX          150000
#define TB_BURST_MAX         USHRT_MAX
#define TB_QUANTUM_MAX       4096
#define TB_QUANTUM_DEFAULT   1

/*
 * Token bucket API return values
 */
typedef enum tb_retval_t {
    TB_RETVAL_OK,                  /* success */
    TB_RETVAL_INTERNAL_ERROR,      /* internal error, operation failed */
    TB_RETVAL_TB_INVALID,          /* invalid token bucket object supplied */
    TB_RETVAL_RATE_INVALID,        /* invalid rate supplied */ 
    TB_RETVAL_BURST_INVALID,       /* invalid burst supplied */
    TB_RETVAL_QUANTUM_INVALID,     /* invalid quantum supplied */
    TB_RETVAL_INSUFFICIENT_TOKENS, /* not enough tokens in bucket */
} tb_retval_t;


/*
 * Token bucket object.
 *
 * NOTE:  Clients should treat this structure as opaque, and rely on the
 *        public API functions for manipulating it.  It is defined here
 *        only so that token bucket objects may be included as part of 
 *        other structures.
 */
typedef struct token_bucket_info_t_ {

    /*
     * The following fields are specified when the token bucket is initialized:
     */

    uint32_t rate;                        /*
                                           * Token bucket refill rate
                                           * (Measured in tokens/second.)
                                           * If set to zero, then DO NOT 
                                           * replenish the token bucket.
                                           */
    uint32_t burst;                       /*
                                           * Token bucket burst (a.k.a. bucket 
                                           * depth, or the max number of tokens
                                           * it can hold).
                                           * (Measured in tokens.)
                                           */
    uint32_t quantum_size;                /*
                                           * Tokens are credited to the bucket
                                           * atomically in batches of "n" at 
                                           * a time, where this "n" value is 
                                           * called a quantum.
                                           * (Measured in tokens.)
                                           */

    /*
     * This field is derived from the specified fields when the token bucket 
     * is initialized:
     */
    uint32_t replenish_period;            /*
                                           * The amount of time that is needed
                                           * to generate quantum_size tokens.
                                           * I.e., "quoantum_size" tokens are
                                           * credited at a rate equivalent to
                                           * once every "replenish_period" 
                                           * microseconds.
                                           * (Measured in microsec./quantum.)
                                           * If set to zero, then DO NOT 
                                           * replenish. This occurs when rate=0
                                           */

    /*
     * These fields are maintained as part of the token bucket's
     * operational state:
     */
    abs_time_t last_token_replenish_time;  /*
                                            * Last time the token bucket was 
                                            * credited with tokens.
                                            */
    uint32_t tokens_in_bucket;             /*
                                            * Number of tokens currently in the
                                            * bucket.
                                            */

} token_bucket_info_t;

/*
 * Note:  These APIs are not thread safe.  It is the clients' 
 * responsibility to ensure that token buckets are accessed atomically.
 * For instance, tb_drain_tokens() may fail if the following sequence occurs
 * on the same token bucket:
 *
 *         client A                    client B
 *            |                           |
 *  tokens = tb_credit_tokens()           |
 *            |                tokens = tb_credit_tokens()
 *     <use some tokens>                  |
 *            |                    <use some tokens>
 *      tb_drain_tokens()                 |
 *                                 tb_drain_tokens() [may FAIL!]
 */


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
tb_retval_to_str(tb_retval_t retval);

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
 *                                     valid range:  0 < x < TB_RATE_MAX
 * @param[in] burst                   Token bucket burst (a.k.a. bucket depth,
 *                                     or the max number of tokens it can hold)
 *                                     [valid range:  0 < x < TB_BURST_MAX]
 *
 * @return:
 *   TB_RETVAL_OK
 *   TB_RETVAL_TB_INVALID
 *   TB_RETVAL_RATE_INVALID
 *   TB_RETVAL_BURST_INVALID
 *   TB_RETVAL_INTERNAL_ERROR
 */
tb_retval_t
tb_init_simple(token_bucket_info_t  *tb,
               uint32_t             rate,
               uint32_t             burst);

/*
 * tb_init()
 *
 * Initializes a token bucket object.  The bucket is initially filled
 * with tokens.
 *
 * Params:
 * @param[in] tb                      Token bucket to be initialized
 * @param[in] rate                    Token bucket refill rate (tokens/second)
 *                                     valid range:  0 < x < TB_RATE_MAX
 * @param[in] burst                   Token bucket burst (a.k.a. bucket depth,
 *                                     or the max number of tokens it can hold)
 *                                     [valid range:  0 < x < TB_BURST_MAX]
 * @param[in] quantum_size            Tokens are credited to the bucket in
 *                                     batches of "n" at a time, where this
 *                                     "n" value is called a quantum.      
 *                                     I.e. if quantum_size=2, then a token
 *                                     credit will only occur for even amounts
 *                                     of tokens at a time.
 *                                     [valid range:  1 < x < TB_QUANTUM_MAX]
 * @param[in] curr_time               Current time if known, or
 *                                     ABS_TIME_0 if not already known
 *
 * @return::
 *   TB_RETVAL_OK
 *   TB_RETVAL_TB_INVALID
 *   TB_RETVAL_RATE_INVALID
 *   TB_RETVAL_BURST_INVALID
 *   TB_RETVAL_QUANTUM_INVALID
 */
tb_retval_t
tb_init(token_bucket_info_t  *tb,
        uint32_t             rate,
        uint32_t             burst,
        uint32_t             quantum_size,
        abs_time_t           curr_time);

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
 * @return::
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 *  TB_RETVAL_INTERNAL_ERROR
 */
tb_retval_t
tb_credit_tokens(token_bucket_info_t *tb,
                 abs_time_t curr_time,
                 uint32_t *curr_tokens);

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
 * @return::
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 *  TB_RETVAL_INSUFFICIENT_TOKENS
 */
tb_retval_t
tb_drain_tokens(token_bucket_info_t *tb,
                uint32_t number_of_tokens);

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
tb_conform(token_bucket_info_t *tb,
           abs_time_t curr_time,
           uint32_t number_of_tokens);

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
 * @return:
 *  TB_RETVAL_OK
 *  TB_RETVAL_TB_INVALID
 */
inline tb_retval_t
tb_get_properties(const token_bucket_info_t  *tb,
                  uint32_t                   *rate,
                  uint32_t                   *burst,
                  uint32_t                   *quantum_size,
                  uint32_t                   *tokens_in_bucket);

#endif /* __VQEC_TOKEN_BUCKET_H__ */
