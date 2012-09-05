/*------------------------------------------------------------------
* Mpeg Parser.  
* Definitions for TLV Data
* May 2006, Jim Sullivan
*
* Copyright (c) 2006-2009 by cisco Systems, Inc.
* All rights reserved.
*------------------------------------------------------------------
*/
#ifndef __MP_TLV_H__
#define __MP_TLV_H__

#include<utils/vam_types.h>

/** \page page1 TSRAP TLV Description 
* \section overview Overview
* A Fast Channel Change (FCC) exchange between the VAM and EVA includes 
* signaling and control information(sent via RTCP) and a repair burst
* containing the MPEG video in the data plane. Here we describe the
* relationship between these two components of FCC and the encoding
* scheme used to send the control data.
*
* The MPEG video sent in the repair burst contains an Elementary Stream 
* Random Access Point(ESRAP). This generally includes a PES header 
* containing a PTS, followed by a sequence_header, followed by an I-Frame. 
*
* EVA is responsible for receiving the FCC information coming from the VAM 
* and sending the MPEG System Decoder the information it requires to 
* demultiplex and decode the video stream. We refer to this information as 
* the Transport Stream Random Access Point(TSRAP). The TSRAP includes the 
* ESRAP, PAT, PMT, PCR and various other information that may be needed 
* (e.g. ECM data when the video is scrambled). The TSRAP information that 
* is not included in the ESRAP is sent from the VAM to EVA as control data.  
*       
* Below we describe the TLV encoding scheme used to encode the control data. 
* Additionally we define sample data structures and functions that can be 
* used as examples of how to decode the TSRAP data and generate the 
* corresponding MPEG packets.  
*
* The TLV encoding for each component of the TSRAP data will include enough 
* information to generate MPEG TS packets that can be sent to a System Decoder
* without having to decode the raw mpeg data. For example the PA section data
* is carried in a MPEG TS packet with PID=0 and the PM section data is carried 
* in a MPEG TS packet with PID=PMT_PID where PMT_PID is identified in the PA
* section data. The TLV encoding for the PM section will include the 
* PMT_PID in addition to the raw PM section data so the PMT PID is known without
* having to parse the PA section data.  
* 
* References: MPEG-2 Systems (ISO13818-1), MPEG-2 Video (ISO13818-2), 
*      Surfside System Functional Spec, Surfside MPEG Parser Design Spec
*
* \section tlv_encoding TLV Encoding Scheme  
* FCC uses a type-length-value (TLV) encoding scheme to encode much of the 
* data that is carried in the RTCP Video Acceleration Application(VAA-App)
* messages used to exchange data between VAM and EVA components.  
* 
* General Form of the TLV Encoded Data 
* 
* An MP TLV is encoded as a 2 octet field to specify the Type, followed 
* by a 2 octet Length field, followed by a variable length Value field.
*
* Type encodes the value field to be interpreted.
* Length specifies the length of the value field in octets. 
*
* There is no alignment requirement for the first octect of a TLV
* The value field of a TLV may contain other TLVs.
*
* The general encoding for TLV is as follows:
* r in any bit field denotes reserved. These bits are set to 0. 
*<pre> 
* 0                   1                   2                   3
* 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |           Type                |           Len                 | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                           Value                               | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
*
* \section tlsrap_tlv TSRAP TLV Definition  
*
* The TSRAP TLV is used to encode TSRAP data. 
* Its encoding is as follows:   <pre>
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | TSRAP_TYPE (1)                |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_PAT (TLV)*                         | 
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_PMT (TLV)*                         |
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_PCR (TLV)*                         |
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_PID_LIST (TLV)*                    | 
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_SEQ (TLV)                          |
* ~                        MPEG-2 Only                            ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_SPS (TLV)                          |
* ~                        H.264 Only                             ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_PPS (TLV)                          |
* ~                        H.264 Only                             ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_SEI (TLV)                          |
* ~                        H.264 Only                             ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_ECM (TLV)                          |
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_CAT (TLV)                          |
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_EMM (TLV)                          |
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                      TSRAP_PTS (TLV)                          |
* ~                                                               ~
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* * is Required, others are optional
* </pre>
* The length of the TSRAP TLV is variable.  
* The PID LIST, PA, PMAP and PCR TLVs are required. They must appear
* in the given order. 
* The SEQ TLV is not  present if the repair burst has a video 
* sequence_header and sequence_extension between the start of the 
* PES packet and the start of the first I-frame of the PES packet. 
* (see H.262 [ISO13818-2] for a discussion of video sequence_header
* and sequence_extension).
* The CA TLV's are present if the encoded data of one or more ES is scrambled. 
*
* \subsection pa_tlv PA TLV Definition  
* The PA TLV is used to encode information associated with a Program 
* Association(PA) section. 
* <pre> 
* 0                   1                   2                   3
* 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PA_TYPE (2)                   |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r|          sect-len             |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                               sect-data                       | 
* |                                                               |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* The length of the PA TSRAP TLV is variable.
* The PID is a 13 bit field used to identify the ES in a MPTS or SPTS. 
* This is the PID used in the associated TS to carry the PA section 
* data. Its value should always be 0. 
* The sect-len is a 16 bit field that specifies the length of the
* sect-data in octects.
* The sect-data is a PA Section as defined in H.222 section 2.4.4.3 
*
* \subsection pmt_tlv PMAP TLV Definition  
* The PMAP TLV is used to encode information associated with a Program Map
* (PMAP) section. 
* <pre>
* 0                   1                   2                   3
* 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PMAP_TYPE (3)                 |            Length             | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r|           sect-len            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* |                           sect-data                           |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* See PA TLV for description of the fields
* The PID is used in the associated TS to carry the PMAP section data.
*
* \subsection pcr_tlv PCR TLV Definition  
*
* The PCR TLV is used to encode the PCR time stamp corresponding to the 
* first byte of the first TS packet that is received in the repair burst. 
* 
* <pre> 
*  *0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PCR_TYPE (4)                  |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |            pid          |r r r|          pcr_ext              |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                           pcr_base		                  |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |b|                          tsrate                             |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* This is the PID used in the associated TS to carry the PCR data. 
* The PCR base is a 33 bit field that is part of the PCR timestamp. 
* The PCR base occupies the entire third 32-bit word along with the first
* bit of the fourth word. The remainder of the fourth word is the tsrate.
* The PCR ext is a 9 bit field that is part of the PCR timestamp. 
* 
*  \subsection pidlist_tlv PIDLIST TLV Definition 
* A PID is a packet identifier that is used to identify ESs of a 
* program in a SPTS or MPTS. The PID List contains a PID Element
* for each PID referenced in a TSRAP. Each PID element contains
* the PID and associated continuity counter that corresponds to 
* the start of the FCC repair burst. This continuity count should 
* be used on TS packets of the corresponding PID that immediately 
* precedes the start of the burst. This occurs when MPEG TS packets 
* are generated from the TSRAP data and prepended to the repair 
* stream before forwarding to an MPEG System Decoder.   
* <pre>
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PID_LIST_TYPE (5)             |          Length               | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                     PID Element  1                            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* ~                                                               ~
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                     PID Element  n                            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* 
* PID Element 
* <pre>
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |          pid            |r r r|r r r r|  cc   |  reserved     |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* The Length of the PID List TLV is variable. 
* PID - a 13 bit field used to identify the ES in a MPTS or SPTS. 
* CC  - a 4 bit field containing the continuity count corresponding
*       to the start of the repair burst for this PID. This cc 
*       value is the one that would be used for a TS packet that 
*       immediately precedes the first TS repair packet with this PID.  
* 
* \subsection seq_tlv SEQ TLV Definition  
* The SEQ TLV is used to encode information from the Video Sequence header 
* of an MPEG-2 Video Elementary Stream. The Video Sequence Header contains
* information such as frame rate, aspect ratio, and picture size.
* <pre>
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | SEQ_TYPE (6)                  |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r|         sect-len              |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* |                           sect-data                           |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* See PA TLV for description of fields. 
* See H.262 6.2.2.1 for detailed structure of Video Sequence Header.
* This applies only to Transport Streams carrying MPEG-2 (H.262) Video.
* 
* \subsection sps_tlv SPS TLV Definition
* The SPS TLV is used to encode information from the Sequence Parameter
* Set Network Abstraction Layer(NAL) Unit. 
* <pre>
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | SPS_TYPE (7)                  |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r|           sect-len            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* |                           sect-data                           |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* See PA TLV for description of fields.
* See H.262 7.4.2.1 for semantics of the Sequence Parameter Set NAL.
* This applies only to TS Streams carrying H.264 Video
* 
* \subsection pps_tlv PPS TLV Definition
* The PPS TLV is used to encode information from the Picture Parameter
* Set Network Abstraction Layer(NAL) Unit. 
* <pre>
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PPS_TYPE (8)                  |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r|             sect-len          |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* |                           sect-data                           |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* See PA TLV for description of fields.
* See H.262 7.4.2.2 for semantics of the Picture Parameter Set NAL.
* This applies only to TS Streams carrying H.264 Video
* 
* \subsection sei_tlv SEI TLV Definition
* The SEI TLV is used to encode information from the Supplemental Enhanced
* Information Network Abstraction Layer(NAL) Unit. 
* <pre>
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | SEI_TYPE (9)                  |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r|                   sect-len    |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* |                           sect-data                           |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* See PA TLV for description of fields.
* See H.262 Annex D for details of the SEI NAL.
* This applies only to TS Streams carrying H.264 Video
* 
* \subsection ecm_tlv ECM TLV Definition  
* The ECM TLV - 
*  <pre>
* This applies only to TS Streams which are encrypted.
* Value depends on type of encryption used.  
*
* For non-Widevine-encrypted streams, there are two TLVs per 
* CA PID (one for Table ID 80, the other for 80). Value carries 
* a PSI section found on CA PID, for either table ID 80 or 81.
* The order the two TLVs appear in the TSRAP TLV is oldest first
* (the same order they appeared in the stream).
*
* For Widevine-encrypted streams, there is one of these TLVs per
* CA PID.  Value carries an entire 188-byte PES packet found on
* CA PID (presumably contains ECMs).
*
* For non-Widevine-encrypted streams, MP_TSRAP_ECM_TYPE is used.
* For Widevine-encrypted streams, MP_TSRAP_ECM_PES_TYPE is used.
* 
*
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | ECM_TYPE (10)(200)             |         Length               | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r |          sect-len            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                              Value                            | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre> 
* 
* \subsection cat_tlv CAT TLV Definition  
* The CAT TLV - To be completed 
* <pre> 
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | CAT_TYPE (11)                  |         Length               | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |      pid                |r r r |          sect-len            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                                                               |
* |                            sect-data                          |
* |                                                               |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* See PA TLV for description of the fields
* The PID is used in the associated TS to carry the CA Table section data.
* 
* \subsection ecm_tlv EMM TLV Definition
* 
* The EMM TLV - To be completed 
* <pre> 
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | EMM_TYPE (12)                  |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                           Value                               | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
*
* \subsection pts_tlv PTS TLV Definition  
*
* The PTS TLV is used to encode the PTS time stamp corresponding to the 
* first picture in the unicast repair burst
* 
* <pre> 
*  *0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PTS_TYPE (13)                 |         Length                | 
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |            pid          |r r r|          reserved             |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                           pts_base		                  |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |b|                         reserved                            |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* </pre>
* This is the PID used in the associated TS to carry the PTS data. 
* The PTS base is a 33 bit field that taken from the PES header of the
* current TSRAP. 
* Note: This TLV is not decoded into a TS packet. It is purely
* informational.
*/ 


