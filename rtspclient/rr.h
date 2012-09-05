/*
 *  DNS Resource record manipulation routines
 *
 *	Data structures
 *
 *  Dave Shield		November 1993
 */

#include	<stdio.h>
#include	<sys/types.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<arpa/nameser.h>


/* User information and uid */
#ifndef T_UINFO
#define T_UINFO 100
#endif
#ifndef T_UID
#define T_UID 101
#endif
#ifndef T_GID
#define T_GID 102
#endif
#ifndef T_UNSPEC
#define T_UNSPEC 103
#endif

	/* Some systems don't define this */
#ifndef T_TXT
#define	T_TXT	16
#endif

#ifndef T_AFSDB
#define	T_AFSDB	18
#endif
#ifndef T_SRV
#define T_SRV   33
#endif

	/* additional RFC 1183 types */
#ifndef	T_RP
#define	T_RP	17
#endif
#ifndef	T_X25
#define	T_X25	19
#endif
#ifndef	T_ISDN
#define	T_ISDN	20
#endif
#ifndef	T_RT
#define	T_RT	21
#endif

	/*
	 *  RR-specific data
	 *	Structured types - separate structures
	 *	Single valued types - use union directly
	 */

struct s_SOA
{
    char	*mname;
    char	*rname;
    u_int	serial;
    u_int	refresh;
    u_int	retry;
    u_int	expire;
    u_int	minimum;
};


struct s_NULL
{
    char	*anything;
    u_short	length;		/* Length of valid data */
};


struct s_WKS
{
    struct in_addr	address;
    char	*bitmap;
    u_int	maplength;
    u_char	protocol;
};


struct s_HINFO
{
    char	*cpu;
    char	*os;
};


struct s_MINFO
{
    char	*rmailbx;
    char	*emailbx;
};


struct s_MX
{
    char	*exchange;
    u_short	preference;
};

struct s_TXT
{
    char	*text;
    struct s_TXT	*next;
    u_short	len;
};

struct s_SRV
{
    u_short priority;
    u_short weight;
    u_short port;
    char *target;
};


	/*
	 *  New RR types - RFC 1183
	 */

struct s_AFSDB		/* AFS servers */
{
    u_short	subtype;
    char	*hostname;
};


struct s_RP		/* Responsible Person */
{
    char	*mbox_dname;
    char	*txt_dname;
};

			/* X25  -  Use simple 'string' */

struct s_ISDN		/* ISDN Address */
{
    char	*address;
    char	*sa;	/* optional */
};

struct s_RT		/* Route Through */
{
    u_short	preference;
    char	*int_host;
};




	/*  Generic RDATA RR structure */
union u_rdata
{
    char		*string;	/* Any simple string record */
    u_int		number;		/* Any simple numeric record */
    struct in_addr	address;	/* Simple address (A record) */

					/* other structured RR types */
    struct s_SOA	soa;
    struct s_NULL	null;
    struct s_WKS	wks;
    struct s_HINFO	hinfo;
    struct s_MINFO	minfo;
    struct s_MX		mx;
    struct s_TXT	txt;
    struct s_SRV    srv;
					/* RFC 1183 RR types */
    struct s_AFSDB	afsdb;
    struct s_RP		rp;
    struct s_ISDN	isdn;
    struct s_RT		rt;
};


	/*  Full RR structure */
typedef struct s_rr
{
    char		*name;
    u_short		type;
    u_short		class;
    u_int		ttl;
    u_int		dlen;
    union u_rdata	rdata;
} s_rr;

	/*  DNS Question sctructure */
typedef struct s_question
{
    char		*qname;
    u_short		qtype;
    u_short		qclass;
} s_question;


	/*  Full DNS message structure */
typedef struct s_res_response
{
    HEADER		header;
    s_question		**question;
    s_rr		**answer;
    s_rr		**authority;
    s_rr		**additional;
} res_response ;


		/*  Defined interface */

extern	res_response *	res_parse(char *msg);
extern	res_response *	res_copy(res_response *resp);
extern	void		res_print(res_response *resp);
extern	void		res_free(res_response *resp);
