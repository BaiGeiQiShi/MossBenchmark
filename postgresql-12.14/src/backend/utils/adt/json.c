                                                                            
   
          
                            
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "parser/parse_coerce.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/lsyscache.h"
#include "utils/json.h"
#include "utils/jsonapi.h"
#include "utils/typcache.h"
#include "utils/syscache.h"

   
                                                                    
                                                                      
                           
   
typedef enum                              
{
  JSON_PARSE_VALUE,                               
  JSON_PARSE_STRING,                                                  
  JSON_PARSE_ARRAY_START,                                       
  JSON_PARSE_ARRAY_NEXT,                                                
  JSON_PARSE_OBJECT_START,                                      
  JSON_PARSE_OBJECT_LABEL,                                      
  JSON_PARSE_OBJECT_NEXT,                                              
  JSON_PARSE_OBJECT_COMMA,                                           
  JSON_PARSE_END                                                          
} JsonParseContext;

typedef enum                                        
{
  JSONTYPE_NULL,                                               
  JSONTYPE_BOOL,                                       
  JSONTYPE_NUMERIC,                      
  JSONTYPE_DATE,                                                 
  JSONTYPE_TIMESTAMP,
  JSONTYPE_TIMESTAMPTZ,
  JSONTYPE_JSON,                                   
  JSONTYPE_ARRAY,                
  JSONTYPE_COMPOSITE,                
  JSONTYPE_CAST,                                                   
  JSONTYPE_OTHER                    
} JsonTypeCategory;

typedef struct JsonAggState
{
  StringInfo str;
  JsonTypeCategory key_category;
  Oid key_output_func;
  JsonTypeCategory val_category;
  Oid val_output_func;
} JsonAggState;

