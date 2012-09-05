/*
 *------------------------------------------------------------------
 * SRV lookup module
 *
 * July 2006, Dong Hsu
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/types.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <utils/vam_types.h>
#define __VQEC__ 1
#include <vqec_debug.h>
#include "srv_lookup.h"
#include "rr.h"

// The space allocated for returns from res_query.
#define DNS_RESPONSE_SIZE 4096

static remote_server_t *list = NULL;

static void lookup_A(const char *domain,
                     IpProtocolSocketType proto_code,
                     res_response* in_response,
                     int port,
                     unsigned int priority,
                     unsigned int weight);
/**<
 * If in_response is non-NULL, use it as an initial source of A records.
 *
 * @returns TRUE if one or more addresses were added to the list.
 */

/// Perform a DNS query and parse the results.  Follows CNAME records.
static void res_query_and_parse(const char* in_name,
                                int type,
                                res_response* in_response,
                                const char** out_name,
                                res_response** out_response
    );
/**<
 * Performs a DNS query for a particular type of RR on a given name,
 * doing all the work to follow CNAMEs.  The 'in_name' and 'type'
 * arguments specify the RRs to look for.  If 'in_response' is not NULL,
 * it is the results of some previous search for the same name, for
 * a different type of RR, which might contain RRs for this search.
 *
 * @return out_response is a pointer to a response structure, or NULL.
 * If non-NULL, the RRs of the required type (if any) are in out_response
 * (in either the answer section or the additional section), under the name
 * out_name.
 *
 * The caller is responsible for freeing out_name if it is non-NULL
 * and != in_name.  The caller is responsible for freeing out_response if it
 * is non-NULL and != in_response.
 */

/**
 * Search for an RR with 'name' and 'type' in the answer and additional
 * sections of a DNS response.
 *
 * @return pointer to rdata structure for the first RR founr, or NULL.
 */
static union u_rdata* look_for(res_response* response,
                               const char* name,
                               int type
    );

static void sort_answers(res_response* response);

static int rr_compare(const void* a, const void* b);

static void add_to_list(remote_server_t *server);

/*
 * Look up SRV records for a domain name, and from them find server
 * addresses to insert into the list of servers.
 */
