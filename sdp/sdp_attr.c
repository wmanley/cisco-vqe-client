/*
 *------------------------------------------------------------------
 * sdp_attr.c  -- Routines for parsing and building SDP attributes.
 *
 * April 2001, D. Renee Revis
 *
 * Copyright (c) 2001-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"
#include "sdp_base64.h"

/* Function:    sdp_parse_attribute
 * Description: Figure out the type of attribute and call the appropriate
 *              parsing routine.  If parsing errors are encountered, 
 *              warnings will be printed and the attribute will be ignored.
 *              Unrecognized/invalid attributes do not cause overall parsing
 *              errors.  All errors detected are noted as warnings.
 * Parameters:  sdp_p       The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              ptr         Pointer to the attribute string to parse.
 */
sdp_result_e sdp_parse_attribute (sdp_t *sdp_p, u16 level, const char *ptr)
{
    int           i;
    u8            xcpar_flag = FALSE;
    sdp_result_e  result;
    sdp_mca_t    *mca_p=NULL;
    sdp_attr_t   *attr_p;
    sdp_attr_t   *next_attr_p;
    sdp_attr_t   *prev_attr_p = NULL;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Validate the level */
    if (level != SDP_SESSION_LEVEL) {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_FAILURE);
        }
    }

    /* Find the attribute type. */
    ptr = sdp_getnextstrtok(ptr, tmp, ": \t", &result);
    if (ptr == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No attribute type specified, parse failed.",
                      sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    if (ptr[0] == ':') {
        /* Skip the ':' char for parsing attribute parameters. */
        ptr++;
    }

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No attribute type specified, parse failed.",
                      sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = (sdp_attr_t *)SDP_MALLOC(sizeof(sdp_attr_t));
    if (attr_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }
    attr_p->type = SDP_ATTR_INVALID;
    attr_p->next_p = NULL;
    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_attr[i].name, sdp_attr[i].strlen) == 0) {
            attr_p->type = (sdp_attr_e)i;
            break;
        }
    }
    if (attr_p->type == SDP_ATTR_INVALID) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Unrecognized attribute (%s) ", 
                     sdp_p->debug_str, tmp);
        }
        sdp_free_attr(attr_p);
        return (SDP_SUCCESS);
    }

    /* If this is an X-cpar or cpar attribute, set the flag.  The attribute 
     * type will be changed by the parse. */
    if ((attr_p->type == SDP_ATTR_X_CPAR) ||
	(attr_p->type == SDP_ATTR_CPAR)) {
        xcpar_flag = TRUE;
    }

    /* Parse the attribute. */
    result = sdp_attr[attr_p->type].parse_func(sdp_p, attr_p, ptr);
    if (result != SDP_SUCCESS) {
        sdp_free_attr(attr_p);
        /* Return success so the parse won't fail.  We don't want to
         * fail on errors with attributes but just ignore them.
         */
        return (result);
    }

    /* If this was an X-cpar/cpar attribute, it was hooked into the X-cap/cdsc
     * structure, so we're finished. 
     */
    if (xcpar_flag == TRUE) {
        return (result);
    }

    /* Add the attribute in the appropriate place. */
    if (level == SDP_SESSION_LEVEL) {
        for (next_attr_p = sdp_p->sess_attrs_p; next_attr_p != NULL;
             prev_attr_p = next_attr_p, 
                 next_attr_p = next_attr_p->next_p); /* Empty for */
        if (prev_attr_p == NULL) {
            sdp_p->sess_attrs_p = attr_p;
        } else {
            prev_attr_p->next_p = attr_p;
        }
    } else {  
        for (next_attr_p = mca_p->media_attrs_p; next_attr_p != NULL;
             prev_attr_p = next_attr_p, 
                 next_attr_p = next_attr_p->next_p); /* Empty for */
        if (prev_attr_p == NULL) {
            mca_p->media_attrs_p = attr_p;
        } else {
            prev_attr_p->next_p = attr_p;
        }
    }

    return (result);
}

/* Build all of the attributes defined for the specified level. */
sdp_result_e sdp_build_attribute (sdp_t *sdp_p, u16 level, char **ptr, u16 len)
{
    sdp_attr_t   *attr_p;
    sdp_mca_t    *mca_p=NULL;
    sdp_result_e  result;
    char          *endbuf_p;

    if (level == SDP_SESSION_LEVEL) {
        attr_p = sdp_p->sess_attrs_p;
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_FAILURE);
        }
        attr_p = mca_p->media_attrs_p;
    }
    /* Re-initialize the current capability number for this new level. */
    sdp_p->cur_cap_num = 1;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    /* Build all of the attributes for this level. Note that if there 
     * is a problem building an attribute, we don't fail but just ignore it.*/
    while (attr_p != NULL) {
        if (attr_p->type >= SDP_MAX_ATTR_TYPES) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Invalid attribute type to build (%u)", 
                         sdp_p->debug_str, attr_p->type);
            }
        } else {
            result = sdp_attr[attr_p->type].build_func(sdp_p, attr_p, 
                                                       ptr, len);
            if (result == SDP_SUCCESS) {
                if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                    SDP_PRINT("%s Built a=%s attribute line", sdp_p->debug_str,
                              sdp_get_attr_name(attr_p->type));
                }
            }

            len = endbuf_p - *ptr;

        }
        attr_p = attr_p->next_p;
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_simple_string (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                           const char *ptr)
{
    sdp_result_e  result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.string_val, " \t", &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No string token found for %s attribute",
                     sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (result);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type), 
                      attr_p->attr.string_val);
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_simple_string (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                           char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", sdp_attr[attr_p->type].name,
                     attr_p->attr.string_val);

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_simple_u32 (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                        const char *ptr)
{
    sdp_result_e  result;

    attr_p->attr.u32_val = sdp_getnextnumtok(ptr, &ptr, " \t", &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Numeric token for %s attribute not found",
                     sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %lu", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type), attr_p->attr.u32_val);
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_simple_u32 (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                        char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%lu\r\n", sdp_attr[attr_p->type].name,
                     attr_p->attr.u32_val);

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_simple_bool (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                         const char *ptr)
{
    sdp_result_e  result;

    if (sdp_getnextnumtok(ptr, &ptr, " \t", &result) == 0) {
        attr_p->attr.boolean_val = FALSE;
    } else {
        attr_p->attr.boolean_val= TRUE;
    }

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Boolean token for %s attribute not found",
                     sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_BOOLEAN);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            if (attr_p->attr.boolean_val) {
                SDP_PRINT("%s Parsed a=%s, boolean is TRUE", sdp_p->debug_str,
                          sdp_get_attr_name(attr_p->type));
            } else {
                SDP_PRINT("%s Parsed a=%s, boolean is FALSE", sdp_p->debug_str,
                          sdp_get_attr_name(attr_p->type));
            }
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_simple_bool (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                         char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    char         *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:", sdp_attr[attr_p->type].name);
    len = endbuf_p - *ptr;

    if (attr_p->attr.boolean_val == TRUE) {
        *ptr += snprintf(*ptr, len, "1\r\n");
    } else {
        *ptr += snprintf(*ptr, len, "0\r\n");
    }
    return (SDP_SUCCESS);
}

/*
 * sdp_parse_attr_maxprate
 *
 * This function parses maxprate attribute lines. The ABNF for this a= 
 * line is:
 *    max-p-rate-def = "a" "=" "maxprate" ":" packet-rate CRLF
 *    packet-rate = 1*DIGIT ["." 1*DIGIT]
 *
 * Returns:
 * SDP_INVALID_PARAMETER - If we are unable to parse the string OR if
 *                         packet-rate is not in the right format as per 
 *                         the ABNF.
 *
 * SDP_SUCCESS - If we are able to successfully parse the a= line.
 */
sdp_result_e sdp_parse_attr_maxprate (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      const char *ptr)
{
    sdp_result_e  result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.string_val, " \t", &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No string token found for %s attribute",
                     sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (!sdp_validate_floating_point_string(attr_p->attr.string_val)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s is not a valid maxprate value.", 
                          attr_p->attr.string_val);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type), 
                      attr_p->attr.string_val);
        }
        return (SDP_SUCCESS);
    }
}

/* Note:  The fmtp attribute formats currently handled are:
 *        fmtp:<payload type> <event>,<event>...
 *        fmtp:<payload_type> [annexa=yes/no] [annexb=yes/no] [bitrate=<value>]
 *        [QCIF =<value>] [CIF =<value>] [MaxBR = <value>] one or more 
 *        Other FMTP params as per H.263, H.263+, H.264 codec support.
 *        Note -"value" is a numeric value > 0 and each event is a 
 *        single number or a range separated by a '-'.
 *        Example:  fmtp:101 1,3-15,20
 * Video codecs have annexes that can be listed in the following legal formats:
 * a) a=fmtp:34 param1=token;D;I;J;K=1;N=2;P=1,3
 * b) a=fmtp:34 param1=token;D;I;J;K=1;N=2;P=1,3;T
 * c) a=fmtp:34 param1=token;D;I;J
 *
 */

