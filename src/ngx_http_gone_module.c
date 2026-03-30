/*
 * nginx-gone - HTTP 410 Gone module for nginx
 *
 * Copyright (c) 2026 Rumen Damyanov
 * BSD 3-Clause License - see LICENSE.md
 *
 * https://github.com/RumenDamyanov/nginx-gone
 *
 * Reads a URI list file at config time and returns HTTP 410 Gone
 * for matching requests. Supports exact string matches (via hash table)
 * and PCRE regex patterns.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_gone_module.h"

/*
 * Forward declarations
 */
static ngx_int_t ngx_http_gone_init(ngx_conf_t *cf);
static void *ngx_http_gone_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_gone_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_gone_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_gone_merge_loc_conf(ngx_conf_t *cf, void *parent,
                                          void *child);
static ngx_int_t ngx_http_gone_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_gone_parse_map_file(ngx_conf_t *cf,
                                              ngx_http_gone_main_conf_t *gmcf);

/*
 * Marker value stored in the hash table for matched URIs
 */
static ngx_int_t ngx_http_gone_marker = 1;

/*
 * Configuration directives
 */
static ngx_command_t ngx_http_gone_commands[] = {

    {ngx_string("gone"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
     ngx_conf_set_flag_slot,
     NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_gone_loc_conf_t, enabled),
     NULL},

    {ngx_string("gone_map_file"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot,
     NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(ngx_http_gone_main_conf_t, map_file),
     NULL},

    {ngx_string("gone_use_request_uri"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_FLAG,
     ngx_conf_set_flag_slot,
     NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(ngx_http_gone_main_conf_t, use_request_uri),
     NULL},

    ngx_null_command};

/*
 * Module context
 */
static ngx_http_module_t ngx_http_gone_module_ctx = {
    NULL,               /* preconfiguration */
    ngx_http_gone_init, /* postconfiguration */

    ngx_http_gone_create_main_conf, /* create main configuration */
    ngx_http_gone_init_main_conf,   /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    ngx_http_gone_create_loc_conf, /* create location configuration */
    ngx_http_gone_merge_loc_conf   /* merge location configuration */
};

/*
 * Module definition
 */
ngx_module_t ngx_http_gone_module = {
    NGX_MODULE_V1,
    &ngx_http_gone_module_ctx, /* module context */
    ngx_http_gone_commands,    /* module directives */
    NGX_HTTP_MODULE,           /* module type */
    NULL,                      /* init master */
    NULL,                      /* init module */
    NULL,                      /* init process */
    NULL,                      /* init thread */
    NULL,                      /* exit thread */
    NULL,                      /* exit process */
    NULL,                      /* exit master */
    NGX_MODULE_V1_PADDING};

/*
 * Create main configuration
 */
static void *
ngx_http_gone_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_gone_main_conf_t *gmcf;

    gmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_gone_main_conf_t));
    if (gmcf == NULL)
    {
        return NULL;
    }

    gmcf->use_request_uri = NGX_CONF_UNSET;

    return gmcf;
}

/*
 * Initialize main configuration
 *
 * Parse the map file and build the hash table + regex array.
 */
static char *
ngx_http_gone_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_gone_main_conf_t *gmcf = conf;

    if (gmcf->use_request_uri == NGX_CONF_UNSET)
    {
        gmcf->use_request_uri = 0;
    }

    /* If no map file configured, nothing to do */
    if (gmcf->map_file.len == 0)
    {
        return NGX_CONF_OK;
    }

    /* Parse the map file */
    if (ngx_http_gone_parse_map_file(cf, gmcf) != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/*
 * Create location configuration
 */
static void *
ngx_http_gone_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_gone_loc_conf_t *glcf;

    glcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_gone_loc_conf_t));
    if (glcf == NULL)
    {
        return NULL;
    }

    glcf->enabled = NGX_CONF_UNSET;

    return glcf;
}