remote_server_t* lookup_SRV (const char* domain,
                           const char* service,
                           const char* proto_string)
{
    res_response* response = NULL;
    const char* canonical_name = NULL;
    IpProtocolSocketType proto_code;

    /* Set up protocol type */
    if (strcmp(proto_string, "tcp") == 0) {
        proto_code = TCP;
    }
    else {
        if (strcmp(proto_string, "udp") == 0) {
            proto_code = UDP;
        }
        else {
            VQEC_LOG_ERROR("lookup_SRV::Unsupported protocol %s\n",
                           proto_string);
            return NULL;
        }
    }

    /*****************************************************************
     * Construct buffer to hold the key string for the lookup:
     * _service._protocol.domain
     * 5 bytes suffices for the added components and the ending NUL.
     ****************************************************************/
    char* lookup_name;
    int length;

    /* res_init() will re-read any changes in the resolv.conf file */
    if (!res_init()) {
        /*
         * set RES_DNSRCH to the options so that the old version
         * of res_search() will append the search domain name(s)
         * before sending out the res_query()
         */
        _res.options |= RES_DNSRCH;
    } else {
        VQEC_LOG_ERROR("res_query_and_parse:: res_init() failed\n");
    }

    if (domain == NULL || strlen(domain) == 0) {
        length = strlen(service) + strlen(proto_string) + 5;
        lookup_name = (char*) malloc(length);
        if (lookup_name) {
            snprintf(lookup_name, length, "_%s._%s", service, proto_string);
        }

        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_SRV:: resolv options = 0x%08x\n", _res.options);

        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_SRV:: ndots in _res_state = %d\n", _res.ndots);
        int i;
        for (i = 0; i < MAXDNSRCH; i++) {
            VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                       "lookup_SRV:: search order %d = %s\n", 
                       i, _res.dnsrch[i]);
        }
    }
    else {
        length = strlen(service) + strlen(proto_string) + strlen(domain) + 5;
        lookup_name = (char*) malloc(length);
        if (lookup_name) {
            snprintf(lookup_name, length, "_%s._%s.%s",
                     service, proto_string, domain);
        }
    }

    /* Make the query and parse the response. */
    res_query_and_parse(lookup_name, T_SRV, NULL, &canonical_name, &response);
    if (response != NULL) {
        unsigned int i;
        /* For each answer that is an SRV record for this domain name. */

        /* Search the answer list of RRs. */
        for (i = 0; i < response->header.ancount; i++) {
            if (response->answer[i]->class == C_IN &&
                response->answer[i]->type == T_SRV &&
                strncasecmp(canonical_name, response->answer[i]->name,
                            strlen(canonical_name)) == 0) {
                /**********************************************************
                 * Call lookup_A to get the A records for the target host
                 * name.  Give it the pointer to our current response,
                 * because it might have the A records.  If not, lookup_A
                 * will do a DNS lookup to get them.
                 **********************************************************/
                lookup_A(response->answer[i]->rdata.srv.target,
                         proto_code,
                         response,
                         response->answer[i]->rdata.srv.port,
                         response->answer[i]->rdata.srv.priority,
                         response->answer[i]->rdata.srv.weight);
            }
        }

        /* Search the additional list of RRs. */
        for (i = 0; i < response->header.arcount; i++) {
            if (response->additional[i]->class == C_IN &&
                response->additional[i]->type == T_SRV &&
                strncasecmp(canonical_name, response->additional[i]->name,
                            strlen(canonical_name)) == 0) {
                /*********************************************************
                 * Call lookup_A to get the A records for the target host
                 * name.  Give it the pointer to our current response,
                 * because it might have the A records.  If not, lookup_A
                 * will do a DNS lookup to get them.
                 *********************************************************/
                lookup_A(response->additional[i]->rdata.srv.target,
                         proto_code,
                         response,
                         response->additional[i]->rdata.srv.port,
                         response->additional[i]->rdata.srv.priority,
                         response->additional[i]->rdata.srv.weight);
            }
        }
    }
    else {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_SRV:: No records found for [%s]\n",
                   lookup_name);
    }

    /* Free the result of res_parse. */
    if (response != NULL) {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_SRV::free response %p\n", response);
        res_free(response);
    }

    if (canonical_name != NULL && canonical_name != lookup_name) {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_SRV::free canonical_name %p\n", 
                        canonical_name);
        free((void*) canonical_name);
    }

    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "lookup_SRV::free lookup_name %p\n", lookup_name);
    free(lookup_name);

    return list;
}


/*
 * Look up A records for a domain name, and insert them into the list
 * of servers.
 */
void lookup_A (const char* domain,
               IpProtocolSocketType proto_code,
               res_response* in_response,
               int port,
               unsigned int priority,
               unsigned int weight)
{
    /* To hold the return of res_query_and_parse. */
    res_response* response = NULL;
    const char* canonical_name = NULL;

    /* Make the query and parse the response. */
    res_query_and_parse(domain, T_A, in_response, &canonical_name, &response);

    /* For each answer that is an SRV record for this domain name. */
    if (response != NULL) {
        unsigned int i;
        /* Search the answer list. */
        for (i = 0; i < response->header.ancount; i++) {
            if (response->answer[i]->class == C_IN &&
                response->answer[i]->type == T_A &&
                strncasecmp(canonical_name, response->answer[i]->name,
                            strlen(canonical_name)) == 0) {
                /**************************************************************
                 * An A record has been found.
                 * Assemble the needed information and add it 
                 * to the server list.
                 *************************************************************/
                remote_server_t *server_p = (remote_server_t *) 
                    malloc(sizeof(remote_server_t));
                if (server_p == NULL) {
                    VQEC_LOG_ERROR("lookup_A:: Failed to allocate memory "
                                   "for rtsp_server strucuture\n");
                }
                else {
                    memset(server_p, 0, sizeof(remote_server_t));

                    strncpy(server_p->host,
                            response->answer[i]->name,
                            NAME_LEN);
                    strncpy(server_p->addr,
                            inet_ntoa(response->answer[i]->rdata.address),
                            NAME_LEN);
                    server_p->port = port;
                    server_p->priority = priority;
                    server_p->weight = weight;

                    /* Add it to the overal list */
                    add_to_list(server_p);
                }
            }
        }

        /* Search the additional list. */
        for (i = 0; i < response->header.arcount; i++) {
            if (response->additional[i]->class == C_IN &&
                response->additional[i]->type == T_A &&
                strncasecmp(canonical_name, response->additional[i]->name,
                            strlen(canonical_name)) == 0) {
                /**************************************************************
                 * An A record has been found.
                 * Assemble the needed information and add it 
                 * to the server list.
                 *************************************************************/
                remote_server_t *server_p = 
                    (remote_server_t *) malloc(sizeof(remote_server_t));
                if (server_p == NULL) {
                    VQEC_LOG_ERROR("lookup_A:: Failed to allocate memory "
                                   "for rtsp_server strucuture\n");
                }
                else {
                    memset(server_p, 0, sizeof(remote_server_t));

                    strncpy(server_p->host,
                            response->additional[i]->name,
                            NAME_LEN);
                    strncpy(server_p->addr,
                            inet_ntoa(response->additional[i]->rdata.address),
                            NAME_LEN);
                    server_p->port = port;
                    server_p->priority = priority;
                    server_p->weight = weight;

                    /* Add it to the overal list */
                    add_to_list(server_p);
                }
            }
        }
    }

    // Free the result of res_parse if necessary.
    if (response != NULL && response != in_response) {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_A:: free response %p\n", response);
        res_free(response);
    }

    if (canonical_name != NULL && canonical_name != domain) {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "lookup_A:: free canonical_name %p\n", canonical_name);
        free((void*) canonical_name);
    }
}


