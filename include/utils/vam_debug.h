/*------------------------------------------------------------------
 * Logging API.
 * Derived from unicorn/ad_debug.h
 *
 * March 2006, Atif Faheem
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef VAM_DEBUG_H__
#define VAM_DEBUG_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#define VAM_DEBUG_LEVEL_CRITICAL 0
#define VAM_DEBUG_LEVEL_ERROR 1
#define VAM_DEBUG_LEVEL_TRACE 2
#define VAM_DEBUG_LEVEL_DETAIL 3
#define VAM_DEBUG_LEVEL_EVERYTHING 4

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct _vam_debug_cfg_t {
	int vam_debug_level;
	unsigned int debug_target_client_ip;
	unsigned int debug_target_server_ip;
	unsigned int para[32];
} vam_debug_cfg_t;

#define vam_debug_direct_printf(fmt, args...) \
do { \
	fprintf(stderr, fmt ,## args); \
	if(fmt[0] && fmt[strlen(fmt)-1] != '\n') \
		fprintf(stderr, "\n"); \
} while(0) 


#define vam_debug_decision(level) \
	(vam_debug_cfg_p && \
	 (level <= vam_debug_cfg_p->vam_debug_level ) && \
	 (!vam_debug_cfg_p->debug_target_client_ip || \
	  !vam_debug_current_client_ip || \
	  vam_debug_cfg_p->debug_target_client_ip == vam_debug_current_client_ip) && \
	 (!vam_debug_cfg_p->debug_target_server_ip || \
	  !vam_debug_current_server_ip || \
	  vam_debug_cfg_p->debug_target_server_ip == vam_debug_current_server_ip))


#define vam_debug_print_decision(level) \
	((vam_debug_cfg_p && \
		(level <= VAM_DEBUG_LEVEL_ERROR || \
		 level <= vam_debug_cfg_p->vam_debug_level ) && \
		(!vam_debug_cfg_p->debug_target_client_ip || \
		 !vam_debug_current_client_ip || \
		 vam_debug_cfg_p->debug_target_client_ip == vam_debug_current_client_ip) && \
		(!vam_debug_cfg_p->debug_target_server_ip || \
		 !vam_debug_current_server_ip || \
		 vam_debug_cfg_p->debug_target_server_ip == vam_debug_current_server_ip) ))


#define vam_debug(level, fmt, level_string, args...) \
do { \
	if(!vam_debug_cfg_p){ \
		vam_debug_cfg_direct_read_from_env(); \
	} \
	if (vam_debug_print_decision(level)) { \
		char *vam_debug_tmp_c = (char *)strrchr(__FILE__, '/'); \
                time_t __now = time(NULL);                  \
                struct tm __now_tm;                         \
                char datestring[256];                       \
                (void)strftime (datestring, sizeof(datestring),       \
                                "%h %e %T", localtime_r(&__now, &__now_tm)); \
		if(vam_debug_tmp_c == NULL) \
			vam_debug_tmp_c = __FILE__; \
		else \
			vam_debug_tmp_c++; \
		if(level < VAM_DEBUG_LEVEL_EVERYTHING) { \
			if (vam_debug_cfg_p) \
                            vam_errorlog_printf("%s %s(%d)%s:%s:%d-> "fmt, datestring, vam_debug_getprocname(), getpid(), level_string, vam_debug_tmp_c, __LINE__ ,## args); \
			else \
                            vam_debug_direct_printf("%s (%d)%s:%s:%d-> "fmt, datestring, getpid(), level_string, vam_debug_tmp_c, __LINE__ ,## args); \
		} \
		if (vam_debug_cfg_p && vam_debug_cfg_p->vam_debug_level == VAM_DEBUG_LEVEL_EVERYTHING) \
                    vam_errorlog_printf_special("%s (%d)%s:%s:%d-> "fmt, datestring, getpid(), level_string, vam_debug_tmp_c, __LINE__ ,## args); \
	} \
} while (0)

#define vam_errorlog(level_string, fmt, args...) \
do { \
	vam_errorlog_printf("(%s)(%d)%s:%s:%d-> "fmt, vam_deubg_getprocname(), getpid(), level_string, __FILE__, __LINE__ ,## args); \
} while (0)

#define vam_debug_cfg_print_pkt_lev(level, level_str, buf, len, desc) \
	vam_debug_print_pkt_impl(level, level_str, buf, len, __FILE__, __LINE__, desc)

/*
 * Samples:
 * Please add '\n' to the end of the format string of the following functions.
 *
 * vam_debug_critical("not enough memory! need=%d\n", need);
 * vam_debug_error("cannot recognize message (type = %d)\n", type);
 * vam_debug_trace("got a data packet in recv_data_packet\n");
 *
 * Please DO NOT add '\n' to the end of the description string of the 
 * following functions.
 *
 * vam_debug_critical_perror("write");
 * vam_debug_error_perror("connect");
 * vam_debug_error_pkt("cannot recognize packet", buf, len);
 * vam_debug_trace_pkt("control message from client", buf, len);
 * vam_debug_data_pkt("data pkt to client", buf, len);
 */