static inline void
json_lex(JsonLexContext *lex);
static inline void
json_lex_string(JsonLexContext *lex);
static inline void
json_lex_number(JsonLexContext *lex, char *s, bool *num_err, int *total_len);
static inline void
parse_scalar(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_object_field(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_object(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_array_element(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_array(JsonLexContext *lex, JsonSemAction *sem);
static void
report_parse_error(JsonParseContext ctx, JsonLexContext *lex) pg_attribute_noreturn();
static void
report_invalid_token(JsonLexContext *lex) pg_attribute_noreturn();
static int
report_json_context(JsonLexContext *lex);
static char *
extract_mb_char(char *s);
static void
composite_to_json(Datum composite, StringInfo result, bool use_line_feeds);
static void
array_dim_to_json(StringInfo result, int dim, int ndims, int *dims, Datum *vals, bool *nulls, int *valcount, JsonTypeCategory tcategory, Oid outfuncoid, bool use_line_feeds);
static void
array_to_json_internal(Datum array, StringInfo result, bool use_line_feeds);
static void
json_categorize_type(Oid typoid, JsonTypeCategory *tcategory, Oid *outfuncoid);
static void
datum_to_json(Datum val, bool is_null, StringInfo result, JsonTypeCategory tcategory, Oid outfuncoid, bool key_scalar);
static void
add_json(Datum val, bool is_null, StringInfo result, Oid val_type, bool key_scalar);
static text *
catenate_stringinfo_string(StringInfo buffer, const char *addon);

                                                     
static JsonSemAction nullSemAction = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

                                               

   
            
   
                                         
   
static inline JsonTokenType
lex_peek(JsonLexContext *lex)
{
  return lex->token_type;
}

   
              
   
                                                                           
                                                                                
                                         
   
                                                       
   
static inline bool
lex_accept(JsonLexContext *lex, JsonTokenType token, char **lexeme)
{
  if (lex->token_type == token)
  {
    if (lexeme != NULL)
    {
      if (lex->token_type == JSON_TOKEN_STRING)
      {
        if (lex->strval != NULL)
        {
          *lexeme = pstrdup(lex->strval->data);
        }
      }
      else
      {
        int len = (lex->token_terminator - lex->token_start);
        char *tokstr = palloc(len + 1);

        memcpy(tokstr, lex->token_start, len);
        tokstr[len] = '\0';
        *lexeme = tokstr;
      }
    }
    json_lex(lex);
    return true;
  }
  return false;
}

   
              
   
                                                                            
                                                    
   
static inline void
lex_expect(JsonParseContext ctx, JsonLexContext *lex, JsonTokenType token)
{
  if (!lex_accept(lex, token, NULL))
  {
    report_parse_error(ctx, lex);
  }
}

                                                        
#define JSON_ALPHANUMERIC_CHAR(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') || (c) == '_' || IS_HIGHBIT_SET(c))

   
                                                                 
   
                                                          
   
bool
IsValidJsonNumber(const char *str, int len)
{
  bool numeric_error;
  int total_len;
  JsonLexContext dummy_lex;

  if (len <= 0)
  {
    return false;
  }

     
                                                                        
     
                                                                            
                       
     
  if (*str == '-')
  {
    dummy_lex.input = unconstify(char *, str) + 1;
    dummy_lex.input_length = len - 1;
  }
  else
  {
    dummy_lex.input = unconstify(char *, str);
    dummy_lex.input_length = len;
  }

  json_lex_number(&dummy_lex, dummy_lex.input, &numeric_error, &total_len);

  return (!numeric_error) && (total_len == dummy_lex.input_length);
}

   
          
   
Datum
json_in(PG_FUNCTION_ARGS)
{
  char *json = PG_GETARG_CSTRING(0);
  text *result = cstring_to_text(json);
  JsonLexContext *lex;

                   
  lex = makeJsonLexContext(result, false);
  pg_parse_json(lex, &nullSemAction);

                                                            
  PG_RETURN_TEXT_P(result);
}

   
           
   
Datum
json_out(PG_FUNCTION_ARGS)
{
                                                                   
  Datum txt = PG_GETARG_DATUM(0);

  PG_RETURN_CSTRING(TextDatumGetCString(txt));
}

   
                
   
Datum
json_send(PG_FUNCTION_ARGS)
{
  text *t = PG_GETARG_TEXT_PP(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendtext(&buf, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                   
   
Datum
json_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  char *str;
  int nbytes;
  JsonLexContext *lex;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);

                    
  lex = makeJsonLexContextCstringLen(str, nbytes, false);
  pg_parse_json(lex, &nullSemAction);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(str, nbytes));
}

   
                      
   
                                                      
                           
   
                                                                         
                       
   
                                                                         
                                                             
   
JsonLexContext *
makeJsonLexContext(text *json, bool need_escapes)
{
  return makeJsonLexContextCstringLen(VARDATA_ANY(json), VARSIZE_ANY_EXHDR(json), need_escapes);
}

JsonLexContext *
makeJsonLexContextCstringLen(char *json, int len, bool need_escapes)
{
  JsonLexContext *lex = palloc0(sizeof(JsonLexContext));

  lex->input = lex->token_terminator = lex->line_start = json;
  lex->line_number = 1;
  lex->input_length = len;
  if (need_escapes)
  {
    lex->strval = makeStringInfo();
  }
  return lex;
}

   
                 
   
                                                     
   
                                                                           
                                                                             
                                                                           
                                                             
   
void
pg_parse_json(JsonLexContext *lex, JsonSemAction *sem)
{
  JsonTokenType tok;

                             
  json_lex(lex);

  tok = lex_peek(lex);

                                  
  switch (tok)
  {
  case JSON_TOKEN_OBJECT_START:
    parse_object(lex, sem);
    break;
  case JSON_TOKEN_ARRAY_START:
    parse_array(lex, sem);
    break;
  default:
    parse_scalar(lex, sem);                                
  }

  lex_expect(JSON_PARSE_END, lex, JSON_TOKEN_END);
}

   
                             
   
                                                                           
                                                   
   
                                                    
   
int
json_count_array_elements(JsonLexContext *lex)
{
  JsonLexContext copylex;
  int count;

     
                                                                           
                                                                         
                                                     
     
  memcpy(&copylex, lex, sizeof(JsonLexContext));
  copylex.strval = NULL;                                    
  copylex.lex_level++;

  count = 0;
  lex_expect(JSON_PARSE_ARRAY_START, &copylex, JSON_TOKEN_ARRAY_START);
  if (lex_peek(&copylex) != JSON_TOKEN_ARRAY_END)
  {
    do
    {
      count++;
      parse_array_element(&copylex, &nullSemAction);
    } while (lex_accept(&copylex, JSON_TOKEN_COMMA, NULL));
  }
  lex_expect(JSON_PARSE_ARRAY_NEXT, &copylex, JSON_TOKEN_ARRAY_END);

  return count;
}

   
                                                                      
                               
                                                  
                      
                     
                      
                    
   
static inline void
parse_scalar(JsonLexContext *lex, JsonSemAction *sem)
{
  char *val = NULL;
  json_scalar_action sfunc = sem->scalar;
  char **valaddr;
  JsonTokenType tok = lex_peek(lex);

  valaddr = sfunc == NULL ? NULL : &val;

                                                                 
  switch (tok)
  {
  case JSON_TOKEN_TRUE:
    lex_accept(lex, JSON_TOKEN_TRUE, valaddr);
    break;
  case JSON_TOKEN_FALSE:
    lex_accept(lex, JSON_TOKEN_FALSE, valaddr);
    break;
  case JSON_TOKEN_NULL:
    lex_accept(lex, JSON_TOKEN_NULL, valaddr);
    break;
  case JSON_TOKEN_NUMBER:
    lex_accept(lex, JSON_TOKEN_NUMBER, valaddr);
    break;
  case JSON_TOKEN_STRING:
    lex_accept(lex, JSON_TOKEN_STRING, valaddr);
    break;
  default:
    report_parse_error(JSON_PARSE_VALUE, lex);
  }

  if (sfunc != NULL)
  {
    (*sfunc)(sem->semstate, val, tok);
  }
}

static void
parse_object_field(JsonLexContext *lex, JsonSemAction *sem)
{
     
                                                                         
                                                                        
                                          
     

  char *fname = NULL;                          
  json_ofield_action ostart = sem->object_field_start;
  json_ofield_action oend = sem->object_field_end;
  bool isnull;
  char **fnameaddr = NULL;
  JsonTokenType tok;

  if (ostart != NULL || oend != NULL)
  {
    fnameaddr = &fname;
  }

  if (!lex_accept(lex, JSON_TOKEN_STRING, fnameaddr))
  {
    report_parse_error(JSON_PARSE_STRING, lex);
  }

  lex_expect(JSON_PARSE_OBJECT_LABEL, lex, JSON_TOKEN_COLON);

  tok = lex_peek(lex);
  isnull = tok == JSON_TOKEN_NULL;

  if (ostart != NULL)
  {
    (*ostart)(sem->semstate, fname, isnull);
  }

  switch (tok)
  {
  case JSON_TOKEN_OBJECT_START:
    parse_object(lex, sem);
    break;
  case JSON_TOKEN_ARRAY_START:
    parse_array(lex, sem);
    break;
  default:
    parse_scalar(lex, sem);
  }

  if (oend != NULL)
  {
    (*oend)(sem->semstate, fname, isnull);
  }
}

static void
parse_object(JsonLexContext *lex, JsonSemAction *sem)
{
     
                                                                           
                                            
     
  json_struct_action ostart = sem->object_start;
  json_struct_action oend = sem->object_end;
  JsonTokenType tok;

  check_stack_depth();

  if (ostart != NULL)
  {
    (*ostart)(sem->semstate);
  }

     
                                                                        
                                                                            
                                                                            
                 
     
  lex->lex_level++;

                                                          
  lex_expect(JSON_PARSE_OBJECT_START, lex, JSON_TOKEN_OBJECT_START);

  tok = lex_peek(lex);
  switch (tok)
  {
  case JSON_TOKEN_STRING:
    parse_object_field(lex, sem);
    while (lex_accept(lex, JSON_TOKEN_COMMA, NULL))
    {
      parse_object_field(lex, sem);
    }
    break;
  case JSON_TOKEN_OBJECT_END:
    break;
  default:
                                                            
    report_parse_error(JSON_PARSE_OBJECT_START, lex);
  }

  lex_expect(JSON_PARSE_OBJECT_NEXT, lex, JSON_TOKEN_OBJECT_END);

  lex->lex_level--;

  if (oend != NULL)
  {
    (*oend)(sem->semstate);
  }
}

static void
parse_array_element(JsonLexContext *lex, JsonSemAction *sem)
{
  json_aelem_action astart = sem->array_element_start;
  json_aelem_action aend = sem->array_element_end;
  JsonTokenType tok = lex_peek(lex);

  bool isnull;

  isnull = tok == JSON_TOKEN_NULL;

  if (astart != NULL)
  {
    (*astart)(sem->semstate, isnull);
  }

                                                       
  switch (tok)
  {
  case JSON_TOKEN_OBJECT_START:
    parse_object(lex, sem);
    break;
  case JSON_TOKEN_ARRAY_START:
    parse_array(lex, sem);
    break;
  default:
    parse_scalar(lex, sem);
  }

  if (aend != NULL)
  {
    (*aend)(sem->semstate, isnull);
  }
}

static void
parse_array(JsonLexContext *lex, JsonSemAction *sem)
{
     
                                                                           
                                               
     
  json_struct_action astart = sem->array_start;
  json_struct_action aend = sem->array_end;

  check_stack_depth();

  if (astart != NULL)
  {
    (*astart)(sem->semstate);
  }

     
                                                                      
                                                                            
                                                                           
                
     
  lex->lex_level++;

  lex_expect(JSON_PARSE_ARRAY_START, lex, JSON_TOKEN_ARRAY_START);
  if (lex_peek(lex) != JSON_TOKEN_ARRAY_END)
  {

    parse_array_element(lex, sem);

    while (lex_accept(lex, JSON_TOKEN_COMMA, NULL))
    {
      parse_array_element(lex, sem);
    }
  }

  lex_expect(JSON_PARSE_ARRAY_NEXT, lex, JSON_TOKEN_ARRAY_END);

  lex->lex_level--;

  if (aend != NULL)
  {
    (*aend)(sem->semstate);
  }
}

   
                                        
   
static inline void
json_lex(JsonLexContext *lex)
{
  char *s;
  int len;

                                
  s = lex->token_terminator;
  len = s - lex->input;
  while (len < lex->input_length && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
  {
    if (*s == '\n')
    {
      ++lex->line_number;
    }
    ++s;
    ++len;
  }
  lex->token_start = s;

                             
  if (len >= lex->input_length)
  {
    lex->token_start = NULL;
    lex->prev_token_terminator = lex->token_terminator;
    lex->token_terminator = s;
    lex->token_type = JSON_TOKEN_END;
  }
  else
  {
    switch (*s)
    {
                                                                  
    case '{':
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = s + 1;
      lex->token_type = JSON_TOKEN_OBJECT_START;
      break;
    case '}':
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = s + 1;
      lex->token_type = JSON_TOKEN_OBJECT_END;
      break;
    case '[':
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = s + 1;
      lex->token_type = JSON_TOKEN_ARRAY_START;
      break;
    case ']':
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = s + 1;
      lex->token_type = JSON_TOKEN_ARRAY_END;
      break;
    case ',':
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = s + 1;
      lex->token_type = JSON_TOKEN_COMMA;
      break;
    case ':':
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = s + 1;
      lex->token_type = JSON_TOKEN_COLON;
      break;
    case '"':
                  
      json_lex_string(lex);
      lex->token_type = JSON_TOKEN_STRING;
      break;
    case '-':
                            
      json_lex_number(lex, s + 1, NULL, NULL);
      lex->token_type = JSON_TOKEN_NUMBER;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
                            
      json_lex_number(lex, s, NULL, NULL);
      lex->token_type = JSON_TOKEN_NUMBER;
      break;
    default:
    {
      char *p;

         
                                                        
                                                             
                                                              
                                                                 
                                                              
                                                                 
                                          
         
      for (p = s; p - s < lex->input_length - len && JSON_ALPHANUMERIC_CHAR(*p); p++)
                  ;

         
                                                          
                                                                
                             
         
      if (p == s)
      {
        lex->prev_token_terminator = lex->token_terminator;
        lex->token_terminator = s + 1;
        report_invalid_token(lex);
      }

         
                                                          
                                                              
                         
         
      lex->prev_token_terminator = lex->token_terminator;
      lex->token_terminator = p;
      if (p - s == 4)
      {
        if (memcmp(s, "true", 4) == 0)
        {
          lex->token_type = JSON_TOKEN_TRUE;
        }
        else if (memcmp(s, "null", 4) == 0)
        {
          lex->token_type = JSON_TOKEN_NULL;
        }
        else
        {
          report_invalid_token(lex);
        }
      }
      else if (p - s == 5 && memcmp(s, "false", 5) == 0)
      {
        lex->token_type = JSON_TOKEN_FALSE;
      }
      else
      {
        report_invalid_token(lex);
      }
    }
    }                    
  }
}

   
                                                                       
   
static inline void
json_lex_string(JsonLexContext *lex)
{
  char *s;
  int len;
  int hi_surrogate = -1;

  if (lex->strval != NULL)
  {
    resetStringInfo(lex->strval);
  }

  Assert(lex->input_length > 0);
  s = lex->token_start;
  len = lex->token_start - lex->input;
  for (;;)
  {
    s++;
    len++;
                                      
    if (len >= lex->input_length)
    {
      lex->token_terminator = s;
      report_invalid_token(lex);
    }
    else if (*s == '"')
    {
      break;
    }
    else if ((unsigned char)*s < 32)
    {
                                                          
                                                                        
      lex->token_terminator = s;
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Character with value 0x%02x must be escaped.", (unsigned char)*s), report_json_context(lex)));
    }
    else if (*s == '\\')
    {
                                            
      s++;
      len++;
      if (len >= lex->input_length)
      {
        lex->token_terminator = s;
        report_invalid_token(lex);
      }
      else if (*s == 'u')
      {
        int i;
        int ch = 0;

        for (i = 1; i <= 4; i++)
        {
          s++;
          len++;
          if (len >= lex->input_length)
          {
            lex->token_terminator = s;
            report_invalid_token(lex);
          }
          else if (*s >= '0' && *s <= '9')
          {
            ch = (ch * 16) + (*s - '0');
          }
          else if (*s >= 'a' && *s <= 'f')
          {
            ch = (ch * 16) + (*s - 'a') + 10;
          }
          else if (*s >= 'A' && *s <= 'F')
          {
            ch = (ch * 16) + (*s - 'A') + 10;
          }
          else
          {
            lex->token_terminator = s + pg_mblen(s);
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("\"\\u\" must be followed by four hexadecimal digits."), report_json_context(lex)));
          }
        }
        if (lex->strval != NULL)
        {
          char utf8str[5];
          int utf8len;

          if (ch >= 0xd800 && ch <= 0xdbff)
          {
            if (hi_surrogate != -1)
            {
              ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Unicode high surrogate must not follow a high surrogate."), report_json_context(lex)));
            }
            hi_surrogate = (ch & 0x3ff) << 10;
            continue;
          }
          else if (ch >= 0xdc00 && ch <= 0xdfff)
          {
            if (hi_surrogate == -1)
            {
              ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Unicode low surrogate must follow a high surrogate."), report_json_context(lex)));
            }
            ch = 0x10000 + hi_surrogate + (ch & 0x3ff);
            hi_surrogate = -1;
          }

          if (hi_surrogate != -1)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Unicode low surrogate must follow a high surrogate."), report_json_context(lex)));
          }

             
                                                                 
                                                                   
                                                                    
                                       
             

          if (ch == 0)
          {
                                                                  
            ereport(ERROR, (errcode(ERRCODE_UNTRANSLATABLE_CHARACTER), errmsg("unsupported Unicode escape sequence"), errdetail("\\u0000 cannot be converted to text."), report_json_context(lex)));
          }
          else if (GetDatabaseEncoding() == PG_UTF8)
          {
            unicode_to_utf8(ch, (unsigned char *)utf8str);
            utf8len = pg_utf_mblen((unsigned char *)utf8str);
            appendBinaryStringInfo(lex->strval, utf8str, utf8len);
          }
          else if (ch <= 0x007f)
          {
               
                                                               
                                                                  
                          
               
            appendStringInfoChar(lex->strval, (char)ch);
          }
          else
          {
            ereport(ERROR, (errcode(ERRCODE_UNTRANSLATABLE_CHARACTER), errmsg("unsupported Unicode escape sequence"), errdetail("Unicode escape values cannot be used for code point values above 007F when the server encoding is not UTF8."), report_json_context(lex)));
          }
        }
      }
      else if (lex->strval != NULL)
      {
        if (hi_surrogate != -1)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Unicode low surrogate must follow a high surrogate."), report_json_context(lex)));
        }

        switch (*s)
        {
        case '"':
        case '\\':
        case '/':
          appendStringInfoChar(lex->strval, *s);
          break;
        case 'b':
          appendStringInfoChar(lex->strval, '\b');
          break;
        case 'f':
          appendStringInfoChar(lex->strval, '\f');
          break;
        case 'n':
          appendStringInfoChar(lex->strval, '\n');
          break;
        case 'r':
          appendStringInfoChar(lex->strval, '\r');
          break;
        case 't':
          appendStringInfoChar(lex->strval, '\t');
          break;
        default:
                                                        
          lex->token_terminator = s + pg_mblen(s);
          ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Escape sequence \"\\%s\" is invalid.", extract_mb_char(s)), report_json_context(lex)));
        }
      }
      else if (strchr("\"\\/bfnrt", *s) == NULL)
      {
           
                                                                      
           
                                                                   
                                                                      
                                             
           
        lex->token_terminator = s + pg_mblen(s);
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Escape sequence \"\\%s\" is invalid.", extract_mb_char(s)), report_json_context(lex)));
      }
    }
    else if (lex->strval != NULL)
    {
      if (hi_surrogate != -1)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Unicode low surrogate must follow a high surrogate."), report_json_context(lex)));
      }

      appendStringInfoChar(lex->strval, *s);
    }
  }

  if (hi_surrogate != -1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Unicode low surrogate must follow a high surrogate."), report_json_context(lex)));
  }

                                               
  lex->prev_token_terminator = lex->token_terminator;
  lex->token_terminator = s + 1;
}

   
                                                                       
   
                                             
   
                                     
   
                                                                            
                        
   
                                                                          
                                                                 
                                                                     
                              
   
                                                                       
                                                                      
                                                                     
                                       
   
                                                                        
                                                                        
                                                   
   
                                                                           
                                                                               
                                                                              
   