sdp_result_e sdp_parse_attr_fmtp (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                  const char *ptr)
{
    u16           i;
    u32           mapword;
    u32           bmap;
    u8            low_val;
    u8            high_val;
    u32           bitrate;
    const char    *ptr2;
    const char    *fmtp_ptr;
    sdp_result_e  result1 = SDP_SUCCESS;
    sdp_result_e  result2 = SDP_SUCCESS;
    tinybool      done = FALSE;
    tinybool      codec_info_found = FALSE;
    sdp_fmtp_t   *fmtp_p;
    char          tmp[SDP_MAX_STRING_LEN];
    char          *src_ptr;
    char          *temp_ptr = NULL;
    tinybool flag=FALSE;
    char         *tok=NULL;
    char         *temp=NULL;
    u16          val = 0;
    u16          custom_x = 0;
    u16          custom_y = 0;
    u16          custom_mpi = 0;
    u8           par_width = 0;
    u8           par_height = 0;
    u16          iter = 0;
    short        profile=SDP_INVALID_VALUE;
    short        level=SDP_INVALID_VALUE;
    u32          temp_val = 0;
    int16        pack_mode=0;
    int16        parameter_add;
    u16          annex_k_val=0;
    u16          annex_n_val=0;
    u16          annex_p_val=0;
    ulong        l_val = 0;
    u32          rtcp_per_rcvr_bw = 0;

    /* Find the payload type number. */
    attr_p->attr.fmtp.payload_num = sdp_getnextnumtok(ptr, &ptr, 
                                                      " \t", &result1);
    if (result1 != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No payload type specified for "
                     "fmtp attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_UNKNOWN_TYPE;
    fmtp_p->parameter_add = TRUE;
    fmtp_p->annexb = TRUE;
    fmtp_p->annexa = TRUE;
    fmtp_p->profile = SDP_INVALID_VALUE;
    fmtp_p->level = SDP_INVALID_VALUE;
    fmtp_p->flag = 0;
    fmtp_p->par_width = SDP_DEFAULT_PAR_WIDTH;
    fmtp_p->par_height = SDP_DEFAULT_PAR_HEIGHT;
    sstrncpy(fmtp_p->cpcf, SDP_DEFAULT_CPCF_VAL, sizeof(fmtp_p->cpcf));
 
    for (i=0; i < 5; i++) {
        fmtp_p->video_pic_size[i].type = SDP_VIDEO_PIC_INVALID_VALUE;
        fmtp_p->video_pic_size[i].value = 0;
    }

    /* BEGIN - a typical macro fn to replace '/' with ';' from fmtp line*/
    /* This ugly replacement of '/' with ';' is only done because 
    *  econf/MS client sends in this wierd /illegal format. 
    * fmtp parameters MUST be  separated by ';' 
    */
    temp_ptr = strdup(ptr);
    if (temp_ptr == NULL) {
        return (SDP_FAILURE);
    }
    fmtp_ptr = src_ptr = temp_ptr;
    while (flag == FALSE) {
        if (*src_ptr == '\n' || *src_ptr == '\0') {
            flag = TRUE;
            break;
        }
        if (*src_ptr == '/') {
            *src_ptr =';' ;
        }
        src_ptr++;
    }
    /* END */
    /* Once we move to RFC compliant video codec implementations, the above
    *  patch should be removed */
    while (!done) {
      fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "= ;\t", &result1);
      if (result1 == SDP_SUCCESS) {
        if (strncasecmp(tmp, sdp_fmtp_codec_param[1].name,
	                sdp_fmtp_codec_param[1].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr  = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No annexb value specified for "
                                  "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
            tok = tmp;
	    tok++;
	    if (strncasecmp(tok,sdp_fmtp_codec_param_val[0].name,
	                    sdp_fmtp_codec_param_val[0].strlen) == 0) {
	        fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->annexb_required = TRUE;
	        fmtp_p->annexb = TRUE;
	    } else if (strncasecmp(tok,sdp_fmtp_codec_param_val[1].name,
	                           sdp_fmtp_codec_param_val[1].strlen) == 0) {
	        fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->annexb_required = TRUE;
	        fmtp_p->annexb = FALSE;
	    } else {
	        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invalid annexb value specified for "
                             "fmtp attribute. Valid values are yes/no", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);
	    
	    } 
	    codec_info_found = TRUE;
	
	} else if (strncasecmp(tmp, sdp_fmtp_codec_param[0].name,
	                       sdp_fmtp_codec_param[0].strlen) == 0) {
			
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No annexa value specified for "
                                  "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
	    if (strncasecmp(tok,sdp_fmtp_codec_param_val[0].name,
	                    sdp_fmtp_codec_param_val[0].strlen) == 0) {
	        fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	        fmtp_p->annexa = TRUE;
		fmtp_p->annexa_required = TRUE;
	    } else if (strncasecmp(tok,sdp_fmtp_codec_param_val[1].name,
	                           sdp_fmtp_codec_param_val[1].strlen) == 0) {
	        fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	        fmtp_p->annexa = FALSE;
		fmtp_p->annexa_required = TRUE;
	    } else {
	        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invalid annexa value specified for "
                             "fmtp attribute.Valid values = yes/no", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);
	    
	    } 
	    codec_info_found = TRUE;
	    
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[2].name,
                               sdp_fmtp_codec_param[2].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No bitrate value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
            }	  
            tok = tmp;
            tok++;
            bitrate = atoi(tok);
            if (bitrate <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild bitrate specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
            }
            
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->bitrate = bitrate;   
            codec_info_found = TRUE;
            
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[3].name,
                               sdp_fmtp_codec_param[3].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
               fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
                if (result1 != SDP_SUCCESS) {  
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No qcif value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
                } 
            }
            tok = tmp;
	    tok++; 
            val = atoi(tok);
            if ((val < SDP_MIN_CIF_VALUE) || ( val > SDP_MAX_CIF_VALUE)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild qcif: %u specified for "
                             "fmtp attribute.", sdp_p->debug_str,val);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
            sdp_set_video_pic_val(fmtp_p, SDP_VIDEO_PIC_SIZE_QCIF, val);
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    codec_info_found = TRUE;
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[4].name,
                               sdp_fmtp_codec_param[4].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No cif value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
                }
            } 
            tok = tmp;
            tok++;
            val = atoi(tok);
            if ((val < SDP_MIN_CIF_VALUE) || ( val > SDP_MAX_CIF_VALUE)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild cif: %u specified for "
                             "fmtp attribute.", sdp_p->debug_str,val);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
            sdp_set_video_pic_val(fmtp_p, SDP_VIDEO_PIC_SIZE_CIF, val);
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    codec_info_found = TRUE;
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[5].name,
                               sdp_fmtp_codec_param[5].strlen) == 0) {
	    val = sdp_getnextnumtok(fmtp_ptr, &fmtp_ptr, "= ;\t", &result1);
	    if (result1 != SDP_SUCCESS) {
		if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: No maxbr value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
		sdp_p->conf_p->num_invalid_param++;
                continue;
	    }
	    if (val <= 0 || val > SDP_MAX_BR) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild maxbr specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                continue;
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    fmtp_p->maxbr = val;   
	    codec_info_found = TRUE;
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[6].name,
                               sdp_fmtp_codec_param[6].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No sqcif value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            val = atoi(tok);
            if ((val < SDP_MIN_CIF_VALUE) || ( val > SDP_MAX_CIF_VALUE)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild sqcif specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);    
	    }
            sdp_set_video_pic_val(fmtp_p, SDP_VIDEO_PIC_SIZE_SQCIF, val);
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    codec_info_found = TRUE;
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[7].name,
                               sdp_fmtp_codec_param[7].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No cif4 value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            val = atoi(tok);
            if ((val < SDP_MIN_CIF_VALUE) || ( val > SDP_MAX_CIF_VALUE)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild cif4 specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
            sdp_set_video_pic_val(fmtp_p, SDP_VIDEO_PIC_SIZE_CIF4, val);
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    codec_info_found = TRUE;
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[8].name,
                               sdp_fmtp_codec_param[8].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No cif16 value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            val = atoi(tok);
            if ((val < SDP_MIN_CIF_VALUE) || (val > SDP_MAX_CIF_VALUE)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild cif16 specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
            sdp_set_video_pic_val(fmtp_p, SDP_VIDEO_PIC_SIZE_CIF16, val);
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    codec_info_found = TRUE;
        } else  if (strncasecmp(tmp,sdp_fmtp_codec_param[9].name,
                               sdp_fmtp_codec_param[9].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No CUSTOM value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    ptr2 = tmp;
	    ptr2++;
	    custom_x = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                         ", \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	       if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: Invalid xmax specified for "
                         "fmtp attribute custom parameter", sdp_p->debug_str);
               }
	       sdp_p->conf_p->num_invalid_param++;
	    }
	    if (*ptr2 == ',') {
	        custom_y = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                              ", \t", &result1);
		if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                       SDP_WARN("%s Warning: Invalid ymax specified for "
                            "fmtp attribute custom parameter", sdp_p->debug_str);
                    }
		    sdp_p->conf_p->num_invalid_param++;
		}
		if (*ptr2 == ',') {
		   custom_mpi = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                                  ", \t", &result1);
		   if (result1 != SDP_SUCCESS) {
		      if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                          SDP_WARN("%s Warning: Invalid mpi specified for "
                            "fmtp attribute custom parameter", sdp_p->debug_str);
                       }
		       sdp_p->conf_p->num_invalid_param++;
		    }
		} else {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Invalid syntax comma missing after ymax " 
		          " value fmtp attribute custom parameter", 
			  sdp_p->debug_str);
                     }
		     sdp_p->conf_p->num_invalid_param++;
		}
	    } else {
	       if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                 SDP_WARN("%s Invalid syntax comma missing after xmax " 
		          " value fmtp attribute custom parameter", 
			  sdp_p->debug_str);
               }
	       sdp_p->conf_p->num_invalid_param++;
	    }
	    
	    if ((custom_x % 4) > 0) {
	       if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild CUSTOM x value specified for "
                             "fmtp attribute. : Should be divisible by 4", 
			     sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		
	    }
	    
	    if ((custom_y % 4) > 0) {
	       if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild CUSTOM y value specified for "
                             "fmtp attribute. : Should be divisible by 4", 
			     sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		  
	    }
	    
	    if ((custom_mpi < SDP_MIN_CIF_VALUE) || 
	        (custom_mpi > SDP_MAX_CIF_VALUE)) {
	       if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild CUSTOM mpi value specified for "
                             "fmtp attribute. : Should be in range 1 and 32", 
			     sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		  
	    }
        
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    fmtp_p->custom_x = custom_x;
            fmtp_p->custom_y = custom_y;
            fmtp_p->custom_mpi = custom_mpi;
	    codec_info_found = TRUE;
        } else  if (strncasecmp(tmp,sdp_fmtp_codec_param[10].name,
                               sdp_fmtp_codec_param[10].strlen) == 0) {
            par_width = sdp_getnextnumtok(fmtp_ptr, &fmtp_ptr, "=: \t", &result1);
	    if (result1 != SDP_SUCCESS) {
		if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: No PAR width value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
		sdp_p->conf_p->num_invalid_param++;
                continue;
	    }

            par_height = sdp_getnextnumtok(fmtp_ptr, &fmtp_ptr, ": ;\t", &result1);
            if (result1 != SDP_SUCCESS) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: No PAR height value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                continue;
            }

	    if ((par_width <= 0) || (par_height <=0 )) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild par (height or width) values specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    fmtp_p->par_width = par_width;
            fmtp_p->par_height = par_height;
	    codec_info_found = TRUE;
        } else  if (strncasecmp(tmp,sdp_fmtp_codec_param[11].name,
                               sdp_fmtp_codec_param[11].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; =\t", &result1);
	    if (result1 != SDP_SUCCESS) {
		if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: No CPCF value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
		sdp_p->conf_p->num_invalid_param++;
                continue;
	    }
	    if (!(sdp_validate_floating_point_string(tmp))) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild CPCF value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                continue;
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
	    sstrncpy(fmtp_p->cpcf, tmp, sizeof(fmtp_p->cpcf));
	    codec_info_found = TRUE;

        } else  if (strncasecmp(tmp,sdp_fmtp_codec_param[12].name,
                               sdp_fmtp_codec_param[12].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No BPP value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    ptr2 = tmp;
	    ptr2++;
	    temp_val  = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                     "; \t", &result1);
	    if ((((int) temp_val < 0) || (temp_val > SDP_MAX_BPP_HRD_VALUE)) || 
	        (result1 != SDP_SUCCESS)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild BPP value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->bpp = temp_val;
	    codec_info_found = TRUE;
        } else  if (strncasecmp(tmp,sdp_fmtp_codec_param[13].name,
                               sdp_fmtp_codec_param[13].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No HRD value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    ptr2 = tmp;
	    ptr2++;
	    temp_val = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                     "; \t", &result1);
	    if ((((int) temp_val < 0) || (temp_val > SDP_MAX_BPP_HRD_VALUE)) || 
	        (result1 != SDP_SUCCESS)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild HRD value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->hrd = temp_val;
	    codec_info_found = TRUE;
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[14].name,
                               sdp_fmtp_codec_param[14].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No PROFILE value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            profile = atoi(tok);
	    if (profile < SDP_MIN_PROFILE_LEVEL_VALUE || 
                  profile > SDP_MAX_PROFILE_VALUE) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild PROFILE value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->profile = profile;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[15].name,
                               sdp_fmtp_codec_param[15].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No LEVEL value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            level = atoi(tok);
	    if (level < SDP_MIN_PROFILE_LEVEL_VALUE || 
                  level > SDP_MAX_LEVEL_VALUE) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild LEVEL value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->level = level;
	    codec_info_found = TRUE; 
        } if (strncasecmp(tmp,sdp_fmtp_codec_param[16].name,
                               sdp_fmtp_codec_param[16].strlen) == 0) {
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->is_interlace = TRUE;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[17].name,
                               sdp_fmtp_codec_param[17].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No profile-level-id value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
  	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            sstrncpy(fmtp_p->profile_level_id , tok, sizeof(fmtp_p->profile_level_id));
            fmtp_p->flag |= SDP_PROFILE_LEVEL_ID_FLAG;
	    codec_info_found = TRUE;
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[18].name,
                               sdp_fmtp_codec_param[18].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No parameter-sets value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
  	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            sstrncpy(fmtp_p->parameter_sets , tok, sizeof(fmtp_p->parameter_sets));
            fmtp_p->flag |= SDP_SPROP_PARAMETER_SETS_FLAG;
	    codec_info_found = TRUE;
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[19].name,
                               sdp_fmtp_codec_param[19].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No packetization mode value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            pack_mode = atoi(tok);
	    if (pack_mode < 0 || pack_mode > 2) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild Pack mode value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
	    } else {
		fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->packetization_mode = pack_mode;
                fmtp_p->flag |= SDP_PACKETIZATION_MODE_FLAG;
		codec_info_found = TRUE;
            }
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[20].name,
                               sdp_fmtp_codec_param[20].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No interleaving depth value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            val = atoi(tok);
	    if (val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild interleaving depth value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->interleaving_depth = val;
            fmtp_p->flag |= SDP_SPROP_INTERLEAVING_DEPTH_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[21].name,
                               sdp_fmtp_codec_param[21].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: deint buf req value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
		fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->deint_buf_req = (u32) l_val;
                fmtp_p->flag |= SDP_DEINT_BUF_REQ_FLAG;
		codec_info_found = TRUE; 
            } else {
	        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: Invaild  deint_buf_req value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
            }
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[22].name,
                               sdp_fmtp_codec_param[22].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No max_don_diff value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            temp_val = atoi(tok);
	    if (temp_val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild max_don_diff value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_don_diff = temp_val;
	    fmtp_p->flag |= SDP_SPROP_MAX_DON_DIFF_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[23].name,
                               sdp_fmtp_codec_param[23].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No init_buf_time value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
		fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->init_buf_time = (u32) l_val;
                fmtp_p->flag |= SDP_INIT_BUF_TIME_FLAG;
		codec_info_found = TRUE; 
            } else {
	        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: Invaild  init_buf_time value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
            }
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[24].name,
                               sdp_fmtp_codec_param[24].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No mx-mbps value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            temp_val = atoi(tok);
	    if (temp_val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild max_mbps value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_mbps = temp_val;
	    fmtp_p->flag |= SDP_MAX_MBPS_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[25].name,
                               sdp_fmtp_codec_param[25].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No max_fs value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            temp_val = atoi(tok);
	    if (temp_val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild max_fs value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_fs = temp_val;
	    fmtp_p->flag |= SDP_MAX_FS_FLAG;
            codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[26].name,
                               sdp_fmtp_codec_param[26].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No max_cpb value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            temp_val = atoi(tok);
	    if (temp_val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild max_cpb value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_cpb = temp_val;
	    fmtp_p->flag |= SDP_MAX_CPB_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[27].name,
                               sdp_fmtp_codec_param[27].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No max dpb value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            temp_val = atoi(tok);
	    if (temp_val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild max_dpb value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_dpb = temp_val;
	    fmtp_p->flag |= SDP_MAX_DPB_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[28].name,
                               sdp_fmtp_codec_param[28].strlen) == 0) {
            temp_val = sdp_getnextnumtok(fmtp_ptr, &fmtp_ptr, "=; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
		if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: No max_br value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
		sdp_p->conf_p->num_invalid_param++;
                continue;
	    }
	    if (temp_val <= 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild max_br value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                continue;
	    }
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_br = temp_val;
	    fmtp_p->flag |= SDP_MAX_BR_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[29].name,
                               sdp_fmtp_codec_param[29].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No red pic_cap value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            val = atoi(tok);
	    if (val > 1) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild redundant-pic-cap value specified " 
                             "for fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_PARAMETER);    
	    }

	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            if (val == 1) {
                fmtp_p->redundant_pic_cap = TRUE;
            } else { 
                fmtp_p->redundant_pic_cap = FALSE;
            }
            fmtp_p->flag |= SDP_REDUNDANT_PIC_CAP_FLAG;
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[30].name,
                               sdp_fmtp_codec_param[30].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No deint_buf_cap value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
		fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->deint_buf_cap = (u32) l_val;
                fmtp_p->flag |= SDP_DEINT_BUF_CAP_FLAG;
		codec_info_found = TRUE; 
            } else {
	        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: Invaild deint_buf_cap value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
            }
        }  else if (strncasecmp(tmp,sdp_fmtp_codec_param[31].name,
                               sdp_fmtp_codec_param[31].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No max_rcmd_nalu_size value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
		fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
		fmtp_p->max_rcmd_nalu_size = (u32) l_val;
                fmtp_p->flag |= SDP_MAX_RCMD_NALU_SIZE_FLAG;
		codec_info_found = TRUE; 
            } else {
	        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: Invaild  max_rcmd_nalu_size value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
            }
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[32].name,
                               sdp_fmtp_codec_param[32].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
	        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
	        if (result1 != SDP_SUCCESS) {
		    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No parameter add value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
		    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_PARAMETER);
		}
	    } 
	    tok = tmp;
	    tok++;
            parameter_add = atoi(tok);
            if (parameter_add < 0 || parameter_add > 1) {
		if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
		    SDP_WARN("%s Warning: Invalid parameter add value specified for "
			     "fmtp attribute.", sdp_p->debug_str);
		}
            } else {
		fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

		if (parameter_add == 1) {
		    fmtp_p->parameter_add = TRUE;
		} else {
		    fmtp_p->parameter_add = FALSE;
		}
                fmtp_p->flag |= SDP_PARAMETER_ADD_FLAG;
            }
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[33].name,
                               sdp_fmtp_codec_param[33].strlen) == 0) {
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_d = TRUE;                
	    codec_info_found = TRUE;
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[34].name,
                               sdp_fmtp_codec_param[34].strlen) == 0) {
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_f = TRUE;                
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[35].name,
                               sdp_fmtp_codec_param[35].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_i = TRUE;                
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[36].name,
                               sdp_fmtp_codec_param[36].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_j = TRUE;                
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[37].name,
                               sdp_fmtp_codec_param[36].strlen) == 0) {
	    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_t = TRUE;                
	    codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[38].name,
                             sdp_fmtp_codec_param[38].strlen) == 0) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
                    if (result1 != SDP_SUCCESS) {
                        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                            SDP_WARN("%s Warning: No Annex K value specified for "
                                     "fmtp attribute.", sdp_p->debug_str);
                        }
                        sdp_p->conf_p->num_invalid_param++;
			continue;
                    }
                } 
                tok = tmp;
                tok++;
                annex_k_val = atoi(tok);
                if ((annex_k_val < SDP_MIN_ANNEX_K_VALUE) ||
                    (annex_k_val > SDP_MAX_ANNEX_K_VALUE)) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: Invaild annex K value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    continue;
                }
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->annex_k_val = annex_k_val;
                codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[39].name,
                               sdp_fmtp_codec_param[39].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No Annex N value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    continue;
                }
            }  
            tok = tmp;
            tok++;
            annex_n_val = atoi(tok);
            if ((annex_n_val < SDP_MIN_ANNEX_N_VALUE) ||
                (annex_n_val > SDP_MAX_ANNEX_N_VALUE)) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild annex N value specified for "
                             "fmtp attribute.", sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                continue;
            } 
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_n_val = annex_n_val;
            codec_info_found = TRUE; 
        } else if (strncasecmp(tmp,sdp_fmtp_codec_param[40].name,
                               sdp_fmtp_codec_param[40].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No Annex P value specified for "
                                 "fmtp attribute.", sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    continue;
                }
            } 
            fmtp_p->annex_p_val_picture_resize = 0;
            fmtp_p->annex_p_val_warp = 0;
            tok = tmp;
            tok++; 
            temp=strtok(tok,",");
            iter=0;
            while (temp != NULL) {
                if (iter == 0) {
                    if (strlen(temp) != 1) {
                        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                            SDP_WARN("\n%s Invalid Annex P pic_resize_value specified for "
                                     "fmtp attribute.\n", sdp_p->debug_str);
                        }
                        sdp_p->conf_p->num_invalid_param++;
                        break;
                    }
                           
                    annex_p_val = atoi(temp);
                    if ((annex_p_val < SDP_MIN_ANNEX_P_PIC_RESIZE_VALUE) ||
                        (annex_p_val > SDP_MAX_ANNEX_P_PIC_RESIZE_VALUE)) {
                        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                            SDP_WARN("\n%s Invalid Annex P pic_resize_value specified for "
                                     "fmtp attribute.\n", sdp_p->debug_str);
                        }
                        sdp_p->conf_p->num_invalid_param++;
                        break;                                
                    } else {
                        fmtp_p->annex_p_val_picture_resize = annex_p_val;
                    }
                } 
                if (iter == 1) {
                    if (strlen(temp) != 1) {
                        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                            SDP_WARN("\n%s Invalid Annex P warp_value specified for "
                                     "fmtp attribute.\n", sdp_p->debug_str);
                        }
                        sdp_p->conf_p->num_invalid_param++;
                    }

                    annex_p_val = atoi(temp);
                    if ((annex_p_val < SDP_MIN_ANNEX_P_WARP_VALUE) ||
                        (annex_p_val > SDP_MAX_ANNEX_P_WARP_VALUE)) {
                        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                            SDP_WARN("\n%s Invalid Annex P warp_value specified for "
                                     "fmtp attribute.\n", sdp_p->debug_str);
                        } 
                        sdp_p->conf_p->num_invalid_param++;
                        break;
                    } else {
                        fmtp_p->annex_p_val_warp = annex_p_val; 
                    }
                }
                temp=strtok(NULL,",");
                iter++;
                if ((iter == 2) && temp) {
                    /*
                     * The input is P=1,3,2. Give a warning.
                     */
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("\n%s Invalid Annex P value specified for "
                                 "fmtp attribute.\n", sdp_p->debug_str);
                        sdp_p->conf_p->num_invalid_param++;
                        break;
                    }
                }
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            codec_info_found = TRUE; 
	} else if (strncasecmp(tmp,sdp_fmtp_codec_param[41].name,
                               sdp_fmtp_codec_param[41].strlen) == 0) {
	    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "; \t", &result1);
	    if (result1 != SDP_SUCCESS) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: No rtcp per receiver "
                             "bandwidth value specified for fmtp "
                             "attribute.",
                             sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
                SDP_FREE(temp_ptr);
                return (SDP_INVALID_RTCP_PER_RCVR_BW_LINE);
            }
            else {
                rtcp_per_rcvr_bw = sdp_getnextnumtok(tmp, 
                                                     (const char **)&tmp, 
                                                     " =", &result1);
                if (result1 != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No rtcp per receiver "
                                 "bandwidth value specified for fmtp "
                                 "attribute.",
                                 sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_RTCP_PER_RCVR_BW_LINE);
		}
            }

            if ((int) rtcp_per_rcvr_bw < 0) {
                if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                    SDP_WARN("%s Warning: Invaild rtcp per receiver bandwidth "
                             "specified for fmtp attribute.",
                             sdp_p->debug_str);
                }
                sdp_p->conf_p->num_invalid_param++;
		SDP_FREE(temp_ptr);
                return (SDP_INVALID_RTCP_PER_RCVR_BW_LINE);    
            }
            
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->rtcp_per_rcvr_bw = rtcp_per_rcvr_bw;
            codec_info_found = TRUE;
            
        } else if (*fmtp_ptr == '\n') { 
            temp=strtok(tmp,";");
            if (temp) {
                if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                    SDP_PRINT("\n%s Annexes are possibly there for this fmtp %s  tmp: %s line\n", 
                              sdp_p->debug_str, fmtp_ptr, tmp);
                }
                while (temp != NULL) {
                    if (strchr(temp, 'D') !=NULL) {
                        attr_p->attr.fmtp.annex_d = TRUE;
                    } 
                    if (strchr(temp, 'F') !=NULL) {
                        attr_p->attr.fmtp.annex_f = TRUE;
                    } 
                    if (strchr(temp, 'I') !=NULL) {
                        attr_p->attr.fmtp.annex_i = TRUE;
                    } 
                    if (strchr(temp, 'J') !=NULL) {
                        attr_p->attr.fmtp.annex_j = TRUE;
                    } 
                    if (strchr(temp, 'T') !=NULL) {
                        attr_p->attr.fmtp.annex_t = TRUE;
                    } 
                    temp=strtok(NULL,";");
                }
            } /* if (temp) */         
            done = TRUE;
        }
        fmtp_ptr++;
      } else {
          done = TRUE;
      }
    } /* while  - done loop*/

    if (codec_info_found) {
        
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, payload type %u, bitrate %lu, MAXBR=%u, CUSTOM=%u,%u,%u , PAR=%u:%u,CPCF=%s, BPP=%lu, HRD=%lu \n", 
                      sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.fmtp.payload_num,
                      attr_p->attr.fmtp.bitrate,
                      attr_p->attr.fmtp.maxbr,
                      attr_p->attr.fmtp.custom_x,attr_p->attr.fmtp.custom_y, 
                      attr_p->attr.fmtp.custom_mpi,
                      attr_p->attr.fmtp.par_width,
                      attr_p->attr.fmtp.par_height,
                      attr_p->attr.fmtp.cpcf,
                      attr_p->attr.fmtp.bpp,
                      attr_p->attr.fmtp.hrd
		      );
        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, payload type %u,PROFILE=%u,LEVEL=%u, INTERLACE - %s", 
                      sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.fmtp.payload_num,
                      attr_p->attr.fmtp.profile,
                      attr_p->attr.fmtp.level,
                      attr_p->attr.fmtp.is_interlace ? "YES":"NO");
        }	              

	if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed H.264 attributes: profile-level-id=%s, parameter-sets=%s, packetization-mode=%d interleaving-depth=%d deint-buf-req=%lu max-don-diff=%lu, init_buf-time=%lu\n",
                      sdp_p->debug_str,
                      attr_p->attr.fmtp.profile_level_id,
                      attr_p->attr.fmtp.parameter_sets,
                      attr_p->attr.fmtp.packetization_mode,
                      attr_p->attr.fmtp.interleaving_depth,
                      attr_p->attr.fmtp.deint_buf_req,
                      attr_p->attr.fmtp.max_don_diff,
                      attr_p->attr.fmtp.init_buf_time
		      );
        }

	if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("\n%s Parsed H.264 opt attributes: max-mbps=%lu, max-fs=%lu, max-cpb=%lu max-dpb=%lu max-br=%lu redundant-pic-cap=%d, deint-buf-cap=%lu, max-rcmd-nalu-size=%lu , parameter-add=%d\n",
                      sdp_p->debug_str,
                      attr_p->attr.fmtp.max_mbps,
                      attr_p->attr.fmtp.max_fs,
                      attr_p->attr.fmtp.max_cpb,
                      attr_p->attr.fmtp.max_dpb,
                      attr_p->attr.fmtp.max_br,
                      attr_p->attr.fmtp.redundant_pic_cap,
                      attr_p->attr.fmtp.deint_buf_cap,
                      attr_p->attr.fmtp.max_rcmd_nalu_size,
                      attr_p->attr.fmtp.parameter_add);

        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed annexes are : D=%d F=%d I=%d J=%d T=%d, K=%d N=%d P=%d,%d\n",
                      sdp_p->debug_str,
                      attr_p->attr.fmtp.annex_d,
                      attr_p->attr.fmtp.annex_f,  attr_p->attr.fmtp.annex_i, 
                      attr_p->attr.fmtp.annex_j,  attr_p->attr.fmtp.annex_t,  
                      attr_p->attr.fmtp.annex_k_val,  
		      attr_p->attr.fmtp.annex_n_val,  
                      attr_p->attr.fmtp.annex_p_val_picture_resize,
                      attr_p->attr.fmtp.annex_p_val_warp);

        }

	SDP_FREE(temp_ptr);
        return (SDP_SUCCESS);
    } else {
        done = FALSE;
	fmtp_ptr = temp_ptr;
        tmp[0] = '\0';
    }
    
    /*  Check for retransmission param */
    attr_p->attr.fmtp.rtx_time = 0; /* default rtx-time to 0 */
    attr_p->attr.fmtp.apt = 0; /* default apt to 0 */
    while (!done) {
        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "=; \t", &result1);
        if (result1 == SDP_SUCCESS) {
            if (strncasecmp(tmp,sdp_fmtp_rtp_retrans_param[0].name,
                            sdp_fmtp_rtp_retrans_param[0].strlen) == 0) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "=; \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No value specified for apt ",
                                 sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    SDP_FREE(temp_ptr);                    
                    return (SDP_INVALID_FMTP_RTX_LINE);
                }
                attr_p->attr.fmtp.apt = atoi(tmp);
                fmtp_p->fmtp_format = SDP_FMTP_RTP_RETRANS;
                
            } else if (strncasecmp(tmp,sdp_fmtp_rtp_retrans_param[1].name,
                                   sdp_fmtp_rtp_retrans_param[1].strlen) == 0) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, "=; \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: No value specified for rtx-time ",
                                 sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    SDP_FREE(temp_ptr);
                    return (SDP_INVALID_FMTP_RTX_LINE);
                }
                attr_p->attr.fmtp.rtx_time = atoi(tmp);
                fmtp_p->fmtp_format = SDP_FMTP_RTP_RETRANS;
            }
        } else {
            break;
        }
    } /* End While */

    if (fmtp_p->fmtp_format == SDP_FMTP_RTP_RETRANS) {
        SDP_FREE(temp_ptr);
        return (SDP_SUCCESS);
    }
    else {
        done = FALSE;
        fmtp_ptr = temp_ptr;
        tmp[0] = '\0';
    }
    
    for (i=0; !done; i++) {
        fmtp_p->fmtp_format = SDP_FMTP_NTE;
        /* Look for comma separated events */
        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, ", \t", &result1);
        if (result1 != SDP_SUCCESS) {
            done = TRUE;
            continue;
        }
        /* Now look for '-' separated range */
        ptr2 = tmp;
        low_val = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                    "- \t", &result1);
        if (*ptr2 == '-') {
            high_val = sdp_getnextnumtok(ptr2, (const char **)&ptr2, 
                                         "- \t", &result2);
        } else {
            high_val = low_val;
        }

        if ((result1 != SDP_SUCCESS) || (result2 != SDP_SUCCESS)) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: Invalid named events specified for "
                         "fmtp attribute.", sdp_p->debug_str);
            }
            sdp_p->conf_p->num_invalid_param++;
	    SDP_FREE(temp_ptr);
            return (SDP_INVALID_PARAMETER);
        }

        for (iter = low_val; iter <= high_val; iter++) {
            mapword = iter/SDP_NE_BITS_PER_WORD;
            bmap = SDP_NE_BIT_0 << (iter%32);
            fmtp_p->bmap[mapword] |= bmap;
        }
        if (high_val > fmtp_p->maxval) {
            fmtp_p->maxval = high_val;
        }
    }

    if (fmtp_p->maxval == 0) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No named events specified for "
                     "fmtp attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
	SDP_FREE(temp_ptr);
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, payload type %u, maxval = %ld", 
	          sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.fmtp.payload_num,
		  fmtp_p->maxval);
    }
    SDP_FREE(temp_ptr);
    return (SDP_SUCCESS);
}