/*
 * Merge location configurations
 */
static char *
ngx_http_gone_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_gone_loc_conf_t *prev = parent;
    ngx_http_gone_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enabled, prev->enabled, 0);

    return NGX_CONF_OK;
}

/*
 * Parse the gone map file
 *
 * Reads the file, separates entries into exact matches (hash table)
 * and regex patterns (compiled array).
 *
 * File format:
 *   /exact/uri 1            # exact match
 *   ~^/regex/pattern 1      # case-sensitive regex
 *   ~*^/regex/pattern 1     # case-insensitive regex
 *   # comment               # ignored
 *
 * The second column (value) is ignored, kept for nginx map compatibility.
 */
static ngx_int_t
ngx_http_gone_parse_map_file(ngx_conf_t *cf, ngx_http_gone_main_conf_t *gmcf)
{
    ngx_file_t file;
    ngx_buf_t *buf;
    u_char *line_start, *line_end, *p, *uri_end;
    ssize_t n;
    size_t len, uri_len;
    off_t file_size;
    ngx_file_info_t fi;
    ngx_array_t exact_keys;
    ngx_hash_key_t *hk;
    ngx_hash_init_t hash_init;
    ngx_str_t full_path;
#if (NGX_PCRE)
    ngx_http_gone_regex_t *regex_entry;
    ngx_regex_compile_t rc;
    u_char errstr[NGX_MAX_CONF_ERRSTR];
#endif

    /* Resolve relative path against nginx prefix */
    if (ngx_conf_full_name(cf->cycle, &gmcf->map_file, 1) != NGX_OK)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "gone: failed to resolve path \"%V\"",
                           &gmcf->map_file);
        return NGX_ERROR;
    }

    full_path = gmcf->map_file;

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.name = full_path;
    file.log = cf->log;

    file.fd = ngx_open_file(full_path.data, NGX_FILE_RDONLY,
                            NGX_FILE_OPEN, 0);
    if (file.fd == NGX_INVALID_FILE)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "gone: failed to open map file \"%V\"",
                           &full_path);
        return NGX_ERROR;
    }

    /* Get file size */
    if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "gone: failed to stat \"%V\"", &full_path);
        ngx_close_file(file.fd);
        return NGX_ERROR;
    }

    file_size = ngx_file_size(&fi);
    if (file_size == 0)
    {
        ngx_log_error(NGX_LOG_WARN, cf->log, 0,
                      "gone: map file \"%V\" is empty", &full_path);
        ngx_close_file(file.fd);
        return NGX_OK;
    }

    /* Limit file size (10MB max) */
    if (file_size > 10 * 1024 * 1024)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "gone: map file \"%V\" too large (%O bytes, max 10MB)",
                           &full_path, file_size);
        ngx_close_file(file.fd);
        return NGX_ERROR;
    }

    /* Allocate buffer for file content */
    buf = ngx_create_temp_buf(cf->pool, (size_t)file_size + 1);
    if (buf == NULL)
    {
        ngx_close_file(file.fd);
        return NGX_ERROR;
    }

    /* Read entire file */
    n = ngx_read_file(&file, buf->pos, (size_t)file_size, 0);
    ngx_close_file(file.fd);

    if (n == NGX_ERROR)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "gone: failed to read \"%V\"", &full_path);
        return NGX_ERROR;
    }

    buf->last = buf->pos + n;
    *buf->last = '\0';

    /* Initialize temporary array for hash keys */
    if (ngx_array_init(&exact_keys, cf->pool, 64,
                       sizeof(ngx_hash_key_t)) != NGX_OK)
    {
        return NGX_ERROR;
    }

#if (NGX_PCRE)
    /* Initialize regex entries array */
    gmcf->regex_entries = ngx_array_create(cf->pool, 8,
                                           sizeof(ngx_http_gone_regex_t));
    if (gmcf->regex_entries == NULL)
    {
        return NGX_ERROR;
    }