static inline void
json_lex_number(JsonLexContext *lex, char *s, bool *num_err, int *total_len)
{
  bool error = false;
  int len = s - lex->input;

                                         
                                                      

                                          
  if (len < lex->input_length && *s == '0')
  {
    s++;
    len++;
  }
  else if (len < lex->input_length && *s >= '1' && *s <= '9')
  {
    do
    {
      s++;
      len++;
    } while (len < lex->input_length && *s >= '0' && *s <= '9');
  }
  else
  {
    error = true;
  }

                                                 
  if (len < lex->input_length && *s == '.')
  {
    s++;
    len++;
    if (len == lex->input_length || *s < '0' || *s > '9')
    {
      error = true;
    }
    else
    {
      do
      {
        s++;
        len++;
      } while (len < lex->input_length && *s >= '0' && *s <= '9');
    }
  }

                                          
  if (len < lex->input_length && (*s == 'e' || *s == 'E'))
  {
    s++;
    len++;
    if (len < lex->input_length && (*s == '+' || *s == '-'))
    {
      s++;
      len++;
    }
    if (len == lex->input_length || *s < '0' || *s > '9')
    {
      error = true;
    }
    else
    {
      do
      {
        s++;
        len++;
      } while (len < lex->input_length && *s >= '0' && *s <= '9');
    }
  }

     
                                                                           
                                                                     
               
     
  for (; len < lex->input_length && JSON_ALPHANUMERIC_CHAR(*s); s++, len++)
  {
    error = true;
  }

  if (total_len != NULL)
  {
    *total_len = len;
  }

  if (num_err != NULL)
  {
                                         
    *num_err = error;
  }
  else
  {
                               
    lex->prev_token_terminator = lex->token_terminator;
    lex->token_terminator = s;
                             
    if (error)
    {
      report_invalid_token(lex);
    }
  }
}

   
                         
   
                                                                               
   
static void
report_parse_error(JsonParseContext ctx, JsonLexContext *lex)
{
  char *token;
  int toklen;

                                                      
  if (lex->token_start == NULL || lex->token_type == JSON_TOKEN_END)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("The input string ended unexpectedly."), report_json_context(lex)));
  }

                                       
  toklen = lex->token_terminator - lex->token_start;
  token = palloc(toklen + 1);
  memcpy(token, lex->token_start, toklen);
  token[toklen] = '\0';

                                                      
  if (ctx == JSON_PARSE_END)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected end of input, but found \"%s\".", token), report_json_context(lex)));
  }
  else
  {
    switch (ctx)
    {
    case JSON_PARSE_VALUE:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected JSON value, but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_STRING:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected string, but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_ARRAY_START:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected array element or \"]\", but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_ARRAY_NEXT:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected \",\" or \"]\", but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_OBJECT_START:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected string or \"}\", but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_OBJECT_LABEL:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected \":\", but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_OBJECT_NEXT:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected \",\" or \"}\", but found \"%s\".", token), report_json_context(lex)));
      break;
    case JSON_PARSE_OBJECT_COMMA:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Expected string, but found \"%s\".", token), report_json_context(lex)));
      break;
    default:
      elog(ERROR, "unexpected json parse state: %d", ctx);
    }
  }
}

   
                                  
   
                                                                       
   
static void
report_invalid_token(JsonLexContext *lex)
{
  char *token;
  int toklen;

                                         
  toklen = lex->token_terminator - lex->token_start;
  token = palloc(toklen + 1);
  memcpy(token, lex->token_start, toklen);
  token[toklen] = '\0';

  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "json"), errdetail("Token \"%s\" is invalid.", token), report_json_context(lex)));
}

   
                                               
   
                                                                            
                                                                               
                 
   
                                                                           
                                    
   