char* sdp_get_video_pic_str (sdp_video_picture_size type) 
{
    switch (type) {
        case SDP_VIDEO_PIC_SIZE_CIF:
	    return "CIF";
	break;

	case SDP_VIDEO_PIC_SIZE_CIF4:
	    return "CIF4";
	break;

	case SDP_VIDEO_PIC_SIZE_CIF16:
	    return "CIF16";
	break;

	case SDP_VIDEO_PIC_SIZE_QCIF:
	    return "QCIF";
	break;

	case SDP_VIDEO_PIC_SIZE_SQCIF:
	    return "SQCIF";
	break;

	default:
	    return NULL;
	break;
    }
}

sdp_result_e sdp_build_attr_fmtp (sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr,
                                  u16 len)
{
    GCC_UNUSED(sdp_p);
    u16         event_id;
    u32         mask;
    u32         mapword;
    u8          min = 0;
    u8          max = 0;
    tinybool    range_start = FALSE;
    tinybool    range_end = FALSE;
    tinybool    semicolon = FALSE;
    char       *endbuf_p;
    sdp_fmtp_t *fmtp_p;
    int         i = 0;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%u ", sdp_attr[attr_p->type].name,
                     attr_p->attr.fmtp.payload_num);
    len = endbuf_p - *ptr;

    fmtp_p = &(attr_p->attr.fmtp);
    switch (fmtp_p->fmtp_format) {
    
      case SDP_FMTP_CODEC_INFO:
         if (fmtp_p->bitrate > 0) {
	    *ptr += snprintf(*ptr, len, "bitrate=%ld",attr_p->attr.fmtp.bitrate);
            len = endbuf_p - *ptr;
	    semicolon = TRUE;
	 }
	 if (fmtp_p->annexa_required) {
	    if (fmtp_p->annexa) {
	        if (semicolon) {
	            *ptr += snprintf(*ptr, len, ";annexa=yes");
                    len = endbuf_p - *ptr;
	            semicolon = TRUE;
	        } else {
	            *ptr += snprintf(*ptr, len, "annexa=yes");
                    len = endbuf_p - *ptr;
	            semicolon = TRUE;
	        }
	    } else {
	        if (semicolon) {
	            *ptr += snprintf(*ptr, len, ";annexa=no");;
                    len = endbuf_p - *ptr;
	            semicolon = TRUE;
	        } else {
	            *ptr += snprintf(*ptr, len, "annexa=no");
                    len = endbuf_p - *ptr;
	            semicolon = TRUE;
	        }
	    }
	
	 }
	
	 if (fmtp_p->annexb_required) {
	    if (fmtp_p->annexb) {
	        if (semicolon) {
	            *ptr += snprintf(*ptr, len, ";annexb=yes");
                    len = endbuf_p - *ptr;
	            semicolon = TRUE;
	        } else {
	           *ptr += snprintf(*ptr, len, "annexb=yes");
                   len = endbuf_p - *ptr;
	           semicolon = TRUE;
	        }
	   } else {
	       if (semicolon) {
	            *ptr += snprintf(*ptr, len, ";annexb=no");
                    len = endbuf_p - *ptr;
	            semicolon = TRUE;
	       } else {
	           *ptr += snprintf(*ptr, len, "annexb=no");
                   len = endbuf_p - *ptr;
	           semicolon = TRUE;
	      }
	   }
	 }

         for (i=0; i < 5; i++) {
	     if (attr_p->attr.fmtp.video_pic_size[i].value > 0) {
		if (semicolon) {
		    *ptr += snprintf(*ptr, len, ";%s=%u",
                       sdp_get_video_pic_str(
                          attr_p->attr.fmtp.video_pic_size[i].type),
		       attr_p->attr.fmtp.video_pic_size[i].value);
		     len = endbuf_p - *ptr;
		     semicolon = TRUE;
		} else {
		    *ptr += snprintf(*ptr, len, "%s=%u",
                           sdp_get_video_pic_str(
                              attr_p->attr.fmtp.video_pic_size[i].type),
			   attr_p->attr.fmtp.video_pic_size[i].value);
		     len = endbuf_p - *ptr;
		     semicolon = TRUE;
		}
	     }
         }	
         
         if (fmtp_p->maxbr > 0) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";MAXBR=%u",attr_p->attr.fmtp.maxbr);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "MAXBR=%u",attr_p->attr.fmtp.maxbr);
                len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    }
	 }
         
         if ((fmtp_p->custom_x > 0) && (fmtp_p->custom_y > 0) && 
	     (fmtp_p->custom_mpi > 0)) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";CUSTOM=%u,%u,%u", attr_p->attr.fmtp.custom_x, attr_p->attr.fmtp.custom_y, attr_p->attr.fmtp.custom_mpi);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "CUSTOM=%u,%u,%u", attr_p->attr.fmtp.custom_x, attr_p->attr.fmtp.custom_y, attr_p->attr.fmtp.custom_mpi);
                len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    }
	 }

         if ((fmtp_p->par_height > 0) && (fmtp_p->par_width > 0) &&
              !((fmtp_p->par_height == SDP_DEFAULT_PAR_HEIGHT) && 
                (fmtp_p->par_width == SDP_DEFAULT_PAR_WIDTH))) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";PAR=%u:%u", attr_p->attr.fmtp.par_width, 
                                 attr_p->attr.fmtp.par_height);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "PAR=%u:%u", attr_p->attr.fmtp.par_width, 
                                 attr_p->attr.fmtp.par_height);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }

         if (strncmp(attr_p->attr.fmtp.cpcf, SDP_DEFAULT_CPCF_VAL, 4) != 0) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";CPCF=%s", attr_p->attr.fmtp.cpcf);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "CPCF=%s", attr_p->attr.fmtp.cpcf);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }

         if (fmtp_p->bpp > 0) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";BPP=%ld", attr_p->attr.fmtp.bpp);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "BPP=%ld", attr_p->attr.fmtp.bpp);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }

         if (fmtp_p->hrd > 0) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";HRD=%ld", attr_p->attr.fmtp.hrd);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "HRD=%ld", attr_p->attr.fmtp.hrd);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }

         if (fmtp_p->profile >= 0) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";PROFILE=%d", 
                                 attr_p->attr.fmtp.profile);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "PROFILE=%d", 
                                 attr_p->attr.fmtp.profile);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }

         if (fmtp_p->level >= 0) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";LEVEL=%d", 
                                  attr_p->attr.fmtp.level);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "LEVEL=%d", 
                                  attr_p->attr.fmtp.level);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
	 }

         if (fmtp_p->is_interlace) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";INTERLACE");
             } else {
                 *ptr += snprintf(*ptr, len, "INTERLACE");
	     }
             len = endbuf_p - *ptr;
             semicolon = TRUE;
         }

         if (fmtp_p->annex_d) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";D");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "D");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 

         if (fmtp_p->annex_f) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";F");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "F");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 
         if (fmtp_p->annex_i) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";I");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "I");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 
         if (fmtp_p->annex_j) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";J");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "J");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 
         if (fmtp_p->annex_t) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";T");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "T");
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 
         if (fmtp_p->annex_k_val >0) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";K=%u", 
                                  attr_p->attr.fmtp.annex_k_val);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "K=%u", 
                                  attr_p->attr.fmtp.annex_k_val);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 
         if (fmtp_p->annex_n_val >0) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";N=%u", 
                                  attr_p->attr.fmtp.annex_n_val);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "N=%u", 
                                  attr_p->attr.fmtp.annex_n_val);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 
         if ((fmtp_p->annex_p_val_picture_resize > 0) && (fmtp_p->annex_p_val_warp > 0)) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";P=%d,%d", 
                                  attr_p->attr.fmtp.annex_p_val_picture_resize, 
                                  attr_p->attr.fmtp.annex_p_val_warp); 
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "P=%d,%d", 
                                  attr_p->attr.fmtp.annex_p_val_picture_resize,
                                  attr_p->attr.fmtp.annex_p_val_warp); 
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         } 

         if (fmtp_p->profile_level_id[0] != '\0') {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";profile-level-id=%s",
                                  attr_p->attr.fmtp.profile_level_id);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "profile-level-id=%s",
                                  attr_p->attr.fmtp.profile_level_id);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
         }
         
         if (fmtp_p->parameter_sets[0] != '\0') {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";sprop-parameter-sets=%s",
                                  attr_p->attr.fmtp.parameter_sets);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "sprop-parameter-sets=%s",
                                  attr_p->attr.fmtp.parameter_sets);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
	 }
         
         if (fmtp_p->flag & SDP_PACKETIZATION_MODE_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";packetization-mode=%u",
                                 attr_p->attr.fmtp.packetization_mode);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "packetization-mode=%u",
                                 attr_p->attr.fmtp.packetization_mode);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    }
	 }
         if (fmtp_p->flag & SDP_SPROP_INTERLEAVING_DEPTH_FLAG) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";sprop-interleaving-depth=%u",
                                 attr_p->attr.fmtp.interleaving_depth);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "sprop-interleaving-depth=%u",
                                 attr_p->attr.fmtp.interleaving_depth);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
	    }
	 }
         if (fmtp_p->flag & SDP_DEINT_BUF_REQ_FLAG) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";sprop-deint-buf-req=%ld",
                                  attr_p->attr.fmtp.deint_buf_req);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "sprop-deint-buf-req=%ld",
                                  attr_p->attr.fmtp.deint_buf_req);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
	 }
         if (fmtp_p->flag & SDP_SPROP_MAX_DON_DIFF_FLAG) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";sprop-max-don-diff=%ld",
                                  attr_p->attr.fmtp.max_don_diff);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "sprop-max-don-diff=%ld",
                                  attr_p->attr.fmtp.max_don_diff);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
	 }

         if (fmtp_p->flag & SDP_INIT_BUF_TIME_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";sprop-init-buf-time=%ld",
                                 attr_p->attr.fmtp.init_buf_time);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "sprop-init-buf-time=%ld",
                                 attr_p->attr.fmtp.init_buf_time);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_MAX_MBPS_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";max-mbps=%ld",
                                 attr_p->attr.fmtp.max_mbps);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "max-mbps=%ld",
                                 attr_p->attr.fmtp.max_mbps);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_MAX_FS_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";max-fs=%ld",
                                 attr_p->attr.fmtp.max_fs);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "max-fs=%ld",
                                 attr_p->attr.fmtp.max_fs);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_MAX_CPB_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";max-cpb=%ld",
                                 attr_p->attr.fmtp.max_cpb);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "max-cpb=%ld",
                                 attr_p->attr.fmtp.max_cpb);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_MAX_DPB_FLAG) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";max-dpb=%ld",
                                  attr_p->attr.fmtp.max_dpb);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "max-dpb=%ld",
                                  attr_p->attr.fmtp.max_dpb);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_MAX_BR_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";max-br=%ld",
                                 attr_p->attr.fmtp.max_br);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "max-br=%ld",
                                 attr_p->attr.fmtp.max_br);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->redundant_pic_cap > 0) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";redundant-pic-cap=%u",
                                 attr_p->attr.fmtp.redundant_pic_cap);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "redundant-pic-cap=%u",
                                 attr_p->attr.fmtp.redundant_pic_cap);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_DEINT_BUF_CAP_FLAG) {
	    if (semicolon) {
                *ptr += snprintf(*ptr, len, ";deint-buf-cap=%ld",
                                 attr_p->attr.fmtp.deint_buf_cap);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    } else {
                *ptr += snprintf(*ptr, len, "deint-buf-cap=%ld",
                                 attr_p->attr.fmtp.deint_buf_cap);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
	    }
	 }
	 
         if (fmtp_p->flag & SDP_MAX_RCMD_NALU_SIZE_FLAG) {
             if (semicolon) {
                 *ptr += snprintf(*ptr, len, ";max-rcmd-nalu-size=%ld",
                                  attr_p->attr.fmtp.max_rcmd_nalu_size);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "max-rcmd-nalu-size=%ld",
                                  attr_p->attr.fmtp.max_rcmd_nalu_size);
                 len = endbuf_p - *ptr;
                semicolon = TRUE;
             }
	 }
	 
         if (fmtp_p->parameter_add == FALSE) {
             if (semicolon) {
                *ptr += snprintf(*ptr, len, ";parameter-add=%u",
                                 attr_p->attr.fmtp.parameter_add);
                len = endbuf_p - *ptr;
                semicolon = TRUE;
             } else {
                 *ptr += snprintf(*ptr, len, "parameter-add=%u",
                                 attr_p->attr.fmtp.parameter_add);
                 len = endbuf_p - *ptr;
                 semicolon = TRUE;
             }
	 }
     case SDP_FMTP_RTP_RETRANS:
         semicolon = FALSE;
         if (fmtp_p->apt) {
             *ptr += snprintf(*ptr, len, "apt=%u",attr_p->attr.fmtp.apt);
             len = endbuf_p - *ptr;
             semicolon = TRUE;             
         }
         if (fmtp_p->rtx_time) {
             if (semicolon == TRUE) {
                 *ptr += snprintf(*ptr, len, ";rtx-time=%ld",
                                  attr_p->attr.fmtp.rtx_time);
                 len = endbuf_p - *ptr;
             }
             else {
                 *ptr += snprintf(*ptr, len, "rtx-time=%ld",
                                  attr_p->attr.fmtp.rtx_time);
                 len = endbuf_p - *ptr;
             }         
         }
         break;
     case SDP_FMTP_NTE:
      default:
         for(event_id = 0, mapword = 0, mask = SDP_NE_BIT_0;
             event_id <= fmtp_p->maxval;
             event_id++, mapword = event_id/SDP_NE_BITS_PER_WORD ) {

             if (event_id % SDP_NE_BITS_PER_WORD) {
                 mask <<= 1;
             } else {
             /* crossed a bitmap word boundary */
             mask = SDP_NE_BIT_0;
                 if (!range_start && !range_end && !fmtp_p->bmap[mapword]) {
	        /* no events in this word, skip to the last event id
                 * in this bitmap word. */
                    event_id += SDP_NE_BITS_PER_WORD - 1;
                    continue;
                }
             }

            if (fmtp_p->bmap[mapword] & mask) {
                if (!range_start) {
                    range_start = TRUE;
                    min = max = event_id;
                } else {
                    max = event_id;
                }
            range_end = (max == fmtp_p->maxval);
            } else {
            /* If we were in the middle of a range, then we've hit the
             * end.  If we weren't, there is no end to hit. */
                range_end = range_start;
            }

            /* If this is the end of the range, print it to the string. */
            if (range_end) {
                range_start = range_end = FALSE;

                *ptr += snprintf(*ptr, len, "%u", min);
                len = endbuf_p - *ptr;

                if (min != max) {
                    *ptr += snprintf(*ptr, len, "-%u", max);
                    len = endbuf_p - *ptr;
                }

                if (max != fmtp_p->maxval) {
                    *ptr += snprintf(*ptr, len, ",");
                    len = endbuf_p - *ptr;
                }
            }
          }

    }

    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_direction (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                       const char *ptr)
{
    GCC_UNUSED(ptr);
    /* No parameters to parse. */
    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_direction (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                       char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s\r\n", 
                     sdp_get_attr_name(attr_p->type));

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_qos (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the strength tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No qos strength tag specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.qos.strength = SDP_QOS_STRENGTH_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STRENGTH; i++) {
        if (strncasecmp(tmp, sdp_qos_strength[i].name,
                        sdp_qos_strength[i].strlen) == 0) {
            attr_p->attr.qos.strength = (sdp_qos_strength_e)i;
        }
    }
    if (attr_p->attr.qos.strength == SDP_QOS_STRENGTH_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: QOS strength tag unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No qos direction specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.qos.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.qos.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.qos.direction == SDP_QOS_DIR_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: QOS direction unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* See if confirm was specified.  Defaults to FALSE. */
    attr_p->attr.qos.confirm = FALSE;
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result == SDP_SUCCESS) {
        if (strncasecmp(tmp, "confirm", sizeof("confirm")) == 0) {
            attr_p->attr.qos.confirm = TRUE;
        }
        if (attr_p->attr.qos.confirm == FALSE) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: QOS confirm parameter invalid (%s)",
                         sdp_p->debug_str, tmp);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, strength %s, direction %s, confirm %s", 
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_qos_strength_name(attr_p->attr.qos.strength),
                  sdp_get_qos_direction_name(attr_p->attr.qos.direction),
                  (attr_p->attr.qos.confirm ? "set" : "not set"));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_qos (sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr,
                                 u16 len)
{
    GCC_UNUSED(sdp_p);
    char       *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s %s", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_qos_strength_name(attr_p->attr.qos.strength),
                     sdp_get_qos_direction_name(attr_p->attr.qos.direction));
    len = endbuf_p - *ptr;
    if (attr_p->attr.qos.confirm == TRUE) {
        *ptr += snprintf(*ptr, len, " confirm\r\n");
    } else {
        *ptr += snprintf(*ptr, len, "\r\n");
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_curr (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];
   
    /* Find the curr type tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No curr attr type specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.curr.type = SDP_CURR_UNKNOWN_TYPE;
    for (i=0; i < SDP_MAX_CURR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_curr_type[i].name,
                        sdp_curr_type[i].strlen) == 0) {
            attr_p->attr.curr.type = (sdp_curr_type_e)i;
        }
    }
    
    if (attr_p->attr.curr.type != SDP_CURR_QOS_TYPE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Unknown curr type.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);   
    }
    
    /* Check qos status type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
     if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No curr attr type specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.curr.status_type = SDP_QOS_STATUS_TYPE_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STATUS_TYPES; i++) {
        if (strncasecmp(tmp, sdp_qos_status_type[i].name,
                        sdp_qos_status_type[i].strlen) == 0) {
            attr_p->attr.curr.status_type = (sdp_qos_status_types_e)i;
        }
    }
    

    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No qos direction specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.curr.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.curr.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.curr.direction == SDP_QOS_DIR_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: QOS direction unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, type %s status type %s, direction %s", 
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_curr_type_name(attr_p->attr.curr.type),
                  sdp_get_qos_status_type_name(attr_p->attr.curr.status_type),
                  sdp_get_qos_direction_name(attr_p->attr.curr.direction));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_curr (sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr,
                                 u16 len)
{
    GCC_UNUSED(sdp_p);
    char       *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s %s %s", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_curr_type_name(attr_p->attr.curr.type),
                     sdp_get_qos_status_type_name(attr_p->attr.curr.status_type),
                     sdp_get_qos_direction_name(attr_p->attr.curr.direction));
    len = endbuf_p - *ptr;
    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_des (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];
   
    /* Find the curr type tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No des attr type specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.type = SDP_DES_UNKNOWN_TYPE;
    for (i=0; i < SDP_MAX_CURR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_des_type[i].name,
                        sdp_des_type[i].strlen) == 0) {
            attr_p->attr.des.type = (sdp_des_type_e)i;
        }
    }
    
    if (attr_p->attr.des.type != SDP_DES_QOS_TYPE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Unknown conf type.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);   
    }
    
    /* Find the strength tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No qos strength tag specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.strength = SDP_QOS_STRENGTH_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STRENGTH; i++) {
        if (strncasecmp(tmp, sdp_qos_strength[i].name,
                        sdp_qos_strength[i].strlen) == 0) {
            attr_p->attr.des.strength = (sdp_qos_strength_e)i;
        }
    }
    if (attr_p->attr.des.strength == SDP_QOS_STRENGTH_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: QOS strength tag unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    
    /* Check qos status type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
     if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No des attr type specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.status_type = SDP_QOS_STATUS_TYPE_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STATUS_TYPES; i++) {
        if (strncasecmp(tmp, sdp_qos_status_type[i].name,
                        sdp_qos_status_type[i].strlen) == 0) {
            attr_p->attr.des.status_type = (sdp_qos_status_types_e)i;
        }
    }
    

    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No qos direction specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.des.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.des.direction == SDP_QOS_DIR_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: QOS direction unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, type %s strength %s status type %s, direction %s", 
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_des_type_name(attr_p->attr.des.type),
                  sdp_get_qos_strength_name(attr_p->attr.qos.strength),
                  sdp_get_qos_status_type_name(attr_p->attr.des.status_type),
                  sdp_get_qos_direction_name(attr_p->attr.des.direction));
    }

    return (SDP_SUCCESS);
}


sdp_result_e sdp_build_attr_des (sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr,
                                 u16 len)
{
    GCC_UNUSED(sdp_p);
    char       *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s %s %s %s", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_curr_type_name((sdp_curr_type_e)attr_p->attr.des.type),
                     sdp_get_qos_strength_name(attr_p->attr.des.strength),
                     sdp_get_qos_status_type_name(attr_p->attr.des.status_type),
                     sdp_get_qos_direction_name(attr_p->attr.des.direction));
    len = endbuf_p - *ptr;
    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_conf (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];
   
    /* Find the curr type tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No conf attr type specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.conf.type = SDP_CONF_UNKNOWN_TYPE;
    for (i=0; i < SDP_MAX_CURR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_conf_type[i].name,
                        sdp_conf_type[i].strlen) == 0) {
            attr_p->attr.conf.type = (sdp_conf_type_e)i;
        }
    }
    
    if (attr_p->attr.conf.type != SDP_CONF_QOS_TYPE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Unknown conf type.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);   
    }
    
    /* Check qos status type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
     if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No conf attr type specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.conf.status_type = SDP_QOS_STATUS_TYPE_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STATUS_TYPES; i++) {
        if (strncasecmp(tmp, sdp_qos_status_type[i].name,
                        sdp_qos_status_type[i].strlen) == 0) {
            attr_p->attr.conf.status_type = (sdp_qos_status_types_e)i;
        }
    }
    

    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No qos direction specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.conf.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.conf.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.conf.direction == SDP_QOS_DIR_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: QOS direction unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, type %s status type %s, direction %s", 
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_conf_type_name(attr_p->attr.conf.type),
                  sdp_get_qos_status_type_name(attr_p->attr.conf.status_type),
                  sdp_get_qos_direction_name(attr_p->attr.conf.direction));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_conf (sdp_t *sdp_p, sdp_attr_t *attr_p, char **ptr,
                                 u16 len)
{
    GCC_UNUSED(sdp_p);
    char       *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s %s %s", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_conf_type_name(attr_p->attr.conf.type),
                     sdp_get_qos_status_type_name(attr_p->attr.conf.status_type),
                     sdp_get_qos_direction_name(attr_p->attr.conf.direction));
    len = endbuf_p - *ptr;
    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

/*
 *  Parse a rtpmap or a sprtmap. Both formats use the same structure
 *  the only difference being the keyword "rtpmap" vs "sprtmap". The
 *  rtpmap field in the sdp_attr_t is used to store both mappings.
 */
sdp_result_e sdp_parse_attr_transport_map (sdp_t *sdp_p, sdp_attr_t *attr_p, 
	const char *ptr)
{
    sdp_result_e  result;

    attr_p->attr.transport_map.payload_num = 0;
    attr_p->attr.transport_map.encname[0]  = '\0';
    attr_p->attr.transport_map.clockrate   = 0;
    attr_p->attr.transport_map.num_chan    = 1;

    /* Find the payload type number. */
    attr_p->attr.transport_map.payload_num = 
	sdp_getnextnumtok(ptr, &ptr, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid payload type specified for "
                     "%s attribute.", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTPMAP_LINE);
    }

    /* Find the encoding name. */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.transport_map.encname,"/ \t",
	                    &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No encoding name specified in %s "
                     "attribute.", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTPMAP_LINE);
    }

    /* Find the clockrate. */
    attr_p->attr.transport_map.clockrate = 
	sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No clockrate specified for "
                     "%s attribute, set to default of 90000.",
                     sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_CLOCK_IN_RTPMAP_LINE);
    }
    
    /* Find the number of channels, if specified. This is optional. */
    if (*ptr == '/') {
        /* If a '/' exists, expect something valid beyond it. */
        attr_p->attr.transport_map.num_chan = 
	    sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
        if (result != SDP_SUCCESS) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: Invalid number of channels parameter"
                         " for rtpmap attribute.", sdp_p->debug_str);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_RTPMAP_LINE);
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, payload type %u, encoding name %s, "
                  "clockrate %lu", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.transport_map.payload_num,
                  attr_p->attr.transport_map.encname,
                  attr_p->attr.transport_map.clockrate);
        if (attr_p->attr.transport_map.num_chan != 1) {
            SDP_PRINT("/%u", attr_p->attr.transport_map.num_chan);
        }
    }

    return (SDP_SUCCESS);
}