/**
* Definitions for the TLV Types used to carry TSRAP data.  
*/
typedef enum mp_tsraptlv_types_ {
    MP_TSRAP_TYPE_NONE = 0,    /* not a valid Type */
    MP_TSRAP_TYPE,
    MP_TSRAP_PA_TYPE,          /* Program Association Table Data */ 
    MP_TSRAP_PMAP_TYPE,        /* Program Map Table Data */
    MP_TSRAP_PCR_TYPE,         /* Program Clock Reference Data */
    MP_TSRAP_PIDLIST_TYPE,     /* Pidlist Data */
    MP_TSRAP_SEQPEXT_TYPE,     /* Video Sequence Header Data (H.262) */
    MP_TSRAP_SPS_TYPE,         /* Sequence Parameter Set Data (H.264) */
    MP_TSRAP_PPS_TYPE,         /* Picture Parameter Set Data (H.264) */
    MP_TSRAP_SEI_TYPE,         /* Supplemental Enhancement Info (H.264) */
    MP_TSRAP_ECM_TYPE,         /* Entitlement Control Message Data */
    MP_TSRAP_CA_TYPE,          /* Conditional Access Table Data */
    MP_TSRAP_EMM_TYPE,         /* Entitlement Management Message Data */
    MP_TSRAP_PTS_TYPE,         /* Presentation Timestamp (PTS) */
    MP_TSRAP_ECM_PES_TYPE = 200, /* ECM, Widevine non-standard format - PES */
}mp_tsraptlv_types_t;

