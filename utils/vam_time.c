/*------------------------------------------------------------------
 * VAM Time representation
 *
 * May 2006, Josh Gahm
 *
 * Copyright (c) 2006-2007, 2009-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../include/utils/vam_time.h"


const char * abs_time_to_str(abs_time_t t, char * buff, uint32_t buff_len)
{
    
    struct timeval tv = TIME_GET_A(timeval, t);
    struct tm bd_time;
    struct tm * bd_result;
    size_t strftime_result;
    size_t sprintf_result;


#define DATE_AND_TIME_LENGTH_INCLUDING_NULL 20
#define LONG_DATE_AND_TIME_LENGTH_INCLUDING_NULL 40
#define FRAC_SEC_LENGTH_INCLUDING_NULL 8
#define TOTAL_LENGTH_INCLUDING_NULL \
(DATE_AND_TIME_LENGTH_INCLUDING_NULL + FRAC_SEC_LENGTH_INCLUDING_NULL - 1)

    if (! buff || buff_len == 0)
        return NULL;

    if (buff_len < TOTAL_LENGTH_INCLUDING_NULL) {
        buff[0] = 0;
        return buff;
    }

    bd_result = localtime_r(&tv.tv_sec, &bd_time);
    ASSERT(bd_result == &bd_time, "Bad return from localtime_r\n");


    strftime_result = strftime(buff, DATE_AND_TIME_LENGTH_INCLUDING_NULL, "%F %T", &bd_time);
    ASSERT(strftime_result == DATE_AND_TIME_LENGTH_INCLUDING_NULL-1, 
           "Bad result, %"PRIdPTR", from strftime\n", strftime_result);

    sprintf_result = snprintf(&buff[DATE_AND_TIME_LENGTH_INCLUDING_NULL-1], 
                              buff_len - DATE_AND_TIME_LENGTH_INCLUDING_NULL + 1,
                             ".%06ld", tv.tv_usec);

    ASSERT(sprintf_result == FRAC_SEC_LENGTH_INCLUDING_NULL-1, 
           "Bad result from sprintf\n");
    
    return buff;
}

/*
 * Generates a printable string from an abs_time_t structure, with
 * granularity of whole seconds.
 *
 * @param[in]   t        - time for which a string is to be generated
 * @param[out]  buff     - buffer into which string will be written
 * @param[in]   buff_len - length of caller-supplied buffer
 */
const char * abs_time_to_str_secs(abs_time_t t, char * buff, uint32_t buff_len)
{
    struct timeval tv = TIME_GET_A(timeval, t);
    struct tm bd_time;
    struct tm * bd_result;
    size_t strftime_result;

    if (! buff || buff_len == 0) {
        return NULL;
    }

    if (buff_len < DATE_AND_TIME_LENGTH_INCLUDING_NULL) {
        buff[0] = 0;
        return buff;
    }

    bd_result = localtime_r(&tv.tv_sec, &bd_time);
    ASSERT(bd_result == &bd_time, "Bad return from localtime_r\n");

    strftime_result = strftime(buff, DATE_AND_TIME_LENGTH_INCLUDING_NULL,
                               "%FT%T", &bd_time);
    ASSERT(strftime_result == DATE_AND_TIME_LENGTH_INCLUDING_NULL-1, 
           "Bad result, %"PRIdPTR", from strftime\n", strftime_result);
    return buff;    
}


/*
 * Generates a printable string from an abs_time_t structure, with
 * granularity of whole seconds. This is formatted in a manner which 
 * is suitable for user consumption
 *
 * @param[in]   t        - time for which a string is to be generated
 * @param[out]  buff     - buffer into which string will be written
 * @param[in]   buff_len - length of caller-supplied buffer
 */
const char * abs_time_to_clean_str (abs_time_t t, char *buff, uint32_t buff_len)
{
    struct timeval tv = TIME_GET_A(timeval, t);
    struct tm bd_time;
    struct tm * bd_result;
    size_t strftime_result;

    if (! buff || buff_len == 0) {
        return NULL;
    }

    if (buff_len < LONG_DATE_AND_TIME_LENGTH_INCLUDING_NULL) {
        buff[0] = 0;
        return buff;
    }

    bd_result = localtime_r(&tv.tv_sec, &bd_time);
    ASSERT(bd_result == &bd_time, "Bad return from localtime_r\n");

    strftime_result = strftime(buff, LONG_DATE_AND_TIME_LENGTH_INCLUDING_NULL,
                               "%B %d, %Y %T", &bd_time);
    ASSERT(strftime_result < LONG_DATE_AND_TIME_LENGTH_INCLUDING_NULL-1, 
           "Bad result, %"PRIdPTR", from strftime\n", strftime_result);
    return buff; 
}

const char * rel_time_to_str(rel_time_t t, char * buff, uint32_t buff_len)
{
    boolean negative = TIME_CMP_R(lt, t, REL_TIME_0);
    size_t snprintf_result;

    if (! buff || ! buff_len)
        return NULL;
    
    if (negative)
        t = TIME_NEG_R(t);

    struct timeval tv = TIME_GET_R(timeval, t);

    snprintf_result = snprintf(buff, buff_len, "%s%ld.%06ld",
                               negative ? "-" : "",
                               tv.tv_sec, tv.tv_usec);
    ASSERT(snprintf_result > 0, "Bad snprintf result\n");
    return buff;
}


/**
 * Convert time string of format YY-MM-DDThh:mm:ss to time_t value
 * 
 * @param  : pointer to char that stores time string
 * @retval : time_t value of the time string
 *          
 */
time_t convert_strtime_to_timet(const char* strtime)
{
    int yy, mm, dd, hour, min, sec; 
    struct tm timem;
    time_t tme;
    
    sscanf(strtime, "%d-%d-%dT%d:%d:%d", &yy, &mm, &dd, &hour, &min, &sec);
 
    time(&tme);
    timem = *localtime(&tme);
    timem.tm_year = yy - 1900; 
    timem.tm_mon = mm - 1; 
    timem.tm_mday = dd;
    timem.tm_hour = hour; 
    timem.tm_min = min; 
    timem.tm_sec = sec;
    return(mktime(&timem));
}
