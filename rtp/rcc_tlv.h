/*------------------------------------------------------------------
 * rcc_tlv.h  -- Define the TLVs used in RTCP APP packets to pass RCC
 *               parameters between VQE server and client
 * 
 * March 2008, Donghai Ma
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RCC_TLV_H__
#define __RCC_TLV_H__

#include "rtp_session.h"


/* 
 * TLV Definitions
 * ===============
 *  Type:   A single-octet identifier which defines the parameter
 *  Length: A _two-octet_ field specifying the length of the Value field 
 *          (not including Type and Length fields)
 *  Value:  A set of octets Length long containing the specific value for 
 *          the parameter
 *
 *
 * Parameters
 * ==========
 * 0. Padding
 *    This has no length or value fields and is only used following the 
 *    end-of-data marker to pad the Application Dependent Data to a multiple 
 *    of 32-bit words, per RFC3550.
 *      Type
 *      0
 *
 * 1. First FCC repair packet sequence number
 *    The starting sequence number of the FCC segmented repair bursts.
 *      Type        Length       Value
 *      1           4            32-bit extended RTCP sequence number
 *
 * 2. First FCC repair packet RTP timestamp
 *    The 32-bit RTP timestamp from the first FCC repair packet.
 *      Type         Length       Value
 *      2            4            32-bit RTP timestap
 *
 * 3. Earliest IGMP join time 
 *    Delta RTP time as measured from the start of the repair burst,
 *    to the earliest RTP time a STB could join the multicast stream 
 *    of the new channel.
 *      Type         Length       Value
 *      3            4            32-bit delta time in RTP clock ticks.
 *
 * 4. RCC repair end time
 *    Delta RTP time as measured from the end of the first repair leg,
 *    to the time when the FCC repair burst finished.
 *      Type         Length       Value
 *      4            4            32-bit delta time in RTP clock ticks.
 *
 * 5. TSRAP Data
 *    TSRAP data is TLV formatted data from MPEG Parser. 
 *      Type        Length       Value
 *      5           n            Encoded TSRAP data
 *
 * 6. Minimum RCC Fill 
 *    The minimum backfill of the STB's buffer that must be accomplished 
 *    by the RCC burst, expressed in milliseconds at the nominal rate of 
 *    the channel.
 *      Type        Length       Value
 *      6           4            32-bit value (milliseconds)
 *
 * 7. Maximum RCC Fill 
 *    The maximum backfill of the STB's buffer that may occur due to the 
 *    RCC burst, expressed in milliseconds at the nominal rate of the channel.
 *      Type        Length       Value
 *      7           4            32-bit value (milliseconds)
 *
 * 8. Actual RCC Fill
 *    The actual amount of backfill that the STB can expect in its buffer 
 *    as a result of the RCC burst, expressed in milliseconds.
 *      Type        Length       Value
 *      8           4            32-bit value (milliseconds)
 *
 * 9. Actual RCC Fill at IGMP Join
 *    The actual amount of backfill that the STB can expect in its buffer 
 *    as a result of the RCC burst, at the time of the earliest IGMP join.
 *      Type        Length       Value
 *      9           4            32-bit value (milliseconds)
 *
 * 10. ER Hold-off Time 
 *    The amount of time, in milliseconds, after the earliest IGMP join time, 
 *    that the STB should wait before requesting any Error Repairs.
 *      Type        Length       Value
 *      10          4            32-bit value (milliseconds)
 *   
 * 11. First Multicast Sequence Number
 *    Extended (32 bit) RTP sequence number of the first packet received 
 *    from the multicast stream, in network byte order.
 *      Type        Length       Value
 *      11          4            32-bit extended RTP sequence number
 *
 * 12. First Multicast Receive Time
 *    Also known as IGMP join time, which is the amount of time, in 
 *    milliseconds, a STB has waited to receive the first primary multicast 
 *    packet, after doing an IGMP join to the new channel. The value is in
 *    network byte order.
 *      Type        Length       Value
 *      12          4            32-bit value in milliseconds
 *
 * 13. Do fast fill flag
 *    This flag tells the server to prepare a RCC burst which is large enough
 *    to fill both the VQE-C buffer and the STB decoder buffer. The value is in
 *    network byte order.
 *    This information is sent from client to the server
 *      Type        Length       Value
 *      13          4            boolean  0 no fast fill, 1 do fast fill
 * 
 * 14. Actual fast fill time 
 *    This value tells the client to fast fill the amount of data to 
 *    STB decoder buffer. 
 *    This information is sent from server to the client
 *      Type        Length       Value
 *      14          4            32-bit value in RTP clock ticks
 *
 * 15. Maximum Receive Bandwidth
 *    This value communicates the MRB of the client back to the VQE-S for
 *    use in e-factor computation. The MRB is the sum of channel bandwidth
 *    and any excess bandwidth available for VQE retransmission. The value
 *    is given in bits per second. This value is sent from client to server
 *    as part of a PLII App(sent with PLI/NACK) or a ERRI App(sent with each 
 *    Generic NACK)
 *      Type        Length       Value
 *      15          4            32-bit value of MRB in bits/second
 *
 * 16. Maximum Fast fill Time
 *    The amount of time, in milliseconds at configured stream rate, that the
 *    VQE-C is able to fast fill into the decoder's hardware buffer. This 
 *    should be set to 0 if the decoder is not capable of fast fill.
 *      Type        Length       Value
 *      16          4            32-bit value (milliseconds)
 *
 * 17. RCC Status Code
 *   A numeric value indicating the status of the RCC. 
 *      Type        Length       Value
 *      16          4            32-bit value
 *   
 */