typedef struct VAM_PACKED mp_tlvhdr_pac_ {
    uint16_t type;
    uint16_t len; 
}mp_tlvhdr_pac_t; 
#define MP_GET_TLV_LEN(tlvhdr_ptr)   \
        (ntohs((tlvhdr_ptr)->len))
#define MP_GET_TLV_TYPE(tlvhdr_ptr)   \
        (ntohs((tlvhdr_ptr)->type))
/**
* \struct mp_tlvsect_pac_t
* This will be used for PAT, PMT, SEQ_HDR, and possibly conditional access 
*/
typedef struct VAM_PACKED mp_tlvsect_pac_ {
    mp_tlvhdr_pac_t tlvhdr; 
    uint16_t pid;              	/** PID */
    uint16_t sect_len;      	/** PSI section length */
    uint8_t sect_data[];       	/** PSI section data */
}mp_tlvsect_pac_t; 
#define MP_GET_SECTTLV_LEN(secttlv_ptr)   \
        (ntohs((secttlv_ptr)->sect_len))
#define MP_GET_SECTTLV_CC(secttlv_ptr)	\
	(ntohs((secttlv_ptr)->cc>>4))
#define MP_GET_SECTTLV_PID(secttlv_ptr)  \
        (ntohs((secttlv_ptr)->pid))