/*
 *  Build a rtpmap or a sprtmap. Both formats use the same structure
 *  the only difference being the keyword "rtpmap" vs "sprtmap". The
 *  rtpmap field in the sdp_attr_t is used for both mappings.
 */
sdp_result_e sdp_build_attr_transport_map (sdp_t *sdp_p, sdp_attr_t *attr_p, 
	char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    char         *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%u %s/%ld", 
                     sdp_attr[attr_p->type].name, 
                     attr_p->attr.transport_map.payload_num,
                     attr_p->attr.transport_map.encname,
                     attr_p->attr.transport_map.clockrate);

    len = endbuf_p - *ptr;
    if (attr_p->attr.transport_map.num_chan != 1) {
        *ptr += snprintf(*ptr, len, "/%u\r\n", 
		attr_p->attr.transport_map.num_chan);
    } else {
        *ptr += snprintf(*ptr, len, "\r\n");
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_subnet (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                    const char *ptr)
{
    int i;
    char         *slash_ptr;
    sdp_result_e  result;
    tinybool      type_found = FALSE;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Find the subnet network type. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No network type specified in subnet "
                     "attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.subnet.nettype = SDP_NT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (strncasecmp(tmp, sdp_nettype[i].name, 
                        sdp_nettype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.subnet.nettype = (sdp_nettype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.subnet.nettype == SDP_NT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Subnet network type "
                     "unsupported (%s).", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the subnet address type. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No address type specified in subnet"
                     " attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.subnet.addrtype = SDP_AT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.subnet.addrtype = (sdp_addrtype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.subnet.addrtype == SDP_AT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Subnet address type unsupported "
                     "(%s).", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the subnet address.  */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.subnet.addr, " \t", 
                            &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No subnet address specified in "
                     "subnet attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    slash_ptr = sdp_findchar(attr_p->attr.subnet.addr, "/");
    if (*slash_ptr == '/') {
        *slash_ptr++ = '\0';
        /* If the '/' exists, expect a valid prefix to follow. */
        attr_p->attr.subnet.prefix = sdp_getnextnumtok(slash_ptr, 
                                                  (const char **)&slash_ptr, 
                                                  " \t", &result);
        if (result != SDP_SUCCESS) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: Invalid subnet prefix specified in "
                         "subnet attribute.", sdp_p->debug_str);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    } else {
        attr_p->attr.subnet.prefix = SDP_INVALID_VALUE;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, network %s, addr type %s, address %s ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_network_name(attr_p->attr.subnet.nettype),
                  sdp_get_address_name(attr_p->attr.subnet.addrtype),
                  attr_p->attr.subnet.addr);
        if (attr_p->attr.subnet.prefix != SDP_INVALID_VALUE) {
            SDP_PRINT("/%u", (ushort)attr_p->attr.subnet.prefix);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_subnet (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                    char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    char         *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s %s %s", sdp_attr[attr_p->type].name,
                     sdp_get_network_name(attr_p->attr.subnet.nettype),
                     sdp_get_address_name(attr_p->attr.subnet.addrtype),
                     attr_p->attr.subnet.addr);
    len = endbuf_p - *ptr;
    if (attr_p->attr.subnet.prefix != SDP_INVALID_VALUE) {
        *ptr += snprintf(*ptr, len, "/%u", (ushort)attr_p->attr.subnet.prefix);
        len = endbuf_p - *ptr;
    }
    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_t38_ratemgmt (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                          const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the rate mgmt. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No t38 rate management specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.t38ratemgmt = SDP_T38_UNKNOWN_RATE;
    for (i=0; i < SDP_T38_MAX_RATES; i++) {
        if (strncasecmp(tmp, sdp_t38_rate[i].name,
                        sdp_t38_rate[i].strlen) == 0) {
            attr_p->attr.t38ratemgmt = (sdp_t38_ratemgmt_e)i;
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, rate %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  sdp_get_t38_ratemgmt_name(attr_p->attr.t38ratemgmt));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_t38_ratemgmt (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                          char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_t38_ratemgmt_name(attr_p->attr.t38ratemgmt));

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_t38_udpec (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                       const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the udpec. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No t38 udpEC specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.t38udpec = SDP_T38_UDPEC_UNKNOWN;
    for (i=0; i < SDP_T38_MAX_UDPEC; i++) {
        if (strncasecmp(tmp, sdp_t38_udpec[i].name,
                        sdp_t38_udpec[i].strlen) == 0) {
            attr_p->attr.t38udpec = (sdp_t38_udpec_e)i;
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, udpec %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  sdp_get_t38_udpec_name(attr_p->attr.t38udpec));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_t38_udpec (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                       char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_t38_udpec_name(attr_p->attr.t38udpec));

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_pc_codec (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      const char *ptr)
{
    u16 i;
    sdp_result_e result;

    for (i=0; i < SDP_MAX_PAYLOAD_TYPES; i++) {
        attr_p->attr.pccodec.payload_type[i] = sdp_getnextnumtok(ptr, &ptr,
                                                               " \t", &result);
        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.pccodec.num_payloads++;
    }

    if (attr_p->attr.pccodec.num_payloads == 0) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No payloads specified for %s attr.",
                     sdp_p->debug_str, sdp_attr[attr_p->type].name);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, num payloads %u, payloads: ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  attr_p->attr.pccodec.num_payloads);
        for (i=0; i < attr_p->attr.pccodec.num_payloads; i++) {
            SDP_PRINT("%u ", attr_p->attr.pccodec.payload_type[i]);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_pc_codec (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    u16           i;
    char         *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s: ", sdp_attr[attr_p->type].name);
    len = endbuf_p - *ptr;

    for (i=0; i < attr_p->attr.pccodec.num_payloads; i++) {
        *ptr += snprintf(*ptr, len, "%u ", 
                         attr_p->attr.pccodec.payload_type[i]);
        len = endbuf_p - *ptr;
    }
    len = endbuf_p - *ptr;
    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}


sdp_result_e sdp_parse_attr_cap (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                 const char *ptr)
{
    u16           i;
    u16           cap_num;
    sdp_result_e  result;
    sdp_mca_t    *cap_p;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Set the capability pointer to NULL for now in case we encounter
     * an error in parsing.
     */
    attr_p->attr.cap_p = NULL;
    /* Set the capability valid flag to FALSE in case we encounter an
     * error.  If we do, we don't want to process any X-cpar/cpar attributes
     * from this point until we process the next valid X-cap/cdsc attr. */
    sdp_p->cap_valid = FALSE;

    /* Allocate resource for new capability. Note that the capability 
     * uses the same structure used for media lines.
     */
    cap_p = sdp_alloc_mca();
    if (cap_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }

    /* Find the capability number. We don't need to store this since we
     * calculate it for ourselves as we need to. But it must be specified. */
    cap_num = sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Capability not specified for %s, "
                     "unable to parse.", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type));
                     
        }
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    
    /* Find the media type. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s No media type specified for %s attribute, "
                     "unable to parse.", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type));
        }
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    cap_p->media = SDP_MEDIA_UNSUPPORTED;
    for (i=0; i < SDP_MAX_MEDIA_TYPES; i++) {
        if (strncasecmp(tmp, sdp_media[i].name, sdp_media[i].strlen) == 0) {
            cap_p->media = (sdp_media_e)i;
            break;
        }
    }
    if (cap_p->media == SDP_MEDIA_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Media type unsupported (%s).", 
                     sdp_p->debug_str, tmp);
        }
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the transport protocol type. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s No transport protocol type specified, "
                     "unable to parse.", sdp_p->debug_str);
        }
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    cap_p->transport = SDP_TRANSPORT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_TRANSPORT_TYPES; i++) {
        if (strncasecmp(tmp, sdp_transport[i].name,
                        sdp_transport[i].strlen) == 0) {
            cap_p->transport = (sdp_transport_e)i;
            break;
        }
    }
    if (cap_p->transport == SDP_TRANSPORT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Transport protocol type unsupported "
                     "(%s).", sdp_p->debug_str, tmp);
        }
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find payload formats. AAL2 X-cap lines allow multiple 
     * transport/profile types per line, so these are handled differently.
     */
    if ((cap_p->transport == SDP_TRANSPORT_AAL2_ITU) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_ATMF) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_CUSTOM)) {
        /* Capability processing is not currently defined for AAL2 types
         * with multiple profiles. We don't process. */
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: AAL2 profiles unsupported with "
                     "%s attributes.", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type));
        }
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        /* Transport is a non-AAL2 type.  Parse payloads normally. */
        sdp_parse_payload_types(sdp_p, cap_p, ptr);
        if (cap_p->num_payloads == 0) {
            SDP_FREE(cap_p);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    }

    attr_p->attr.cap_p = cap_p;
    /* 
     * This capability attr is valid.  We can now handle X-cpar or 
     * cpar attrs. 
     */
    sdp_p->cap_valid = TRUE;
    sdp_p->last_cap_inst++;

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed %s media type %s, Transport %s, "
                  "Num payloads %u", sdp_p->debug_str, 
		  sdp_get_attr_name(attr_p->type), 
                  sdp_get_media_name(cap_p->media),
                  sdp_get_transport_name(cap_p->transport), 
                  cap_p->num_payloads);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_cap (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                 char **ptr, u16 len)
{
    u16                   i, j;
    char                 *endbuf_p;
    sdp_mca_t            *cap_p;
    sdp_result_e          result;
    sdp_media_profiles_t *profile_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    /* Get a pointer to the capability structure. */
    cap_p = attr_p->attr.cap_p;

    if (cap_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Invalid %s attribute, unable to build.",
		    sdp_p->debug_str,
		    sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        /* Return success so build won't fail. */
        return (SDP_SUCCESS);
    }

    /* Validate params for this capability line */
    if ((cap_p->media >= SDP_MAX_MEDIA_TYPES) ||
        (cap_p->transport >= SDP_MAX_TRANSPORT_TYPES)) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Media or transport type invalid for %s "
                     "attribute, unable to build.", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        /* Return success so build won't fail. */
        return (SDP_SUCCESS);
    }

    *ptr += snprintf(*ptr, len, "a=%s: %u ", sdp_attr[attr_p->type].name,
                     sdp_p->cur_cap_num);
    len = endbuf_p - *ptr;

    /* Build the media type */
    *ptr += snprintf(*ptr, len, "%s ", sdp_get_media_name(cap_p->media));
    len = endbuf_p - *ptr;

    /* If the X-cap line has AAL2 profiles, build them differently. */
    if ((cap_p->transport == SDP_TRANSPORT_AAL2_ITU) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_ATMF) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_CUSTOM)) {
        profile_p = cap_p->media_profiles_p;
        for (i=0; i < profile_p->num_profiles; i++) {
            *ptr += snprintf(*ptr, len, "%s",
                             sdp_get_transport_name(profile_p->profile[i]));
            len = endbuf_p - *ptr;
            for (j=0; j < profile_p->num_payloads[i]; j++) {
                *ptr += snprintf(*ptr, len, " %u", 
                                 profile_p->payload_type[i][j]);
                len = endbuf_p - *ptr;
            }
            *ptr += snprintf(*ptr, len, " "); 
            len = endbuf_p - *ptr;
        }
        *ptr += snprintf(*ptr, len, "\n");
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Built m= media line", sdp_p->debug_str);
        }
        return (SDP_SUCCESS);
    }

    /* Build the transport name */
    *ptr += snprintf(*ptr, len, "%s", 
                     sdp_get_transport_name(cap_p->transport));
    len = endbuf_p - *ptr;

    /* Build the format lists */
    for (i=0; i < cap_p->num_payloads; i++) {
        if (cap_p->payload_indicator[i] == SDP_PAYLOAD_ENUM) {
            *ptr += snprintf(*ptr, len, " %s",
                             sdp_get_payload_name((sdp_payload_e)cap_p->payload_type[i]));
        } else {
            *ptr += snprintf(*ptr, len, " %u", cap_p->payload_type[i]);
        }
        len = endbuf_p - *ptr;
    }
    *ptr += snprintf(*ptr, len, "\r\n");

    /* Increment the current capability number for the next X-cap/cdsc attr. */
    sdp_p->cur_cap_num += cap_p->num_payloads;
    sdp_p->last_cap_type = attr_p->type;

    /* Build any X-cpar/cpar attributes associated with this X-cap/cdsc line. */
    result = sdp_build_attr_cpar(sdp_p, cap_p->media_attrs_p, ptr, len);

    return (result);
}