/* Perform a DNS query and parse the results.  Follows CNAME records. */
void res_query_and_parse (const char* in_name,
                          int type,
                          res_response* in_response,
                          const char** out_name,
                          res_response** out_response)
{
    /* The number of CNAMEs we have followed. */
    int cname_count = 0;
    /* The response currently being examined. */
    res_response* response = in_response;
    /* The name currently being examined. */
    const char* name = in_name;
    /* TRUE if 'response' was a lookup for 'name' and 'type'. */
    boolean response_for_this_name = FALSE;
    /* Buffer into which to read DNS replies. */
    char answer[DNS_RESPONSE_SIZE];
    union u_rdata* p;

    
    /* Loop until we find a reason to exit.  Each turn around the loop does
       another DNS lookup. */
    while (1) {
        /* While response != NULL and there is a CNAME record for name
           in response. */
        while (response != NULL &&
               (p = look_for(response, name, T_CNAME)) != NULL) {
            cname_count++;

            if (cname_count > 5) {
                break;
            }

            /* If necessary, free the current 'name'. */
            if (name != in_name) {
                VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                           "res_query_and_parse:: free name %p\n", name);
                free((void*) name);
            }

            /* Copy the canonical name from the CNAME record into 'name', so
               we can still use it after freeing 'response'. */
            name = strdup(p->string);

            /********************************************************************
             * Remember that we are now looking for a name that was not the one
             * that we searched for to produce this response.  Hence, if we don't
             * find any RRs for it, that is not authoritative and we have to do
             * another DNS query.
             ********************************************************************/
            response_for_this_name = FALSE;
            /* Go back and check whether the result name of the CNAME is listed
               in this response. */
        }

        /*********************************************************************
         * This response does not contain a CNAME for 'name'.  So it is either
         * a final response that gives us the RRs we are looking for, or
         * we need to do a lookup on 'name'.
         *
         * Check whether the response was for this name, or contains records
         * of the type we are looking for.  If either, then any records we
         * are looking for are in this response, so we can return.
         ********************************************************************/
        if (response_for_this_name ||
            (response != NULL && look_for(response, name, type) != NULL)) {
            break;
        }

        /* We must do another lookup. Start by freeing 'response' if we need to. */
        if (response != in_response) {
            VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                       "res_query_and_parse:: free response %p\n", response);
            res_free(response);
        }

        response = NULL;
        /* Now, 'response' will be from a query for 'name'. */
        //response_for_this_name = TRUE;
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "res_query_and_parse:: calling res_search(\"%s\", "
                   "class = %d, type = %d)\n",
                   name, C_IN, type);
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "res_query_and_parse:: resolv options = 0x%08x\n", 
                   _res.options);

        if (res_search(name, C_IN, type,
                       (unsigned char*) answer, sizeof (answer)) == -1) {
            VQEC_LOG_ERROR("res_query_and_parse:: res_search() failed\n");
            break;
        }

        response = res_parse((char*) &answer);
        if (response == NULL) {
            VQEC_LOG_ERROR("res_query_and_parse:: res_parse() failed\n");
            break;
        }

        sort_answers(response);
