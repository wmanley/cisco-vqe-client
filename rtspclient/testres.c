/*
 *  testres.c
 *
 *  Test program for the DNS parsing routines
 *
 *  v1.0
 *
 *  Dave Shield		January 1994
 *
 *  v1.2	Major tidy up
 *		    added command line options for testing copy/free
 *
 *  v1.3	Changed name of error string routine
 *			July 1997
 *
 * Copyright (c) 1994-2007 by cisco Systems, Inc.
 */

#include	<stdio.h>
#include	<string.h>
#include        <stdlib.h>
#include        <unistd.h>
#include	"rr.h"
#include	"res_info.h"
#include	<resolv.h>

char	*prog;
char	name[BUFSIZ];
int	type;

int	free_flag=0;
int	copy_flag=0;
int	debug_flag=0;
 
void usage(void);

int
main(int argc, char *argv[])
{
	res_response	*resp;
	res_response	*resp2;
	u_char		answer[BUFSIZ];
	char		c;
	extern int	optind;

		/*
		 * Parse the command line ....
		 */
	if (( prog=strrchr( argv[0], '/' )) == NULL )
	    prog = argv[0];
	else
	    prog++;

	while (( c=getopt( argc, argv, "cdfh" )) != EOF )
	    switch( c ) {
		case 'c':	copy_flag=1;
				break;
		case 'd':	debug_flag=1;
				break;
		case 'f':	free_flag=1;
				break;
		case 'h':	usage();
				/* NOT REACHED */
		case '?':	usage();
				/* NOT REACHED */
	    }


		/*
		 *  .. and determine required query ...
		 */
	if ( argc == optind ) {
		printf("Enter name to search for: ");
                /* sa_ignore IGNORE_RETURN */
		fgets(name, BUFSIZ, stdin);
		if ( name[BUFSIZ-1] == '\012' )
		    name[BUFSIZ-1] = '\0';
	}
	else
            strncpy(name, argv[optind++], BUFSIZ);

		/*
		 * ... and type.
		 */
	if ( argc == optind )
		type = C_ANY;
	else
		type = which_res_type(argv[optind++]);

	if ( argc != optind )
		usage();

	if ( debug_flag ) {
		printf("Looking for %s type %d\n", name, type);
		_res.options |= RES_DEBUG;
	}


		/*
		 *  Make the query....
		 */
	if (res_search(name, C_IN, type, answer, BUFSIZ) == -1 ) {
		fprintf(stderr, "%s: %s\n", prog, res_error_str());
		exit(1);
	}

		/*
		 * ... and process it.
		 *	(which is the point of this package after all!)
		 */
	if ((resp = res_parse((char *) answer )) == NULL ) {
		perror("res_parse");
		exit(1);
	}

		/*
		 *  Test the copy routine....
		 */
	if ( copy_flag ) {
	    resp2 = res_copy(resp);
	    if ( free_flag )
		res_free(resp);

            if (resp2) 
                res_print(resp2);
	    if ( free_flag && resp2 )
		res_free(resp2);
	}
		/*
		 *  ... or just print it.
		 */
	else {
	    res_print(resp);
	    if ( free_flag )
		res_free(resp);
	}

        return 0;
}

void usage(void)
{
	fprintf(stderr, "Usage: %s [-c|-f] [query [type]]\n", prog);
	fprintf(stderr, "\t-c\tcopy response tree before printing\n");
	fprintf(stderr, "\t-f\texplicitly free response tree(s)\n");
	fprintf(stderr, "\n%s will prompt for a name to query if this is not specified.\n", prog);
	fprintf(stderr, "The query type defaults to T_ANY.\n");
	exit(1);
}
