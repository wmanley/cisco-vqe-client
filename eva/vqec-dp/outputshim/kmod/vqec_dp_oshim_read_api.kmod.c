/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: output shim reader.
 *
 * Documents:
 *
 *****************************************************************************/
#include <vqec_dp_output_shim_api.h>
#include <vqec_dp_output_shim_private.h>
#include <vqec_dp_io_stream.h>
#include <vqec_dp_oshim_read_api.h>

#define VQEC_DP_OUTPUTSHIM_MAX_TUNERS 32

/*
 * This is made static to avoid races between threads waiting on the queues and
 * deinit of the module.
 */
static wait_queue_head_t s_waitq_array[VQEC_DP_OUTPUTSHIM_MAX_TUNERS];

/**---------------------------------------------------------------------------
 * Empty stub.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_oshim_read_tuner_read (vqec_dp_tunerid_t id,
                               vqec_iobuf_t *iobuf,
                               uint32_t iobuf_num,
                               uint32_t *len,
                               int32_t timeout_msec)
{
    return (VQEC_DP_ERR_INVALIDARGS);
}

/**---------------------------------------------------------------------------
 * This initialization is particular to the case when reads are constrained
 * to the kernel. We can create a zone of wait queue heads. However, it
 * should be noted that in this case we cannot free the memory until the
 * output thread has actually died. Hence, we use a simple kmalloc instead
 * of a zone, which may result in access after free.
 *
 * @param[in] max_tuners Maximum tuners.
 * @param[out] boolean Returns true if operation succeeds.
 *---------------------------------------------------------------------------*/
boolean vqec_dp_oshim_read_init (uint32_t max_tuners)
{
    uint32_t i;

    memset(s_waitq_array,
           0,
           sizeof(*s_waitq_array) * max_tuners);

    for (i = 0; i < max_tuners; i++) {
        init_waitqueue_head(s_waitq_array + i);
    }

    return (TRUE);
}


void vqec_dp_oshim_read_deinit (void)
{
}


/**---------------------------------------------------------------------------
 * Get the wait queue head pointer by dp tuner id. Dataplane tuner id's
 * start from 1 instead of 0.
 *
 * @param[in] tid Dataplane tuner id.
 * @param[out] wait_queue_head_t* Pointer to the wait queue head.
 *---------------------------------------------------------------------------*/
wait_queue_head_t *
vqec_dp_outputshim_get_waitq_by_id (vqec_dp_tunerid_t tid)
{
    if (likely((tid > 0) &&
               (tid <= g_output_shim.max_tuners) &&
               s_waitq_array)) {
        return (s_waitq_array + (tid - 1));
    }

    return (NULL);
}
