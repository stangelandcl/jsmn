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
    JSMN_PRIMITIVE = 4
} jsmntype_t;

enum jsmnerr {
    /* Not enough tokens were provided */
    JSMN_ERROR_NOMEM = -1,
    /* Invalid character inside JSON string */
    JSMN_ERROR_INVAL = -2,
    /* The string is not a full JSON packet, more bytes expected */
    JSMN_ERROR_PART = -3
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
    unsigned int pos; /* offset in the JSON string */
    unsigned int toknext; /* next token to allocate */
    int toksuper; /* superior token node, e.g parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object. return >=0 is number of tokens. < 0 is error
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
        jsmntok_t *tokens, unsigned int num_tokens);

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

/* print token text */
void jsmn_print_text(const char* json_text, jsmntok_t* token);

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

/* assume token is a JSMN_ARRAY. return the child at i if i is in bounds.
   else return NULL */
jsmntok_t* jsmn_array_at(jsmntok_t* token, size_t i);
/* copy token value to null terminated string allocated with malloc.
   be sure to free the result and check for NULL in case allocation failed */
char* jsmn_string(const char* json_text, jsmntok_t* token);



#ifdef __cplusplus
}
#endif

#endif /* __JSMN_H_ */