sdp_result_e sdp_parse_attr_cpar (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                  const char *ptr)
{
    u16           i;
    sdp_result_e  result;
    sdp_mca_t    *cap_p;
    sdp_attr_t   *cap_attr_p = NULL;
    sdp_attr_t   *prev_attr_p;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Make sure we've processed a valid X-cap/cdsc attr prior to this and
     * if so, get the cap pointer. */
    if (sdp_p->cap_valid == TRUE) {
	sdp_attr_e cap_type;

	if (attr_p->type == SDP_ATTR_CPAR) {
	    cap_type = SDP_ATTR_CDSC;
	} else {
	    /* Default to X-CAP for everything else */
	    cap_type = SDP_ATTR_X_CAP;
	}

        if (sdp_p->mca_count == 0) {
            cap_attr_p = sdp_find_attr(sdp_p, SDP_SESSION_LEVEL, 0,
                                       cap_type, sdp_p->last_cap_inst);
        } else {
            cap_attr_p = sdp_find_attr(sdp_p, sdp_p->mca_count, 0,
                                       cap_type, sdp_p->last_cap_inst);
        }
    }
    if ((cap_attr_p == NULL) || (cap_attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: %s attribute specified with no "
                     "prior %s attribute", sdp_p->debug_str,
		     sdp_get_attr_name(attr_p->type), 
		     (attr_p->type == SDP_ATTR_CPAR)?
			(sdp_get_attr_name(SDP_ATTR_CDSC)) :
			(sdp_get_attr_name(SDP_ATTR_X_CAP)) );
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* 
     * Ensure there is no mixed syntax like CDSC followed by X-CPAR
     * or X-CAP followed by CPAR.
     */
    if (((cap_attr_p->type == SDP_ATTR_CDSC) && 
	 (attr_p->type == SDP_ATTR_X_CPAR)) || 
	( (cap_attr_p->type == SDP_ATTR_X_CAP) && 
	  (attr_p->type == SDP_ATTR_CPAR)) ) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
	    SDP_WARN("%s Warning: %s attribute inconsistent with "
		    "prior %s attribute", sdp_p->debug_str,
		    sdp_get_attr_name(attr_p->type), 
		    sdp_get_attr_name(cap_attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    cap_p = cap_attr_p->attr.cap_p;

    /* a= is the only token we handle in an X-cpar/cpar attribute. */
    ptr = sdp_getnextstrtok(ptr, tmp, "= \t", &result);
	     
    if ((result != SDP_SUCCESS) || (tmp[0] != 'a') || (tmp[1] != '\0')) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid token type (%s) in %s "
                     "attribute, unable to parse", sdp_p->debug_str, tmp,
		     sdp_get_attr_name(attr_p->type)); 
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    if (*ptr == '=') {
        ptr++;
    }

    /* Find the attribute type. */
    ptr = sdp_getnextstrtok(ptr, tmp, ": \t", &result);
    if (ptr[0] == ':') {
        /* Skip the ':' char for parsing attribute parameters. */
        ptr++;
    }
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No attribute type specified for %s "
                      "attribute, unable to parse.", sdp_p->debug_str,
		      sdp_get_attr_name(attr_p->type)); 
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Reset the type of the attribute from X-cpar/cpar to whatever the 
     * specified type is. */
    attr_p->type = SDP_ATTR_INVALID;
    attr_p->next_p = NULL;
    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_attr[i].name, sdp_attr[i].strlen) == 0) {
            attr_p->type = (sdp_attr_e)i;
        }
    }
    if (attr_p->type == SDP_ATTR_INVALID) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Unrecognized attribute (%s) for %s"
		    " attribute, unable to parse.", sdp_p->debug_str, tmp,
		    sdp_get_attr_name(attr_p->type)); 
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* We don't allow recursion with the capability attributes. */
    if ((attr_p->type == SDP_ATTR_X_SQN) ||
        (attr_p->type == SDP_ATTR_X_CAP) ||
        (attr_p->type == SDP_ATTR_X_CPAR) ||
	(attr_p->type == SDP_ATTR_SQN) ||
	(attr_p->type == SDP_ATTR_CDSC) ||
	(attr_p->type == SDP_ATTR_CPAR)) {
	if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid attribute (%s) for %s"
		    " attribute, unable to parse.", sdp_p->debug_str, tmp,
		    sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Parse the attribute. */
    result = sdp_attr[attr_p->type].parse_func(sdp_p, attr_p, ptr);
    if (result != SDP_SUCCESS) {
        return (result);
    }

    /* Hook the attribute into the capability structure. */
    if (cap_p->media_attrs_p == NULL) {
        cap_p->media_attrs_p = attr_p;
    } else {
        for (prev_attr_p = cap_p->media_attrs_p;
             prev_attr_p->next_p != NULL;
             prev_attr_p = prev_attr_p->next_p); /* Empty for */
        prev_attr_p->next_p = attr_p;
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_cpar (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                  char **ptr, u16 len)
{
    char         *endbuf_p;
    sdp_result_e  result;
    const char	 *cpar_name;

    /* Determine whether to use cpar or X-cpar */
    if (sdp_p->last_cap_type == SDP_ATTR_CDSC) {
	cpar_name = sdp_get_attr_name(SDP_ATTR_CPAR);
    } else {
	/* 
	 * Default to X-CPAR if anything else. This is the backward
	 * compatible value.
	 */
	cpar_name = sdp_get_attr_name(SDP_ATTR_X_CPAR);
    }

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    while (attr_p != NULL) {
        if (attr_p->type >= SDP_MAX_ATTR_TYPES) {
            SDP_WARN("%s Invalid attribute type to build (%u)", 
                     sdp_p->debug_str, attr_p->type);
        } else {
            *ptr += snprintf(*ptr, len, "a=%s: ", cpar_name);
            len = endbuf_p - *ptr;

            result = sdp_attr[attr_p->type].build_func(sdp_p, attr_p, 
                                                       ptr, len);
            if (result == SDP_SUCCESS) {
                if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                    SDP_PRINT("%s Built %s a=%s attribute line",
                              sdp_p->debug_str, cpar_name,
                              sdp_get_attr_name(attr_p->type));
                }
            }
        }
        attr_p = attr_p->next_p;
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_rtr (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                           const char *ptr)
{
    sdp_result_e  result;
    char tmp[SDP_MAX_STRING_LEN];

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s, %s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type),
                     tmp);
    }
    /*Default confirm to FALSE. */
    attr_p->attr.rtr.confirm = FALSE;

    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS){ // No confirm tag specified is not an error
        return (SDP_SUCCESS);
    } else {
       /* See if confirm was specified.  Defaults to FALSE. */
       if (strncasecmp(tmp, "confirm", sizeof("confirm")) == 0) {
           attr_p->attr.rtr.confirm = TRUE;
       }
       if (attr_p->attr.rtr.confirm == FALSE) {
           if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
               SDP_WARN("%s Warning: RTR confirm parameter invalid (%s)",
                        sdp_p->debug_str, tmp);
           }
           sdp_p->conf_p->num_invalid_param++;
           return (SDP_INVALID_PARAMETER);
       }
       if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
           SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type), 
                     tmp);
       }
       return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_rtr (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                           char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    if (attr_p->attr.rtr.confirm){
        *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", sdp_attr[attr_p->type].name,
                         "confirm");
    } else {  
        *ptr += snprintf(*ptr, len, "a=%s\r\n", sdp_attr[attr_p->type].name);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_comediadir (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                        const char *ptr)
{
    int i;
    sdp_result_e  result;
    tinybool      type_found = FALSE;
    char          tmp[SDP_MAX_STRING_LEN];

    attr_p->attr.comediadir.role = SDP_MEDIADIR_ROLE_PASSIVE;
    attr_p->attr.comediadir.conn_info_present = FALSE;
    attr_p->attr.comediadir.conn_info.nettype = SDP_NT_INVALID;
    attr_p->attr.comediadir.src_port = 0;

    /* Find the media direction role. */
    ptr = sdp_getnextstrtok(ptr, tmp, ": \t", &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No role parameter specified for "
                     "comediadir attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.comediadir.role = SDP_MEDIADIR_ROLE_UNSUPPORTED;
    for (i=0; i < SDP_MAX_MEDIADIR_ROLES; i++) {
        if (strncasecmp(tmp, sdp_mediadir_role[i].name, 
                        sdp_mediadir_role[i].strlen) == 0) {
            type_found = TRUE;
            attr_p->attr.comediadir.role = (sdp_mediadir_role_e)i;
            break;
        }
    }
    if (attr_p->attr.comediadir.role == SDP_MEDIADIR_ROLE_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid role type specified for "
                     "comediadir attribute (%s).", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* If the role is passive, we don't expect any more params. */
    if (attr_p->attr.comediadir.role == SDP_MEDIADIR_ROLE_PASSIVE) {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, passive",
                      sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        return (SDP_SUCCESS);
    }

    /* Find the connection information if present */
    /* parse to get the nettype */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No network type specified in comediadir "
                     "attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_SUCCESS); /* as the optional parameters are not there */
    }
    attr_p->attr.comediadir.conn_info.nettype = SDP_NT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (strncasecmp(tmp, sdp_nettype[i].name, 
                        sdp_nettype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.comediadir.conn_info.nettype = (sdp_nettype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.comediadir.conn_info.nettype == SDP_NT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: ConnInfo in Comediadir: network type "
                     "unsupported (%s).", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
    }

    /* Find the comedia address type. */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No address type specified in comediadir"
                     " attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
    }
    attr_p->attr.comediadir.conn_info.addrtype = SDP_AT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.comediadir.conn_info.addrtype = (sdp_addrtype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.comediadir.conn_info.addrtype == SDP_AT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Conninfo address type unsupported "
                     "(%s).", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
    }

    /* Find the conninfo address.  */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.comediadir.conn_info.conn_addr, 
                            " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No conninfo address specified in "
                     "comediadir attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
    }

    /* Find the src port info , if any */
    attr_p->attr.comediadir.src_port  = sdp_getnextnumtok(ptr, &ptr, " \t", 
                                                          &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No src port specified in "
                     "comediadir attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, network %s, addr type %s, address %s "
                  "srcport %u ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_network_name(attr_p->attr.comediadir.conn_info.nettype),
                  sdp_get_address_name(attr_p->attr.comediadir.conn_info.addrtype),
                  attr_p->attr.comediadir.conn_info.conn_addr,
                  (unsigned int)attr_p->attr.comediadir.src_port);
    }

    if (sdp_p->conf_p->num_invalid_param > 0) {
        return (SDP_INVALID_PARAMETER);
    }
    return (SDP_SUCCESS);
}

sdp_result_e 
sdp_build_attr_comediadir (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                    char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    char         *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;
    *ptr += snprintf(*ptr, len, "a=%s:%s", 
                     sdp_attr[attr_p->type].name,
                     sdp_get_mediadir_role_name(attr_p->attr.
                                                comediadir.role));
    len = endbuf_p - *ptr;
    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_silencesupp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find silenceSuppEnable */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No silenceSupp enable value specified, parse failed.",
                      sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (strncasecmp(tmp, "on", sizeof("on")) == 0) {
        attr_p->attr.silencesupp.enabled = TRUE;
    } else if (strncasecmp(tmp, "off", sizeof("off")) == 0) {
        attr_p->attr.silencesupp.enabled = FALSE;
    } else if (strncasecmp(tmp, "-", sizeof("-")) == 0) {
        attr_p->attr.silencesupp.enabled = FALSE;
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: silenceSuppEnable parameter invalid (%s)",
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find silenceTimer -- u16 or "-" */

    attr_p->attr.silencesupp.timer =
        (u16)sdp_getnextnumtok_or_null(ptr, &ptr, " \t",
                                       &attr_p->attr.silencesupp.timer_null,
                                       &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid timer value specified for "
                     "silenceSupp attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find suppPref */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No silenceSupp pref specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.silencesupp.pref = SDP_SILENCESUPP_PREF_UNKNOWN;
    for (i=0; i < SDP_MAX_SILENCESUPP_PREF; i++) {
        if (strncasecmp(tmp, sdp_silencesupp_pref[i].name,
                        sdp_silencesupp_pref[i].strlen) == 0) {
            attr_p->attr.silencesupp.pref = (sdp_silencesupp_pref_e)i;
        }
    }
    if (attr_p->attr.silencesupp.pref == SDP_SILENCESUPP_PREF_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: silenceSupp pref unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find sidUse */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No silenceSupp sidUse specified.",
                     sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.silencesupp.siduse = SDP_SILENCESUPP_SIDUSE_UNKNOWN;
    for (i=0; i < SDP_MAX_SILENCESUPP_SIDUSE; i++) {
        if (strncasecmp(tmp, sdp_silencesupp_siduse[i].name,
                        sdp_silencesupp_siduse[i].strlen) == 0) {
            attr_p->attr.silencesupp.siduse = (sdp_silencesupp_siduse_e)i;
        }
    }
    if (attr_p->attr.silencesupp.siduse == SDP_SILENCESUPP_SIDUSE_UNKNOWN) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: silenceSupp sidUse unrecognized (%s)", 
                     sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find fxnslevel -- u8 or "-" */
    attr_p->attr.silencesupp.fxnslevel =
        (u8)sdp_getnextnumtok_or_null(ptr, &ptr, " \t",
                                      &attr_p->attr.silencesupp.fxnslevel_null,
                                      &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid fxnslevel value specified for "
                     "silenceSupp attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, enabled %s",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  (attr_p->attr.silencesupp.enabled ? "on" : "off"));
        if (attr_p->attr.silencesupp.timer_null) {
            SDP_PRINT(" timer=-");
        } else {
            SDP_PRINT(" timer=%u,", attr_p->attr.silencesupp.timer);
        }
        SDP_PRINT(" pref=%s, siduse=%s,",
                  sdp_get_silencesupp_pref_name(attr_p->attr.silencesupp.pref),
                  sdp_get_silencesupp_siduse_name(
                                             attr_p->attr.silencesupp.siduse));
        if (attr_p->attr.silencesupp.fxnslevel_null) {
            SDP_PRINT(" fxnslevel=-");
        } else {
            SDP_PRINT(" fxnslevel=%u,", attr_p->attr.silencesupp.fxnslevel);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_silencesupp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    char       *endbuf_p;

    /* Find ptr to the end of the buf for recalculating len remaining. */
    endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s ",
                     sdp_attr[attr_p->type].name,
                     (attr_p->attr.silencesupp.enabled ? "on" : "off"));
    len = endbuf_p - *ptr;

    if (attr_p->attr.silencesupp.timer_null) {
        *ptr += snprintf(*ptr, len, "-");
    } else {
        *ptr += snprintf(*ptr, len, "%u", attr_p->attr.silencesupp.timer);
    }
    len = endbuf_p - *ptr;

    *ptr += snprintf(*ptr, len, " %s %s ",
                     sdp_get_silencesupp_pref_name(
                                             attr_p->attr.silencesupp.pref),
                     sdp_get_silencesupp_siduse_name(
                                             attr_p->attr.silencesupp.siduse));
    len = endbuf_p - *ptr;

    if (attr_p->attr.silencesupp.fxnslevel_null) {
        *ptr += snprintf(*ptr, len, "-");
    } else {
        *ptr += snprintf(*ptr, len, "%u", attr_p->attr.silencesupp.fxnslevel);
    }
    len = endbuf_p - *ptr;

    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

/*
 * sdp_parse_context_crypto_suite
 *
 * This routine parses the crypto suite pointed to by str, stores the crypto suite value into the
 * srtp context suite component of the LocalConnectionOptions pointed to by lco_node_ptr and stores
 * pointer to the next crypto parameter in tmp_ptr
 */
tinybool sdp_parse_context_crypto_suite(char * str,  sdp_attr_t *attr_p, sdp_t *sdp_p)
{
      /*
       * Three crypto_suites are defined: (Notice no SPACE between "crypto:" and the <crypto-suite>
       * AES_CM_128_HMAC_SHA1_80
       * AES_CM_128_HMAC_SHA1_32
       * F8_128_HMAC_SHA1_80
       */

       int i;

       /* Check crypto suites */    
       for(i=0; i<SDP_SRTP_MAX_NUM_CRYPTO_SUITES; i++) {
	 if (!strcasecmp(sdp_srtp_crypto_suite_array[i].crypto_suite_str, str)) {
	   attr_p->attr.srtp_context.suite = sdp_srtp_crypto_suite_array[i].crypto_suite_val;
	   attr_p->attr.srtp_context.master_key_size_bytes = 
	       sdp_srtp_crypto_suite_array[i].key_size_bytes;
	   attr_p->attr.srtp_context.master_salt_size_bytes = 
	       sdp_srtp_crypto_suite_array[i].salt_size_bytes;
	   return TRUE; /* There is a succesful match so exit */ 
	 }
       }
       /* couldn't find a matching crypto suite */
       if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No Matching crypto suite for SRTP Context(%s)-'X-crypto:v1' expected",
                      sdp_p->debug_str, str);
       }

       return FALSE;
}


sdp_result_e sdp_build_attr_srtpcontext (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                    char **ptr, u16 len)
{
#define MAX_BASE64_ENCODE_SIZE_BYTES 60
    int          output_len = MAX_BASE64_ENCODE_SIZE_BYTES;
    int		 key_size = attr_p->attr.srtp_context.master_key_size_bytes;
    int		 salt_size = attr_p->attr.srtp_context.master_salt_size_bytes;
    unsigned char  base64_encoded_data[MAX_BASE64_ENCODE_SIZE_BYTES];
    unsigned char  base64_encoded_input[MAX_BASE64_ENCODE_SIZE_BYTES];
    base64_result_t status;

    output_len = MAX_BASE64_ENCODE_SIZE_BYTES;

    /* Append master and salt keys */
    bcopy(attr_p->attr.srtp_context.master_key, base64_encoded_input, 
	    key_size );   
    bcopy(attr_p->attr.srtp_context.master_salt,
	    base64_encoded_input + key_size, salt_size );

    if ((status = base64_encode(base64_encoded_input, key_size + salt_size,
		      base64_encoded_data, &output_len)) != BASE64_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Error: Failure to Base64 Encoded data (%s) ", 
		      sdp_p->debug_str, BASE64_RESULT_TO_STRING((int) status));
        }
	return (SDP_INVALID_PARAMETER);
    }

    *(base64_encoded_data + output_len) = '\0';

    *ptr += snprintf(*ptr, len, "a=%s:%s inline:%s||\r\n", 
                     sdp_attr[attr_p->type].name, 
                     sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
		     base64_encoded_data);

    return (SDP_SUCCESS);
}

/*
 * sdp_parse_attr_mptime
 * This function parses the a=mptime sdp line. This parameter consists of
 * one or more numbers or hyphens ("-"). The first parameter must be a
 * number. The number of parameters must match the number of formats specified
 * on the m= line. This function is liberal in that it does not match against
 * the m= line or require a number for the first parameter.
 */
sdp_result_e sdp_parse_attr_mptime (
    sdp_t *sdp_p,
    sdp_attr_t *attr_p,
    const char *ptr)
{
    u16 i;                      /* loop counter for parameters */
    sdp_result_e result;        /* value returned by this function */
    tinybool null_ind;          /* true if a parameter is "-" */

    /*
     * Scan the input line up to the maximum number of parameters supported.
     * Look for numbers or hyphens and store the resulting values. Hyphens
     * are stored as zeros.
     */
    for (i=0; i<SDP_MAX_PAYLOAD_TYPES; i++) {
        attr_p->attr.mptime.intervals[i] =
            sdp_getnextnumtok_or_null(ptr,&ptr," \t",&null_ind,&result);
        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.mptime.num_intervals++;
    }

    /*
     * At least one parameter must be supplied. If not, return an error
     * and optionally log the failure.
     */
    if (attr_p->attr.mptime.num_intervals == 0) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No intervals specified for %s attr.",
                     sdp_p->debug_str, sdp_attr[attr_p->type].name);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /*
     * Here is some debugging code that helps us track what data
     * is received and parsed.
     */
    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, num intervals %u, intervals: ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  attr_p->attr.mptime.num_intervals);
        for (i=0; i < attr_p->attr.mptime.num_intervals; i++) {
            SDP_PRINT("%u ", attr_p->attr.mptime.intervals[i]);
        }
    }

    return SDP_SUCCESS;
}