static int
report_json_context(JsonLexContext *lex)
{
  const char *context_start;
  const char *context_end;
  const char *line_start;
  int line_number;
  char *ctxt;
  int ctxtlen;
  const char *prefix;
  const char *suffix;

                                                                   
  context_start = lex->input;
  context_end = lex->token_terminator;
  line_start = context_start;
  line_number = 1;
  for (;;)
  {
                                      
    if (context_start < context_end && *context_start == '\n')
    {
      context_start++;
      line_start = context_start;
      line_number++;
      continue;
    }
                                                                       
    if (context_end - context_start < 50)
    {
      break;
    }
                                             
    if (IS_HIGHBIT_SET(*context_start))
    {
      context_start += pg_mblen(context_start);
    }
    else
    {
      context_start++;
    }
  }

     
                                                                    
                                                                       
                                                                       
     
  if (context_start - line_start <= 3)
  {
    context_start = line_start;
  }

                                                         
  ctxtlen = context_end - context_start;
  ctxt = palloc(ctxtlen + 1);
  memcpy(ctxt, context_start, ctxtlen);
  ctxt[ctxtlen] = '\0';

     
                                                                             
                                                   
     
  prefix = (context_start > line_start) ? "..." : "";
  suffix = (lex->token_type != JSON_TOKEN_END && context_end - lex->input < lex->input_length && *context_end != '\n' && *context_end != '\r') ? "..." : "";

  return errcontext("JSON data, line %d: %s%s%s", line_number, prefix, ctxt, suffix);
}

   
                                                                     
   
static char *
extract_mb_char(char *s)
{
  char *res;
  int len;

  len = pg_mblen(s);
  res = palloc(len + 1);
  memcpy(res, s, len);
  res[len] = '\0';

  return res;
}

   
                                                                           
   
                                                                              
                                                                       
                                                           
   
static void
json_categorize_type(Oid typoid, JsonTypeCategory *tcategory, Oid *outfuncoid)
{
  bool typisvarlena;

                               
  typoid = getBaseType(typoid);

  *outfuncoid = InvalidOid;

     
                                                                       
                                                                           
                                         
     

  switch (typoid)
  {
  case BOOLOID:
    *tcategory = JSONTYPE_BOOL;
    break;

  case INT2OID:
  case INT4OID:
  case INT8OID:
  case FLOAT4OID:
  case FLOAT8OID:
  case NUMERICOID:
    getTypeOutputInfo(typoid, outfuncoid, &typisvarlena);
    *tcategory = JSONTYPE_NUMERIC;
    break;

  case DATEOID:
    *tcategory = JSONTYPE_DATE;
    break;

  case TIMESTAMPOID:
    *tcategory = JSONTYPE_TIMESTAMP;
    break;

  case TIMESTAMPTZOID:
    *tcategory = JSONTYPE_TIMESTAMPTZ;
    break;

  case JSONOID:
  case JSONBOID:
    getTypeOutputInfo(typoid, outfuncoid, &typisvarlena);
    *tcategory = JSONTYPE_JSON;
    break;

  default:
                                         
    if (OidIsValid(get_element_type(typoid)) || typoid == ANYARRAYOID || typoid == RECORDARRAYOID)
    {
      *tcategory = JSONTYPE_ARRAY;
    }
    else if (type_is_rowtype(typoid))                         
    {
      *tcategory = JSONTYPE_COMPOSITE;
    }
    else
    {
                                              
      *tcategory = JSONTYPE_OTHER;
                                                                   
      if (typoid >= FirstNormalObjectId)
      {
        Oid castfunc;
        CoercionPathType ctype;

        ctype = find_coercion_pathway(JSONOID, typoid, COERCION_EXPLICIT, &castfunc);
        if (ctype == COERCION_PATH_FUNC && OidIsValid(castfunc))
        {
          *tcategory = JSONTYPE_CAST;
          *outfuncoid = castfunc;
        }
        else
        {
                                             
          getTypeOutputInfo(typoid, outfuncoid, &typisvarlena);
        }
      }
      else
      {
                                    
        getTypeOutputInfo(typoid, outfuncoid, &typisvarlena);
      }
    }
    break;
  }
}

   
                                                                  
   
                                                                              
                                                            
   
                                                                         
                                                          
   
static void
datum_to_json(Datum val, bool is_null, StringInfo result, JsonTypeCategory tcategory, Oid outfuncoid, bool key_scalar)
{
  char *outputstr;
  text *jsontext;

  check_stack_depth();

                                                                       
  Assert(!(key_scalar && is_null));

  if (is_null)
  {
    appendStringInfoString(result, "null");
    return;
  }

  if (key_scalar && (tcategory == JSONTYPE_ARRAY || tcategory == JSONTYPE_COMPOSITE || tcategory == JSONTYPE_JSON || tcategory == JSONTYPE_CAST))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("key value must be scalar, not array, composite, or json")));
  }

  switch (tcategory)
  {
  case JSONTYPE_ARRAY:
    array_to_json_internal(val, result, false);
    break;
  case JSONTYPE_COMPOSITE:
    composite_to_json(val, result, false);
    break;
  case JSONTYPE_BOOL:
    outputstr = DatumGetBool(val) ? "true" : "false";
    if (key_scalar)
    {
      escape_json(result, outputstr);
    }
    else
    {
      appendStringInfoString(result, outputstr);
    }
    break;
  case JSONTYPE_NUMERIC:
    outputstr = OidOutputFunctionCall(outfuncoid, val);

       
                                                                 
               
       
    if (!key_scalar && IsValidJsonNumber(outputstr, strlen(outputstr)))
    {
      appendStringInfoString(result, outputstr);
    }
    else
    {
      escape_json(result, outputstr);
    }
    pfree(outputstr);
    break;
  case JSONTYPE_DATE:
  {
    char buf[MAXDATELEN + 1];

    JsonEncodeDateTime(buf, val, DATEOID);
    appendStringInfo(result, "\"%s\"", buf);
  }
  break;
  case JSONTYPE_TIMESTAMP:
  {
    char buf[MAXDATELEN + 1];

    JsonEncodeDateTime(buf, val, TIMESTAMPOID);
    appendStringInfo(result, "\"%s\"", buf);
  }
  break;
  case JSONTYPE_TIMESTAMPTZ:
  {
    char buf[MAXDATELEN + 1];

    JsonEncodeDateTime(buf, val, TIMESTAMPTZOID);
    appendStringInfo(result, "\"%s\"", buf);
  }
  break;
  case JSONTYPE_JSON:
                                                       
    outputstr = OidOutputFunctionCall(outfuncoid, val);
    appendStringInfoString(result, outputstr);
    pfree(outputstr);
    break;
  case JSONTYPE_CAST:
                                                                      
    jsontext = DatumGetTextPP(OidFunctionCall1(outfuncoid, val));
    outputstr = text_to_cstring(jsontext);
    appendStringInfoString(result, outputstr);
    pfree(outputstr);
    pfree(jsontext);
    break;
  default:
    outputstr = OidOutputFunctionCall(outfuncoid, val);
    escape_json(result, outputstr);
    pfree(outputstr);
    break;
  }
}

   
                                                                                
                                         
   