#include "utils/vam_types.h"
#include "utils/vam_time.h"

/**
 * app_msg_type_t
 * @brief
 * Type enumeration of the RTCP APP messages that are utilized in RCC
 */
typedef enum app_msg_type_t_ {
    /**@brief 
     * Previous Payload Data of Decoder: sent from server to client */
    PPDD = 0,
    /**@brief 
     * Packet Lost Indication Information: sent from client to server */
    PLII,
    /**@brief 
     * New Channel Start Information: sent from client to server */
    NCSI,
    /**@brief
     * Error Repair Request Information: sent from client to server*/
    ERRI,
} app_msg_type_t;


#define APP_NAME_LENGTH 4 
#define PPDD_APP_NAME "PPDD"
#define PLII_APP_NAME "PLII"
#define NCSI_APP_NAME "NCSI"
#define ERRI_APP_NAME "ERRI"


/* 1-octet type field; 2-octet length field */
#define TYPE_FIELD_LEN     1
#define LENGTH_FIELD_LEN   2

/* Parameter type enumeration */
typedef enum rcc_tlv_type_t_ {
    PADDING = 0,
    START_SEQ_NUMBER,
    START_RTP_TIMESTAMP,
    DT_EARLIEST_TIME_TO_JOIN,
    DT_REPAIR_END_TIME,
    TSRAP,
    MIN_RCC_FILL,
    MAX_RCC_FILL,
    ACT_RCC_FILL,
    ACT_RCC_FILL_AT_JOIN,
    ER_HOLDOFF_TIME,
    FIRST_MCAST_PKT_SEQ_NUM,
    FIRST_MCAST_PKT_RECV_TIME,
    DO_FASTFILL,
    ACTUAL_FASTFILL,
    MAXIMUM_RECV_BW,
    MAXIMUM_FASTFILL_TIME,
    RCC_STATUS, /* UNUSED */
     
    /* Must be Last!*/
    MAX_RCC_TLV_TYPE
} rcc_tlv_type_t;

/**
 * ppdd_tlv_t
 * @brief
 * RCC TLV data structure that is encoded in a PPDD APP message
 */
typedef struct ppdd_tlv_t_ {
    uint32_t start_seq_number;
    uint32_t start_rtp_time;
    uint32_t dt_earliest_join;
    uint32_t dt_repair_end;
    uint32_t act_rcc_fill;
    uint32_t act_rcc_fill_at_join;
    uint32_t er_holdoff_time;
    uint32_t actual_fastfill;
    uint16_t tsrap_len;
    uint8_t  tsrap[0];
} ppdd_tlv_t;

/**
 * plii_tlv_t
 * @brief
 * RCC TLV data structure that is encoded in a PLII APP message
 */
typedef struct plii_tlv_t_ {
    uint32_t min_rcc_fill;
    uint32_t max_rcc_fill;
    uint32_t do_fastfill;
    uint32_t maximum_recv_bw;
    uint32_t maximum_fastfill_time;
} plii_tlv_t;

/**
 * ncsi_tlv_t
 * @brief
 * RCC TLV data structure that is encoded in a NCSI APP message
 */
typedef struct ncsi_tlv_t_ {
    uint32_t first_mcast_seq_number;
    uint32_t first_mcast_recv_time;
} ncsi_tlv_t;

/**
 * erri_tlv_t
 * @brief
 * RCC TLV data structure that is encoded in an ERRI APP message
 */
typedef struct erri_tlv_t_ {
    uint32_t maximum_recv_bw;
} erri_tlv_t;

/* API functions to encode/decode RCC TLV data */

/**
 * ppdd_tlv_encode_allocate
 * @brief
 *  Encode a ppdd_tlv_t struct into TLV formated octet string.
 *  This function allocates memory to store the encoded octets.
 * @note
 *  This is used by VQE-S to prepare a PPDD APP message.
 * @param[in] tlv       - a ppdd_tlv_t struct
 * @param[in] tsrap     - ptr to a TLV formated TSRAP octet string, or NULL
 * @param[in] tsrap_len - length of the TLV formated TSRAP octet string, or 0
 * @param[out] data_len - length of the encoded TLV data octet string
 * @return
 *  pointer to the encoded TLV octet string
 */