/*
 * sdp_build_attr_mptime
 * This function builds the a=mptime sdp line. It reads the selected attribute
 * from the sdp structure. Parameters with a value of zero are replaced by
 * hyphens.
 */
sdp_result_e sdp_build_attr_mptime (
    sdp_t *sdp_p,
    sdp_attr_t *attr_p,
    char **ptr,
    u16 len)
{
    GCC_UNUSED(sdp_p);
    u16 i;
    char *endbuf_p;

    /*
     * Find the pointer to the end of the buffer
     * for recalculating the length remaining.
     */
    endbuf_p = *ptr + len;

    /*
     * Write out the fixed part of the sdp line.
     */
    *ptr += snprintf(*ptr, len, "a=%s:", sdp_attr[attr_p->type].name);
    len = endbuf_p - *ptr;

    /*
     * Run the list of mptime parameter values and write each one
     * to the sdp line. Replace zeros with hyphens.
     */
    for (i=0; i<attr_p->attr.mptime.num_intervals; i++) {
        if (attr_p->attr.mptime.intervals[i]==0) {
            *ptr += snprintf(*ptr,len,"-");
        } else {
            *ptr += snprintf(*ptr, len, "%s%u", (i==0)?"":" ", attr_p->attr.mptime.intervals[i]);
        }
        len = endbuf_p - *ptr;
    }

    /*
     * Close out the line.
     */
    len = endbuf_p - *ptr;
    *ptr += snprintf(*ptr, len, "\r\n");

    return SDP_SUCCESS;
}



sdp_result_e sdp_parse_attr_x_sidin (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                     const char *ptr)
{
    sdp_result_e  result;
    attr_p->attr.stream_data.x_sidin[0]  = '\0';

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type));
    }

    /* Find the X-sidin value */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.stream_data.x_sidin," \t",
                            &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No Stream Id incoming specified for "
                     "X-sidin attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type), 
                  attr_p->attr.stream_data.x_sidin);
    }
   return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_x_sidin (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", 
                     sdp_attr[attr_p->type].name, 
                     attr_p->attr.stream_data.x_sidin);
    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_x_sidout (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      const char *ptr)
{
    sdp_result_e  result;
    attr_p->attr.stream_data.x_sidout[0]  = '\0';

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type));
    }

    /* Find the X-sidout value */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.stream_data.x_sidout," \t",
                            &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No Stream Id outgoing specified for "
                     "X-sidout attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type), 
                  attr_p->attr.stream_data.x_sidout);
    }
   return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_x_sidout (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", 
                     sdp_attr[attr_p->type].name, 
                     attr_p->attr.stream_data.x_sidout);
    return (SDP_SUCCESS);
}


sdp_result_e sdp_parse_attr_x_confid (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      const char *ptr)
{
    sdp_result_e  result;
    attr_p->attr.stream_data.x_confid[0]  = '\0';

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }
    
    /* Find the X-confid value */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.stream_data.x_confid," \t",
                            &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No Conf Id incoming specified for "
                     "X-confid attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    
    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type), 
                  attr_p->attr.stream_data.x_confid);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_x_confid (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                      char **ptr, u16 len)
{
    if (attr_p->attr.stream_data.x_confid[0] != '\0') {
        *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", 
                         sdp_attr[attr_p->type].name, 
                         attr_p->attr.stream_data.x_confid);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s X-confid value is not set. Cannot build a=X-confid line\n", 
                      sdp_p->debug_str);
        }
    }
    return (SDP_SUCCESS);
}

/*
 * Parsing api for session level a=group as define in RFC3388
 * 
 * group-attribute    = "a=group:" semantics
 *                          *(space identification-tag)
 * semantics          = "LS" | "FID" | "FEC"
 *    where *(space identification-tag) = 0 or more tokens seperated
 *     by spaces
 */
