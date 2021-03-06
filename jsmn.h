#ifndef __JSMN_H_
#define __JSMN_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JSMN_PARENT_LINKS
/* enable fast parsing */
#define JSMN_PARENT_LINKS
#endif

/**
 * JSON type identifier. Basic types are:
 *      o Object
 *      o Array
 *      o String
 *      o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1,
    JSMN_ARRAY = 2,
    JSMN_STRING = 3,
    /* number, bool (true/false) or null */
    JSMN_PRIMITIVE = 4
} jsmntype_t;

enum jsmnerr {
    /* Not enough tokens were provided */
    JSMN_ERROR_NOMEM = -1,
    /* Invalid character inside JSON string */
    JSMN_ERROR_INVAL = -2,
    /* The string is not a full JSON packet, more bytes expected */
    JSMN_ERROR_PART = -3,
    /* key not found */
    JSMN_ERROR_NOFOUND = -4,
    /* wrong type */
    JSMN_ERROR_WRONG_TYPE = -5,
    /* parsing failed */
    JSMN_ERROR_NOPARSE = -6
};

const char* jsmn_strerror(int error_code);

/**
 * JSON token description.
 * type         type (object, array, string etc.)
 * start    start position in JSON data string
 * end      end position in JSON data string
 */
typedef struct {
    jsmntype_t type;
    int start;
    int end;
    int size;
#ifdef JSMN_PARENT_LINKS
    int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
    jsmntok_t* tokens;
    unsigned int num_tokens;
    unsigned int pos; /* offset in the JSON string */
    unsigned int toknext; /* next token to allocate */
    int toksuper; /* superior token node, e.g parent object or array */
    int owns_tokens;
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser *parser);
void jsmn_destroy(jsmn_parser* parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object. return >=0 is number of tokens. < 0 is error
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
        jsmntok_t *tokens, unsigned int num_tokens);
/* dynamically allocate and resize tokens.
   they will be available on the parser when finished
   be sure to call jsmn_destroy to free the tokens */
int jsmn_parse_dynamic(jsmn_parser *parser, const char *js, size_t len);
int jsmn_parse_dynamic_str(jsmn_parser *parser, const char *js);
int jsmn_parse_text(const char *js, jsmntok_t *tokens, unsigned int num_tokens);

/* assume token is an JSMN_OBJECT. return value for key_name if found
   else return NULL */
jsmntok_t* jsmn_lookup(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name);

/* move to next key in this object, skip current key and value + children */
jsmntok_t* jsmn_obj_next(jsmntok_t* token);
/* next element of array */
jsmntok_t* jsmn_array_next(jsmntok_t* token);
jsmntok_t* jsmn_array_first(jsmntok_t* token);

/* print token text */
void jsmn_print_text(const char* json_text, jsmntok_t* token);
/* to stderr for debugging */
void jsmn_print_token(const char* json_text, jsmntok_t* token);

/* assume token is an JSMN_OBJECT. try to find a key with value of type value_type.
   return value if found. else return NULL */
jsmntok_t* jsmn_lookup_type(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name,
    jsmntype_t value_type);

/* assume token is a JSMN_OBJECT. lookup key. if found and value is a string,
   malloc return space and copy value text. be sure to NULL check and free
   the result */
char* jsmn_lookup_string_copy(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name);

/* returns 0 on success, or error if not found, wrong type or parsing failed.
 result is set to zero on failure */
int jsmn_lookup_double(
    const char* json_text,
    jsmntok_t* token,
    const char* key_name,
    double* result);

/* assume token is a JSMN_ARRAY. return the child at i if i is in bounds.
   else return NULL */
jsmntok_t* jsmn_array_at(jsmntok_t* token, size_t i);
/* copy token value to null terminated string allocated with malloc.
   be sure to free the result and check for NULL in case allocation failed */
char* jsmn_string(const char* json_text, jsmntok_t* token);

/* try to goto the value described by path_format and extra args
   returns token on success, NULL on failure
   path_format is a string containing a sequence of 'a's or 'o's
   a for array, o for object. each character must have a matching paramater
   for arrays this is an integer index. for objects this is the string field key.

   example:
   found = jsmn_find(text, tok, "oaoo", "data", 0, "weather", "cloud");
 */
jsmntok_t* jsmn_find(
    const char* json, jsmntok_t* token, const char* path_format, ...);

/* find and copy string value */
char* jsmn_find_string_copy(
    const char* json, jsmntok_t* token,
    const char* path_format, ...);

/* 0 on success, 1 on failure to parse */
int jsmn_try_parse_double(const char* text, jsmntok_t* token, double* result);
double jsmn_parse_double(const char* text, jsmntok_t* token);

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_H_ */