#define vam_debug_critical_perror(_str) \
	vam_debug_critical("%s: %s\n", _str, strerror(errno))
#define vam_debug_critical(fmt, args...) \
	vam_debug(VAM_DEBUG_LEVEL_CRITICAL, fmt, "CRTC" ,## args)

#define vam_debug_log(fmt, args...) \
	vam_debug(VAM_DEBUG_LEVEL_ERROR, fmt, "LOG " ,## args)
#define vam_debug_log_perror(_str) \
	vam_debug_log("%s: %s\n", _str, strerror(errno))
#define vam_debug_log_pkt(desc, buf, len) \
	vam_debug_cfg_print_pkt_lev(VAM_DEBUG_LEVEL_ERROR, "LOG", buf, len, desc)

#define vam_debug_error_perror(_str) \
	vam_debug_error("%s: %s\n", _str, strerror(errno))
#define vam_debug_error(fmt, args...) \
	vam_debug(VAM_DEBUG_LEVEL_ERROR, fmt, "ERRO" ,## args)
#define vam_debug_error_pkt(desc, buf, len) \
	vam_debug_cfg_print_pkt_lev(VAM_DEBUG_LEVEL_ERROR, "ERRO", buf, len, desc)


#define vam_debug_trace_perror(_str) \
	vam_debug_trace("%s: %s\n", _str, strerror(errno))
#define vam_debug_trace(fmt, args...) \
	vam_debug(VAM_DEBUG_LEVEL_TRACE, fmt, "TRCE" ,## args)
#define vam_debug_trace_pkt(desc, buf, len) \
	vam_debug_cfg_print_pkt_lev(VAM_DEBUG_LEVEL_TRACE, "TRCE", buf, len, desc)

#define vam_debug_detail_perror(_str) \
	vam_debug_detail("%s: %s\n", _str, strerror(errno))
#define vam_debug_detail(fmt, args...) \
	vam_debug(VAM_DEBUG_LEVEL_DETAIL, fmt, "DETL" ,## args)
#define vam_debug_detail_pkt(desc, buf, len) \
	vam_debug_cfg_print_pkt_lev(VAM_DEBUG_LEVEL_DETAIL, "DETL", buf, len, desc)

#define vam_debug_data_pkt(desc, buf, len) \
	vam_debug_cfg_print_pkt_lev(VAM_DEBUG_LEVEL_EVERYTHING, "DATA", buf, len, desc)

#define vam_debug_assert(expr) \
do { \
	if(!(expr)) \
		vam_debug_critical(__STRING(expr)); \
	assert(expr); \
} while(0)

extern vam_debug_cfg_t * vam_debug_cfg_p;
extern unsigned int vam_debug_current_client_ip;
extern unsigned int vam_debug_current_server_ip;
extern unsigned int vam_debug_module_id;

extern int vam_debug_init_lib(char * send_path);
extern int vam_debug_set_default_module_id(unsigned int module_id);
extern void vam_debug_print_pkt_impl(int level, const char *level_str,
				     unsigned char *buf, int len,
				     char * file, int line, const char* description);
extern void vam_errorlog_printf(const char *fmt, ...);
extern void vam_errorlog_printf_special(const char *fmt, ...);
extern char* vam_debug_getprocname(void);
extern int vam_debug_setprocname(const char* name);
extern int vam_debug_get_cfg_p_no_create(char * path, vam_debug_cfg_t ** cfg_pp);
extern int vam_debug_get_cfg_p(char * path, vam_debug_cfg_t ** cfg_pp);
extern int vam_debug_return_cfg_p(vam_debug_cfg_t * cfg_p);
extern void vam_debug_set_parameter(unsigned int cip, unsigned int sip, int level,
				    vam_debug_cfg_t * cfg_p);  
extern void vam_debug_set_debug_level_directly(int level);
extern void vam_debug_set_debug_parameter_directly(unsigned int cip, unsigned int sip, int level);
extern void vam_debug_cfg_direct_read_from_env(void);

#ifdef __cplusplus
} 
#endif // __cplusplus

#endif // VAM_DEBUG_H__