sdp_result_e sdp_parse_attr_group (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                   const char *ptr)
{
    sdp_result_e  result;
    char tmp[SDP_MAX_STRING_LEN];
    int i=0;

    if (!attr_p) {
        return (SDP_FAILURE);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }
    
    /* Init group attr values. */
    attr_p->attr.group_data.group_attr = SDP_GROUP_ATTR_UNSUPPORTED;
    attr_p->attr.group_data.num_group_id =0;
    for (i=0; i<SDP_MAX_GROUP_STREAM_ID; i++) {
        attr_p->attr.group_data.group_id_arr[i] = NULL;
    }

    /* Find the a=group:<attrib> <id1> < id2> ... values */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Error: No group attribute value specified for "
                     "a=group line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_GROUP_LINE);
    }
    
    for (i=0; i < SDP_MAX_GROUP_ATTR_VAL; i++) {
        if (strncasecmp(tmp, sdp_group_attr_val[i].name,
                        sdp_group_attr_val[i].strlen) == 0) {
            attr_p->attr.group_data.group_attr = (sdp_group_attr_e)i;

            if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                SDP_PRINT("%s Parsed a=%s:%s", sdp_p->debug_str,
                          sdp_get_attr_name(attr_p->type), 
                          sdp_get_group_attr_name(
                            attr_p->attr.group_data.group_attr));
            }
            break;
        }
    }

    if (attr_p->attr.group_data.group_attr == SDP_GROUP_ATTR_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Group attribute type unsupported (%s).", 
                     sdp_p->debug_str, tmp);
        }
    }

    
    /*
     * Scan the input line after group:<attr>  to the maximum number 
     * of ids available.
     */
    for (i=0; i<SDP_MAX_GROUP_STREAM_ID; i++) {

        ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
        if (result != SDP_SUCCESS) {
            if (result == SDP_FIELD_VALUE_OVERFLOW) {
                sdp_p->conf_p->num_invalid_param++;
                return result;
            }
            break;
        }
        attr_p->attr.group_data.group_id_arr[i] = strdup(tmp);
        attr_p->attr.group_data.num_group_id++;

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s    Parsed group line id : %s", 
                      sdp_p->debug_str,
                      attr_p->attr.group_data.group_id_arr[i]);
        }
    }
    
    if (attr_p->attr.group_data.num_group_id == 0) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No group ids "
                     "for a=group line", sdp_p->debug_str);
        }
    }
    return (SDP_SUCCESS);
}

/*
 * Building api for session level a=group as define in RFC3388
 * 
 * group-attribute    = "a=group:" semantics
 *                          *(space identification-tag)
 * semantics          = "LS" | "FID"
 *    where *(space identification-tag) = 0 or more tokens seperated
 *     by spaces
 */
sdp_result_e sdp_build_attr_group (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                   char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    int i=0;
    char *endbuf_p = *ptr + len;
    
    *ptr += snprintf(*ptr, len, "a=%s:%s", 
                     sdp_attr[attr_p->type].name, 
                     sdp_get_group_attr_name (attr_p->attr.group_data.group_attr));

    len = endbuf_p - *ptr;
    for (i=0; i < attr_p->attr.group_data.num_group_id; i++) {
        if (attr_p->attr.group_data.group_id_arr[i]) {
            *ptr += snprintf(*ptr, len, " %s", 
                         attr_p->attr.group_data.group_id_arr[i]);
            len = endbuf_p - *ptr;
        }
    }

    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);

}

/* Parse the rtcp attribute 
 * "a=rtcp:<rtcp-port> <optional-address>"
 *  <optional-address> = <nettype><addrtype><connect-addr>
 */
sdp_result_e sdp_parse_attr_rtcp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                  const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    attr_p->attr.rtcp.port = 0;
    attr_p->attr.rtcp.nettype = SDP_NT_UNSUPPORTED;
    attr_p->attr.rtcp.addrtype = SDP_AT_UNSUPPORTED;
    attr_p->attr.rtcp.conn_addr[0] = '\0';

    /* Find the rtcp port */
    attr_p->attr.rtcp.port = sdp_getnextnumtok(ptr, &ptr, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No rtcp port value specified for "
                     "a=rtcp line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_LINE);
    }
    
    /* Find the network type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        /* This is o.k. because there is no connection address here */
        return (SDP_SUCCESS);
    }

    for (i = 0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (strncasecmp(tmp, sdp_nettype[i].name,
                        sdp_nettype[i].strlen) == 0) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.rtcp.nettype = (sdp_nettype_e)i;
            }
        }
    }
    if (attr_p->attr.rtcp.nettype == SDP_NT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Network type unsupported "
                     "(%s) for a=rtcp", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_LINE);
    }

    /* Find the address type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_LINE);
    }
    for (i = 0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.rtcp.addrtype = (sdp_addrtype_e)i;
            }
        }
    }
    if (attr_p->attr.rtcp.addrtype == SDP_AT_UNSUPPORTED) {
        if (strncmp(tmp, "*", 1) == 0) {
            attr_p->attr.rtcp.addrtype = SDP_AT_FQDN;
        } else {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: Address type unsupported "
                         "(%s) for a=rtcp", sdp_p->debug_str, tmp);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_RTCP_LINE);
        }
    }

    /* Find the destination addr */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.rtcp.conn_addr, 
                            " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No connect address specified for "
                      "a=rtcp", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_LINE);
    }

    return (SDP_SUCCESS);
}


sdp_result_e sdp_build_attr_rtcp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                  char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    if (attr_p->attr.rtcp.conn_addr[0] != '\0') {
        *ptr += snprintf(*ptr, len, "a=%s:%ld %s %s %s", 
                         sdp_get_attr_name(attr_p->type),
                         attr_p->attr.rtcp.port,
                         sdp_get_network_name(attr_p->attr.rtcp.nettype),
                         sdp_get_address_name(attr_p->attr.rtcp.addrtype),
                         attr_p->attr.rtcp.conn_addr);
    }
    else {
        *ptr += snprintf(*ptr, len, "a=%s:%ld",
                         sdp_get_attr_name(attr_p->type),
                         attr_p->attr.rtcp.port);
    }

    *ptr += snprintf(*ptr, len, "\r\n");
    return (SDP_SUCCESS);
}


/* Parse the source-filter attribute 
 * "a=source-filter:<filter-mode><filter-spec>"
 *  <filter-spec> = <nettype><addrtype><dest-addr><src_addr><src_addr>...
 */
sdp_result_e sdp_parse_attr_source_filter (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    attr_p->attr.source_filter.mode = SDP_FILTER_MODE_NOT_PRESENT;
    attr_p->attr.source_filter.nettype = SDP_NT_UNSUPPORTED;
    attr_p->attr.source_filter.addrtype = SDP_AT_UNSUPPORTED;
    attr_p->attr.source_filter.dest_addr[0] = '\0';
    attr_p->attr.source_filter.num_src_addr = 0;

    /* Find the filter mode */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No src filter attribute value specified for "
                     "a=source-filter line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_SRC_FILTER_LINE);
    }
    for (i = 0; i < SDP_MAX_FILTER_MODE; i++) {
        if (strncasecmp(tmp, sdp_src_filter_mode_val[i].name,
                        sdp_src_filter_mode_val[i].strlen) == 0) {
            attr_p->attr.source_filter.mode = (sdp_src_filter_mode_e)i;
            break;
        }
    }
    if (attr_p->attr.source_filter.mode == SDP_FILTER_MODE_NOT_PRESENT) {
        /* No point continuing */
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid src filter mode for a=source-filter "
                     "line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_SRC_FILTER_LINE);
    }
    
    /* Find the network type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    for (i = 0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (strncasecmp(tmp, sdp_nettype[i].name,
                        sdp_nettype[i].strlen) == 0) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.source_filter.nettype = (sdp_nettype_e)i;
            }
        }
    }
    if (attr_p->attr.source_filter.nettype == SDP_NT_UNSUPPORTED) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Network type unsupported "
                     "(%s) for a=source-filter", sdp_p->debug_str, tmp);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_SRC_FILTER_LINE);
    }

    /* Find the address type */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    for (i = 0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.source_filter.addrtype = (sdp_addrtype_e)i;
            }
        }
    }
    if (attr_p->attr.source_filter.addrtype == SDP_AT_UNSUPPORTED) {
        if (strncmp(tmp, "*", 1) == 0) {
            attr_p->attr.source_filter.addrtype = SDP_AT_FQDN;
        } else {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: Address type unsupported "
                         "(%s) for a=source-filter", sdp_p->debug_str, tmp);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_SRC_FILTER_LINE);
        }
    }

    /* Find the destination addr */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.source_filter.dest_addr, 
                            " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s No filter destination address specified for "
                      "a=source-filter", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_SRC_FILTER_LINE);
    }

    /* Find the list of source address to apply the filter */
    for (i = 0; i < SDP_MAX_SRC_ADDR_LIST; i++) {
        ptr = sdp_getnextstrtok(ptr, attr_p->attr.source_filter.src_list[i], 
                                " \t", &result);
        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.source_filter.num_src_addr++;
    }
    if (attr_p->attr.source_filter.num_src_addr == 0) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No source list provided "
                     "for a=source-filter", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_SRC_FILTER_LINE);
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_source_filter (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    int i = 0;
    char *endbuf_p = *ptr + len;

    *ptr += snprintf(*ptr, len, "a=%s:%s %s %s %s", 
         sdp_get_attr_name(attr_p->type),
         sdp_get_src_filter_mode_name(attr_p->attr.source_filter.mode),
         sdp_get_network_name(attr_p->attr.source_filter.nettype),
         sdp_get_address_name(attr_p->attr.source_filter.addrtype),
         attr_p->attr.source_filter.dest_addr);

    len = endbuf_p - *ptr;
    for (i = 0; i < attr_p->attr.source_filter.num_src_addr; i++) {
        *ptr += snprintf(*ptr, len," %s",
                         attr_p->attr.source_filter.src_list[i]);
        len = endbuf_p - *ptr;
    }

    *ptr += snprintf(*ptr, len, "\r\n");

    return (SDP_SUCCESS);
}

/* Parse the rtcp-unicast attribute 
 * "a=rtcp-unicast:<reflection|rsi>"
 */
sdp_result_e sdp_parse_attr_rtcp_unicast (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          const char *ptr)
{
    sdp_result_e result;
    u32 i;
    char tmp[SDP_MAX_STRING_LEN] = {0};

    attr_p->attr.u32_val = SDP_RTCP_UNICAST_MODE_NOT_PRESENT;

    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No rtcp unicast mode specified for "
                     "a=rtcp-unicast line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_UNICAST_LINE);
    }
    for (i = 0; i < SDP_RTCP_MAX_UNICAST_MODE;  i++) {
        if (strncasecmp(tmp, sdp_rtcp_unicast_mode_val[i].name,
                        sdp_rtcp_unicast_mode_val[i].strlen) == 0) {
            attr_p->attr.u32_val = i;
            break;
        }
    }
    if (attr_p->attr.u32_val == SDP_RTCP_UNICAST_MODE_NOT_PRESENT) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid rtcp unicast mode for "
                     "a=rtcp-unicast line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_UNICAST_LINE);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_rtcp_unicast (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    if (attr_p->attr.u32_val >= SDP_RTCP_MAX_UNICAST_MODE) {
        return (SDP_INVALID_PARAMETER);
    }
    *ptr += snprintf(*ptr, len, "a=%s:%s\r\n", 
                     sdp_get_attr_name(attr_p->type),
                     sdp_get_rtcp_unicast_mode_name(
                     (sdp_rtcp_unicast_mode_e)attr_p->attr.u32_val));

    return (SDP_SUCCESS);
}


/* Parse the rtcp-fb attribute 
 * "a=rtcp-fb:<payload type> <nack|nack pli>"
 */
sdp_result_e sdp_parse_attr_rtcp_fb (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr)
{
    sdp_result_e result;
    sdp_rtcp_fb_t *rtcp_fb_p;
    char tmp[SDP_MAX_STRING_LEN] = {0};

    /* Find the payload type number. */
    rtcp_fb_p = &attr_p->attr.rtcp_fb;
    rtcp_fb_p->payload_num = sdp_getnextnumtok(ptr, &ptr, 
                                               " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No payload type specified for "
                     "rtcp-fb attribute.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_FB_LINE);
    }

    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No rtcp-fb value specified for "
                     "a=rtcp-fb line", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_RTCP_FB_LINE);
    }

    /* For now, we only support nack */
    if (strncasecmp(tmp, "nack", 4)== 0) {
        strcpy(rtcp_fb_p->rtcp_fb_val, "nack");
        memset(tmp, 0, SDP_MAX_STRING_LEN);
        ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
        strcpy(rtcp_fb_p->rtcp_fb_nack_param, tmp);
    }
    else {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: rtcp-fb value is not nack in "
                     "a=rtcp-fb line", sdp_p->debug_str);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_rtcp_fb (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:%d %s %s\r\n", 
                     sdp_get_attr_name(attr_p->type),
                     attr_p->attr.rtcp_fb.payload_num,
                     attr_p->attr.rtcp_fb.rtcp_fb_val,
                     attr_p->attr.rtcp_fb.rtcp_fb_nack_param);

    return (SDP_SUCCESS);
}

#define UNSPECIFIED_LOSS_RLE 0xffff
#define MAX_SIZE_LOSS_RLE 0xfffe

/* 
 * Parsing api for media level a=rtcp-rsize attribute as defined in RFC 5506
 *
 * "a=rtcp-rsize" CRLF
 * This is a property attribute which does not take a value
 */
sdp_result_e sdp_parse_attr_rtcp_rsize (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                        const char *ptr)
{
    attr_p->attr.rtcp_rsize = FALSE;  /* Set default */
    if (*ptr == '\0' || *ptr == '\r' || *ptr == '\n') {
        attr_p->attr.rtcp_rsize = TRUE;
        return SDP_SUCCESS;
    }
    return SDP_INVALID_RTCP_RSIZE_LINE;
}

/*
 * Parsing api for media level a=rtcp-xr attribute as define in RFC3611
 * 
 * "a=rtcp-xr:[xr-format * (SP xr-format0] CRLF
 *
 * xr-format = pkt-loss-rle
 *           / stat-summary
 *           / multicast-acq
 *           / vqe-diagnostic-counters
 *
 * pkt-loss-rle = "pkt-loss-rle" ["=" max-size]
 * stat-summary = "stat-summary" ["=" stat-flag * ("," stat-flag)]
 *
 * stat-flag = "loss"
 *           / "dup"
 *           / "jitt"
 *
 * max-size = 1*DIGIT; maximum block size in octets
 * DIGIT = %x30-39

 *
 */
