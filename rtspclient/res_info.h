/*
 *  DNS Resource record manipulation routines
 *
 *	Translation routines - internal use only
 #		(i.e. undocumented!)
 *
 *  Dave Shield		November 1993
 */

extern char * res_error_str(void);
extern char * res_opcode(int i);
extern char * res_rcode(int i);
extern char * res_wks(int wks);
extern char * res_proto(int proto);
extern char * res_type(int type);
extern int    which_res_type(char *str);
extern char * res_class(int class);
extern char * res_time(int value);