#if DEBUG
        res_print(response);
#endif

        /* Now that we have a fresh DNS query to analyze, go back and check it
           for a CNAME for 'name' and then for records of the requested type. */
    }   

    *out_name = name;
    *out_response = response;
}


union u_rdata* look_for (res_response* response,
                         const char* name,
                         int type)
{
    unsigned i;

    for (i = 0; i < response->header.ancount; i++) {
        if (response->answer[i]->class == C_IN &&
            response->answer[i]->type == type &&
            strncasecmp(name, response->answer[i]->name, strlen(name)) == 0) {
            VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                       "look_for:: found the RR for %s\n",
                       name);
            return &response->answer[i]->rdata;
        }
    }

    for (i = 0; i < response->header.arcount; i++) {
        if (response->additional[i]->class == C_IN &&
            response->additional[i]->type == type &&
            strncasecmp(name, response->additional[i]->name, strlen(name)) 
            == 0) {
            return &response->additional[i]->rdata;
        }
    }

    return NULL;
}


/**
 * Post-process the results of res_parse by sorting the lists of "answer" and
 * "additional" RRs, so that responses are reproducible.  (named tends to
 * rotate multiple answer RRs to the same query.)
 */
static void sort_answers (res_response* response)
{
    qsort((void*) response->answer, response->header.ancount,
          sizeof (s_rr*), rr_compare);
    qsort((void*) response->additional, response->header.arcount,
          sizeof (s_rr*), rr_compare);
}

/**
 * Function to compare two RRs for qsort.
 *
 * I was hoping to sort records by TTL values, but Bind cleverly gives all
 * answers the same TTL (the minimum of the lot).  So we have to sort by
 * address (for A records) or port/target (for SRV records).
 */
static int rr_compare (const void* a, const void* b)
{
    int t;

    /* a and b are pointers to entries in the array of s_rr*'s.
       Get the pointers to the s_rr's: */
    s_rr* a_rr = *(s_rr**) a;
    s_rr* b_rr = *(s_rr**) b;

    /* Compare on type. */
    t = a_rr->type - b_rr->type;
    if (t != 0) {
        return t;
    }

    /* Case on type. */
    switch (a_rr->type) {
        case T_SRV:
            /* Compare on target. */
            t = strcmp(a_rr->rdata.srv.target, b_rr->rdata.srv.target);
            if (t != 0) {
                return t;
            }

            /* Compare on port. */
            if (a_rr->rdata.srv.port < b_rr->rdata.srv.port) {
                return -1;
            }
            else if (a_rr->rdata.srv.port > b_rr->rdata.srv.port) {
                return 1;
            }

            /* Give up. */
            return 0;

        case T_A:
            /* Compare on address. */
            return memcmp((const void*) &a_rr->rdata.address,
                          (const void*) &b_rr->rdata.address,
                          sizeof (struct sockaddr));

        default:
            return 0;
    }
}


/*
 * Free the memory of the list of servers.
 */
void free_SRV ()
{
    remote_server_t *server_p, *next;
    for (server_p = list; server_p != NULL;) {
        next = server_p->next;
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "free_SRV:: free the server %p\n",
                   server_p);
        free(server_p);
        server_p = next;
    }

    list = NULL;
    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "free_SRV:: All the servers have been released\n");
}


void add_to_list (remote_server_t *server)
{
    /* Find the last server */
    remote_server_t *server_p = list;

    if (server_p == NULL) {
        server_p = server;
        list = server_p;
    }
    else {
        while(server_p) {
            if (server_p->next == NULL) {
                server_p->next = server;
                break;
            }
            else {
                server_p = server_p->next;
            }
        }
    }

    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "add_to_list:: new server record %p has been added "
               "to the list %p\n",
               server, list);
}