char *
JsonEncodeDateTime(char *buf, Datum value, Oid typid)
{
  if (!buf)
  {
    buf = palloc(MAXDATELEN + 1);
  }

  switch (typid)
  {
  case DATEOID:
  {
    DateADT date;
    struct pg_tm tm;

    date = DatumGetDateADT(value);

                                                   
    if (DATE_NOT_FINITE(date))
    {
      EncodeSpecialDate(date, buf);
    }
    else
    {
      j2date(date + POSTGRES_EPOCH_JDATE, &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday));
      EncodeDateOnly(&tm, USE_XSD_DATES, buf);
    }
  }
  break;
  case TIMEOID:
  {
    TimeADT time = DatumGetTimeADT(value);
    struct pg_tm tt, *tm = &tt;
    fsec_t fsec;

                                                   
    time2tm(time, tm, &fsec);
    EncodeTimeOnly(tm, fsec, false, 0, USE_XSD_DATES, buf);
  }
  break;
  case TIMETZOID:
  {
    TimeTzADT *time = DatumGetTimeTzADTP(value);
    struct pg_tm tt, *tm = &tt;
    fsec_t fsec;
    int tz;

                                                     
    timetz2tm(time, tm, &fsec, &tz);
    EncodeTimeOnly(tm, fsec, true, tz, USE_XSD_DATES, buf);
  }
  break;
  case TIMESTAMPOID:
  {
    Timestamp timestamp;
    struct pg_tm tm;
    fsec_t fsec;

    timestamp = DatumGetTimestamp(value);
                                                        
    if (TIMESTAMP_NOT_FINITE(timestamp))
    {
      EncodeSpecialTimestamp(timestamp, buf);
    }
    else if (timestamp2tm(timestamp, NULL, &tm, &fsec, NULL, NULL) == 0)
    {
      EncodeDateTime(&tm, fsec, false, 0, NULL, USE_XSD_DATES, buf);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }
  break;
  case TIMESTAMPTZOID:
  {
    TimestampTz timestamp;
    struct pg_tm tm;
    int tz;
    fsec_t fsec;
    const char *tzn = NULL;

    timestamp = DatumGetTimestampTz(value);
                                                          
    if (TIMESTAMP_NOT_FINITE(timestamp))
    {
      EncodeSpecialTimestamp(timestamp, buf);
    }
    else if (timestamp2tm(timestamp, &tz, &tm, &fsec, &tzn, NULL) == 0)
    {
      EncodeDateTime(&tm, fsec, true, tz, tzn, USE_XSD_DATES, buf);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }
  break;
  default:
    elog(ERROR, "unknown jsonb value datetime type oid %d", typid);
    return NULL;
  }

  return buf;
}

   
                                           
                                                                      
                                                        
   
static void
array_dim_to_json(StringInfo result, int dim, int ndims, int *dims, Datum *vals, bool *nulls, int *valcount, JsonTypeCategory tcategory, Oid outfuncoid, bool use_line_feeds)
{
  int i;
  const char *sep;

  Assert(dim < ndims);

  sep = use_line_feeds ? ",\n " : ",";

  appendStringInfoChar(result, '[');

  for (i = 1; i <= dims[dim]; i++)
  {
    if (i > 1)
    {
      appendStringInfoString(result, sep);
    }

    if (dim + 1 == ndims)
    {
      datum_to_json(vals[*valcount], nulls[*valcount], result, tcategory, outfuncoid, false);
      (*valcount)++;
    }
    else
    {
         
                                                                      
                       
         
      array_dim_to_json(result, dim + 1, ndims, dims, vals, nulls, valcount, tcategory, outfuncoid, false);
    }
  }

  appendStringInfoChar(result, ']');
}

   
                            
   
static void
array_to_json_internal(Datum array, StringInfo result, bool use_line_feeds)
{
  ArrayType *v = DatumGetArrayTypeP(array);
  Oid element_type = ARR_ELEMTYPE(v);
  int *dim;
  int ndim;
  int nitems;
  int count = 0;
  Datum *elements;
  bool *nulls;
  int16 typlen;
  bool typbyval;
  char typalign;
  JsonTypeCategory tcategory;
  Oid outfuncoid;

  ndim = ARR_NDIM(v);
  dim = ARR_DIMS(v);
  nitems = ArrayGetNItems(ndim, dim);

  if (nitems <= 0)
  {
    appendStringInfoString(result, "[]");
    return;
  }

  get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

  json_categorize_type(element_type, &tcategory, &outfuncoid);

  deconstruct_array(v, element_type, typlen, typbyval, typalign, &elements, &nulls, &nitems);

  array_dim_to_json(result, 0, ndim, dim, elements, nulls, &count, tcategory, outfuncoid, use_line_feeds);

  pfree(elements);
  pfree(nulls);
}

   
                                        
   
static void
composite_to_json(Datum composite, StringInfo result, bool use_line_feeds)
{
  HeapTupleHeader td;
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupdesc;
  HeapTupleData tmptup, *tuple;
  int i;
  bool needsep = false;
  const char *sep;

  sep = use_line_feeds ? ",\n " : ",";

  td = DatumGetHeapTupleHeader(composite);

                                               
  tupType = HeapTupleHeaderGetTypeId(td);
  tupTypmod = HeapTupleHeaderGetTypMod(td);
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

                                                     
  tmptup.t_len = HeapTupleHeaderGetDatumLength(td);
  tmptup.t_data = td;
  tuple = &tmptup;

  appendStringInfoChar(result, '{');

  for (i = 0; i < tupdesc->natts; i++)
  {
    Datum val;
    bool isnull;
    char *attname;
    JsonTypeCategory tcategory;
    Oid outfuncoid;
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

    if (att->attisdropped)
    {
      continue;
    }

    if (needsep)
    {
      appendStringInfoString(result, sep);
    }
    needsep = true;

    attname = NameStr(att->attname);
    escape_json(result, attname);
    appendStringInfoChar(result, ':');

    val = heap_getattr(tuple, i + 1, tupdesc, &isnull);

    if (isnull)
    {
      tcategory = JSONTYPE_NULL;
      outfuncoid = InvalidOid;
    }
    else
    {
      json_categorize_type(att->atttypid, &tcategory, &outfuncoid);
    }

    datum_to_json(val, isnull, result, tcategory, outfuncoid, false);
  }

  appendStringInfoChar(result, '}');
  ReleaseTupleDesc(tupdesc);
}

   
                                           
   
                                                                               
                                                                               
                      
   
static void
add_json(Datum val, bool is_null, StringInfo result, Oid val_type, bool key_scalar)
{
  JsonTypeCategory tcategory;
  Oid outfuncoid;

  if (val_type == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not determine input data type")));
  }

  if (is_null)
  {
    tcategory = JSONTYPE_NULL;
    outfuncoid = InvalidOid;
  }
  else
  {
    json_categorize_type(val_type, &tcategory, &outfuncoid);
  }

  datum_to_json(val, is_null, result, tcategory, outfuncoid, key_scalar);
}

   
                                   
   
Datum
array_to_json(PG_FUNCTION_ARGS)
{
  Datum array = PG_GETARG_DATUM(0);
  StringInfo result;

  result = makeStringInfo();

  array_to_json_internal(array, result, false);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                               
   
Datum
array_to_json_pretty(PG_FUNCTION_ARGS)
{
  Datum array = PG_GETARG_DATUM(0);
  bool use_line_feeds = PG_GETARG_BOOL(1);
  StringInfo result;

  result = makeStringInfo();

  array_to_json_internal(array, result, use_line_feeds);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                 
   
Datum
row_to_json(PG_FUNCTION_ARGS)
{
  Datum array = PG_GETARG_DATUM(0);
  StringInfo result;

  result = makeStringInfo();

  composite_to_json(array, result, false);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                             
   
Datum
row_to_json_pretty(PG_FUNCTION_ARGS)
{
  Datum array = PG_GETARG_DATUM(0);
  bool use_line_feeds = PG_GETARG_BOOL(1);
  StringInfo result;

  result = makeStringInfo();

  composite_to_json(array, result, use_line_feeds);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                  
   
Datum
to_json(PG_FUNCTION_ARGS)
{
  Datum val = PG_GETARG_DATUM(0);
  Oid val_type = get_fn_expr_argtype(fcinfo->flinfo, 0);
  StringInfo result;
  JsonTypeCategory tcategory;
  Oid outfuncoid;

  if (val_type == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not determine input data type")));
  }

  json_categorize_type(val_type, &tcategory, &outfuncoid);

  result = makeStringInfo();

  datum_to_json(val, false, result, tcategory, outfuncoid, false);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                
   
                                                 
   
Datum
json_agg_transfn(PG_FUNCTION_ARGS)
{
  MemoryContext aggcontext, oldcontext;
  JsonAggState *state;
  Datum val;

  if (!AggCheckCallContext(fcinfo, &aggcontext))
  {
                                                                     
    elog(ERROR, "json_agg_transfn called in non-aggregate context");
  }

  if (PG_ARGISNULL(0))
  {
    Oid arg_type = get_fn_expr_argtype(fcinfo->flinfo, 1);

    if (arg_type == InvalidOid)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not determine input data type")));
    }

       
                                                                         
                                                                      
                                                                        
                                                                 
       
    oldcontext = MemoryContextSwitchTo(aggcontext);
    state = (JsonAggState *)palloc(sizeof(JsonAggState));
    state->str = makeStringInfo();
    MemoryContextSwitchTo(oldcontext);

    appendStringInfoChar(state->str, '[');
    json_categorize_type(arg_type, &state->val_category, &state->val_output_func);
  }
  else
  {
    state = (JsonAggState *)PG_GETARG_POINTER(0);
    appendStringInfoString(state->str, ", ");
  }

                           
  if (PG_ARGISNULL(1))
  {
    datum_to_json((Datum)0, true, state->str, JSONTYPE_NULL, InvalidOid, false);
    PG_RETURN_POINTER(state);
  }

  val = PG_GETARG_DATUM(1);

                                                                 
  if (!PG_ARGISNULL(0) && (state->val_category == JSONTYPE_ARRAY || state->val_category == JSONTYPE_COMPOSITE))
  {
    appendStringInfoString(state->str, "\n ");
  }

  datum_to_json(val, false, state->str, state->val_category, state->val_output_func, false);

     
                                                                            
                                                                           
                                                                     
     
  PG_RETURN_POINTER(state);
}

   
                           
   
Datum
json_agg_finalfn(PG_FUNCTION_ARGS)
{
  JsonAggState *state;

                                                                   
  Assert(AggCheckCallContext(fcinfo, NULL));

  state = PG_ARGISNULL(0) ? NULL : (JsonAggState *)PG_GETARG_POINTER(0);

                                                                  
  if (state == NULL)
  {
    PG_RETURN_NULL();
  }

                                                                 
  PG_RETURN_TEXT_P(catenate_stringinfo_string(state->str, "]"));
}

   
                                        
   
                                                              
   
Datum
json_object_agg_transfn(PG_FUNCTION_ARGS)
{
  MemoryContext aggcontext, oldcontext;
  JsonAggState *state;
  Datum arg;

  if (!AggCheckCallContext(fcinfo, &aggcontext))
  {
                                                                     
    elog(ERROR, "json_object_agg_transfn called in non-aggregate context");
  }

  if (PG_ARGISNULL(0))
  {
    Oid arg_type;

       
                                                                      
                                                                        
                                                                        
                                                                 
       
    oldcontext = MemoryContextSwitchTo(aggcontext);
    state = (JsonAggState *)palloc(sizeof(JsonAggState));
    state->str = makeStringInfo();
    MemoryContextSwitchTo(oldcontext);

    arg_type = get_fn_expr_argtype(fcinfo->flinfo, 1);

    if (arg_type == InvalidOid)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not determine data type for argument %d", 1)));
    }

    json_categorize_type(arg_type, &state->key_category, &state->key_output_func);

    arg_type = get_fn_expr_argtype(fcinfo->flinfo, 2);

    if (arg_type == InvalidOid)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not determine data type for argument %d", 2)));
    }

    json_categorize_type(arg_type, &state->val_category, &state->val_output_func);

    appendStringInfoString(state->str, "{ ");
  }
  else
  {
    state = (JsonAggState *)PG_GETARG_POINTER(0);
    appendStringInfoString(state->str, ", ");
  }

     
                                                                         
                                                                           
                                                                         
                                                                  
                              
     

  if (PG_ARGISNULL(1))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("field name must not be null")));
  }

  arg = PG_GETARG_DATUM(1);

  datum_to_json(arg, false, state->str, state->key_category, state->key_output_func, true);

  appendStringInfoString(state->str, " : ");

  if (PG_ARGISNULL(2))
  {
    arg = (Datum)0;
  }
  else
  {
    arg = PG_GETARG_DATUM(2);
  }

  datum_to_json(arg, PG_ARGISNULL(2), state->str, state->val_category, state->val_output_func, false);

  PG_RETURN_POINTER(state);
}

   
                                   
   
Datum
json_object_agg_finalfn(PG_FUNCTION_ARGS)
{
  JsonAggState *state;

                                                                   
  Assert(AggCheckCallContext(fcinfo, NULL));

  state = PG_ARGISNULL(0) ? NULL : (JsonAggState *)PG_GETARG_POINTER(0);

                                                                  
  if (state == NULL)
  {
    PG_RETURN_NULL();
  }

                                                                  
  PG_RETURN_TEXT_P(catenate_stringinfo_string(state->str, " }"));
}

   
                                                                           
                                                                               
                                                                  
   
static text *
catenate_stringinfo_string(StringInfo buffer, const char *addon)
{
                                                  
  int buflen = buffer->len;
  int addlen = strlen(addon);
  text *result = (text *)palloc(buflen + addlen + VARHDRSZ);

  SET_VARSIZE(result, buflen + addlen + VARHDRSZ);
  memcpy(VARDATA(result), buffer->data, buflen);
  memcpy(VARDATA(result) + buflen, addon, addlen);

  return result;
}

   
                                                  
   
Datum
json_build_object(PG_FUNCTION_ARGS)
{
  int nargs = PG_NARGS();
  int i;
  const char *sep = "";
  StringInfo result;
  Datum *args;
  bool *nulls;
  Oid *types;

                                                 
  nargs = extract_variadic_args(fcinfo, 0, false, &args, &types, &nulls);

  if (nargs < 0)
  {
    PG_RETURN_NULL();
  }

  if (nargs % 2 != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument list must have even number of elements"),
                                                                  
                       errhint("The arguments of %s must consist of alternating keys and values.", "json_build_object()")));
  }

  result = makeStringInfo();

  appendStringInfoChar(result, '{');

  for (i = 0; i < nargs; i += 2)
  {
    appendStringInfoString(result, sep);
    sep = ", ";

                     
    if (nulls[i])
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument %d cannot be null", i + 1), errhint("Object keys should be text.")));
    }

    add_json(args[i], false, result, types[i], true);

    appendStringInfoString(result, " : ");

                       
    add_json(args[i + 1], nulls[i + 1], result, types[i + 1], false);
  }

  appendStringInfoChar(result, '}');

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                                                   
   
