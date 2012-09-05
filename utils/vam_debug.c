/*------------------------------------------------------------------
 * Logging Implementation.
 * Derived from utils/ad_debug.c
 *
 * March 2006, Atif Faheem
 *
 * Copyright (c) 2006-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
# include <sys/shm.h>
#include <inttypes.h>

#include <utils/vam_debug.h>

vam_debug_cfg_t *vam_debug_cfg_p = NULL;
vam_debug_cfg_t vam_debug_cfg_no_share_mem;
unsigned int vam_debug_in_sighandler = 0;
unsigned int vam_debug_module_id = 0;
char vam_debug_process_name[256] = "";


static struct sockaddr_un sunx_errorlog = {AF_UNIX, "/tmp/vam_errorlog"};
static struct sockaddr_un sunx_errorsp = {AF_UNIX, "/tmp/vam_errorlog_sp"};
static struct sigaction old_usr2_act;

unsigned int vam_debug_current_client_ip = 0;
unsigned int vam_debug_current_server_ip = 0;

void
vam_debug_cfg_direct_read_from_env(){
	char * e;
	vam_debug_cfg_no_share_mem.vam_debug_level = -2;
	vam_debug_cfg_no_share_mem.debug_target_client_ip = INADDR_NONE;
	vam_debug_cfg_no_share_mem.debug_target_client_ip = INADDR_NONE;
	if((e = getenv("DEBUG_LEVEL")) != NULL){
		vam_debug_cfg_no_share_mem.vam_debug_level = atoi(e);
	}
	if((e = getenv("DEBUG_CLIENT_IP")) != NULL)
		vam_debug_cfg_no_share_mem.debug_target_client_ip = inet_addr(e);
	if((e = getenv("DEBUG_SERVER_IP")) != NULL)
		vam_debug_cfg_no_share_mem.debug_target_server_ip = inet_addr(e);
	if(vam_debug_cfg_no_share_mem.vam_debug_level < 0  ||
	   vam_debug_cfg_no_share_mem.vam_debug_level > VAM_DEBUG_LEVEL_DETAIL)
		vam_debug_cfg_no_share_mem.vam_debug_level =  VAM_DEBUG_LEVEL_ERROR;
	if(vam_debug_cfg_no_share_mem.debug_target_server_ip == INADDR_NONE)
		vam_debug_cfg_no_share_mem.debug_target_server_ip = 0;
	if(vam_debug_cfg_no_share_mem.debug_target_client_ip == INADDR_NONE)
		vam_debug_cfg_no_share_mem.debug_target_client_ip = 0;
	
	vam_debug_cfg_p = &vam_debug_cfg_no_share_mem;
	return;
}

static void
vam_debug_read_cfg(char * path, int * level, unsigned int * cip, unsigned int * sip){
	char buf[1024];
	int fd;
	int read_len;
	char * e;

	assert(path && level && cip && sip);
		
	*level = -1;
	*cip = INADDR_NONE;
	*sip = INADDR_NONE;
	sprintf(buf, "%s.cfg",  path);
	if((fd = open(buf, O_RDONLY)) >= 0){
		while((read_len = read(fd, buf, sizeof(buf)))== -1 &&
		      errno == EINTR);
		if (read_len > 0){
			buf[sizeof(buf)-1] = 0;
			if((e = strstr(buf, "debug_level=")) != NULL)
				*level = atoi(e + 12);
			if((e = strstr(buf, "debug_client_ip=")) != NULL)
				*cip = inet_addr(e+16);
			if((e = strstr(buf, "debug_server_ip=")) != NULL)
				*sip = inet_addr(e);	
		}
		close(fd);
	} else {
		if((e = getenv("DEBUG_LEVEL")) != NULL){
			*level = atoi(e);
		}
		if((e = getenv("DEBUG_CLIENT_IP")) != NULL)
			*cip = inet_addr(e);
		if((e = getenv("DEBUG_SERVER_IP")) != NULL)
			*sip = inet_addr(e);
	}
}

static void
sig_user2 (int sig){
	int level;
	unsigned int cip, sip;
	
	vam_debug_read_cfg(sunx_errorlog.sun_path, &level, &cip, &sip);
	vam_debug_set_parameter(cip, sip, level, vam_debug_cfg_p);
	
	if (old_usr2_act.sa_handler)
		(old_usr2_act.sa_handler)(sig);
}

int
vam_debug_get_cfg_p_impl (char * path, vam_debug_cfg_t ** cfg_pp, int no_create) {
	int is_created = 0;
	vam_debug_cfg_t * cfg_p_tmp;
	
	cfg_p_tmp = NULL;
	
	if(path) {
		key_t key;
		int id;
		char * shmat_result;
		key = ftok(path, 'd');
		if(key == -1) {
			fprintf(stderr, "ftok(%s) return -1\n", path);			
		} else {
			if((id = shmget(key, 0, 0)) == -1 && no_create == 0) {
				if((id = shmget(key, sizeof(vam_debug_cfg_t), IPC_CREAT | 00666)) == -1){
					perror("shmget");
				} else
					is_created = 1;
			}
			if(id != -1) {
				struct shmid_ds buff;
				shmctl(id, IPC_STAT, &buff);
				if (buff.shm_segsz != sizeof(vam_debug_cfg_t)) {
					fprintf(stderr, "IPCSTAT shows size mismatch: %d vs %d\n",
						(uint32_t)buff.shm_segsz, (uint32_t)sizeof(vam_debug_cfg_t));
				} else if((shmat_result = shmat(id, NULL, 0)) == (void *)-1) {
					perror("shmat");
					if(shmctl(id, IPC_RMID, 0) == -1)
						perror("shmctl");
				} else {
					cfg_p_tmp = (vam_debug_cfg_t *)shmat_result;
				}
			}
		}
	}
	if(cfg_p_tmp) {
		if (is_created == 1){
			bzero(cfg_p_tmp, sizeof(vam_debug_cfg_t));
		}
		*cfg_pp = cfg_p_tmp;
		return 0;
	} else {
		fprintf(stderr, "vam_debug cannot attache to the shared memory. %s\n", path);
		*cfg_pp = &vam_debug_cfg_no_share_mem;
		return 1;
	}
}

int
vam_debug_get_cfg_p_no_create(char * path, vam_debug_cfg_t ** cfg_pp) {
	return vam_debug_get_cfg_p_impl(path, cfg_pp, 1);
}

int
vam_debug_get_cfg_p (char * path, vam_debug_cfg_t **cfg_pp) {
	return vam_debug_get_cfg_p_impl(path, cfg_pp, 0);
}

int
vam_debug_return_cfg_p(vam_debug_cfg_t * cfg_p) {
	if(cfg_p && cfg_p != &vam_debug_cfg_no_share_mem)
		return shmdt(cfg_p);
	return 1;
}

int
vam_debug_init_lib (char * send_path) {
	struct  sigaction new_act;
	int level;
	unsigned int cip, sip;
	static char old_send_path[512];

	if (strcmp(send_path, old_send_path) == 0) {
		// already inited
		return 0;
	}

	bzero(&vam_debug_cfg_no_share_mem, sizeof(vam_debug_cfg_no_share_mem));
	sunx_errorsp.sun_path[0] = 0;
	if (send_path == NULL)
		send_path = getenv("ERROR_LOG_SOCKET");
	if (send_path == NULL){
		sunx_errorlog.sun_path[0] = 0;
		return -1;
	} else {
		sprintf(sunx_errorlog.sun_path, "%s.%d", send_path, (int)(getuid()));
		if(vam_debug_get_cfg_p(sunx_errorlog.sun_path, &vam_debug_cfg_p) != 0) {
			sprintf(sunx_errorlog.sun_path, "%s.%d", send_path, 0);
			vam_debug_get_cfg_p(sunx_errorlog.sun_path, &vam_debug_cfg_p);
		}
		vam_debug_read_cfg(sunx_errorlog.sun_path, &level, &cip, &sip);
		vam_debug_set_parameter(cip, sip, level, vam_debug_cfg_p);
		
		new_act.sa_handler = sig_user2;
		old_usr2_act.sa_handler = NULL;
		sigaction(SIGUSR1, &new_act, &old_usr2_act);
		strcpy(old_send_path, send_path);
		return 0;
	}
}

int
vam_debug_set_default_module_id(unsigned int module_id){
	vam_debug_module_id = module_id;
	return vam_debug_module_id;
}

int vam_debug_setproc_name(const char *s)
{
	return snprintf(vam_debug_process_name, 256, "%s", s);
}

inline static int
vam_write_to_logd(struct sockaddr * addr, socklen_t tolen, const char * fmt, va_list va){
	static int fd = -1;
	static int logd_enabled = -1;
	int len;
	char buf[4096];
	if(fd == -1){ /* first time run */
		fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	}
	if(fd >= 0){
		len = vsnprintf(buf, sizeof(buf)-sizeof(struct timeval)-1, fmt, va);
		if(len > 0){
			int str_len = strlen(buf);
			buf[str_len]=0;
			int send_len;
			gettimeofday((struct timeval*)(buf+str_len+1), NULL);
			while((send_len = sendto(fd, buf, str_len+1+sizeof(struct timeval), 0,
						 addr, tolen)) == -1 &&
			      errno == EINTR);
			if(send_len < 0){
				if (logd_enabled != 0){
					if(!vam_debug_in_sighandler)
						fprintf(stderr, "WARNING: cannot contact vam_logd, will keep trying. Output redirected to stderr.\n");
					logd_enabled = 0;
				}
				if(str_len > 0) {
					if(buf[str_len-1] == '\n')
						fprintf(stderr, "%s", buf);
					else
						fprintf(stderr, "%s\n", buf);
				}
			} else if (logd_enabled != 1){
				logd_enabled = 1;
			}
		}
		return len;
	} else if (fd == -2){
		return -1;
	} else {
		fd = -2;
		fprintf(stderr, "WARNING: vam_logd is NOT enabled, no error will be logged in this session.\n");
		return -1;
	}
}