/**
 * \struct mp_tlv_pidelement_pac_t
 * Holds TLV encoded element of pidlist 
 */
typedef struct VAM_PACKED mp_tlv_pidelement_pac_{
    uint16_t pid;       /** Program ID */
    uint8_t cc;         /** Continuity Counter */
    uint8_t reserved;   /** Reserved */
}mp_tlv_pidelement_pac_t;
#define MP_GET_PID_ELEMENT_CC(pide_ptr)   \
        (pide_ptr)->cc

/**
 * \struct mp_tlv_pidlist_pac_t
 * Holds elements of type mp_tlv_pidelement_pac_t to represent
 * a pidlist.
 */
typedef struct VAM_PACKED mp_tlv_pidlist_pac_ {
    mp_tlvhdr_pac_t tlvhdr;         /** TLV Header Data */
    mp_tlv_pidelement_pac_t dat[];  /** List of pidelements */
}mp_tlv_pidlist_pac_t; 

/**
 * \struct mp_tlvpcr_pac_t
 * TLV encoded PCR data 
 */
typedef struct VAM_PACKED mp_tlvpcr_pac_ {
    mp_tlvhdr_pac_t tlvhdr; 
    uint16_t pid;           	/** PCR pid */
    uint16_t pcr_ext;		/** Continuity Counter and PCR Extension */
    uint64_t pcr_base_tsrate;  	/** PCR Base and TS rate in BPS(0 if unknown)*/
}mp_tlvpcr_pac_t;
#define MP_TLV_PCR_EXT_MASK    0x0FFF
#define MP_TLV_PCR_BASE_MASK   0xFFFFFFFF80000000LL
#define MP_TLV_PCR_TSRATE_MASK 0x000000007FFFFFFFLL
#define MP_GET_PCR_EXT(pcrtlv_ptr) \
	(ntohs((pcrtlv_ptr)->pcr_ext) & MP_TLV_PCR_EXT_MASK)
