#include "mc_ascii.h"

int fail_return(ohc_request_t *r)
{
    //r->keepalive = 0;
    r->http_code = 400;

    return OHC_ERROR;
}

string_t* mc_code_page(int code, int method)
{
    static string_t ret[] = {
        STRING_INIT("CLIENT_ERROR\r\n"),
        STRING_INIT("END\r\n"), 
        STRING_INIT("STORED\r\n"), 
        STRING_INIT("NOT_STORED\r\n"), 
        STRING_INIT("DELETED\r\n"), 
        STRING_INIT("NOT_FOUND\r\n"),
        };

    if (method == OHC_HTTP_METHOD_INVALID)
    {
        return &ret[0];
    }
    else if (method == OHC_HTTP_METHOD_GET)
    {
        return &ret[1];
    }
    else if (method == OHC_HTTP_METHOD_POST || method == OHC_HTTP_METHOD_PUT)
    {
        if (code == 201)
        {
            return &ret[2];
        }
        else if (code == 204)
        {
            return &ret[3];
        } 
        else
        {
            return &ret[0];
        }
    }
    else if (method == OHC_HTTP_METHOD_DELETE)
    {
        if (code == 204)
        {
            return &ret[4];
        }
        else
        {
            return &ret[5];
        }
    }
    else
    {
        return &ret[0];
    }
}

ssize_t mc_make_200_response_header(ssize_t max_length, char *output, ohc_request_t *r)
{
    //header format: VALUE $key $flag $len 0 $timestap
    r->uri.base[r->uri.len] = '\0';
    r->mc_flag.base[r->mc_flag.len] = '\0';
    return snprintf(output, max_length, "VALUE %s %s %d 0 %d\r\n",
        r->uri.base, r->mc_flag.base, r->content_length-2,
        (r->expire == 0 ? r->expire : timer_now(&master_timer)));
}

size_t asc_tokenize(char *command, struct token *token, int ntoken_max)
{
    char *s = NULL;
    char *e = NULL; /* start and end marker */
    int ntoken = 0;  /* # tokens */

    for (s = e = command, ntoken = 0; ntoken < ntoken_max - 1; e++) {
        if (*e == ' ') {
            if (s != e) {
                /* save token */
                token[ntoken].val = s;
                token[ntoken].len = e - s;
                ntoken++;
            }
            s = e + 1;
        } else if (*e == '\0') {
            if (s != e) {
                /* save final token */
                token[ntoken].val = s;
                token[ntoken].len = e - s;
                ntoken++;
            }
            break;
        }
    }

    /*
     * If we scanned the whole string, the terminal value pointer is NULL,
     * otherwise it is the first unprocessed character.
     */
    token[ntoken].val = (*e == '\0') ? NULL : e;
    token[ntoken].len = 0;
    ntoken++;

    return ntoken;
}

int mc_parse_value_length(ohc_request_t *r, char *p, ssize_t len)
{
    char *endp = NULL;
    r->content_length = strtoul(p, &endp, 10);
    if(endp == p || endp != p + len) {
        r->error_reason = "InvalidContentLength";
        return OHC_ERROR;
    }
    //add \r\n
    r->content_length += 2;
    return OHC_OK;
}

int mc_parse_expire(ohc_request_t *r, char *p, ssize_t len)
{
    char *endp = NULL;
    int ex = strtoul(p, &endp, 10);
    if(endp == p || endp != p + len) {
        r->error_reason = "InvalidExpire";
        return OHC_ERROR;
    }

    if (ex != 0)
    {
        r->expire = ex;
        r->expire += timer_now(&master_timer);
    }

    return OHC_OK;
}