#endif

    gmcf->exact_count = 0;
    gmcf->regex_count = 0;

    /* Parse file line by line */
    line_start = buf->pos;
    while (line_start < buf->last)
    {
        /* Find end of line */
        line_end = line_start;
        while (line_end < buf->last && *line_end != '\n' && *line_end != '\r')
        {
            line_end++;
        }

        /* Skip leading whitespace */
        p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t'))
        {
            p++;
        }

        len = line_end - p;

        /* Skip empty lines and comments */
        if (len == 0 || *p == '#')
        {
            goto next_line;
        }

        /* Extract the URI (first column) - stop at whitespace */
        uri_end = p;
        while (uri_end < line_end && *uri_end != ' ' && *uri_end != '\t')
        {
            uri_end++;
        }
        uri_len = uri_end - p;

        if (uri_len == 0)
        {
            goto next_line;
        }

        /* Determine if regex or exact match */
        if (uri_len >= 2 && p[0] == '~')
        {

#if (NGX_PCRE)
            /* Regex entry */
            ngx_int_t caseless = 0;
            u_char *pattern_start = p + 1;
            size_t pattern_len = uri_len - 1;

            if (p[1] == '*')
            {
                caseless = 1;
                pattern_start = p + 2;
                pattern_len = uri_len - 2;
            }

            if (pattern_len == 0)
            {
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "gone: empty regex pattern at line in \"%V\"",
                                   &full_path);
                goto next_line;
            }

            regex_entry = ngx_array_push(gmcf->regex_entries);
            if (regex_entry == NULL)
            {
                return NGX_ERROR;
            }

            regex_entry->pattern.len = uri_len;
            regex_entry->pattern.data = ngx_pnalloc(cf->pool, uri_len);
            if (regex_entry->pattern.data == NULL)
            {
                return NGX_ERROR;
            }
            ngx_memcpy(regex_entry->pattern.data, p, uri_len);

            ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

            rc.pattern.data = pattern_start;
            rc.pattern.len = pattern_len;
            rc.pool = cf->pool;
            rc.err.data = errstr;
            rc.err.len = NGX_MAX_CONF_ERRSTR;
            if (caseless)
            {
                rc.options = NGX_REGEX_CASELESS;
            }

            if (ngx_regex_compile(&rc) != NGX_OK)
            {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "gone: invalid regex \"%*s\" in \"%V\": %V",
                                   pattern_len, pattern_start,
                                   &full_path, &rc.err);
                return NGX_ERROR;
            }

            regex_entry->regex = rc.regex;
            gmcf->regex_count++;

#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "gone: regex patterns require PCRE support, "
                               "found \"%*s\" in \"%V\"",
                               uri_len, p, &full_path);
            return NGX_ERROR;
#endif
        }
        else
        {
            /* Exact match entry */
            hk = ngx_array_push(&exact_keys);
            if (hk == NULL)
            {
                return NGX_ERROR;
            }

            hk->key.len = uri_len;
            hk->key.data = ngx_pnalloc(cf->pool, uri_len);
            if (hk->key.data == NULL)
            {
                return NGX_ERROR;
            }
            ngx_strlow(hk->key.data, p, uri_len);

            hk->key_hash = ngx_hash_key_lc(p, uri_len);
            hk->value = &ngx_http_gone_marker;

            gmcf->exact_count++;
        }

    next_line:
        /* Move to next line */
        line_start = line_end;
        while (line_start < buf->last && (*line_start == '\n' || *line_start == '\r'))
        {
            line_start++;
        }
    }

    /* Build the hash table for exact matches */
    if (exact_keys.nelts > 0)
    {
        ngx_memzero(&hash_init, sizeof(ngx_hash_init_t));

        hash_init.hash = &gmcf->exact_hash;
        hash_init.key = ngx_hash_key_lc;
        hash_init.max_size = 4096;
        hash_init.bucket_size = ngx_align(64, ngx_cacheline_size);
        hash_init.name = "gone_exact_hash";
        hash_init.pool = cf->pool;
        hash_init.temp_pool = cf->temp_pool;

        if (ngx_hash_init(&hash_init, exact_keys.elts,
                          exact_keys.nelts) != NGX_OK)
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "gone: failed to build exact match hash table");
            return NGX_ERROR;
        }
    }

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0,
                  "gone: loaded \"%V\": %ui exact, %ui regex entries",
                  &full_path, gmcf->exact_count, gmcf->regex_count);

    return NGX_OK;
}