Datum
json_build_object_noargs(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(cstring_to_text_with_len("{}", 2));
}

   
                                                 
   
Datum
json_build_array(PG_FUNCTION_ARGS)
{
  int nargs;
  int i;
  const char *sep = "";
  StringInfo result;
  Datum *args;
  bool *nulls;
  Oid *types;

                                                
  nargs = extract_variadic_args(fcinfo, 0, false, &args, &types, &nulls);

  if (nargs < 0)
  {
    PG_RETURN_NULL();
  }

  result = makeStringInfo();

  appendStringInfoChar(result, '[');

  for (i = 0; i < nargs; i++)
  {
    appendStringInfoString(result, sep);
    sep = ", ";
    add_json(args[i], nulls[i], result, types[i], false);
  }

  appendStringInfoChar(result, ']');

  PG_RETURN_TEXT_P(cstring_to_text_with_len(result->data, result->len));
}

   
                                                                  
   
Datum
json_build_array_noargs(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(cstring_to_text_with_len("[]", 2));
}

   
                                    
   
                                                                  
                      
   
Datum
json_object(PG_FUNCTION_ARGS)
{
  ArrayType *in_array = PG_GETARG_ARRAYTYPE_P(0);
  int ndims = ARR_NDIM(in_array);
  StringInfoData result;
  Datum *in_datums;
  bool *in_nulls;
  int in_count, count, i;
  text *rval;
  char *v;

  switch (ndims)
  {
  case 0:
    PG_RETURN_DATUM(CStringGetTextDatum("{}"));
    break;

  case 1:
    if ((ARR_DIMS(in_array)[0]) % 2)
    {
      ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("array must have even number of elements")));
    }
    break;

  case 2:
    if ((ARR_DIMS(in_array)[1]) != 2)
    {
      ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("array must have two columns")));
    }
    break;

  default:
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("wrong number of array subscripts")));
  }

  deconstruct_array(in_array, TEXTOID, -1, false, 'i', &in_datums, &in_nulls, &in_count);

  count = in_count / 2;

  initStringInfo(&result);

  appendStringInfoChar(&result, '{');

  for (i = 0; i < count; ++i)
  {
    if (in_nulls[i * 2])
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("null value not allowed for object key")));
    }

    v = TextDatumGetCString(in_datums[i * 2]);
    if (i > 0)
    {
      appendStringInfoString(&result, ", ");
    }
    escape_json(&result, v);
    appendStringInfoString(&result, " : ");
    pfree(v);
    if (in_nulls[i * 2 + 1])
    {
      appendStringInfoString(&result, "null");
    }
    else
    {
      v = TextDatumGetCString(in_datums[i * 2 + 1]);
      escape_json(&result, v);
      pfree(v);
    }
  }

  appendStringInfoChar(&result, '}');

  pfree(in_datums);
  pfree(in_nulls);

  rval = cstring_to_text_with_len(result.data, result.len);
  pfree(result.data);

  PG_RETURN_TEXT_P(rval);
}

   
                                            
   
                                                                         
             
   