int asc_parse_type(ohc_request_t *r, struct token *token, int ntoken)
{
    char *tval = NULL;      /* token value */
    size_t tlen = 0;     /* token length */

    //at least has command and key
    if (ntoken <= TOKEN_COMMAND || ntoken <= TOKEN_KEY)
    {
        return OHC_ERROR;
    }

    r->method = OHC_HTTP_METHOD_INVALID;

    tval = token[TOKEN_COMMAND].val;
    tlen = token[TOKEN_COMMAND].len;

    switch (tlen) 
    {
        case 3:
            if (strncmp(tval, "get", tlen) == 0)
            {
                r->method = OHC_HTTP_METHOD_GET;
            }
            else if (strncmp(tval, "add", tlen) == 0)
            {
                r->method = OHC_HTTP_METHOD_POST;
            }
            else if (strncmp(tval, "set", tlen) == 0)
            {
                r->method = OHC_HTTP_METHOD_PUT;
            }
            break;
        case 5:
            //getex current same as get
            if (strncmp(tval, "getex", tlen) == 0)
            {
                r->method = OHC_HTTP_METHOD_GET;
            }
            break;
        case 6:
            if (strncmp(tval, "delete", tlen) == 0)
            {
                r->method = OHC_HTTP_METHOD_DELETE;
            }

            break;
        default:
            r->method = OHC_HTTP_METHOD_INVALID;
    }

    if (r->method == OHC_HTTP_METHOD_INVALID)
    {
        return OHC_ERROR;
    }

    r->uri.base = token[TOKEN_KEY].val;
    r->uri.len = token[TOKEN_KEY].len;

    if (r->method == OHC_HTTP_METHOD_PUT || r->method == OHC_HTTP_METHOD_POST)
    {
        if (ntoken <= TOKEN_VLEN)
        {
            return OHC_ERROR;
        }

        r->mc_flag.base = token[TOKEN_FLAGS].val;
        r->mc_flag.len = token[TOKEN_FLAGS].len;
        //VALUE key flag len 0 time\r\n
        r->put_header_length = token[TOKEN_KEY].len +
            token[TOKEN_FLAGS].len + token[TOKEN_VLEN].len + 23;

        int code = mc_parse_value_length(r, token[TOKEN_VLEN].val, token[TOKEN_VLEN].len);
        if (code != OHC_OK)
        {
            return code;
        }

        code = mc_parse_expire(r, token[TOKEN_EXPIRY].val, token[TOKEN_EXPIRY].len);
        if (code != OHC_OK)
        {
            return code;
        }
    }

    return OHC_OK;

}

int asc_dispatch(ohc_request_t *r)
{
    struct token token[TOKEN_MAX];
    int ntoken = 0;
    int code = 0;
    ntoken = asc_tokenize(r->_buffer, token, TOKEN_MAX);
    code = asc_parse_type(r, token, ntoken);

    return code;
}

int mc_request_parse(ohc_request_t *r)
{
    if (r->input_size == 0)
    {
        return OHC_AGAIN;
    }

    *r->buf_pos = '\0';
    r->put_header_nr = 0;
    r->put_headers[0].base = NULL;
    r->put_headers[0].len = 0;
    r->put_header_length = 0;

    char *el = NULL;
    char *cont = NULL;
    el = memchr(r->_buffer, '\n', r->input_size);

    //参考twemcache实现，如果找不到\n且当前未处理内容过长，则断开连接
    if (el == NULL)
    {
        if (r->input_size > 1024)
        {
            char *ptr = r->_buffer;
            while (*ptr == ' ') {
                ++ptr;
            }

            if (ptr - r->_buffer > 100 || (strncmp(ptr, "get ", 4)))
            {
                //返回错误，关闭连接
                r->error_reason = "RequestHeadTooBig";
                r->keepalive = 0;
                return fail_return(r);
            }
        }

        return OHC_AGAIN;
    }

    cont = el + 1;

    if ((el - r->_buffer) > 1 && *(el - 1) == '\r') {
        el--;
    }

    char c = *el;
    *el = '\0';

    if (asc_dispatch(r) == OHC_ERROR)
    {
        r->error_reason = "RequestAscDispatchError";
        return fail_return(r);
    }

    *el = c;

    if (r->method == OHC_HTTP_METHOD_PUT || r->method == OHC_HTTP_METHOD_POST)
    {
        if (r->buf_pos - cont > 0)
        {
            r->put_headers[0].base = cont;
            r->put_headers[0].len = r->buf_pos - cont;
            r->put_header_nr = 1;
        }
    }

    return OHC_DONE;
}