sdp_result_e sdp_parse_attr_rtcp_xr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr)
{
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN] = {0};
    sdp_rtcp_xr_t *rtcp_xr_p;
    /* Set the default values */
    rtcp_xr_p = &attr_p->attr.rtcp_xr;
    rtcp_xr_p->pkt_loss_rle_val = UNSPECIFIED_LOSS_RLE;
    rtcp_xr_p->per_loss_rle_val = UNSPECIFIED_LOSS_RLE;
    rtcp_xr_p->stat_summary_val = 0x0;
    rtcp_xr_p->multicast_acq_val = FALSE;
    rtcp_xr_p->diagnostic_counters_val = FALSE;
    
    /* For now we will only support pkt-loss-rle, post-repair-loss-rle, 
       and stat-summary */
    while(1) {
        /* Check whether there is anything left in this line */
        if (*ptr == '\0' || *ptr == '\r' || *ptr == '\n') {
            return (SDP_SUCCESS);
        }

        ptr = sdp_getnextstrtok(ptr, tmp, "= \t", &result);
        if (result != SDP_SUCCESS) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                SDP_WARN("%s Warning: No rtcp xr mode specified for "
                         "a=rtcp-xr line", sdp_p->debug_str);
            }
            return (SDP_INVALID_RTCP_XR_LINE);
        }

        /* Process pkt-loss-rle attribute */
        if (strcasecmp(tmp, sdp_rtcp_xr_param[SDP_RTCP_XR_PLR].name) == 0) {
            /* CHeck whether there is a maximum defined */
            if (*ptr == '=') {
                ptr = sdp_getnextstrtok(ptr, tmp, "= ", &result);
                if (result != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: Incompleted pkt-loss-rle mode "
                                 "specified for a=rtcp-xr line "
                                 "missing the value of max-size", 
                                 sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    return (SDP_INVALID_RTCP_XR_LINE);
                }
                rtcp_xr_p->pkt_loss_rle_val = atoi(tmp);
            }
            else {
                /* Set maximum number allowed and let the application 
                   to decide what the proper value should be used */
                rtcp_xr_p->pkt_loss_rle_val = MAX_SIZE_LOSS_RLE;
            }
        }
        /* Process stat-summary attribute */
        else if (strcasecmp(tmp, sdp_rtcp_xr_param[SDP_RTCP_XR_SS].name) == 0){
            /* Check whether there are any stat flags */
            if (*ptr == '=') {
                /* Skip the equal sign */
                ptr++;
                while(1) {
                    if (*ptr == '\0' || *ptr == '\r' || 
                        *ptr == '\n' || *ptr == ' ' ) {
                        break;
                    }
                    else {
                        ptr = sdp_getnextstrtok(ptr, tmp, ", ", &result);
                        if (result != SDP_SUCCESS) {
                            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                                SDP_WARN("%s Warning: Incompleted stat-flag "
                                         "specified for a=rtcp-xr line", 
                                         sdp_p->debug_str);
                            }
                            sdp_p->conf_p->num_invalid_param++;
                            return (SDP_INVALID_RTCP_XR_LINE);
                        }

                        if (strncasecmp(tmp, "loss", 4) == 0) {
                            SDP_RTCP_XR_SET_LOSS(rtcp_xr_p->stat_summary_val);
                        }
                        else if (strncasecmp(tmp, "dup", 3) == 0) {
                            SDP_RTCP_XR_SET_DUP(rtcp_xr_p->stat_summary_val);
                        }
                        else if (strncasecmp(tmp, "jitt", 4) == 0) {
                            SDP_RTCP_XR_SET_JITT(rtcp_xr_p->stat_summary_val);
                        }
                        else if (strncasecmp(tmp, "TTL", 3) == 0) {
                            SDP_RTCP_XR_SET_TTL(rtcp_xr_p->stat_summary_val);
                        }
                        else if (strncasecmp(tmp, "HL", 2) == 0) {
                            SDP_RTCP_XR_SET_HL(rtcp_xr_p->stat_summary_val);
                        }
                        else {
                            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                                SDP_WARN("%s Warning: Illegal stat-flag "
                                         "specified for a=rtcp-xr line", 
                                         sdp_p->debug_str);
                            }
                            sdp_p->conf_p->num_invalid_param++;
                            return (SDP_INVALID_RTCP_XR_LINE);
                        }
                    }
                }
            }
            else {
                /* We will set the default to everything and let
                   the application make the decision */
                SDP_RTCP_XR_SET_LOSS(rtcp_xr_p->stat_summary_val);
                SDP_RTCP_XR_SET_DUP(rtcp_xr_p->stat_summary_val);
                SDP_RTCP_XR_SET_JITT(rtcp_xr_p->stat_summary_val);
                SDP_RTCP_XR_SET_TTL(rtcp_xr_p->stat_summary_val);
                SDP_RTCP_XR_SET_HL(rtcp_xr_p->stat_summary_val);
            }
        }
        /* Process post-repair-loss-rle attribute */
        else if (strcasecmp(tmp, sdp_rtcp_xr_param[SDP_RTCP_XR_PERLR].name)
                 == 0){
            /* CHeck whether there is a maximum defined */
            if (*ptr == '=') {
                ptr = sdp_getnextstrtok(ptr, tmp, "= ", &result);
                if (result != SDP_SUCCESS) {
                    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                        SDP_WARN("%s Warning: Incompleted post-repair-loss-rle "
                                 "mode specified for a=rtcp-xr line "
                                 "missing the value of max-size", 
                                 sdp_p->debug_str);
                    }
                    sdp_p->conf_p->num_invalid_param++;
                    return (SDP_INVALID_RTCP_XR_LINE);
                }
                rtcp_xr_p->per_loss_rle_val = atoi(tmp);
            }
            else {
                /* Set maximum number allowed and let the application 
                   to decide what the proper value should be used */
                rtcp_xr_p->per_loss_rle_val = MAX_SIZE_LOSS_RLE;
            }
        }
        /* Process multicast-acq attribute */
        else if (strcasecmp(tmp, sdp_rtcp_xr_param[SDP_RTCP_XR_MA].name)
                 == 0) {
            rtcp_xr_p->multicast_acq_val = TRUE;
        }
        else if (strcasecmp(tmp, sdp_rtcp_xr_param[SDP_RTCP_XR_DC].name)
                 == 0) {
            rtcp_xr_p->diagnostic_counters_val = TRUE;
        }
        /* New options will be added here for RTCP XR attribute */
        else {
            if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                SDP_PRINT("%s Info: unsupported option "
                          "specified for a=rtcp-xr line", 
                          sdp_p->debug_str);
            }
        }
    }

    return (SDP_SUCCESS);
}


sdp_result_e sdp_build_attr_rtcp_rsize (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                        char **ptr, u16 len) 
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:", sdp_get_attr_name(attr_p->type));
    *ptr += snprintf(*ptr, len, "\r\n");
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_rtcp_xr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     char **ptr, u16 len)
{
    GCC_UNUSED(sdp_p);
    *ptr += snprintf(*ptr, len, "a=%s:", sdp_get_attr_name(attr_p->type));

    if (attr_p->attr.rtcp_xr.pkt_loss_rle_val != 0xffff) {
        if (attr_p->attr.rtcp_xr.pkt_loss_rle_val == 0) {
            *ptr += snprintf(*ptr, len, "pkt-loss-rle ");
        }
        else {
            *ptr += snprintf(*ptr, len, "pkt-loss-rle=%d ",
                             attr_p->attr.rtcp_xr.pkt_loss_rle_val);
        }
    }

    if (attr_p->attr.rtcp_xr.stat_summary_val != 0) {
        *ptr += snprintf(*ptr, len, "stat-summary=loss,dup,jitt");        
    }

    *ptr += snprintf(*ptr, len, "\r\n");
    
    return (SDP_SUCCESS);
}


/*
 * store_sdescriptions_mki_or_lifetime
 *
 * Verifies the syntax of the MKI or lifetime parameter and stores 
 * it in the sdescriptions attribute struct.
 *
 * Inputs:
 *   buf    - pointer to MKI or lifetime string assumes string is null
 *            terminated.
 *   attr_p - pointer to attribute struct
 *
 * Outputs:
 *   Return TRUE all is good otherwise FALSE for error.
 */
tinybool 
store_sdescriptions_mki_or_lifetime (char *buf, sdp_attr_t *attr_p) 
{
   
    tinybool  result;
    u16       mkiLen;
    char      mkiValue[SDP_SRTP_MAX_MKI_SIZE_BYTES];
    
    /* MKI has a colon */
    if (strstr(buf, ":")) {
        result = verify_sdescriptions_mki(buf, mkiValue, &mkiLen);
	if (result) {
	    attr_p->attr.srtp_context.mki_size_bytes = mkiLen;
	    sstrncpy((char*)attr_p->attr.srtp_context.mki, mkiValue,
	             SDP_SRTP_MAX_MKI_SIZE_BYTES);
	}
	
    } else {
        result =  verify_sdescriptions_lifetime(buf);
	if (result) {
	    sstrncpy((char*)attr_p->attr.srtp_context.master_key_lifetime, buf,
	             SDP_SRTP_MAX_LIFETIME_BYTES);
	}
    }
    
    return result;

}

/*
 * sdp_parse_sdescriptions_key_param
 *
 * This routine parses the srtp key-params pointed to by str.
 *
 * key-params    = <key-method> ":" <key-info> 
 * key-method    = "inline" / key-method-ext [note V9 only supports 'inline']
 * key-info      = srtp-key-info 
 * srtp-key-info = key-salt ["|" lifetime] ["|" mki] 
 * key-salt      = 1*(base64)   ; binary key and salt values 
 *                              ; concatenated together, and then 
 *                              ; base64 encoded [section 6.8 of  
 *                              ; RFC2046] 
 *   
 * lifetime      = ["2^"] 1*(DIGIT)   
 * mki           = mki-value ":" mki-length 
 * mki-value     = 1*DIGIT 
 * mki-length    = 1*3DIGIT   ; range 1..128.   
 *
 * Inputs: str - pointer to beginning of key-params and assumes
 *               null terminated string.
 */
tinybool 
sdp_parse_sdescriptions_key_param (const char *str, sdp_attr_t *attr_p, 
                                   sdp_t *sdp_p) 
{
    char            buf[SDP_MAX_STRING_LEN],
                    base64decodeData[SDP_MAX_STRING_LEN],
                    *ptr;
    sdp_result_e    result = SDP_SUCCESS;
    tinybool        keyFound = FALSE;
    int             len,
                    keySize,
    		    saltSize;
    base64_result_t status;
  
    ptr = (char*)str;
    if (strncasecmp(ptr, "inline:", 7) != 0) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Could not find keyword inline", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    }
    
    /* advance pass the inline key word */
    ptr = ptr + 7;
    ptr = sdp_getnextstrtok(ptr, buf, "|", &result);
    while (result == SDP_SUCCESS) {
        /* the fist time this loop executes, the key is gotten */
        if (keyFound == FALSE) {
	    keyFound = TRUE;
	    len = SDP_MAX_STRING_LEN;
	    /* The key is base64 encoded composed of the master key concatenated with the
	     * master salt.
	     */
	    status = base64_decode((unsigned char *)buf, strlen(buf), 
	                           (unsigned char *)base64decodeData, &len);
				   
	    if (status != BASE64_SUCCESS) {
	        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
		    SDP_ERROR("%s key-salt error decoding buffer: %s",
			      sdp_p->debug_str, BASE64_RESULT_TO_STRING((int) status));
	        }
	        return FALSE;
	   
	    }
	   
	    keySize = attr_p->attr.srtp_context.master_key_size_bytes;
	    saltSize = attr_p->attr.srtp_context.master_salt_size_bytes;
	   
	    if (len != keySize + saltSize) {
		      
	        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
		    SDP_ERROR("%s key-salt size doesn't match: (%d, %d, %d)",
		              sdp_p->debug_str, len, keySize, saltSize);
	        }

	        return(FALSE);
		      
	    }
	   
	    bcopy(base64decodeData, attr_p->attr.srtp_context.master_key, keySize);
		 
	    bcopy(base64decodeData + keySize,
	          attr_p->attr.srtp_context.master_salt, saltSize);
	   
	    /* Used only for MGCP */
	    SDP_SRTP_CONTEXT_SET_MASTER_KEY
	             (attr_p->attr.srtp_context.selection_flags);
	    SDP_SRTP_CONTEXT_SET_MASTER_SALT
	             (attr_p->attr.srtp_context.selection_flags);
		     
       } else if (store_sdescriptions_mki_or_lifetime(buf, attr_p) == FALSE) {
           return FALSE;
       }
       
       /* if we haven't reached the end of line, get the next token */
       ptr = sdp_getnextstrtok(ptr, buf, "|", &result);
    }
   
    /* if we didn't find the key, error out */
    if (keyFound == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Could not find sdescriptions key", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    }
   
    return TRUE;
       
}

/*
 * sdp_build_attr_sdescriptions
 *
 * Builds a=crypto line for attribute type SDP_ATTR_SDESCRIPTIONS.
 *
 * a=crypto:tag 1*WSP crypto-suite 1*WSP key-params
 *
 * Where key-params = inline: <key|salt> ["|"lifetime] ["|" MKI:length]
 * The key and salt is base64 encoded and lifetime and MKI/length are optional.
 */
 
sdp_result_e
sdp_build_attr_sdescriptions (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                              char **ptr, u16 len)
{
    
    unsigned char  base64_encoded_data[60];
    unsigned char  base64_encoded_input[60];
    int            keySize,
                   saltSize,
		   outputLen;
    base64_result_t status;
    
    keySize = attr_p->attr.srtp_context.master_key_size_bytes;
    saltSize = attr_p->attr.srtp_context.master_salt_size_bytes;
    
    /* concatenate the master key + salt then base64 encode it */
    bcopy(attr_p->attr.srtp_context.master_key, 
          base64_encoded_input, keySize);
	  
    bcopy(attr_p->attr.srtp_context.master_salt, 
          base64_encoded_input + keySize, saltSize);
	
    outputLen = 60;  
    status = base64_encode(base64_encoded_input, keySize + saltSize,
		           base64_encoded_data, &outputLen);
			   
    if (status != BASE64_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Error: Failure to Base64 Encoded data (%s) ", 
                       sdp_p->debug_str, BASE64_RESULT_TO_STRING((int) status));
        }
	return (SDP_INVALID_PARAMETER);
    
    }
    
    base64_encoded_data[outputLen] = 0;
    
    /* lifetime and MKI parameters are optional. Only inlcude them if 
     * they were set.
     */
     
    
    if (attr_p->attr.srtp_context.master_key_lifetime[0] != 0 && 
        attr_p->attr.srtp_context.mki[0] != 0) {
	
	*ptr += snprintf(*ptr, len, "a=%s:%ld %s inline:%s|%s|%s:%d\r\n",
	                 sdp_attr[attr_p->type].name, 
			 attr_p->attr.srtp_context.tag,
                         sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
			 base64_encoded_data,
			 attr_p->attr.srtp_context.master_key_lifetime,
			 attr_p->attr.srtp_context.mki,
			 attr_p->attr.srtp_context.mki_size_bytes);
			 
	return SDP_SUCCESS;
	
    }
    
    /* if we get here, either lifetime is populated and mki and is not or mki is populated
     * and lifetime is not or neither is populated
     */
     
    if (attr_p->attr.srtp_context.master_key_lifetime[0] != 0) {
        *ptr += snprintf(*ptr, len, "a=%s:%ld %s inline:%s|%s\r\n",
	                 sdp_attr[attr_p->type].name, 
			 attr_p->attr.srtp_context.tag,
                         sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
			 base64_encoded_data,
			 attr_p->attr.srtp_context.master_key_lifetime);
    
    } else if (attr_p->attr.srtp_context.mki[0] != 0) {
        *ptr += snprintf(*ptr, len, "a=%s:%ld %s inline:%s|%s:%d\r\n",
	                 sdp_attr[attr_p->type].name, 
			 attr_p->attr.srtp_context.tag,
                         sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
			 base64_encoded_data,
			 attr_p->attr.srtp_context.mki,
			 attr_p->attr.srtp_context.mki_size_bytes);
    
    } else {
        *ptr += snprintf(*ptr, len, "a=%s:%ld %s inline:%s\r\n",
	                 sdp_attr[attr_p->type].name, 
			 attr_p->attr.srtp_context.tag,
                         sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
			 base64_encoded_data);
    
    }
       
    return SDP_SUCCESS;

}


/*
 * sdp_parse_attr_srtp
 *
 * Parses Session Description for Protocol Security Descriptions
 * version 2 or version 9. Grammar is of the form:
 *
 * a=crypto:<tag> <crypto-suite> <key-params> [<session-params>] 
 *
 * Note session-params is not supported and will not be parsed. 
 * Version 2 does not contain a tag.
 *
 * Inputs:
 *   sdp_p  - pointer to sdp handle
 *   attr_p - pointer to attribute structure
 *   ptr    - pointer to string to be parsed
 *   vtype  - version type
 */
sdp_result_e 
sdp_parse_attr_srtp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                     const char *ptr, sdp_attr_e vtype)
{

    char         tmp[SDP_MAX_STRING_LEN];
    sdp_result_e result;
    int          k = 0;
       
    /* initialize only the optional parameters */
    attr_p->attr.srtp_context.master_key_lifetime[0] = 0;
    attr_p->attr.srtp_context.mki[0] = 0;
    
     /* used only for MGCP */
    SDP_SRTP_CONTEXT_SET_ENCRYPT_AUTHENTICATE
             (attr_p->attr.srtp_context.selection_flags);
	     
    /* get the tag only if we are version 9 */
    if (vtype == SDP_ATTR_SDESCRIPTIONS) {
        attr_p->attr.srtp_context.tag = 
                sdp_getnextnumtok(ptr, &ptr, " \t", &result);

        if (result != SDP_SUCCESS) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Could not find sdescriptions tag",
		          sdp_p->debug_str);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
       
        }
    }
    
    /* get the crypto suite */
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Could not find sdescriptions crypto suite", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
       
    if (!sdp_parse_context_crypto_suite(tmp, attr_p, sdp_p)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Unsupported crypto suite", sdp_p->debug_str);
        }
	return (SDP_INVALID_PARAMETER);
    }
   
    ptr = sdp_getnextstrtok(ptr, tmp, " \t", &result);
    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Could not find sdescriptions key params", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
   
    if (!sdp_parse_sdescriptions_key_param(tmp, attr_p, sdp_p)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Failed to parse key-params", sdp_p->debug_str);
        }
	return (SDP_INVALID_PARAMETER);
    } 
    
    /* if there are session parameters, scan the session parameters
     * into tmp until we reach end of line. Currently the sdp parser
     * does not parse session parameters but if they are present,
     * we store them for the application.
     */
     
    while (*ptr && *ptr != '\n' && *ptr != '\r' && k < SDP_MAX_STRING_LEN) {
         tmp[k++] = *ptr++;
    }
    
    if (k) {
        tmp[k] = 0;
	attr_p->attr.srtp_context.session_parameters = strdup(tmp);
    }
       
    return SDP_SUCCESS;  
       
}

/* Parses crypto attribute based on the sdescriptions version
 * 9 grammar.
 *
 */
 
sdp_result_e 
sdp_parse_attr_sdescriptions (sdp_t *sdp_p, sdp_attr_t *attr_p,
                              const char *ptr)
{

   return sdp_parse_attr_srtp(sdp_p, attr_p, ptr, 
                              SDP_ATTR_SDESCRIPTIONS);

}

/* Parses X-crypto attribute based on the sdescriptions version
 * 2 grammar.
 *
 */
 
sdp_result_e sdp_parse_attr_srtpcontext (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         const char *ptr)
{
   
    return sdp_parse_attr_srtp(sdp_p, attr_p, ptr, 
                               SDP_ATTR_SRTP_CONTEXT);    
}

sdp_result_e sdp_parse_attr_string (sdp_t *sdp_p, sdp_attr_t *attr_p, 
                                    const char *ptr)
{
    sdp_result_e  result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.string_val, "", &result);

    if (result != SDP_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: No string token found for %s attribute",
                     sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        sdp_p->conf_p->num_invalid_param++;
        SDP_PRINT("%s() invalid parameter? cnt=%ld\n", 
                  __FUNCTION__, sdp_p->conf_p->num_invalid_param); 
        return (result);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type), 
                      attr_p->attr.string_val);
        }
        return (SDP_SUCCESS);
    }
}