#define MP_GET_PCR_BASE(pcrtlv_ptr)	\
    ((ntohll((pcrtlv_ptr)->pcr_base_tsrate) & MP_TLV_PCR_BASE_MASK)>>31)
#define MP_GET_PCR_TSRATE(pcrtlv_ptr)	\
	(ntohll((pcrtlv_ptr)->pcr_base_tsrate) & MP_TLV_PCR_TSRATE_MASK)  

/** 
 * PIDLIST Structure 
 *
 * 15 comes from overkill for:
 *
 *    MP_TSRAP_PA_TYPE:      1
 *    MP_TSRAP_PMAP_TYPE:    1
 *    MP_TSRAP_PCR_TYPE:     1
 *    MP_TSRAP_SEQPEXT_TYPE: 1
 *    MP_TSRAP_SPS_TYPE      1
 *    MP_TSRAP_PPS_TYPE      1
 *    MP_TSRAP_ECM_TYPE      3 (at most, or 3 _PES)
 *                          ---
 *                           9 
 */

#define MP_MAX_TSRAP_PIDS 15
typedef struct {
    uint32_t cnt;               /**< Number of pidlist entries */
    struct {
        uint16_t pid;           /**< PID of entry */
        uint8_t cc;             /**< Continuity Count of entry */
    }dat[MP_MAX_TSRAP_PIDS];
}mp_tlv_pidlist_t; 

boolean mp_set_cc_for_pid (mp_tlv_pidlist_t* pidlist_ptr,
                           uint16_t pid, uint8_t cc);

boolean mp_get_cc_for_pid (mp_tlv_pidlist_t* pidlist_ptr,
                           uint16_t pid, uint8_t *cc_ptr,
                           boolean decrement_flag);



typedef enum mp_tlv_status_ {
    MP_STATUS_TLV_SUCCESS=0,
    MP_STATUS_TLV_INVALID_LEN,          /* invalid TLV length */
    MP_STATUS_TLV_SHOULD_BE_FIRST,      /* TLV type should be first */
    MP_STATUS_TLV_INVALID_TYPE,         /* invalid TLV type */
    MP_STATUS_TLV_DECODE_FAILED,        /* error decoding TLV section */
    MP_STATUS_TLV_PAT_DECODE_FAILED,    /* error decoding PAT TLV */
    MP_STATUS_TLV_PMT_DECODE_FAILED,    /* error decoding PMT TLV */
    MP_STATUS_TLV_PCR_DECODE_FAILED,    /* error decoding PCR TLV */
    MP_STATUS_TLV_PIDLIST_DECODE_FAILED,/* error decoding PIDLIST TLV */
    MP_STATUS_TLV_PATPMTDUP_FAILED,     /* No buffer for patpmt dup */
    MP_STATUS_BUFF_TOO_SMALL,           /* TLV couldn't fit in decode buffer */
}mp_tlv_status_t;

#endif