extern uint8_t *ppdd_tlv_encode_allocate(const ppdd_tlv_t tlv, 
                                         uint8_t *tsrap, uint16_t tsrap_len,
                                         uint32_t *data_len);

/**
 * plii_tlv_encode_allocate
 * @brief
 *  Encode a plii_tlv_t struct into TLV formated octet string.
 *  This function allocates memory to store the encoded octets.
 * @note
 *  This is used by VQE-C to prepare a PLII APP message.
 * @param[in] tlv       - a plii_tlv_t struct
 * @param[out] data_len - length of the encoded TLV data octet string
 * @return
 *  pointer to the encoded TLV octet string
 */
extern uint8_t *plii_tlv_encode_allocate(const plii_tlv_t tlv,
                                         uint32_t *data_len);

/**
 * ncsi_tlv_encode_allocate
 * @brief
 *  Encode a ncsi_tlv_t struct into TLV formated octet string.
 *  This function allocates memory to store the encoded octets.
 * @note
 *  This is used by VQE-C to prepare a NCSI APP message.
 * @param[in] tlv       - a ncsi_tlv_t struct
 * @param[out] data_len - length of the encoded TLV data octet string
 * @return
 *  pointer to the encoded TLV octet string
 */
extern uint8_t *ncsi_tlv_encode_allocate(const ncsi_tlv_t tlv,
                                         uint32_t *data_len);

/**
 * @brief
 *  Encode a erri_tlv_t struct into TLV formated octet string.
 *  This function allocates memory to store the encoded octets.
 * @note
 *  This is used by VQE-C to prepare a NRRI APP message.
 * @param[in] tlv       - a erri_tlv_t struct
 * @param[out] data_len - length of the encoded TLV data octet string
 * @return
 *  pointer to the encoded TLV octet string
 */
extern uint8_t *erri_tlv_encode_allocate(const erri_tlv_t tlv,
                                         uint32_t *data_len);

/**
 * rcc_tlv_encode_destroy
 * @brief
 * Free up a previously allocated data buffer when doing 
 * xxx_tlv_encode_allocate().
 * @param[in] p_data     - ptr to a previously allocated data buffer while
 *                         encoding APP TLVs
 * @return
 *  none
 */
extern void rcc_tlv_encode_destroy(uint8_t *p_data);



/**
 * ppdd_tlv_decode_allocate
 * @brief
 * Decode a PPDD APP message
 * @note
 *  This function allocates memory to store the encoded octets.
 * @param[in] data     - ptr to the PPDD APP message, as an octet string
 * @param[in] data_len - length of the octet string
 * @return
 *   ptr to the allocated ppdd_tlv_t struct that stores the decoded PPDD TLVs;
 *   or NULL if decoding fails
 */
extern ppdd_tlv_t *ppdd_tlv_decode_allocate(uint8_t *data, uint16_t data_len);


/**
 * plii_tlv_decode_allocate
 * @brief
 * Decode a PLII APP message
 * @note
 *  This function allocates memory to store the encoded octets.
 * @param[in] data     - ptr to the PLII APP message, as an octet string
 * @param[in] data_len - length of the octet string
 * @return
 *   ptr to the allocated plii_tlv_t struct that stores the decoded PLII TLVs;
 *   or NULL if decoding fails
 */
extern plii_tlv_t *plii_tlv_decode_allocate(uint8_t *data, uint16_t data_len);


/**
 * ncsi_tlv_decode_allocate
 * @brief
 * Decode a NCSI APP message
 * @note
 *  This function allocates memory to store the encoded octets.
 * @param[in] data     - ptr to the NCSI APP message, as an octet string
 * @param[in] data_len - length of the octet string
 * @return
 *   ptr to the allocated ncsi_tlv_t struct that stores the decoded NCSI TLVs;
 *   or NULL if decoding fails
 */
extern ncsi_tlv_t *ncsi_tlv_decode_allocate(uint8_t *data, uint16_t data_len);


/**
 * erri_tlv_decode_allocate
 * @brief
 * Decode a ERRI APP message
 * @note
 *  This function allocates memory to store the encoded octets.
 * @param[in] data     - ptr to the ERRI APP message, as an octet string
 * @param[in] data_len - length of the octet string
 * @return
 *   ptr to the allocated erri_tlv_t struct that stores the decoded ERRI TLVs;
 *   or NULL if decoding fails
 */
extern erri_tlv_t *erri_tlv_decode_allocate(uint8_t *data, uint16_t data_len);


/**
 * rcc_tlv_decode_destroy
 * @brief
 * Free up a previously allocated xxx_tlv_t struct while doing 
 * xxx_tlv_decode_allocate().
 * @param[in] p_tlv  - ptr to a previously allocated xxx_tlv_t struct
 *                     while decoding an APP octet string
 * @return
 *  none
 */
extern void rcc_tlv_decode_destroy(void *p_tlv);


#endif /* ifndef __RCC_TLV_H__ */
