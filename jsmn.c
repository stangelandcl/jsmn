#include "jsmn.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Allocates a fresh unused token from the token pull.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser)
{
    jsmntok_t *tok;
    unsigned int sz;
    if (parser->toknext >= parser->num_tokens)
    {
        if(parser->owns_tokens)
        {
            sz = parser->num_tokens * 2;
            if(!sz)
                sz = 64;
            if(!(tok = realloc(parser->tokens, sz)))
                return NULL;
            parser->tokens = tok;
            parser->num_tokens = sz;
        }
        else return NULL;
    }
    tok = &parser->tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
#ifdef JSMN_PARENT_LINKS
    tok->parent = -1;
#endif
    return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type,
                            int start, int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(
    jsmn_parser *parser, const char *js, size_t len)
{
    jsmntok_t *token;
    int start;

    start = parser->pos;

    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        switch (js[parser->pos]) {
#ifndef JSMN_STRICT
            /* In strict mode primitive must be followed by "," or "}" or "]" */
            case ':':
#endif
            case '\t' : case '\r' : case '\n' : case ' ' :
            case ','  : case ']'  : case '}' :
                goto found;
        }
        if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
        }
    }
#ifdef JSMN_STRICT
    /* In strict mode primitive must be followed by a comma/object/array */
    parser->pos = start;
    return JSMN_ERROR_PART;
#endif