/*
 * Server rewrite phase handler
 *
 * Checks the request URI against exact matches and regex patterns.
 * Returns HTTP 410 Gone on match, NGX_DECLINED otherwise.
 */
static ngx_int_t
ngx_http_gone_handler(ngx_http_request_t *r)
{
    ngx_http_gone_main_conf_t *gmcf;
    ngx_http_gone_loc_conf_t *glcf;
    ngx_str_t uri;
    ngx_uint_t hash;
    void *value;
#if (NGX_PCRE)
    ngx_http_gone_regex_t *regex_entries;
    ngx_uint_t i;
    ngx_int_t n;
#endif

    glcf = ngx_http_get_module_loc_conf(r, ngx_http_gone_module);

    /* If disabled, pass through */
    if (!glcf->enabled)
    {
        return NGX_DECLINED;
    }

    gmcf = ngx_http_get_module_main_conf(r, ngx_http_gone_module);

    /* No map file loaded */
    if (gmcf->map_file.len == 0)
    {
        return NGX_DECLINED;
    }

    /* Determine which URI to check */
    if (gmcf->use_request_uri)
    {
        /* Use the original unparsed request URI */
        uri.data = r->uri_start;
        uri.len = r->uri_end - r->uri_start;
    }
    else
    {
        uri = r->uri;
    }

    /* Try exact hash lookup first (fast path) */
    if (gmcf->exact_count > 0)
    {
        hash = ngx_hash_key_lc(uri.data, uri.len);
        value = ngx_hash_find(&gmcf->exact_hash, hash, uri.data, uri.len);

        if (value != NULL)
        {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "gone: 410 for \"%V\" (exact match)", &uri);
            return NGX_HTTP_GONE;
        }
    }

#if (NGX_PCRE)
    /* Try regex patterns */
    if (gmcf->regex_count > 0 && gmcf->regex_entries != NULL)
    {
        regex_entries = gmcf->regex_entries->elts;

        for (i = 0; i < gmcf->regex_entries->nelts; i++)
        {
            n = ngx_regex_exec(regex_entries[i].regex, &uri, NULL, 0);

            if (n >= 0)
            {
                ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                              "gone: 410 for \"%V\" (regex \"%V\")",
                              &uri, &regex_entries[i].pattern);
                return NGX_HTTP_GONE;
            }

            if (n == NGX_REGEX_NO_MATCHED)
            {
                continue;
            }

            /* Regex execution error */
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "gone: regex exec error %i for \"%V\"",
                          n, &regex_entries[i].pattern);
            break;
        }
    }
#endif

    return NGX_DECLINED;
}

/*
 * Module postconfiguration
 *
 * Register the handler in the server rewrite phase.
 */
static ngx_int_t
ngx_http_gone_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;
    ngx_http_gone_main_conf_t *gmcf;

    gmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_gone_module);

    /* Only register handler if a map file was configured */
    if (gmcf->map_file.len == 0)
    {
        return NGX_OK;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_SERVER_REWRITE_PHASE].handlers);
    if (h == NULL)
    {
        return NGX_ERROR;
    }

    *h = ngx_http_gone_handler;

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0,
                  "gone: module v%s initialized (%ui exact, %ui regex)",
                  NGX_HTTP_GONE_VERSION,
                  gmcf->exact_count, gmcf->regex_count);

    return NGX_OK;
}
