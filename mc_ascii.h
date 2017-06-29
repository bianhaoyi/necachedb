#ifndef APP_SEARCH_KSARCH_STORE_CACHEDB_MC_ASCII_H
#define APP_SEARCH_KSARCH_STORE_CACHEDB_MC_ASCII_H
//#ifndef _MC_ASCII_H_
//#define _MC_ASCII_H_

#include "olivehc.h"
#include "request.h"
#include "http.h"

/*
void asc_complete_nread(struct conn *c);
rstatus_t asc_parse(struct conn *c);
void asc_append_stats(struct conn *c, const char *key, uint16_t klen, const char *val, uint32_t vlen);
size_t asc_rsp_server_error(struct conn *c);
*/

#define TOKEN_COMMAND           0
#define TOKEN_KEY               1

#define TOKEN_FLAGS             2
#define TOKEN_EXPIRY            3
#define TOKEN_VLEN              4
/*
#define TOKEN_CAS               5
#define TOKEN_DELTA             2
#define TOKEN_SUBCOMMAND        1
#define TOKEN_CACHEDUMP_ID      2
#define TOKEN_CACHEDUMP_LIMIT   3
#define TOKEN_AGGR_COMMAND      2
#define TOKEN_EVICT_COMMAND     2
#define TOKEN_KLOG_COMMAND      2
#define TOKEN_KLOG_SUBCOMMAND   3
*/
#define TOKEN_MAX               8

//#define SUFFIX_MAX_LEN 65 /* =11+11+21+21+1 enough to hold " <uint32_t> <uint32_t> <uint64_t> <uint64_t>\0" */

struct token {
    char   *val; /* token value */
    size_t len;  /* token length */
};

int mc_request_parse(ohc_request_t *r);
string_t* mc_code_page(int code, int method);
ssize_t mc_make_200_response_header(ssize_t max_length, char *output, ohc_request_t *r);

#endif