found:
    if (parser->tokens == NULL) {
        parser->pos--;
        return 0;
    }
    token = jsmn_alloc_token(parser);
    if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
    token->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(
    jsmn_parser *parser, const char *js, size_t len)
{
    jsmntok_t *token;

    int start = parser->pos;

    parser->pos++;

    /* Skip starting quote */
    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        char c = js[parser->pos];

        /* Quote: end of string */
        if (c == '\"') {
            if (parser->tokens == NULL) {
                return 0;
            }
            token = jsmn_alloc_token(parser);
            if (token == NULL) {
                parser->pos = start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start+1, parser->pos);
#ifdef JSMN_PARENT_LINKS
            token->parent = parser->toksuper;
#endif
            return 0;
        }

        /* Backslash: Quoted symbol expected */
        if (c == '\\' && parser->pos + 1 < len) {
            int i;
            parser->pos++;
            switch (js[parser->pos]) {
                /* Allowed escaped symbols */
                case '\"': case '/' : case '\\' : case 'b' :
                case 'f' : case 'r' : case 'n'  : case 't' :
                    break;
                /* Allows escaped symbol \uXXXX */
                case 'u':
                    parser->pos++;
                    for(i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
                        /* If it isn't a hex character we have an error */
                        if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
                                    (js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
                                    (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
                            parser->pos = start;
                            return JSMN_ERROR_INVAL;
                        }
                        parser->pos++;
                    }
                    parser->pos--;
                    break;
                /* Unexpected symbol */
                default:
                    parser->pos = start;
                    return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = start;
    return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
        jsmntok_t *tokens, unsigned int num_tokens) {
    int r;
    int i;
    jsmntok_t *token;
    int count = parser->toknext;

    if(tokens)
    {
        parser->tokens = tokens;
        parser->num_tokens = num_tokens;
        parser->owns_tokens = 0;
    }
    else if(!parser->tokens)
    {
        parser->tokens = malloc(sizeof(jsmntok_t) * 64);
        parser->num_tokens = parser->tokens ? 64 : 0;
        parser->owns_tokens = 1;
    }


    for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        char c;
        jsmntype_t type;

        c = js[parser->pos];
        switch (c) {
            case '{': case '[':
                count++;
                if (parser->tokens == NULL) {
                    break;
                }
                token = jsmn_alloc_token(parser);
                if (token == NULL)
                    return JSMN_ERROR_NOMEM;
                if (parser->toksuper != -1) {
                    parser->tokens[parser->toksuper].size++;
#ifdef JSMN_PARENT_LINKS
                    token->parent = parser->toksuper;
#endif
                }
                token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
                token->start = parser->pos;
                parser->toksuper = parser->toknext - 1;
                break;
            case '}': case ']':
                if (parser->tokens == NULL)
                    break;
                type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
                if (parser->toknext < 1) {
                    return JSMN_ERROR_INVAL;
                }
                token = &parser->tokens[parser->toknext - 1];
                for (;;) {
                    if (token->start != -1 && token->end == -1) {
                        if (token->type != type) {
                            return JSMN_ERROR_INVAL;
                        }
                        token->end = parser->pos + 1;
                        parser->toksuper = token->parent;
                        break;
                    }
                    if (token->parent == -1) {
                        if(token->type != type || parser->toksuper == -1) {
                            return JSMN_ERROR_INVAL;
                        }
                        break;
                    }
                    token = &parser->tokens[token->parent];
                }
#else
                for (i = parser->toknext - 1; i >= 0; i--) {
                    token = &parser->tokens[i];
                    if (token->start != -1 && token->end == -1) {
                        if (token->type != type) {
                            return JSMN_ERROR_INVAL;
                        }
                        parser->toksuper = -1;
                        token->end = parser->pos + 1;
                        break;
                    }
                }
                /* Error if unmatched closing bracket */
                if (i == -1) return JSMN_ERROR_INVAL;
                for (; i >= 0; i--) {
                    token = &parser->tokens[i];
                    if (token->start != -1 && token->end == -1) {
                        parser->toksuper = i;
                        break;
                    }
                }
#endif
                break;
            case '\"':
                r = jsmn_parse_string(parser, js, len);
                if (r < 0) return r;
                count++;
                if (parser->toksuper != -1 && parser->tokens != NULL)
                    parser->tokens[parser->toksuper].size++;
                break;
            case '\t' : case '\r' : case '\n' : case ' ':
                break;
            case ':':
                parser->toksuper = parser->toknext - 1;
                break;
            case ',':
                if (parser->tokens != NULL && parser->toksuper != -1 &&
                        parser->tokens[parser->toksuper].type != JSMN_ARRAY &&
                        parser->tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
                    parser->toksuper = parser->tokens[parser->toksuper].parent;
#else
                    for (i = parser->toknext - 1; i >= 0; i--) {
                        if (parser->tokens[i].type == JSMN_ARRAY ||
                            parser->tokens[i].type == JSMN_OBJECT) {
                            if (parser->tokens[i].start != -1 &&
                                parser->tokens[i].end == -1) {
                                parser->toksuper = i;
                                break;
                            }
                        }
                    }
#endif
                }
                break;
#ifdef JSMN_STRICT
            /* In strict mode primitives are: numbers and booleans */
            case '-': case '0': case '1' : case '2': case '3' : case '4':
            case '5': case '6': case '7' : case '8': case '9':
            case 't': case 'f': case 'n' :
                /* And they must not be keys of the object */
                if (parser->tokens != NULL && parser->toksuper != -1) {
                    jsmntok_t *t = &parser->tokens[parser->toksuper];
                    if (t->type == JSMN_OBJECT ||
                            (t->type == JSMN_STRING && t->size != 0)) {
                        return JSMN_ERROR_INVAL;
                    }
                }
#else
            /* In non-strict mode every unquoted value is a primitive */
            default:
#endif
                r = jsmn_parse_primitive(parser, js, len);
                if (r < 0) return r;
                count++;
                if (parser->toksuper != -1 && parser->tokens != NULL)
                    parser->tokens[parser->toksuper].size++;
                break;

#ifdef JSMN_STRICT
            /* Unexpected char in strict mode */
            default:
                return JSMN_ERROR_INVAL;
#endif
        }
    }

    if (parser->tokens != NULL) {
        for (i = parser->toknext - 1; i >= 0; i--) {
            /* Unmatched opened object or array */
            if (parser->tokens[i].start != -1 && parser->tokens[i].end == -1) {
                return JSMN_ERROR_PART;
            }
        }
    }

    return count;
}

int jsmn_parse_dynamic(jsmn_parser *parser, const char *js, size_t len)
{
    return jsmn_parse(parser, js, len, NULL, 0);
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens
 * available.
 */
void jsmn_init(jsmn_parser *parser) {
    parser->tokens = NULL;
    parser->num_tokens = 0;
    parser->owns_tokens = 0;
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

void jsmn_destroy(jsmn_parser* parser)
{
    if(parser->owns_tokens)
        free(parser->tokens);
}

const char* jsmn_strerror(int error_code)
{
    switch(error_code)
    {
        case JSMN_ERROR_NOMEM: return "jsmn: Not enough tokens provided";
        case JSMN_ERROR_INVAL: return "jsmn: Invalid character in json string";
        case JSMN_ERROR_PART: return "jsmn: Not full json packet";
        default: return "jsmn: Success. Token count";
    }
}

jsmntok_t* jsmn_array_first(jsmntok_t* token)
{
    if(token->type != JSMN_ARRAY)
        return NULL;
    return token + 1;
}

jsmntok_t* jsmn_array_next(jsmntok_t* token)
{
    jsmntok_t *t, *c;
    int i;

    t = token;
    if(t->type == JSMN_OBJECT)
    {
        c = t;
        ++t; /* move to first key */
        for(i=0;i<c->size;i++)
            t = jsmn_obj_next(t);
    }
    else if(t->type == JSMN_ARRAY)
    {
        c = t;
        ++t;
        for(i=0;i<c->size;i++)
            t = jsmn_array_next(t);
    }
    else
        ++t;
    return t;
}

jsmntok_t* jsmn_obj_next(jsmntok_t* token)
{
    jsmntok_t *t, *c;
    int j;

#if 0
    fprintf(stderr, "obj next type=%d\n", token->type);
#endif
    t = token + 1; /* assume token is key so token + 1 is value */
    if(t->type == JSMN_OBJECT)
    {
        c = t;
        ++t; /* move to first key */
        for(j=0;j<c->size;j++)
            t = jsmn_obj_next(t);
    }
    else if(t->type == JSMN_ARRAY)
    {
        c = t;
        ++t;
#if 0
        fprintf(stderr, "array size=%d\n", c->size);
        fprintf(stderr, "t=%d\n", t->type);
#endif
        for(j=0;j<c->size;j++)
            t = jsmn_array_next(t);
    }
    else
        ++t;
    return t;
}
jsmntok_t* jsmn_lookup(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name)
{
    return jsmn_lookup_type(json_text, token, key_name, 0);
}

jsmntok_t* jsmn_lookup_type(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name,
    jsmntype_t value_type)
{
	int i;
	size_t sz = strlen(key_name);
    jsmntok_t* t = token + 1 /* move to first key */, *val;
    for(i=0;i<token->size;i++,t=jsmn_obj_next(t))
    {
#if 0
        fprintf(stderr, "searching i=%d sz=%d token=%.*s",
                (int)i, token->size, t->end - t->start, json_text + t->start);
        fprintf(stderr, "type=%d sz=%d s1=%d s2=%d memcmp=%d\n",
                t->type == value_type,
                sz == t->end - t->start,
                (int)sz, t->end - t->start,
                !memcmp(key_name, json_text + t->start, sz));
#endif
        val = t+1;
        if((!value_type || val->type == value_type)
           && sz == t->end - t->start &&
           !memcmp(key_name, json_text + t->start, sz))
            return val;
    }
    return NULL;
}

jsmntok_t* jsmn_array_at(jsmntok_t* token, size_t i)
{
    size_t j;
    jsmntok_t* t;

    if((int)i >= token->size)
        return NULL;

    t = token + 1;
    for(j=0;j<i;j++)
        t = jsmn_array_next(t);
    return t;
}

char* jsmn_string(const char* json_text, jsmntok_t* token)
{
    size_t sz = token->end - token->start;
    char* c;
    if((c = malloc(sz + 1)))
    {
        memcpy(c, json_text + token->start, sz);
        c[sz] = '\0';
    }
    return c;
}

char* jsmn_lookup_string_copy(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name)
{
    if(!(token = jsmn_lookup_type(json_text, token, key_name, JSMN_STRING)))
        return NULL;

    return jsmn_string(json_text, token);
}
void jsmn_print_text(const char* json_text, jsmntok_t* t)
{
    fprintf(stderr, "%.*s", t->end - t->start, json_text + t->start);
}
void jsmn_print_token(const char* json_text, jsmntok_t* t)
{
    fprintf(stderr, "start=%d end=%d text='%.*s'\n",
            t->start,
            t->end,
            t->end - t->start,
            json_text + t->start);
}
jsmntok_t* jsmn_findv(
    const char* json, jsmntok_t* token,
    const char* path_format, va_list args)
{
    jsmntok_t* t = token;
    char *c, *key;
    int idx;

    for(c=(char*)path_format;*c;++c)
    {
#if 0
        fprintf(stderr, "path=%s t=%d c=%c\n", path_format, token->type, *c);
#endif
        if(*c == 'o')
        {
            key = va_arg(args, char*);
#if 0
            fprintf(stderr, "searching for %s in %d\n", key, t != NULL);
#endif
            if(t && t->type == JSMN_OBJECT)
                t = jsmn_lookup(json, t, key);
        }
        else if(*c == 'a')
        {
            idx = va_arg(args, int);
#if 0
            fprintf(stderr, "searching for %d in %d\n", idx, t != NULL);
#endif
            if(t && t->type == JSMN_ARRAY)
                t = jsmn_array_at(t, idx);
        }
        else t = NULL;
    }
    return t;
}
jsmntok_t* jsmn_find(
    const char* json, jsmntok_t* token,
    const char* path_format, ...)
{
    va_list args;
    jsmntok_t* t;
    va_start(args, path_format);
    t = jsmn_findv(json, token, path_format, args);
    va_end(args);
    return t;
}

int jsmn_parse_text(const char *js, jsmntok_t *tokens, unsigned int num_tokens)
{
    jsmn_parser p;
    jsmn_init(&p);
    return jsmn_parse(&p, js, strlen(js), tokens, num_tokens);
}

char* jsmn_find_string_copy(
    const char* json, jsmntok_t* token,
    const char* path_format, ...)
{
    va_list args;
    jsmntok_t* t;
    va_start(args, path_format);
    t = jsmn_findv(json, token, path_format, args);
    va_end(args);
    if(t)
        return jsmn_string(json, t);
    return NULL;
}