Datum
json_object_two_arg(PG_FUNCTION_ARGS)
{
  ArrayType *key_array = PG_GETARG_ARRAYTYPE_P(0);
  ArrayType *val_array = PG_GETARG_ARRAYTYPE_P(1);
  int nkdims = ARR_NDIM(key_array);
  int nvdims = ARR_NDIM(val_array);
  StringInfoData result;
  Datum *key_datums, *val_datums;
  bool *key_nulls, *val_nulls;
  int key_count, val_count, i;
  text *rval;
  char *v;

  if (nkdims > 1 || nkdims != nvdims)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("wrong number of array subscripts")));
  }

  if (nkdims == 0)
  {
    PG_RETURN_DATUM(CStringGetTextDatum("{}"));
  }

  deconstruct_array(key_array, TEXTOID, -1, false, 'i', &key_datums, &key_nulls, &key_count);

  deconstruct_array(val_array, TEXTOID, -1, false, 'i', &val_datums, &val_nulls, &val_count);

  if (key_count != val_count)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("mismatched array dimensions")));
  }

  initStringInfo(&result);

  appendStringInfoChar(&result, '{');

  for (i = 0; i < key_count; ++i)
  {
    if (key_nulls[i])
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("null value not allowed for object key")));
    }

    v = TextDatumGetCString(key_datums[i]);
    if (i > 0)
    {
      appendStringInfoString(&result, ", ");
    }
    escape_json(&result, v);
    appendStringInfoString(&result, " : ");
    pfree(v);
    if (val_nulls[i])
    {
      appendStringInfoString(&result, "null");
    }
    else
    {
      v = TextDatumGetCString(val_datums[i]);
      escape_json(&result, v);
      pfree(v);
    }
  }

  appendStringInfoChar(&result, '}');

  pfree(key_datums);
  pfree(key_nulls);
  pfree(val_datums);
  pfree(val_nulls);

  rval = cstring_to_text_with_len(result.data, result.len);
  pfree(result.data);

  PG_RETURN_TEXT_P(rval);
}

   
                                                                            
   
void
escape_json(StringInfo buf, const char *str)
{
  const char *p;

  appendStringInfoCharMacro(buf, '"');
  for (p = str; *p; p++)
  {
    switch (*p)
    {
    case '\b':
      appendStringInfoString(buf, "\\b");
      break;
    case '\f':
      appendStringInfoString(buf, "\\f");
      break;
    case '\n':
      appendStringInfoString(buf, "\\n");
      break;
    case '\r':
      appendStringInfoString(buf, "\\r");
      break;
    case '\t':
      appendStringInfoString(buf, "\\t");
      break;
    case '"':
      appendStringInfoString(buf, "\\\"");
      break;
    case '\\':
      appendStringInfoString(buf, "\\\\");
      break;
    default:
      if ((unsigned char)*p < ' ')
      {
        appendStringInfo(buf, "\\u%04x", (int)*p);
      }
      else
      {
        appendStringInfoCharMacro(buf, *p);
      }
      break;
    }
  }
  appendStringInfoCharMacro(buf, '"');
}

   
                                          
   
                                                                             
                                                                 
   
                                                                               
                                                                            
                                                                           
                                                                              
                                                          
   
Datum
json_typeof(PG_FUNCTION_ARGS)
{
  text *json;

  JsonLexContext *lex;
  JsonTokenType tok;
  char *type;

  json = PG_GETARG_TEXT_PP(0);
  lex = makeJsonLexContext(json, false);

                                                                
  json_lex(lex);
  tok = lex_peek(lex);
  switch (tok)
  {
  case JSON_TOKEN_OBJECT_START:
    type = "object";
    break;
  case JSON_TOKEN_ARRAY_START:
    type = "array";
    break;
  case JSON_TOKEN_STRING:
    type = "string";
    break;
  case JSON_TOKEN_NUMBER:
    type = "number";
    break;
  case JSON_TOKEN_TRUE:
  case JSON_TOKEN_FALSE:
    type = "boolean";
    break;
  case JSON_TOKEN_NULL:
    type = "null";
    break;
  default:
    elog(ERROR, "unexpected json token: %d", tok);
  }

  PG_RETURN_TEXT_P(cstring_to_text(type));
}