inline void
vam_errorlog_printf(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	vam_write_to_logd((struct sockaddr *) & sunx_errorlog, sizeof sunx_errorlog,
			  fmt, ap);
	va_end(ap);
	return;
}


inline void
vam_errorlog_printf_special(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	vam_write_to_logd((struct sockaddr *) & sunx_errorsp , sizeof sunx_errorsp ,
			  fmt, ap);
	va_end(ap);
	return;
}

inline char* vam_debug_getprocname()
{
	return vam_debug_process_name;
}

#define vam_debug_print_pkt_one_line(level, fmt, args...) \
do { \
	if(level < VAM_DEBUG_LEVEL_EVERYTHING) { \
		if(vam_debug_cfg_p) \
			vam_errorlog_printf(fmt ,## args); \
		else \
			fprintf(stderr, fmt ,## args); \
	} \
	if(vam_debug_cfg_p && vam_debug_cfg_p->vam_debug_level == VAM_DEBUG_LEVEL_EVERYTHING) \
		vam_errorlog_printf_special(fmt ,## args); \
} while(0)

void
vam_debug_print_pkt_impl(int level, const char *level_str,
			 unsigned char *buf, int len,
			 char * file, int line, const char* description){
	
	if(vam_debug_print_decision(level)){
		int debug_i;
		char * file_without_path;
		int pid = getpid();
		file_without_path = strrchr(file, '/');
		if(file_without_path)
			file_without_path++;
		else
			file_without_path = file;
		vam_debug_print_pkt_one_line(level, "(%d)%s:%s:%d-> DUMP length = 0x%x (%s)\n",
					     pid, level_str, file_without_path, line, len, description);
		for(debug_i = 0; debug_i < len ;) {
			if(len - debug_i >= 16){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9], buf[debug_i+10], buf[debug_i+11],
							     buf[debug_i+12], buf[debug_i+13], buf[debug_i+14], buf[debug_i+15]
					);
				debug_i += 16;
			} else if(len - debug_i == 15){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9], buf[debug_i+10], buf[debug_i+11],
							     buf[debug_i+12], buf[debug_i+13], buf[debug_i+14]
					);
				break;
			} else if(len - debug_i == 14){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9], buf[debug_i+10], buf[debug_i+11],
							     buf[debug_i+12], buf[debug_i+13]
					);
				break;
			} else if(len - debug_i == 13){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9], buf[debug_i+10], buf[debug_i+11],
							     buf[debug_i+12]
					);
				break;
			} else if(len - debug_i == 12){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9], buf[debug_i+10], buf[debug_i+11]
					);
				break;
			} else if(len - debug_i == 11){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9], buf[debug_i+10]
					);
				break;
			} else if(len - debug_i == 10){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8], buf[debug_i+9]
					);
				break;
			} else if(len - debug_i == 9){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx"
							     ",  0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7],
							     buf[debug_i+8]
					);
				break;
			} else if (len - debug_i == 8){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6], buf[debug_i+7]
					);				
				break;
			} else if (len - debug_i == 7){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5], buf[debug_i+6]
					);				
				break;
			} else if (len - debug_i == 6){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4], buf[debug_i+5]
					);				
				break;
			} else if (len - debug_i == 5){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3],
							     buf[debug_i+4]
					);				
				break;
			} else if (len - debug_i == 4){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2], buf[debug_i+3]
					);
				break;
			} else if (len - debug_i == 3){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1], buf[debug_i+2]
					);
				break;
			} else if (len - debug_i == 2){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx,0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0], buf[debug_i+1]
					);
				break;;
			} else if (len - debug_i == 1){
				vam_debug_print_pkt_one_line(level, "(%d)%s: 0x%03x: 0x%02hx\n"
							     , pid, level_str, debug_i,
							     buf[debug_i+0]
					);
				break;
			}
		}
	}
}

