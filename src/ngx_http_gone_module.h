/*
 * nginx-gone - HTTP 410 Gone module for nginx
 *
 * Copyright (c) 2026 Rumen Damyanov
 * BSD 3-Clause License - see LICENSE.md
 *
 * https://github.com/RumenDamyanov/nginx-gone
 */

#ifndef _NGX_HTTP_GONE_MODULE_H_INCLUDED_
#define _NGX_HTTP_GONE_MODULE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_HTTP_GONE_VERSION "0.1.0"
#define NGX_HTTP_GONE_VERSION_NUM 000100

/* nginx does not define HTTP 410 Gone */
#ifndef NGX_HTTP_GONE
#define NGX_HTTP_GONE 410
#endif

#if (NGX_PCRE)
/*
 * Compiled regex entry
 *
 * Stores a compiled PCRE pattern and the original string for logging.
 */
typedef struct
{
    ngx_regex_t *regex;
    ngx_str_t pattern;
} ngx_http_gone_regex_t;
#endif

/*
 * Main configuration (http context)
 *
 * Stores the map file path, the hash table for exact URI matches,
 * and the array of compiled regex patterns.
 * Populated once at configuration load time.
 */
typedef struct
{
    ngx_str_t map_file;         /* path to gone.map */
    ngx_hash_t exact_hash;      /* hash for exact matches */
    ngx_array_t *regex_entries; /* array of ngx_http_gone_regex_t */
    ngx_flag_t use_request_uri; /* match $request_uri */
    ngx_uint_t exact_count;     /* number of exact entries */
    ngx_uint_t regex_count;     /* number of regex entries */
} ngx_http_gone_main_conf_t;

/*
 * Location configuration (http, server, location contexts)
 *
 * Per-context on/off switch with inheritance.
 * Uses NGX_CONF_UNSET for proper merge behavior.
 */
typedef struct
{
    ngx_flag_t enabled; /* gone on|off */
} ngx_http_gone_loc_conf_t;

/*
 * Module declaration
 */
extern ngx_module_t ngx_http_gone_module;

#endif /* _NGX_HTTP_GONE_MODULE_H_INCLUDED_ */