void
vam_debug_set_parameter (unsigned int cip, unsigned int sip, int level, vam_debug_cfg_t * cfg_p) {
	assert(cfg_p);
	if(cip != INADDR_NONE)
		cfg_p->debug_target_client_ip = cip;
	if(sip != INADDR_NONE)
		cfg_p->debug_target_server_ip = sip;
	if(level >= 0 ) {
		// currently "special" log feature is disabled
		if(level >= VAM_DEBUG_LEVEL_EVERYTHING)
			level = VAM_DEBUG_LEVEL_EVERYTHING - 1;

		if(cfg_p->vam_debug_level != level &&
		   cfg_p->vam_debug_level == VAM_DEBUG_LEVEL_EVERYTHING){
			vam_errorlog_printf_special("reset special log");
		}
		cfg_p->vam_debug_level = level;
	}
}

void
vam_debug_set_debug_parameter_directly (unsigned int cip, unsigned int sip, int level) {
	if (vam_debug_cfg_p) {
		vam_debug_set_parameter(cip, sip, level, vam_debug_cfg_p);
	} else {
		vam_debug_error("vam_debug_init_lib() not called, no level change\n");
	}
}

void
vam_debug_set_debug_level_directly (int level) {
	if (vam_debug_cfg_p) {
		vam_debug_set_parameter(INADDR_NONE, INADDR_NONE, level, vam_debug_cfg_p);
	} else {
		vam_debug_error("vam_debug_init_lib() not called, no level change\n");
	}
}


/*
int
main(){
	vam_debug_critical("!!!\n");
	vam_debug_error("abcd\n");
	vam_debug_error("ab %d", 121221);
	return 1;
}
*/
