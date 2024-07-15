#line 2 "psqlscan.c"
                                                                            
   
              
                                      
   
                                                                         
                                                                          
                                                                             
                                                                        
                       
   
                                                                         
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
   
                                                                             
   
                                                                             
   
                                                 
   
   
                                                                         
                                                                        
   
                  
                             
   
                                                                            
   
#include "postgres_fe.h"

#include "common/logging.h"
#include "fe_utils/psqlscan.h"

#include "libpq-fe.h"

#line 43 "psqlscan.c"

#define YY_INT_ALIGNED short int

                                         

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define psql_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer psql_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define psql_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer psql_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define psql_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer psql_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define psql_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string psql_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define psql_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes psql_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define psql_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer psql_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define psql_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer psql_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define psql_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state psql_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define psql_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer psql_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define psql_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state psql_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define psql_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state psql_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define psql_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack psql_yyensure_buffer_stack
#endif

#ifdef yylex
#define psql_yylex_ALREADY_DEFINED
#else
#define yylex psql_yylex
#endif

#ifdef yyrestart
#define psql_yyrestart_ALREADY_DEFINED
#else
#define yyrestart psql_yyrestart
#endif

#ifdef yylex_init
#define psql_yylex_init_ALREADY_DEFINED
#else
#define yylex_init psql_yylex_init
#endif

#ifdef yylex_init_extra
#define psql_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra psql_yylex_init_extra
#endif

#ifdef yylex_destroy
#define psql_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy psql_yylex_destroy
#endif

#ifdef yyget_debug
#define psql_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug psql_yyget_debug
#endif

#ifdef yyset_debug
#define psql_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug psql_yyset_debug
#endif

#ifdef yyget_extra
#define psql_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra psql_yyget_extra
#endif

#ifdef yyset_extra
#define psql_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra psql_yyset_extra
#endif

#ifdef yyget_in
#define psql_yyget_in_ALREADY_DEFINED
#else
#define yyget_in psql_yyget_in
#endif

#ifdef yyset_in
#define psql_yyset_in_ALREADY_DEFINED
#else
#define yyset_in psql_yyset_in
#endif

#ifdef yyget_out
#define psql_yyget_out_ALREADY_DEFINED
#else
#define yyget_out psql_yyget_out
#endif

#ifdef yyset_out
#define psql_yyset_out_ALREADY_DEFINED
#else
#define yyset_out psql_yyset_out
#endif

#ifdef yyget_leng
#define psql_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng psql_yyget_leng
#endif

#ifdef yyget_text
#define psql_yyget_text_ALREADY_DEFINED
#else
#define yyget_text psql_yyget_text
#endif

#ifdef yyget_lineno
#define psql_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno psql_yyget_lineno
#endif

#ifdef yyset_lineno
#define psql_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno psql_yyset_lineno
#endif

#ifdef yyget_column
#define psql_yyget_column_ALREADY_DEFINED
#else
#define yyget_column psql_yyget_column
#endif

#ifdef yyset_column
#define psql_yyset_column_ALREADY_DEFINED
#else
#define yyset_column psql_yyset_column
#endif

#ifdef yywrap
#define psql_yywrap_ALREADY_DEFINED
#else
#define yywrap psql_yywrap
#endif

#ifdef yyget_lval
#define psql_yyget_lval_ALREADY_DEFINED
#else
#define yyget_lval psql_yyget_lval
#endif

#ifdef yyset_lval
#define psql_yyset_lval_ALREADY_DEFINED
#else
#define yyset_lval psql_yyset_lval
#endif

#ifdef yyalloc
#define psql_yyalloc_ALREADY_DEFINED
#else
#define yyalloc psql_yyalloc
#endif

#ifdef yyrealloc
#define psql_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc psql_yyrealloc
#endif

#ifdef yyfree
#define psql_yyfree_ALREADY_DEFINED
#else
#define yyfree psql_yyfree
#endif

                                                                         

                               
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

                             

                                   

#ifndef FLEXINT_H
#define FLEXINT_H

                                                                    

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

                                                                     
                                                         
   
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <inttypes.h>
typedef int8_t flex_int8_t;
typedef uint8_t flex_uint8_t;
typedef int16_t flex_int16_t;
typedef uint16_t flex_uint16_t;
typedef int32_t flex_int32_t;
typedef uint32_t flex_uint32_t;
#else
typedef signed char flex_int8_t;
typedef short int flex_int16_t;
typedef int flex_int32_t;
typedef unsigned char flex_uint8_t;
typedef unsigned short int flex_uint16_t;
typedef unsigned int flex_uint32_t;

                               
#ifndef INT8_MIN
#define INT8_MIN (-128)
#endif
#ifndef INT16_MIN
#define INT16_MIN (-32767 - 1)
#endif
#ifndef INT32_MIN
#define INT32_MIN (-2147483647 - 1)
#endif
#ifndef INT8_MAX
#define INT8_MAX (127)
#endif
#ifndef INT16_MAX
#define INT16_MAX (32767)
#endif
#ifndef INT32_MAX
#define INT32_MAX (2147483647)
#endif
#ifndef UINT8_MAX
#define UINT8_MAX (255U)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX (65535U)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX (4294967295U)
#endif

#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

#endif            

#endif                  

                                 

                                                
#define yyconst const

#if defined(__GNUC__) && __GNUC__ >= 3
#define yynoreturn __attribute__((__noreturn__))
#else
#define yynoreturn
#endif

                                
#define YY_NULL 0

                                                            
                                                          
   
#define YY_SC_TO_UI(c) ((YY_CHAR)(c))

                        
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void *yyscan_t;
#endif

                                                               
                                          
#define yyin yyg->yyin_r
#define yyout yyg->yyout_r
#define yyextra yyg->yyextra_r
#define yyleng yyg->yyleng_r
#define yytext yyg->yytext_r
#define yylineno (YY_CURRENT_BUFFER_LVALUE->yy_bs_lineno)
#define yycolumn (YY_CURRENT_BUFFER_LVALUE->yy_bs_column)
#define yy_flex_debug yyg->yy_flex_debug_r

                                                                          
                                                                      
                        
   
#define BEGIN yyg->yy_start = 1 + 2 *
                                                                           
                                                                  
                  
   
#define YY_START ((yyg->yy_start - 1) / 2)
#define YYSTATE YY_START
                                                        
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
                                                           
#define YY_NEW_FILE yyrestart(yyin, yyscanner)
#define YY_END_OF_BUFFER_CHAR 0

                                   
#ifndef YY_BUF_SIZE
#ifdef __ia64__
                                             
                                                                    
                                            
   
#define YY_BUF_SIZE 32768
#else
#define YY_BUF_SIZE 16384
#endif               
#endif

                                                                             
                
   
#define YY_STATE_BUF_SIZE ((YY_BUF_SIZE + 2) * sizeof(yy_state_type))

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef size_t yy_size_t;
#endif

#define EOB_ACT_CONTINUE_SCAN 0
#define EOB_ACT_END_OF_FILE 1
#define EOB_ACT_LAST_MATCH 2

#define YY_LESS_LINENO(n)
#define YY_LINENO_REWIND_TO(ptr)

                                                                               
#define yyless(n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    int yyless_macro_arg = (n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    YY_LESS_LINENO(yyless_macro_arg);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    *yy_cp = yyg->yy_hold_char;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    YY_RESTORE_YY_MORE_OFFSET                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    yyg->yy_c_buf_p = yy_cp = yy_bp + yyless_macro_arg - YY_MORE_ADJ;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    YY_DO_BEFORE_ACTION;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)
#define unput(c) yyunput(c, yyg->yytext_ptr, yyscanner)

#ifndef YY_STRUCT_YY_BUFFER_STATE
#define YY_STRUCT_YY_BUFFER_STATE
struct yy_buffer_state
{
  FILE *yy_input_file;

  char *yy_ch_buf;                    
  char *yy_buf_pos;                                       

                                                               
                 
     
  int yy_buf_size;

                                                                 
                 
     
  int yy_n_chars;

                                                                
                                                              
                
     
  int yy_is_our_buffer;

                                                               
                                                                
                                                                   
                   
     
  int yy_is_interactive;

                                                                
                                                                  
          
     
  int yy_at_bol;

  int yy_bs_lineno;                        
  int yy_bs_column;                          

                                                               
                
     
  int yy_fill_buffer;

  int yy_buffer_status;

#define YY_BUFFER_NEW 0
#define YY_BUFFER_NORMAL 1
                                                                    
                                                                    
                                                                     
                                                               
                          
     
                                                                 
                                                                  
                                             
     
#define YY_BUFFER_EOF_PENDING 2
};
#endif                                 

                                                                
                                                             
                    
   
                                          
   
#define YY_CURRENT_BUFFER (yyg->yy_buffer_stack ? yyg->yy_buffer_stack[yyg->yy_buffer_stack_top] : NULL)
                                                                                
                                                          
   
#define YY_CURRENT_BUFFER_LVALUE yyg->yy_buffer_stack[yyg->yy_buffer_stack_top]

void
yyrestart(FILE *input_file, yyscan_t yyscanner);
void
yy_switch_to_buffer(YY_BUFFER_STATE new_buffer, yyscan_t yyscanner);
YY_BUFFER_STATE
yy_create_buffer(FILE *file, int size, yyscan_t yyscanner);
void
yy_delete_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner);
void
yy_flush_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner);
void
yypush_buffer_state(YY_BUFFER_STATE new_buffer, yyscan_t yyscanner);
void
yypop_buffer_state(yyscan_t yyscanner);

static void
yyensure_buffer_stack(yyscan_t yyscanner);
static void
yy_load_buffer_state(yyscan_t yyscanner);
static void
yy_init_buffer(YY_BUFFER_STATE b, FILE *file, yyscan_t yyscanner);
#define YY_FLUSH_BUFFER yy_flush_buffer(YY_CURRENT_BUFFER, yyscanner)

YY_BUFFER_STATE
yy_scan_buffer(char *base, yy_size_t size, yyscan_t yyscanner);
YY_BUFFER_STATE
yy_scan_string(const char *yy_str, yyscan_t yyscanner);
YY_BUFFER_STATE
yy_scan_bytes(const char *bytes, int len, yyscan_t yyscanner);

void *
yyalloc(yy_size_t, yyscan_t yyscanner);
void *
yyrealloc(void *, yy_size_t, yyscan_t yyscanner);
void
yyfree(void *, yyscan_t yyscanner);

#define yy_new_buffer yy_create_buffer
#define yy_set_interactive(is_interactive)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!YY_CURRENT_BUFFER)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      yyensure_buffer_stack(yyscanner);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE, yyscanner);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    YY_CURRENT_BUFFER_LVALUE->yy_is_interactive = is_interactive;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  }
#define yy_set_bol(at_bol)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!YY_CURRENT_BUFFER)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      yyensure_buffer_stack(yyscanner);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE, yyscanner);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    YY_CURRENT_BUFFER_LVALUE->yy_at_bol = at_bol;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  }
#define YY_AT_BOL() (YY_CURRENT_BUFFER_LVALUE->yy_at_bol)

                      

#define psql_yywrap(yyscanner) (              1)
#define YY_SKIP_YYWRAP
typedef flex_uint8_t YY_CHAR;

typedef int yy_state_type;

#define yytext_ptr yytext_r

static const flex_int16_t yy_nxt[][44] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

    {25, 26, 27, 28, 27, 29, 30, 31, 32, 33, 31, 34, 35, 36, 33, 33, 37, 38, 39, 40, 41, 41, 42, 43, 44, 45, 46, 31, 47, 48, 47, 47, 49, 47, 50, 47, 47, 51, 52, 53, 51, 52, 26, 26

    },

    {25, 26, 27, 28, 27, 29, 30, 31, 32, 33, 31, 34, 35, 36, 33, 33, 37, 38, 39, 40, 41, 41, 42, 43, 44, 45, 46, 31, 47, 48, 47, 47, 49, 47, 50, 47, 47, 51, 52, 53, 51, 52, 26, 26},

    {25, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 55, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54

    },

    {25, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 55, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54},

    {25, 56, 56, 56, 56, 57, 56, 57, 56, 57, 57, 56, 56, 56, 58, 57, 56, 57, 56, 59, 56, 56, 56, 56, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56

    },

    {25, 56, 56, 56, 56, 57, 56, 57, 56, 57, 57, 56, 56, 56, 58, 57, 56, 57, 56, 59, 56, 56, 56, 56, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56},

    {25, 60, 60, 60, 60, 60, 61, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60

    },

    {25, 60, 60, 60, 60, 60, 61, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60},

    {25, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 63, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62

    },

    {25, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 63, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62},

    {25, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 65, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64

    },

    {25, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 65, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},

    {25, 66, 66, 67, 66, 66, 66, 66, 66, 66, 66, 68, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 69, 66, 66, 66, 66

    },

    {25, 66, 66, 67, 66, 66, 66, 66, 66, 66, 66, 68, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 69, 66, 66, 66, 66},

    {25, 70, 70, 71, 70, 70, 70, 70, 72, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70

    },

    {25, 70, 70, 71, 70, 70, 70, 70, 72, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70},

    {25, 60, 60, 60, 60, 60, 73, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60

    },

    {25, 60, 60, 60, 60, 60, 73, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60},

    {25, 74, 75, 76, 75, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 77, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 78, 74, 74, 78, 74, 74, 74

    },

    {25, 74, 75, 76, 75, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 77, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 78, 74, 74, 78, 74, 74, 74},

    {25, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 79, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64

    },

    {25, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 79, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},

    {25, 80, 81, 82, 81, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 83, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 84, 80, 80, 84, 80, 80, 80

    },

    {25, 80, 81, 82, 81, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 83, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 84, 80, 80, 84, 80, 80, 80},

    {-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25

    },

    {25, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26},

    {25, -27, 85, 85, 85, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27

    },

    {25, -28, 85, 85, 85, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28},

    {25, -29, -29, -29, -29, 86, -29, 86, -29, 86, 86, -29, -29, -29, 86, 86, -29, 86, -29, 86, -29, -29, -29, -29, 86, 87, 86, 86, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29

    },

    {25, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30},

    {25, -31, -31, -31, -31, 86, -31, 86, -31, 86, 86, -31, -31, -31, 86, 86, -31, 86, -31, 86, -31, -31, -31, -31, 86, 86, 86, 86, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31

    },

    {25, -32, -32, -32, -32, -32, -32, -32, 88, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 89, 89, -32, -32, -32, -32, -32, -32, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, -32, 90, 90, -32, -32},

    {25, -33, -33, -33, -33, 86, -33, 86, -33, 86, 86, -33, -33, -33, 86, 86, -33, 86, -33, 86, -33, -33, -33, -33, 86, 86, 86, 86, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33, -33

    },

    {25, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34},

    {25, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35

    },

    {25, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36},

    {25, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37

    },

    {25, -38, -38, -38, -38, 86, -38, 86, -38, 86, 86, -38, -38, -38, 86, 86, -38, 91, -38, 86, -38, -38, -38, -38, 86, 86, 86, 86, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38},

    {25, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, 92, -39, 93, 93, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39, -39

    },

    {25, -40, -40, -40, -40, 86, -40, 86, -40, 86, 86, -40, -40, -40, 94, 86, -40, 86, -40, 86, -40, -40, -40, -40, 86, 86, 86, 86, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40},

    {25, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, 95, -41, 96, 96, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, 97, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41

    },

    {25, -42, -42, -42, -42, -42, 98, -42, -42, -42, -42, 99, -42, -42, -42, -42, -42, -42, -42, -42, 100, 100, 101, -42, -42, 102, -42, -42, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, -42, 100, 100, 103, -42},

    {25, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43

    },

    {25, -44, -44, -44, -44, 86, -44, 86, -44, 86, 86, -44, -44, -44, 86, 86, -44, 86, -44, 86, -44, -44, -44, -44, 86, 104, 105, 86, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44},

    {25, -45, -45, -45, -45, 86, -45, 86, -45, 86, 86, -45, -45, -45, 86, 86, -45, 86, -45, 86, -45, -45, -45, -45, 86, 86, 106, 86, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45

    },

    {25, -46, -46, -46, -46, 86, -46, 86, -46, 86, 86, -46, -46, -46, 86, 86, -46, 86, -46, 86, -46, -46, -46, -46, 86, 107, 86, 86, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46},

    {25, -47, -47, -47, -47, -47, -47, -47, 108, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, 108, 108, -47, -47, -47, -47, -47, -47, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -47, 108, 108, -47, -47

    },

    {25, -48, -48, -48, -48, -48, -48, -48, 108, -48, -48, 109, -48, -48, -48, -48, -48, -48, -48, -48, 108, 108, -48, -48, -48, -48, -48, -48, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -48, 108, 108, -48, -48},

    {25, -49, -49, -49, -49, -49, -49, -49, 108, -49, -49, 110, -49, -49, -49, -49, -49, -49, -49, -49, 108, 108, -49, -49, -49, -49, -49, -49, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -49, 108, 108, -49, -49

    },

    {25, -50, -50, -50, -50, -50, -50, -50, 108, -50, -50, 111, -50, -50, -50, -50, -50, -50, -50, -50, 108, 108, -50, -50, -50, -50, -50, -50, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -50, 108, 108, -50, -50},

    {25, -51, -51, -51, -51, -51, -51, -51, 108, -51, 112, -51, -51, -51, -51, -51, -51, -51, -51, -51, 108, 108, -51, -51, -51, -51, -51, -51, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -51, 108, 108, -51, -51

    },

    {25, -52, -52, -52, -52, -52, -52, -52, 108, -52, -52, 113, -52, -52, -52, -52, -52, -52, -52, -52, 108, 108, -52, -52, -52, -52, -52, -52, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -52, 108, 108, -52, -52},

    {25, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, 114, 114, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53

    },

    {25, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, -54, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115},

    {25, -55, 116, 117, 117, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, 118, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55

    },

    {25, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, -56, 119, 119, 119, 119, -56, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119},

    {25, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, -57, 119, 119, 119, 119, -57, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119

    },

    {25, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, 120, -58, -58, -58, -58, 121, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58},

    {25, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, 122, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59

    },

    {25, 123, 123, 123, 123, 123, -60, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123},

    {25, -61, -61, -61, -61, -61, 124, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61

    },

    {25, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, -62, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125},

    {25, -63, 126, 127, 127, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, 128, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63

    },

    {25, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, -64, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129},

    {25, -65, 130, 131, 131, -65, -65, -65, -65, -65, -65, 132, -65, -65, -65, -65, -65, 133, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65

    },

    {25, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, -66, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, -66, 134, 134, 134, 134},

    {25, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, -67, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, -67, 134, 134, 134, 134

    },

    {25, -68, 130, 131, 131, -68, -68, -68, -68, -68, -68, 132, -68, -68, -68, -68, -68, 133, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68},

    {25, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 136, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 137, 135, 135, 138, 139, 135, 135

    },

    {25, 140, 140, 140, 140, 140, 140, 140, -70, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140},

    {25, 140, 140, 140, 140, 140, 140, 140, -71, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140

    },

    {25, -72, -72, -72, -72, -72, -72, -72, 141, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, -72, 142, 142, -72, -72},

    {25, -73, -73, -73, -73, -73, 124, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73

    },

    {25, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74, -74},

    {25, -75, 143, 143, 143, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75

    },

    {25, -76, 143, 143, 143, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76},

    {25, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, 144, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77

    },

    {25, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, 145, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78},

    {25, -79, 146, 147, 147, -79, -79, -79, -79, -79, -79, 132, -79, -79, -79, -79, -79, 148, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79, -79

    },

    {25, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80},

    {25, -81, 149, 149, 149, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81, -81

    },

    {25, -82, 149, 149, 149, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82, -82},

    {25, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, 150, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83, -83

    },

    {25, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, 151, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84, -84},

    {25, -85, 85, 85, 85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85, -85

    },

    {25, -86, -86, -86, -86, 86, -86, 86, -86, 86, 86, -86, -86, -86, 86, 86, -86, 86, -86, 86, -86, -86, -86, -86, 86, 86, 86, 86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86},

    {25, -87, -87, -87, -87, 86, -87, 86, -87, 86, 86, -87, -87, -87, 86, 86, -87, 86, -87, 86, -87, -87, -87, -87, 86, 86, 86, 86, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87

    },

    {25, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88, -88},

    {25, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, 89, 89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89, -89

    },

    {25, -90, -90, -90, -90, -90, -90, -90, 88, -90, -90, -90, -90, -90, -90, -90, -90, -90, -90, -90, 152, 152, -90, -90, -90, -90, -90, -90, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, -90, 152, 152, -90, -90},

    {25, 153, 153, -91, -91, 154, 153, 154, 153, 154, 154, 153, 153, 153, 154, 154, 153, 154, 153, 154, 153, 153, 153, 153, 154, 154, 154, 154, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153

    },

    {25, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92, -92},

    {25, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, 93, 93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, 97, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93, -93

    },

    {25, -94, -94, -94, -94, 155, -94, 155, -94, 155, 155, -94, -94, -94, 155, 155, -94, 155, -94, 155, -94, -94, -94, -94, 155, 155, 155, 155, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94, -94},

    {25, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, 156, -95, 157, 157, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, 97, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95, -95

    },

    {25, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, 95, -96, 96, 96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, 97, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96, -96},

    {25, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, 158, -97, 158, -97, -97, 159, 159, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97, -97

    },

    {25, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, -98, 160, 160, -98, -98, -98, -98, -98, -98, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, -98, 160, 160, -98, -98},

    {25, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, -99, 161, 161, -99, -99, -99, -99, -99, -99, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, -99, 161, 161, -99, -99

    },

    {25, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, -100, 100, 100, -100, -100, -100, -100, -100, -100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, -100, 100, 100, -100, -100},

    {25, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101, -101

    },

    {25, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102, -102},

    {25, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, 162, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103, -103

    },

    {25, -104, -104, -104, -104, 86, -104, 86, -104, 86, 86, -104, -104, -104, 86, 86, -104, 86, -104, 86, -104, -104, -104, -104, 86, 86, 86, 86, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104, -104},

    {25, -105, -105, -105, -105, 86, -105, 86, -105, 86, 86, -105, -105, -105, 86, 86, -105, 86, -105, 86, -105, -105, -105, -105, 86, 86, 86, 86, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105, -105

    },

    {25, -106, -106, -106, -106, 86, -106, 86, -106, 86, 86, -106, -106, -106, 86, 86, -106, 86, -106, 86, -106, -106, -106, -106, 86, 86, 86, 86, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106, -106},

    {25, -107, -107, -107, -107, 86, -107, 86, -107, 86, 86, -107, -107, -107, 86, 86, -107, 86, -107, 86, -107, -107, -107, -107, 86, 86, 86, 86, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107, -107

    },

    {25, -108, -108, -108, -108, -108, -108, -108, 108, -108, -108, -108, -108, -108, -108, -108, -108, -108, -108, -108, 108, 108, -108, -108, -108, -108, -108, -108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, -108, 108, 108, -108, -108},

    {25, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109, -109

    },

    {25, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110, -110},

    {25, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111, -111

    },

    {25, -112, -112, -112, -112, -112, 163, -112, -112, -112, -112, 164, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112, -112},

    {25, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113, -113

    },

    {25, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114, -114},

    {25, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, -115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115

    },

    {25, -116, 116, 117, 117, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, 118, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116, -116},

    {25, -117, 165, 165, 165, -117, -117, -117, -117, -117, -117, 166, -117, -117, -117, -117, -117, 167, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117, -117

    },

    {25, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, 168, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118, -118},

    {25, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, -119, 119, 119, 119, 119, -119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119

    },

    {25, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, 120, -120, -120, -120, -120, 121, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120, -120},

    {25, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121, -121

    },

    {25, -122, -122, -122, -122, 169, -122, 169, -122, 169, 169, -122, -122, -122, 169, 169, -122, 169, -122, 169, -122, -122, -122, -122, 169, 169, 169, 169, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122, -122},

    {25, 123, 123, 123, 123, 123, -123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123

    },

    {25, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124},

    {25, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, -125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125

    },

    {25, -126, 126, 127, 127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, 128, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126},

    {25, -127, 170, 170, 170, -127, -127, -127, -127, -127, -127, 171, -127, -127, -127, -127, -127, 172, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127

    },

    {25, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 173, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128},

    {25, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, -129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129

    },

    {25, -130, 130, 131, 131, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, 133, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130, -130},

    {25, -131, 174, 174, 174, -131, -131, -131, -131, -131, -131, 175, -131, -131, -131, -131, -131, 176, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131, -131

    },

    {25, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132, -132},

    {25, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, 177, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133, -133

    },

    {25, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, -134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, -134, 134, 134, 134, 134},

    {25, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135, -135

    },

    {25, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, 178, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136, -136},

    {25, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, 179, 179, -137, -137, -137, -137, -137, -137, 179, 179, 179, 179, 179, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137, -137

    },

    {25, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, 180, 180, -138, -138, -138, -138, -138, -138, 180, 180, 180, 180, 180, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138, -138},

    {25, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, 181, 181, -139, -139, -139, -139, -139, -139, 181, 181, 181, 181, 181, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139, -139

    },

    {25, 140, 140, 140, 140, 140, 140, 140, -140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140},

    {25, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141, -141

    },

    {25, -142, -142, -142, -142, -142, -142, -142, 141, -142, -142, -142, -142, -142, -142, -142, -142, -142, -142, -142, 182, 182, -142, -142, -142, -142, -142, -142, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, -142, 182, 182, -142, -142},

    {25, -143, 143, 143, 143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143, -143

    },

    {25, 183, 183, -144, -144, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183},

    {25, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, -145, 184, -145, -145, -145, -145, -145, -145, -145

    },

    {25, -146, 146, 147, 147, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, 148, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146, -146},

    {25, -147, 185, 185, 185, -147, -147, -147, -147, -147, -147, 175, -147, -147, -147, -147, -147, 186, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147, -147

    },

    {25, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, 187, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148, -148},

    {25, -149, 149, 149, 149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149, -149

    },

    {25, 188, 188, -150, -150, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188},

    {25, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, -151, 189, -151, -151, -151, -151, -151, -151, -151

    },

    {25, -152, -152, -152, -152, -152, -152, -152, 88, -152, -152, -152, -152, -152, -152, -152, -152, -152, -152, -152, 152, 152, -152, -152, -152, -152, -152, -152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, -152, 152, 152, -152, -152},

    {25, 153, 153, -153, -153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153

    },

    {25, 153, 153, -154, -154, 154, 153, 154, 153, 154, 154, 153, 153, 153, 154, 154, 153, 154, 153, 154, 153, 153, 153, 153, 154, 154, 154, 154, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153},

    {25, -155, -155, -155, -155, 155, -155, 155, -155, 155, 155, -155, -155, -155, 155, 155, -155, 155, -155, 155, -155, -155, -155, -155, 155, 155, 155, 155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155, -155

    },

    {25, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156, -156},

    {25, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, 157, 157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, 97, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157, -157

    },

    {25, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, 159, 159, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158, -158},

    {25, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, 159, 159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159, -159

    },

    {25, -160, -160, -160, -160, -160, 190, -160, -160, -160, -160, -160, -160, -160, -160, -160, -160, -160, -160, -160, 160, 160, -160, -160, -160, -160, -160, -160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, -160, 160, 160, -160, -160},

    {25, -161, -161, -161, -161, -161, -161, -161, -161, -161, -161, 191, -161, -161, -161, -161, -161, -161, -161, -161, 161, 161, -161, -161, -161, -161, -161, -161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, -161, 161, 161, -161, -161

    },

    {25, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, -162, 192, 192, -162, -162, -162, -162, -162, -162, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, -162, 192, 192, -162, -162},

    {25, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163, -163

    },

    {25, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164, -164},

    {25, -165, 165, 165, 165, -165, -165, -165, -165, -165, -165, 166, -165, -165, -165, -165, -165, 167, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165, -165

    },

    {25, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166, -166},

    {25, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, 193, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167, -167

    },

    {25, 194, 195, 117, 117, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 196, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194},

    {25, -169, -169, -169, -169, 169, -169, 169, -169, 169, 169, -169, -169, -169, 169, 169, -169, 169, -169, 169, -169, -169, -169, -169, 169, 169, 169, 169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169, -169

    },

    {25, -170, 170, 170, 170, -170, -170, -170, -170, -170, -170, 171, -170, -170, -170, -170, -170, 172, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170, -170},

    {25, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171, -171

    },

    {25, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, 197, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172, -172},

    {25, 198, 199, 127, 127, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 200, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198

    },

    {25, -174, 174, 174, 174, -174, -174, -174, -174, -174, -174, 175, -174, -174, -174, -174, -174, 176, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174, -174},

    {25, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175, -175

    },

    {25, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, 201, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176, -176},

    {25, 202, 203, 131, 131, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 204, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202

    },

    {25, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, 205, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178, -178},

    {25, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, 206, 206, -179, -179, -179, -179, -179, -179, 206, 206, 206, 206, 206, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179, -179

    },

    {25, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, 207, 207, -180, -180, -180, -180, -180, -180, 207, 207, 207, 207, 207, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180, -180},

    {25, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, 208, 208, -181, -181, -181, -181, -181, -181, 208, 208, 208, 208, 208, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181, -181

    },

    {25, -182, -182, -182, -182, -182, -182, -182, 141, -182, -182, -182, -182, -182, -182, -182, -182, -182, -182, -182, 182, 182, -182, -182, -182, -182, -182, -182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, -182, 182, 182, -182, -182},

    {25, 183, 183, -183, -183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183

    },

    {25, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, 209, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184, -184},

    {25, -185, 185, 185, 185, -185, -185, -185, -185, -185, -185, 175, -185, -185, -185, -185, -185, 186, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185, -185

    },

    {25, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, 210, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186, -186},

    {25, 211, 212, 147, 147, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 213, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211

    },

    {25, 188, 188, -188, -188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188},

    {25, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, 214, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189, -189

    },

    {25, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190, -190},

    {25, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191, -191

    },

    {25, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, -192, 192, 192, -192, -192, -192, -192, -192, -192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, -192, 192, 192, -192, 215},

    {25, 216, 217, 218, 218, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 219, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216

    },

    {25, 194, 195, 117, 117, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 196, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194},

    {25, 194, 195, 117, 117, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 196, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194

    },

    {25, 194, 195, 117, 117, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 220, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194},

    {25, 221, 222, 223, 223, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 224, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221

    },

    {25, 198, 199, 127, 127, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 200, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198},

    {25, 198, 199, 127, 127, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 200, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198

    },

    {25, 198, 199, 127, 127, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 225, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198},

    {25, 226, 227, 228, 228, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 229, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226

    },

    {25, 202, 203, 131, 131, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 204, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202},

    {25, 202, 203, 131, 131, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 204, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202

    },

    {25, 202, 203, 131, 131, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 230, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202},

    {25, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205, -205

    },

    {25, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, 231, 231, -206, -206, -206, -206, -206, -206, 231, 231, 231, 231, 231, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206, -206},

    {25, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, 232, 232, -207, -207, -207, -207, -207, -207, 232, 232, 232, 232, 232, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207, -207

    },

    {25, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208, -208},

    {25, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, 233, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209, -209

    },

    {25, 234, 235, 236, 236, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 237, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234},

    {25, 211, 212, 147, 147, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 213, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211

    },

    {25, 211, 212, 147, 147, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 213, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211},

    {25, 211, 212, 147, 147, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 238, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211

    },

    {25, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, 239, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214, -214},

    {25, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215, -215

    },

    {25, 216, 217, 218, 218, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 219, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216},

    {25, 216, 217, 218, 218, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 219, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216

    },

    {25, -218, 165, 165, 165, -218, -218, -218, -218, -218, -218, 166, -218, -218, -218, -218, -218, 167, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218, -218},

    {25, 216, 217, 218, 218, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 240, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216

    },

    {25, 194, 195, 117, 117, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 220, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194},

    {25, 221, 222, 223, 223, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 224, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221

    },

    {25, 221, 222, 223, 223, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 224, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221},

    {25, -223, 170, 170, 170, -223, -223, -223, -223, -223, -223, 171, -223, -223, -223, -223, -223, 172, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223, -223

    },

    {25, 221, 222, 223, 223, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 241, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221},

    {25, 198, 199, 127, 127, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 225, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198

    },

    {25, 226, 227, 228, 228, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 229, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226},

    {25, 226, 227, 228, 228, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 229, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226

    },

    {25, -228, 174, 174, 174, -228, -228, -228, -228, -228, -228, 175, -228, -228, -228, -228, -228, 176, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228, -228},

    {25, 226, 227, 228, 228, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 242, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226

    },

    {25, 202, 203, 131, 131, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 230, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202},

    {25, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, 243, 243, -231, -231, -231, -231, -231, -231, 243, 243, 243, 243, 243, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231, -231

    },

    {25, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, 244, 244, -232, -232, -232, -232, -232, -232, 244, 244, 244, 244, 244, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232, -232},

    {25, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, -233, 245, -233, -233, -233, -233, -233, -233, -233, -233

    },

    {25, 234, 235, 236, 236, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 237, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234},

    {25, 234, 235, 236, 236, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 237, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234

    },

    {25, -236, 185, 185, 185, -236, -236, -236, -236, -236, -236, 175, -236, -236, -236, -236, -236, 186, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236, -236},

    {25, 234, 235, 236, 236, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 246, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234

    },

    {25, 211, 212, 147, 147, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 238, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211},

    {25, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, -239, 247, -239, -239, -239, -239, -239, -239, -239, -239

    },

    {25, 216, 217, 218, 218, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 240, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216},

    {25, 221, 222, 223, 223, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 241, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221

    },

    {25, 226, 227, 228, 228, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 242, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226},

    {25, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, 248, 248, -243, -243, -243, -243, -243, -243, 248, 248, 248, 248, 248, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243, -243

    },

    {25, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244, -244},

    {25, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, 249, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245, -245

    },

    {25, 234, 235, 236, 236, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 246, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234},

    {25, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, 250, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247, -247

    },

    {25, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, 251, 251, -248, -248, -248, -248, -248, -248, 251, 251, 251, 251, 251, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248, -248},

    {25, -249, 252, 252, 252, -249, -249, -249, -249, -249, -249, 253, -249, -249, -249, -249, -249, 254, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249, -249

    },

    {25, -250, 255, 255, 255, -250, -250, -250, -250, -250, -250, 256, -250, -250, -250, -250, -250, 257, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250, -250},

    {25, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, 258, 258, -251, -251, -251, -251, -251, -251, 258, 258, 258, 258, 258, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251, -251

    },

    {25, -252, 252, 252, 252, -252, -252, -252, -252, -252, -252, 253, -252, -252, -252, -252, -252, 254, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252, -252},

    {25, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, -253, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259

    },

    {25, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, 260, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254, -254},

    {25, -255, 255, 255, 255, -255, -255, -255, -255, -255, -255, 256, -255, -255, -255, -255, -255, 257, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255, -255

    },

    {25, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, -256, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261},

    {25, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, 262, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257, -257

    },

    {25, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, 244, 244, -258, -258, -258, -258, -258, -258, 244, 244, 244, 244, 244, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258, -258},

    {25, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, 263, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259, -259

    },

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 266, 264, 264, 264, 264, 264, 267, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264},

    {25, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, 268, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261, -261

    },

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 271, 269, 269, 269, 269, 269, 272, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269},

    {25, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263

    },

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 266, 264, 264, 264, 264, 264, 267, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264},

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 266, 264, 264, 264, 264, 264, 267, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264

    },

    {25, 273, 274, 275, 275, 273, 273, 273, 273, 273, 273, 266, 273, 273, 273, 273, 273, 276, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273},

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 266, 264, 264, 264, 264, 264, 277, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264

    },

    {25, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268, -268},

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 271, 269, 269, 269, 269, 269, 272, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269

    },

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 271, 269, 269, 269, 269, 269, 272, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269},

    {25, 278, 279, 280, 280, 278, 278, 278, 278, 278, 278, 271, 278, 278, 278, 278, 278, 281, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278

    },

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 271, 269, 269, 269, 269, 269, 282, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269},

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 283, 264, 264, 264, 264, 264, 267, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264

    },

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 283, 264, 264, 264, 264, 264, 267, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264},

    {25, -275, 252, 252, 252, -275, -275, -275, -275, -275, -275, 284, -275, -275, -275, -275, -275, 254, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275, -275

    },

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 283, 264, 264, 264, 264, 264, 277, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264},

    {25, 264, 265, 252, 252, 264, 264, 264, 264, 264, 264, 266, 264, 264, 264, 264, 264, 277, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264, 264

    },

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 285, 269, 269, 269, 269, 269, 272, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269},

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 285, 269, 269, 269, 269, 269, 272, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269

    },

    {25, -280, 255, 255, 255, -280, -280, -280, -280, -280, -280, 286, -280, -280, -280, -280, -280, 257, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280, -280},

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 285, 269, 269, 269, 269, 269, 282, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269

    },

    {25, 269, 270, 255, 255, 269, 269, 269, 269, 269, 269, 271, 269, 269, 269, 269, 269, 282, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269, 269},

    {25, 273, 274, 275, 275, 273, 273, 273, 273, 273, 273, 266, 273, 273, 273, 273, 273, 276, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273, 273

    },

    {25, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, -284, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259, 259},

    {25, 278, 279, 280, 280, 278, 278, 278, 278, 278, 278, 271, 278, 278, 278, 278, 278, 281, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278

    },

    {25, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, -286, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261, 261},

};

static yy_state_type
yy_get_previous_state(yyscan_t yyscanner);
static yy_state_type
yy_try_NUL_trans(yy_state_type current_state, yyscan_t yyscanner);
static int
yy_get_next_buffer(yyscan_t yyscanner);
static void yynoreturn
yy_fatal_error(const char *msg, yyscan_t yyscanner);

                                                                  
                                          
   
#define YY_DO_BEFORE_ACTION                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  yyg->yytext_ptr = yy_bp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  yyleng = (int)(yy_cp - yy_bp);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  yyg->yy_hold_char = *yy_cp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  *yy_cp = '\0';                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  yyg->yy_c_buf_p = yy_cp;
#define YY_NUM_RULES 89
#define YY_END_OF_BUFFER 90
                                            
                                    
struct yy_trans_info
{
  flex_int32_t yy_verify;
  flex_int32_t yy_nxt;
};
static const flex_int16_t yy_accept[287] = {0, 0, 0, 12, 12, 0, 0, 0, 0, 11, 11, 0, 0, 0, 0, 0, 0, 0, 0, 52, 52, 0, 0, 28, 28, 90, 88, 1, 1, 79, 46, 79, 88, 78, 19, 65, 66, 78, 78, 78, 78, 81, 78, 67, 78, 78, 78, 87, 87, 87, 87, 87, 87, 69, 12, 9, 5, 5, 6, 6, 55, 48, 11, 16, 31, 22, 32, 32, 22, 39, 43, 43, 45, 49, 51, 50, 50, 51, 51, 24, 27, 26, 26, 27, 27, 1, 79, 64, 40, 80, 41, 1, 58, 82, 2, 82, 81, 85, 75, 74, 70,

    57, 59, 77, 61, 63, 60, 62, 87, 8, 20, 18, 56, 15, 68, 12, 9, 9, 10, 5, 7, 4, 3, 55, 54, 11, 16, 16, 17, 31, 22, 22, 30, 23, 32, 35, 36, 34, 34, 35, 43, 42, 44, 50, 50, 52, 24, 24, 25, 26, 26, 28, 41, 1, 1, 2, 83, 82, 86, 84, 75, 74, 76, 47, 21, 9, 14, 10, 9, 3, 16, 13, 17, 16, 22, 38, 23, 22, 36, 34, 34, 37, 44, 50, 52, 24, 25, 24, 26, 28, 72, 71, 76, 9, 9, 9, 9, 16, 16, 16, 16,

    22, 22, 22, 22, 36, 34, 34, 37, 52, 24, 24, 24, 24, 28, 73, 9, 9, 9, 9, 9, 16, 16, 16, 16, 16, 22, 22, 22, 22, 22, 34, 34, 52, 24, 24, 24, 24, 24, 28, 9, 16, 22, 34, 33, 52, 24, 28, 34, 52, 28, 34, 52, 52, 52, 28, 28, 28, 34, 52, 52, 28, 28, 53, 52, 52, 52, 52, 29, 28, 28, 28, 28, 52, 52, 52, 52, 52, 28, 28, 28, 28, 28, 52, 52, 28, 28};

static const YY_CHAR yy_ec[256] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 2, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 22, 23, 24, 25, 26, 27, 7, 28, 29, 30, 31, 32, 31, 33, 33, 33, 33, 33, 33, 33, 34, 33, 35, 33, 33, 36, 33, 37, 33, 33, 38, 33, 33, 16, 39, 16, 9, 33, 7, 28, 29, 30, 31,

    32, 31, 33, 33, 33, 33, 33, 33, 33, 34, 33, 35, 33, 33, 36, 33, 40, 33, 33, 41, 33, 33, 42, 7, 43, 7, 1, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,

    33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33};

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
#line 1 "psqlscan.l"

#line 44 "psqlscan.l"

                     

#include "fe_utils/psqlscan_int.h"

   
                                                                             
                                                                           
   
typedef int YYSTYPE;

   
                                                                          
                  
   
#define YY_EXTRA_TYPE PsqlScanState

                                
#define LEXRES_EOL 0                         
#define LEXRES_SEMI 1                                               
#define LEXRES_BACKSLASH 2                              

#define ECHO psqlscan_emit(cur_state, yytext, yyleng)

   
                                                                         
                                                                            
                                                                        
                                          
   
extern int
psql_yyget_column(yyscan_t yyscanner);
extern void
psql_yyset_column(int column_no, yyscan_t yyscanner);

#line 3168 "psqlscan.c"
#define YY_NO_INPUT 1
   
                                                                   
                                                                        
                                                                      
                                                                             
                                                                             
                                                               
   
   
                                                               
                                                                       
                                                                               
                                                                            
                                                                             
                                                                      
   
                                                                  
                                                          
                     
                            
                                   
                                                           
                                    
                                 
                                                                      
                                 
                                                 
                                                                                 
                                             
                                                                             
   
                                                                         
                                                                         
                                                                     
                                         
   

   
                                                                          
                                                                        
                                                                            
                                                                     
                                                                       
   
                                                                          
                                                                             
                                                                         
                               
   
                                                                     
   
                                                                             
             
   
   
                                                                  
                                                                       
                                                                          
                                                                  
   
   
                                                                           
                                                                        
                                                                              
                                                                        
                                                                       
                                                                          
                            
   
              
                                                               
                                                                
                                                                
                                                                
                                                   
                                                                
                          
   
                        
                        
                                                 
                  
                                            
   
                                         
                                                                         
                                                                     
                                                               
                                                
   
                                                                          
                                    
   
                
                                                                         
   
                     
                                
                                            
                                        
                                                                                
                                
                    
   
                                                                              
                                                                          
                                                                              
                                                                            
                                                                             
                                                                           
                             
                                                                       
                                                                          
                                                                               
                                                                          
                                                                          
                                                                              
               
                                                                        
   
                                                              
   
                                                                                
                                                                             
                                                                              
                                                                            
                                                                           
                                                       
   
   
                                                                          
                                                                         
                                                                        
                                                                        
                                                                             
   
                                                                         
                        
   
                                              
                                                          
                                              
   
                                                                                 
   
                                                                         
                                                          
   
                                                         
   
                                                                              
                                                                                
                 
                                                                   
                                                                            
                           
                                                                            
                                          
                                                                      
                                                           
   
#line 3331 "psqlscan.c"

#define INITIAL 0
#define xb 1
#define xc 2
#define xd 3
#define xh 4
#define xq 5
#define xe 6
#define xdolq 7
#define xui 8
#define xuiend 9
#define xus 10
#define xusend 11

#ifndef YY_NO_UNISTD_H
                                                                        
                                                                              
                                                        
   
#include <unistd.h>
#endif

#ifndef YY_EXTRA_TYPE
#define YY_EXTRA_TYPE void *
#endif

                                                      
struct yyguts_t
{

                                          
  YY_EXTRA_TYPE yyextra_r;

                                                                                 
     
  FILE *yyin_r, *yyout_r;
  size_t yy_buffer_stack_top;                                     
  size_t yy_buffer_stack_max;                                 
  YY_BUFFER_STATE *yy_buffer_stack;                           
  char yy_hold_char;
  int yy_n_chars;
  int yyleng_r;
  char *yy_c_buf_p;
  int yy_init;
  int yy_start;
  int yy_did_buffer_switch_on_eof;
  int yy_start_stack_ptr;
  int yy_start_stack_depth;
  int *yy_start_stack;
  yy_state_type yy_last_accepting_state;
  char *yy_last_accepting_cpos;

  int yylineno_r;
  int yy_flex_debug_r;

  char *yytext_r;
  int yy_more_flag;
  int yy_more_len;

  YYSTYPE *yylval_r;

};                          

static int
yy_init_globals(yyscan_t yyscanner);

                                                              
                                    
#define yylval yyg->yylval_r

int
yylex_init(yyscan_t *scanner);

int
yylex_init_extra(YY_EXTRA_TYPE user_defined, yyscan_t *scanner);

                                
                                                                       

int
yylex_destroy(yyscan_t yyscanner);

int
yyget_debug(yyscan_t yyscanner);

void
yyset_debug(int debug_flag, yyscan_t yyscanner);

YY_EXTRA_TYPE
yyget_extra(yyscan_t yyscanner);

void
yyset_extra(YY_EXTRA_TYPE user_defined, yyscan_t yyscanner);

FILE *
yyget_in(yyscan_t yyscanner);

void
yyset_in(FILE *_in_str, yyscan_t yyscanner);

FILE *
yyget_out(yyscan_t yyscanner);

void
yyset_out(FILE *_out_str, yyscan_t yyscanner);

int
yyget_leng(yyscan_t yyscanner);

char *
yyget_text(yyscan_t yyscanner);

int
yyget_lineno(yyscan_t yyscanner);

void
yyset_lineno(int _line_number, yyscan_t yyscanner);

int
yyget_column(yyscan_t yyscanner);

void
yyset_column(int _column_no, yyscan_t yyscanner);

YYSTYPE *
yyget_lval(yyscan_t yyscanner);

void
yyset_lval(YYSTYPE *yylval_param, yyscan_t yyscanner);

                                                                        
              
   

#ifndef YY_SKIP_YYWRAP
#ifdef __cplusplus
extern "C" int
yywrap(yyscan_t yyscanner);
#else
extern int
yywrap(yyscan_t yyscanner);
#endif
#endif

#ifndef YY_NO_UNPUT

#endif

#ifndef yytext_ptr
static void
yy_flex_strncpy(char *, const char *, int, yyscan_t yyscanner);
#endif

#ifdef YY_NEED_STRLEN
static int
yy_flex_strlen(const char *, yyscan_t yyscanner);
#endif

#ifndef YY_NO_INPUT
#ifdef __cplusplus
static int
yyinput(yyscan_t yyscanner);
#else
static int
input(yyscan_t yyscanner);
#endif

#endif

                                                 
#ifndef YY_READ_BUF_SIZE
#ifdef __ia64__
                                              
#define YY_READ_BUF_SIZE 16384
#else
#define YY_READ_BUF_SIZE 8192
#endif               
#endif

                                                                 
#ifndef ECHO
                                                                         
                        
   
#define ECHO                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (fwrite(yytext, (size_t)yyleng, 1, yyout))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)
#endif

                                                                                
                            
   
#ifndef YY_INPUT
#define YY_INPUT(buf, result, max_size)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  if (YY_CURRENT_BUFFER_LVALUE->yy_is_interactive)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int c = '*';                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    int n;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    for (n = 0; n < max_size && (c = getc(yyin)) != EOF && c != '\n'; ++n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      buf[n] = (char)c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    if (c == '\n')                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
      buf[n++] = (char)c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    if (c == EOF && ferror(yyin))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      YY_FATAL_ERROR("input in flex scanner failed");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    result = n;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    errno = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    while ((result = (int)fread(buf, 1, (yy_size_t)max_size, yyin)) == 0 && ferror(yyin))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      if (errno != EINTR)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        YY_FATAL_ERROR("input in flex scanner failed");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      errno = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      clearerr(yyin);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }

#endif

                                                                            
                                                                         
                                                            
   
#ifndef yyterminate
#define yyterminate() return YY_NULL
#endif

                                                             
#ifndef YY_START_STACK_INCR
#define YY_START_STACK_INCR 25
#endif

                           
#ifndef YY_FATAL_ERROR
#define YY_FATAL_ERROR(msg) yy_fatal_error(msg, yyscanner)
#endif

                                                        

                                                                       
                          
   
#ifndef YY_DECL
#define YY_DECL_IS_OURS 1

extern int
yylex(YYSTYPE *yylval_param, yyscan_t yyscanner);

#define YY_DECL int yylex(YYSTYPE *yylval_param, yyscan_t yyscanner)
#endif               

                                                                        
                     
   
#ifndef YY_USER_ACTION
#define YY_USER_ACTION
#endif

                                            
#ifndef YY_BREAK
#define YY_BREAK            break;
#endif

#define YY_RULE_SETUP YY_USER_ACTION

                                                       
   
YY_DECL
{
  yy_state_type yy_current_state;
  char *yy_cp, *yy_bp;
  int yy_act;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  yylval = yylval_param;

  if (!yyg->yy_init)
  {
    yyg->yy_init = 1;

#ifdef YY_USER_INIT
    YY_USER_INIT;
#endif

    if (!yyg->yy_start)
    {
      yyg->yy_start = 1;                        
    }

    if (!yyin)
    {
      yyin = stdin;
    }

    if (!yyout)
    {
      yyout = stdout;
    }

    if (!YY_CURRENT_BUFFER)
    {
      yyensure_buffer_stack(yyscanner);
      YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE, yyscanner);
    }

    yy_load_buffer_state(yyscanner);
  }

  {
#line 370 "psqlscan.l"

#line 374 "psqlscan.l"
                                                                      
    PsqlScanState cur_state = yyextra;
    PQExpBuffer output_buf = cur_state->output_buf;

       
                                                                       
                                                                         
                                                                           
                                                                         
                                                                           
       
    BEGIN(cur_state->start_state);

#line 3633 "psqlscan.c"

    while (              1) /* loops until end-of-file is reached */
    {
      yy_cp = yyg->yy_c_buf_p;

                              
      *yy_cp = yyg->yy_hold_char;

                                                                   
                          
         
      yy_bp = yy_cp;

      yy_current_state = yyg->yy_start;
    yy_match:
      while ((yy_current_state = yy_nxt[yy_current_state][yy_ec[YY_SC_TO_UI(*yy_cp)]]) > 0)
      {
        ++yy_cp;
      }

      yy_current_state = -yy_current_state;

    yy_find_action:
      yy_act = yy_accept[yy_current_state];

      YY_DO_BEFORE_ACTION;

    do_action:                                                     

      switch (yy_act)
      {                                 
      case 1:
                                  
        YY_RULE_SETUP
#line 388 "psqlscan.l"
        {
             
                                                              
                                                               
                                                              
                                                                 
                                                           
                       
             
          if (!(output_buf->len == 0 || yytext[0] == '-'))
          {
            ECHO;
          }
        }
        YY_BREAK
      case 2:
        YY_RULE_SETUP
#line 401 "psqlscan.l"
        {
          cur_state->xcdepth = 0;
          BEGIN(xc);
                                                                  
          yyless(2);
          ECHO;
        }
        YY_BREAK

      case 3:
        YY_RULE_SETUP
#line 410 "psqlscan.l"
        {
          cur_state->xcdepth++;
                                                                  
          yyless(2);
          ECHO;
        }
        YY_BREAK
      case 4:
        YY_RULE_SETUP
#line 417 "psqlscan.l"
        {
          if (cur_state->xcdepth <= 0)
          {
            BEGIN(INITIAL);
          }
          else
          {
            cur_state->xcdepth--;
          }
          ECHO;
        }
        YY_BREAK
      case 5:
                                  
        YY_RULE_SETUP
#line 425 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 6:
        YY_RULE_SETUP
#line 429 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 7:
        YY_RULE_SETUP
#line 433 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
                
      case 8:
        YY_RULE_SETUP
#line 438 "psqlscan.l"
        {
          BEGIN(xb);
          ECHO;
        }
        YY_BREAK
      case 9:
                          
#line 443 "psqlscan.l"
      case 10:
                                   
        YY_RULE_SETUP
#line 443 "psqlscan.l"
        {
          yyless(1);
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 11:
                           
#line 449 "psqlscan.l"
      case 12:
                                   
        YY_RULE_SETUP
#line 449 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 13:
                           
#line 453 "psqlscan.l"
      case 14:
                                   
        YY_RULE_SETUP
#line 453 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 15:
        YY_RULE_SETUP
#line 457 "psqlscan.l"
        {
                                   
                                                            
                                                       
                                                                
                                                               
             
          BEGIN(xh);
          ECHO;
        }
        YY_BREAK
      case 16:
                           
#line 468 "psqlscan.l"
      case 17:
                                   
        YY_RULE_SETUP
#line 468 "psqlscan.l"
        {
          yyless(1);
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 18:
        YY_RULE_SETUP
#line 474 "psqlscan.l"
        {
          yyless(1);                             
          ECHO;
        }
        YY_BREAK
      case 19:
        YY_RULE_SETUP
#line 479 "psqlscan.l"
        {
          if (cur_state->std_strings)
          {
            BEGIN(xq);
          }
          else
          {
            BEGIN(xe);
          }
          ECHO;
        }
        YY_BREAK
      case 20:
        YY_RULE_SETUP
#line 486 "psqlscan.l"
        {
          BEGIN(xe);
          ECHO;
        }
        YY_BREAK
      case 21:
        YY_RULE_SETUP
#line 490 "psqlscan.l"
        {
          BEGIN(xus);
          ECHO;
        }
        YY_BREAK
      case 22:
                           
#line 495 "psqlscan.l"
      case 23:
                                   
        YY_RULE_SETUP
#line 495 "psqlscan.l"
        {
          yyless(1);
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 24:
                           
#line 501 "psqlscan.l"
      case 25:
                                   
        YY_RULE_SETUP
#line 501 "psqlscan.l"
        {
                                            
          yyless(1);
          BEGIN(xusend);
          ECHO;
        }
        YY_BREAK
      case 26:
                                   
        YY_RULE_SETUP
#line 507 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 27:
#line 511 "psqlscan.l"
      case 28:
                                   
        YY_RULE_SETUP
#line 511 "psqlscan.l"
        {
          yyless(0);
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 29:
                                   
        YY_RULE_SETUP
#line 516 "psqlscan.l"
        {
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 30:
        YY_RULE_SETUP
#line 520 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 31:
                                   
        YY_RULE_SETUP
#line 523 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 32:
                                   
        YY_RULE_SETUP
#line 526 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 33:
        YY_RULE_SETUP
#line 529 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 34:
        YY_RULE_SETUP
#line 532 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 35:
                                   
        YY_RULE_SETUP
#line 535 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 36:
        YY_RULE_SETUP
#line 538 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 37:
        YY_RULE_SETUP
#line 541 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 38:
                                   
        YY_RULE_SETUP
#line 544 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 39:
        YY_RULE_SETUP
#line 547 "psqlscan.l"
        {
                                                         
          ECHO;
        }
        YY_BREAK
      case 40:
        YY_RULE_SETUP
#line 552 "psqlscan.l"
        {
          cur_state->dolqstart = pg_strdup(yytext);
          BEGIN(xdolq);
          ECHO;
        }
        YY_BREAK
      case 41:
        YY_RULE_SETUP
#line 557 "psqlscan.l"
        {
                                                  
          yyless(1);
          ECHO;
        }
        YY_BREAK
      case 42:
        YY_RULE_SETUP
#line 562 "psqlscan.l"
        {
          if (strcmp(yytext, cur_state->dolqstart) == 0)
          {
            free(cur_state->dolqstart);
            cur_state->dolqstart = NULL;
            BEGIN(INITIAL);
          }
          else
          {
               
                                                                  
                                                                   
                                                                  
               
            yyless(yyleng - 1);
          }
          ECHO;
        }
        YY_BREAK
      case 43:
                                   
        YY_RULE_SETUP
#line 580 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 44:
        YY_RULE_SETUP
#line 583 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 45:
        YY_RULE_SETUP
#line 586 "psqlscan.l"
        {
                                                                
          ECHO;
        }
        YY_BREAK
      case 46:
        YY_RULE_SETUP
#line 591 "psqlscan.l"
        {
          BEGIN(xd);
          ECHO;
        }
        YY_BREAK
      case 47:
        YY_RULE_SETUP
#line 595 "psqlscan.l"
        {
          BEGIN(xui);
          ECHO;
        }
        YY_BREAK
      case 48:
        YY_RULE_SETUP
#line 599 "psqlscan.l"
        {
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 49:
        YY_RULE_SETUP
#line 603 "psqlscan.l"
        {
          yyless(1);
          BEGIN(xuiend);
          ECHO;
        }
        YY_BREAK
      case 50:
                                   
        YY_RULE_SETUP
#line 608 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 51:
#line 612 "psqlscan.l"
      case 52:
                                   
        YY_RULE_SETUP
#line 612 "psqlscan.l"
        {
          yyless(0);
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 53:
                                   
        YY_RULE_SETUP
#line 617 "psqlscan.l"
        {
          BEGIN(INITIAL);
          ECHO;
        }
        YY_BREAK
      case 54:
        YY_RULE_SETUP
#line 621 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 55:
                                   
        YY_RULE_SETUP
#line 624 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 56:
        YY_RULE_SETUP
#line 628 "psqlscan.l"
        {
                                                  
          yyless(1);
          ECHO;
        }
        YY_BREAK
      case 57:
        YY_RULE_SETUP
#line 634 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 58:
        YY_RULE_SETUP
#line 638 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 59:
        YY_RULE_SETUP
#line 642 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 60:
        YY_RULE_SETUP
#line 646 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 61:
        YY_RULE_SETUP
#line 650 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 62:
        YY_RULE_SETUP
#line 654 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 63:
        YY_RULE_SETUP
#line 658 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 64:
        YY_RULE_SETUP
#line 662 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
         
                                                                         
                                                                         
                                                                             
         
      case 65:
        YY_RULE_SETUP
#line 672 "psqlscan.l"
        {
          cur_state->paren_depth++;
          ECHO;
        }
        YY_BREAK
      case 66:
        YY_RULE_SETUP
#line 677 "psqlscan.l"
        {
          if (cur_state->paren_depth > 0)
          {
            cur_state->paren_depth--;
          }
          ECHO;
        }
        YY_BREAK
      case 67:
        YY_RULE_SETUP
#line 683 "psqlscan.l"
        {
          ECHO;
          if (cur_state->paren_depth == 0)
          {
                                              
            cur_state->start_state = YY_START;
            return LEXRES_SEMI;
          }
        }
        YY_BREAK
         
                                                                       
                                                           
         
      case 68:
        YY_RULE_SETUP
#line 698 "psqlscan.l"
        {
                                                                 
          psqlscan_emit(cur_state, yytext + 1, 1);
        }
        YY_BREAK
      case 69:
        YY_RULE_SETUP
#line 703 "psqlscan.l"
        {
                                            
          cur_state->start_state = YY_START;
          return LEXRES_BACKSLASH;
        }
        YY_BREAK
      case 70:
        YY_RULE_SETUP
#line 709 "psqlscan.l"
        {
                                                   
          char *varname;
          char *value;

          varname = psqlscan_extract_substring(cur_state, yytext + 1, yyleng - 1);
          if (cur_state->callbacks->get_variable)
          {
            value = cur_state->callbacks->get_variable(varname, PQUOTE_PLAIN, cur_state->cb_passthrough);
          }
          else
          {
            value = NULL;
          }

          if (value)
          {
                                                       
            if (psqlscan_var_is_current_source(cur_state, varname))
            {
                                                          
              pg_log_warning("skipping recursive expansion of variable \"%s\"", varname);
                                                 
              ECHO;
            }
            else
            {
                                            
              psqlscan_push_new_buffer(cur_state, value, varname);
                                                             
            }
            free(value);
          }
          else
          {
               
                                                                   
                     
               
            ECHO;
          }

          free(varname);
        }
        YY_BREAK
      case 71:
        YY_RULE_SETUP
#line 755 "psqlscan.l"
        {
          psqlscan_escape_variable(cur_state, yytext, yyleng, PQUOTE_SQL_LITERAL);
        }
        YY_BREAK
      case 72:
        YY_RULE_SETUP
#line 760 "psqlscan.l"
        {
          psqlscan_escape_variable(cur_state, yytext, yyleng, PQUOTE_SQL_IDENT);
        }
        YY_BREAK
      case 73:
        YY_RULE_SETUP
#line 765 "psqlscan.l"
        {
          psqlscan_test_variable(cur_state, yytext, yyleng);
        }
        YY_BREAK
         
                                                                          
                                                      
         
      case 74:
        YY_RULE_SETUP
#line 774 "psqlscan.l"
        {
                                                   
          yyless(1);
          ECHO;
        }
        YY_BREAK
      case 75:
        YY_RULE_SETUP
#line 780 "psqlscan.l"
        {
                                                   
          yyless(1);
          ECHO;
        }
        YY_BREAK
      case 76:
        YY_RULE_SETUP
#line 786 "psqlscan.l"
        {
                                                   
          yyless(1);
          ECHO;
        }
        YY_BREAK
      case 77:
        YY_RULE_SETUP
#line 791 "psqlscan.l"
        {
                                                   
          yyless(1);
          ECHO;
        }
        YY_BREAK
         
                                           
         
      case 78:
        YY_RULE_SETUP
#line 801 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 79:
        YY_RULE_SETUP
#line 805 "psqlscan.l"
        {
             
                                                               
                                                              
                                                            
                                                              
             
          int nchars = yyleng;
          char *slashstar = strstr(yytext, "/*");
          char *dashdash = strstr(yytext, "--");

          if (slashstar && dashdash)
          {
                                                    
            if (slashstar > dashdash)
            {
              slashstar = dashdash;
            }
          }
          else if (!slashstar)
          {
            slashstar = dashdash;
          }
          if (slashstar)
          {
            nchars = slashstar - yytext;
          }

             
                                                              
                                                                    
                                                           
                                                               
                                                                  
                                         
             
          if (nchars > 1 && (yytext[nchars - 1] == '+' || yytext[nchars - 1] == '-'))
          {
            int ic;

            for (ic = nchars - 2; ic >= 0; ic--)
            {
              char c = yytext[ic];
              if (c == '~' || c == '!' || c == '@' || c == '#' || c == '^' || c == '&' || c == '|' || c == '`' || c == '?' || c == '%')
              {
                break;
              }
            }
            if (ic < 0)
            {
                 
                                                               
                                   
                 
              do
              {
                nchars--;
              } while (nchars > 1 && (yytext[nchars - 1] == '+' || yytext[nchars - 1] == '-'));
            }
          }

          if (nchars < yyleng)
          {
                                                         
            yyless(nchars);
          }
          ECHO;
        }
        YY_BREAK
      case 80:
        YY_RULE_SETUP
#line 872 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 81:
        YY_RULE_SETUP
#line 876 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 82:
        YY_RULE_SETUP
#line 879 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 83:
        YY_RULE_SETUP
#line 882 "psqlscan.l"
        {
                                                       
          yyless(yyleng - 2);
          ECHO;
        }
        YY_BREAK
      case 84:
        YY_RULE_SETUP
#line 887 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 85:
        YY_RULE_SETUP
#line 890 "psqlscan.l"
        {
             
                                                              
                                                   
                                                  
             
          yyless(yyleng - 1);
          ECHO;
        }
        YY_BREAK
      case 86:
        YY_RULE_SETUP
#line 899 "psqlscan.l"
        {
                                                             
          yyless(yyleng - 2);
          ECHO;
        }
        YY_BREAK
      case 87:
        YY_RULE_SETUP
#line 906 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case 88:
        YY_RULE_SETUP
#line 910 "psqlscan.l"
        {
          ECHO;
        }
        YY_BREAK
      case YY_STATE_EOF(INITIAL):
      case YY_STATE_EOF(xb):
      case YY_STATE_EOF(xc):
      case YY_STATE_EOF(xd):
      case YY_STATE_EOF(xh):
      case YY_STATE_EOF(xq):
      case YY_STATE_EOF(xe):
      case YY_STATE_EOF(xdolq):
      case YY_STATE_EOF(xui):
      case YY_STATE_EOF(xuiend):
      case YY_STATE_EOF(xus):
      case YY_STATE_EOF(xusend):
#line 914 "psqlscan.l"
      {
        if (cur_state->buffer_stack == NULL)
        {
          cur_state->start_state = YY_START;
          return LEXRES_EOL;                           
        }

           
                                                              
                                 
           
        psqlscan_pop_buffer_stack(cur_state);
        psqlscan_select_top_buffer(cur_state);
      }
        YY_BREAK
      case 89:
        YY_RULE_SETUP
#line 929 "psqlscan.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 4531 "psqlscan.c"

      case YY_END_OF_BUFFER:
      {
                                                                
        int yy_amount_of_matched_text = (int)(yy_cp - yyg->yytext_ptr) - 1;

                                                      
        *yy_cp = yyg->yy_hold_char;
        YY_RESTORE_YY_MORE_OFFSET

        if (YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_NEW)
        {
                                                              
                                                          
                                                          
                                                     
                                                           
                                                                 
                                                             
                                                                
             
          yyg->yy_n_chars = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
          YY_CURRENT_BUFFER_LVALUE->yy_input_file = yyin;
          YY_CURRENT_BUFFER_LVALUE->yy_buffer_status = YY_BUFFER_NORMAL;
        }

                                                                      
                                                                 
                                                                
                                                            
                                                              
                       
           
        if (yyg->yy_c_buf_p <= &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[yyg->yy_n_chars])
        {                             
          yy_state_type yy_next_state;

          yyg->yy_c_buf_p = yyg->yytext_ptr + yy_amount_of_matched_text;

          yy_current_state = yy_get_previous_state(yyscanner);

                                                        
                                           
                                                        
                                                        
                                                           
                                                           
                                    
             

          yy_next_state = yy_try_NUL_trans(yy_current_state, yyscanner);

          yy_bp = yyg->yytext_ptr + YY_MORE_ADJ;

          if (yy_next_state)
          {
                                  
            yy_cp = ++yyg->yy_c_buf_p;
            yy_current_state = yy_next_state;
            goto yy_match;
          }

          else
          {
            yy_cp = yyg->yy_c_buf_p;
            goto yy_find_action;
          }
        }

        else
        {
          switch (yy_get_next_buffer(yyscanner))
          {
          case EOB_ACT_END_OF_FILE:
          {
            yyg->yy_did_buffer_switch_on_eof = 0;

            if (yywrap(yyscanner))
            {
                                                   
                                                     
                                           
                                                  
                                                   
                                                      
                                                     
                                            
                 
              yyg->yy_c_buf_p = yyg->yytext_ptr + YY_MORE_ADJ;

              yy_act = YY_STATE_EOF(YY_START);
              goto do_action;
            }

            else
            {
              if (!yyg->yy_did_buffer_switch_on_eof)
              {
                YY_NEW_FILE;
              }
            }
            break;
          }

          case EOB_ACT_CONTINUE_SCAN:
            yyg->yy_c_buf_p = yyg->yytext_ptr + yy_amount_of_matched_text;

            yy_current_state = yy_get_previous_state(yyscanner);

            yy_cp = yyg->yy_c_buf_p;
            yy_bp = yyg->yytext_ptr + YY_MORE_ADJ;
            goto yy_match;

          case EOB_ACT_LAST_MATCH:
            yyg->yy_c_buf_p = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[yyg->yy_n_chars];

            yy_current_state = yy_get_previous_state(yyscanner);

            yy_cp = yyg->yy_c_buf_p;
            yy_bp = yyg->yytext_ptr + YY_MORE_ADJ;
            goto yy_find_action;
          }
        }
        break;
      }

      default:;
        YY_FATAL_ERROR("fatal flex scanner internal error--no action found");
        break;
      }                           
    }                                
  }                                 
}                   

                                                    
   
                                          
                        
                                                                   
                                     
   
static int
yy_get_next_buffer(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  char *dest = YY_CURRENT_BUFFER_LVALUE->yy_ch_buf;
  char *source = yyg->yytext_ptr;
  int number_to_move, i;
  int ret_val;

  if (yyg->yy_c_buf_p > &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[yyg->yy_n_chars + 1])
  {
    YY_FATAL_ERROR("fatal flex scanner internal error--end of buffer missed");
  }

  if (YY_CURRENT_BUFFER_LVALUE->yy_fill_buffer == 0)
  {                                                       
    if (yyg->yy_c_buf_p - yyg->yytext_ptr - YY_MORE_ADJ == 1)
    {
                                                    
                                    
         
      return EOB_ACT_END_OF_FILE;
    }

    else
    {
                                                      
                     
         
      return EOB_ACT_LAST_MATCH;
    }
  }

                              

                                                 
  number_to_move = (int)(yyg->yy_c_buf_p - yyg->yytext_ptr - 1);

  for (i = 0; i < number_to_move; ++i)
  {
    *(dest++) = *(source++);
  }

  if (YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_EOF_PENDING)
  {
                                                                
                         
       
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = yyg->yy_n_chars = 0;
  }

  else
  {
    int num_to_read = YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;

    while (num_to_read <= 0)
    {                                               

                                                      
      YY_BUFFER_STATE b = YY_CURRENT_BUFFER_LVALUE;

      int yy_c_buf_p_offset = (int)(yyg->yy_c_buf_p - b->yy_ch_buf);

      if (b->yy_is_our_buffer)
      {
        int new_size = b->yy_buf_size * 2;

        if (new_size <= 0)
        {
          b->yy_buf_size += b->yy_buf_size / 8;
        }
        else
        {
          b->yy_buf_size *= 2;
        }

        b->yy_ch_buf = (char *)
                                                  
            yyrealloc((void *)b->yy_ch_buf, (yy_size_t)(b->yy_buf_size + 2), yyscanner);
      }
      else
      {
                                             
        b->yy_ch_buf = NULL;
      }

      if (!b->yy_ch_buf)
      {
        YY_FATAL_ERROR("fatal error - scanner input buffer overflow");
      }

      yyg->yy_c_buf_p = &b->yy_ch_buf[yy_c_buf_p_offset];

      num_to_read = YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;
    }

    if (num_to_read > YY_READ_BUF_SIZE)
    {
      num_to_read = YY_READ_BUF_SIZE;
    }

                            
    YY_INPUT((&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move]), yyg->yy_n_chars, num_to_read);

    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = yyg->yy_n_chars;
  }

  if (yyg->yy_n_chars == 0)
  {
    if (number_to_move == YY_MORE_ADJ)
    {
      ret_val = EOB_ACT_END_OF_FILE;
      yyrestart(yyin, yyscanner);
    }

    else
    {
      ret_val = EOB_ACT_LAST_MATCH;
      YY_CURRENT_BUFFER_LVALUE->yy_buffer_status = YY_BUFFER_EOF_PENDING;
    }
  }

  else
  {
    ret_val = EOB_ACT_CONTINUE_SCAN;
  }

  if ((yyg->yy_n_chars + number_to_move) > YY_CURRENT_BUFFER_LVALUE->yy_buf_size)
  {
                                                                  
    int new_size = yyg->yy_n_chars + number_to_move + (yyg->yy_n_chars >> 1);
    YY_CURRENT_BUFFER_LVALUE->yy_ch_buf = (char *)yyrealloc((void *)YY_CURRENT_BUFFER_LVALUE->yy_ch_buf, (yy_size_t)new_size, yyscanner);
    if (!YY_CURRENT_BUFFER_LVALUE->yy_ch_buf)
    {
      YY_FATAL_ERROR("out of dynamic memory in yy_get_next_buffer()");
    }
                                     
    YY_CURRENT_BUFFER_LVALUE->yy_buf_size = (int)(new_size - 2);
  }

  yyg->yy_n_chars += number_to_move;
  YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[yyg->yy_n_chars] = YY_END_OF_BUFFER_CHAR;
  YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[yyg->yy_n_chars + 1] = YY_END_OF_BUFFER_CHAR;

  yyg->yytext_ptr = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[0];

  return ret_val;
}

                                                                                

static yy_state_type
yy_get_previous_state(yyscan_t yyscanner)
{
  yy_state_type yy_current_state;
  char *yy_cp;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  yy_current_state = yyg->yy_start;

  for (yy_cp = yyg->yytext_ptr + YY_MORE_ADJ; yy_cp < yyg->yy_c_buf_p; ++yy_cp)
  {
    yy_current_state = yy_nxt[yy_current_state][(*yy_cp ? yy_ec[YY_SC_TO_UI(*yy_cp)] : 1)];
  }

  return yy_current_state;
}

                                                                    
   
            
                                                   
   
static yy_state_type
yy_try_NUL_trans(yy_state_type yy_current_state, yyscan_t yyscanner)
{
  int yy_is_jam;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;                                                     

  yy_current_state = yy_nxt[yy_current_state][1];
  yy_is_jam = (yy_current_state <= 0);

  (void)yyg;
  return yy_is_jam ? 0 : yy_current_state;
}

#ifndef YY_NO_UNPUT

#endif

#ifndef YY_NO_INPUT
#ifdef __cplusplus
static int
yyinput(yyscan_t yyscanner)
#else
static int
input(yyscan_t yyscanner)
#endif

{
  int c;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  *yyg->yy_c_buf_p = yyg->yy_hold_char;

  if (*yyg->yy_c_buf_p == YY_END_OF_BUFFER_CHAR)
  {
                                                                 
                                                               
                                                                
       
    if (yyg->yy_c_buf_p < &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[yyg->yy_n_chars])
    {
                                  
      *yyg->yy_c_buf_p = '\0';
    }

    else
    {                      
      int offset = (int)(yyg->yy_c_buf_p - yyg->yytext_ptr);
      ++yyg->yy_c_buf_p;

      switch (yy_get_next_buffer(yyscanner))
      {
      case EOB_ACT_LAST_MATCH:
                                           
                                         
                                           
                                         
                                         
                                            
                                             
                                   
           

                                  
        yyrestart(yyin, yyscanner);

                       

      case EOB_ACT_END_OF_FILE:
      {
        if (yywrap(yyscanner))
        {
          return 0;
        }

        if (!yyg->yy_did_buffer_switch_on_eof)
        {
          YY_NEW_FILE;
        }
#ifdef __cplusplus
        return yyinput(yyscanner);
#else
        return input(yyscanner);
#endif
      }

      case EOB_ACT_CONTINUE_SCAN:
        yyg->yy_c_buf_p = yyg->yytext_ptr + offset;
        break;
      }
    }
  }

  c = *(unsigned char *)yyg->yy_c_buf_p;                            
  *yyg->yy_c_buf_p = '\0';                                    
  yyg->yy_hold_char = *++yyg->yy_c_buf_p;

  return c;
}
#endif                         

                                                    
                                        
                                        
                                                                          
   
void
yyrestart(FILE *input_file, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  if (!YY_CURRENT_BUFFER)
  {
    yyensure_buffer_stack(yyscanner);
    YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE, yyscanner);
  }

  yy_init_buffer(YY_CURRENT_BUFFER, input_file, yyscanner);
  yy_load_buffer_state(yyscanner);
}

                                        
                                           
                                        
   
void
yy_switch_to_buffer(YY_BUFFER_STATE new_buffer, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

                                                                  
          
                            
                                       
     
  yyensure_buffer_stack(yyscanner);
  if (YY_CURRENT_BUFFER == new_buffer)
  {
    return;
  }

  if (YY_CURRENT_BUFFER)
  {
                                               
    *yyg->yy_c_buf_p = yyg->yy_hold_char;
    YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = yyg->yy_c_buf_p;
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = yyg->yy_n_chars;
  }

  YY_CURRENT_BUFFER_LVALUE = new_buffer;
  yy_load_buffer_state(yyscanner);

                                                              
                                                            
                                                            
                                    
     
  yyg->yy_did_buffer_switch_on_eof = 1;
}

static void
yy_load_buffer_state(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  yyg->yy_n_chars = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
  yyg->yytext_ptr = yyg->yy_c_buf_p = YY_CURRENT_BUFFER_LVALUE->yy_buf_pos;
  yyin = YY_CURRENT_BUFFER_LVALUE->yy_input_file;
  yyg->yy_hold_char = *yyg->yy_c_buf_p;
}

                                                   
                                  
                                                                         
                
                                        
                                       
   
YY_BUFFER_STATE
yy_create_buffer(FILE *file, int size, yyscan_t yyscanner)
{
  YY_BUFFER_STATE b;

  b = (YY_BUFFER_STATE)yyalloc(sizeof(struct yy_buffer_state), yyscanner);
  if (!b)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_create_buffer()");
  }

  b->yy_buf_size = size;

                                                                         
                                                   
     
  b->yy_ch_buf = (char *)yyalloc((yy_size_t)(b->yy_buf_size + 2), yyscanner);
  if (!b->yy_ch_buf)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_create_buffer()");
  }

  b->yy_is_our_buffer = 1;

  yy_init_buffer(b, file, yyscanner);

  return b;
}

                        
                                                     
                                        
   
void
yy_delete_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  if (!b)
  {
    return;
  }

  if (b == YY_CURRENT_BUFFER)
  {                                      
    YY_CURRENT_BUFFER_LVALUE = (YY_BUFFER_STATE)0;
  }

  if (b->yy_is_our_buffer)
  {
    yyfree((void *)b->yy_ch_buf, yyscanner);
  }

  yyfree((void *)b, yyscanner);
}

                                          
                                                                        
                                           
   
static void
yy_init_buffer(YY_BUFFER_STATE b, FILE *file, yyscan_t yyscanner)

{
  int oerrno = errno;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  yy_flush_buffer(b, yyscanner);

  b->yy_input_file = file;
  b->yy_fill_buffer = 1;

                                                                    
                                                            
                                                                
     
  if (b != YY_CURRENT_BUFFER)
  {
    b->yy_bs_lineno = 1;
    b->yy_bs_column = 0;
  }

  b->yy_is_interactive = 0;

  errno = oerrno;
}

                                                                                
                                                                          
                                        
   
void
yy_flush_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  if (!b)
  {
    return;
  }

  b->yy_n_chars = 0;

                                                                    
                                                                 
                          
     
  b->yy_ch_buf[0] = YY_END_OF_BUFFER_CHAR;
  b->yy_ch_buf[1] = YY_END_OF_BUFFER_CHAR;

  b->yy_buf_pos = &b->yy_ch_buf[0];

  b->yy_at_bol = 1;
  b->yy_buffer_status = YY_BUFFER_NEW;

  if (b == YY_CURRENT_BUFFER)
  {
    yy_load_buffer_state(yyscanner);
  }
}

                                                               
                                                             
                  
                                     
                                         
   
void
yypush_buffer_state(YY_BUFFER_STATE new_buffer, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  if (new_buffer == NULL)
  {
    return;
  }

  yyensure_buffer_stack(yyscanner);

                                                      
  if (YY_CURRENT_BUFFER)
  {
                                               
    *yyg->yy_c_buf_p = yyg->yy_hold_char;
    YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = yyg->yy_c_buf_p;
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = yyg->yy_n_chars;
  }

                                                        
  if (YY_CURRENT_BUFFER)
  {
    yyg->yy_buffer_stack_top++;
  }
  YY_CURRENT_BUFFER_LVALUE = new_buffer;

                                        
  yy_load_buffer_state(yyscanner);
  yyg->yy_did_buffer_switch_on_eof = 1;
}

                                                          
                                          
                                         
   
void
yypop_buffer_state(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  if (!YY_CURRENT_BUFFER)
  {
    return;
  }

  yy_delete_buffer(YY_CURRENT_BUFFER, yyscanner);
  YY_CURRENT_BUFFER_LVALUE = NULL;
  if (yyg->yy_buffer_stack_top > 0)
  {
    --yyg->yy_buffer_stack_top;
  }

  if (YY_CURRENT_BUFFER)
  {
    yy_load_buffer_state(yyscanner);
    yyg->yy_did_buffer_switch_on_eof = 1;
  }
}

                                             
                                            
   
static void
yyensure_buffer_stack(yyscan_t yyscanner)
{
  yy_size_t num_to_alloc;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  if (!yyg->yy_buffer_stack)
  {

                                                                            
                                                                         
                                           
       
    num_to_alloc = 1;                                                        
    yyg->yy_buffer_stack = (struct yy_buffer_state **)yyalloc(num_to_alloc * sizeof(struct yy_buffer_state *), yyscanner);
    if (!yyg->yy_buffer_stack)
    {
      YY_FATAL_ERROR("out of dynamic memory in yyensure_buffer_stack()");
    }

    memset(yyg->yy_buffer_stack, 0, num_to_alloc * sizeof(struct yy_buffer_state *));

    yyg->yy_buffer_stack_max = num_to_alloc;
    yyg->yy_buffer_stack_top = 0;
    return;
  }

  if (yyg->yy_buffer_stack_top >= (yyg->yy_buffer_stack_max) - 1)
  {

                                                             
    yy_size_t grow_size = 8                          ;

    num_to_alloc = yyg->yy_buffer_stack_max + grow_size;
    yyg->yy_buffer_stack = (struct yy_buffer_state **)yyrealloc(yyg->yy_buffer_stack, num_to_alloc * sizeof(struct yy_buffer_state *), yyscanner);
    if (!yyg->yy_buffer_stack)
    {
      YY_FATAL_ERROR("out of dynamic memory in yyensure_buffer_stack()");
    }

                                 
    memset(yyg->yy_buffer_stack + yyg->yy_buffer_stack_max, 0, grow_size * sizeof(struct yy_buffer_state *));
    yyg->yy_buffer_stack_max = num_to_alloc;
  }
}

                                                                        
                     
                                    
                                                         
                                        
                                                    
   
YY_BUFFER_STATE
yy_scan_buffer(char *base, yy_size_t size, yyscan_t yyscanner)
{
  YY_BUFFER_STATE b;

  if (size < 2 || base[size - 2] != YY_END_OF_BUFFER_CHAR || base[size - 1] != YY_END_OF_BUFFER_CHAR)
  {
                                                  
    return NULL;
  }

  b = (YY_BUFFER_STATE)yyalloc(sizeof(struct yy_buffer_state), yyscanner);
  if (!b)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_scan_buffer()");
  }

  b->yy_buf_size = (int)(size - 2);                                  
  b->yy_buf_pos = b->yy_ch_buf = base;
  b->yy_is_our_buffer = 0;
  b->yy_input_file = NULL;
  b->yy_n_chars = b->yy_buf_size;
  b->yy_is_interactive = 0;
  b->yy_at_bol = 1;
  b->yy_fill_buffer = 0;
  b->yy_buffer_status = YY_BUFFER_NEW;

  yy_switch_to_buffer(b, yyscanner);

  return b;
}

                                                                                 
                                  
                                                
                                        
                                                    
                                                                         
                                  
   
YY_BUFFER_STATE
yy_scan_string(const char *yystr, yyscan_t yyscanner)
{

  return yy_scan_bytes(yystr, (int)strlen(yystr), yyscanner);
}

                                                                           
                                                 
                                          
                                                                                 
                                        
                                                    
   
YY_BUFFER_STATE
yy_scan_bytes(const char *yybytes, int _yybytes_len, yyscan_t yyscanner)
{
  YY_BUFFER_STATE b;
  char *buf;
  yy_size_t n;
  int i;

                                                                       
  n = (yy_size_t)(_yybytes_len + 2);
  buf = (char *)yyalloc(n, yyscanner);
  if (!buf)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_scan_bytes()");
  }

  for (i = 0; i < _yybytes_len; ++i)
  {
    buf[i] = yybytes[i];
  }

  buf[_yybytes_len] = buf[_yybytes_len + 1] = YY_END_OF_BUFFER_CHAR;

  b = yy_scan_buffer(buf, n, yyscanner);
  if (!b)
  {
    YY_FATAL_ERROR("bad buffer in yy_scan_bytes()");
  }

                                                                
                           
     
  b->yy_is_our_buffer = 1;

  return b;
}

#ifndef YY_EXIT_FAILURE
#define YY_EXIT_FAILURE 2
#endif

static void yynoreturn
yy_fatal_error(const char *msg, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  (void)yyg;
  fprintf(stderr, "%s\n", msg);
  exit(YY_EXIT_FAILURE);
}

                                                      

#undef yyless
#define yyless(n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    int yyless_macro_arg = (n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    YY_LESS_LINENO(yyless_macro_arg);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    yytext[yyleng] = yyg->yy_hold_char;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    yyg->yy_c_buf_p = yytext + yyless_macro_arg;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    yyg->yy_hold_char = *yyg->yy_c_buf_p;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    *yyg->yy_c_buf_p = '\0';                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    yyleng = yyless_macro_arg;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

                                                              

                                                
                                        
   
YY_EXTRA_TYPE
yyget_extra(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yyextra;
}

                                 
                                        
   
int
yyget_lineno(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  if (!YY_CURRENT_BUFFER)
  {
    return 0;
  }

  return yylineno;
}

                                   
                                        
   
int
yyget_column(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

  if (!YY_CURRENT_BUFFER)
  {
    return 0;
  }

  return yycolumn;
}

                          
                                        
   
FILE *
yyget_in(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yyin;
}

                           
                                        
   
FILE *
yyget_out(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yyout;
}

                                         
                                        
   
int
yyget_leng(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yyleng;
}

                           
                                        
   

char *
yyget_text(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yytext;
}

                                                                          
                                                                    
                                        
   
void
yyset_extra(YY_EXTRA_TYPE user_defined, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  yyextra = user_defined;
}

                                 
                                   
                                        
   
void
yyset_lineno(int _line_number, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

                                                       
  if (!YY_CURRENT_BUFFER)
  {
    YY_FATAL_ERROR("yyset_lineno called with no buffer");
  }

  yylineno = _line_number;
}

                            
                                   
                                        
   
void
yyset_column(int _column_no, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

                                                       
  if (!YY_CURRENT_BUFFER)
  {
    YY_FATAL_ERROR("yyset_column called with no buffer");
  }

  yycolumn = _column_no;
}

                                                            
                 
                                     
                                        
                            
   
void
yyset_in(FILE *_in_str, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  yyin = _in_str;
}

void
yyset_out(FILE *_out_str, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  yyout = _out_str;
}

int
yyget_debug(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yy_flex_debug;
}

void
yyset_debug(int _bdebug, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  yy_flex_debug = _bdebug;
}

                                            

YYSTYPE *
yyget_lval(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  return yylval;
}

void
yyset_lval(YYSTYPE *yylval_param, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  yylval = yylval_param;
}

                      

                                                                         
                                                                         
                                                                               
               
   
int
yylex_init(yyscan_t *ptr_yy_globals)
{
  if (ptr_yy_globals == NULL)
  {
    errno = EINVAL;
    return 1;
  }

  *ptr_yy_globals = (yyscan_t)yyalloc(sizeof(struct yyguts_t), NULL);

  if (*ptr_yy_globals == NULL)
  {
    errno = ENOMEM;
    return 1;
  }

                                                                              
                 
  memset(*ptr_yy_globals, 0x00, sizeof(struct yyguts_t));

  return yy_init_globals(*ptr_yy_globals);
}

                                                                              
                                                                             
                                                                               
                                                                                
                                                                                
                      
   
int
yylex_init_extra(YY_EXTRA_TYPE yy_user_defined, yyscan_t *ptr_yy_globals)
{
  struct yyguts_t dummy_yyguts;

  yyset_extra(yy_user_defined, &dummy_yyguts);

  if (ptr_yy_globals == NULL)
  {
    errno = EINVAL;
    return 1;
  }

  *ptr_yy_globals = (yyscan_t)yyalloc(sizeof(struct yyguts_t), &dummy_yyguts);

  if (*ptr_yy_globals == NULL)
  {
    errno = ENOMEM;
    return 1;
  }

                                           
                                                 
  memset(*ptr_yy_globals, 0x00, sizeof(struct yyguts_t));

  yyset_extra(yy_user_defined, *ptr_yy_globals);

  return yy_init_globals(*ptr_yy_globals);
}

static int
yy_init_globals(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
                                                                  
                                                                           
     

  yyg->yy_buffer_stack = NULL;
  yyg->yy_buffer_stack_top = 0;
  yyg->yy_buffer_stack_max = 0;
  yyg->yy_c_buf_p = NULL;
  yyg->yy_init = 0;
  yyg->yy_start = 0;

  yyg->yy_start_stack_ptr = 0;
  yyg->yy_start_stack_depth = 0;
  yyg->yy_start_stack = NULL;

                       
#ifdef YY_STDINIT
  yyin = stdin;
  yyout = stdout;
#else
  yyin = NULL;
  yyout = NULL;
#endif

                                                                      
                  
     
  return 0;
}

                                                                     
int
yylex_destroy(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;

                                                      
  while (YY_CURRENT_BUFFER)
  {
    yy_delete_buffer(YY_CURRENT_BUFFER, yyscanner);
    YY_CURRENT_BUFFER_LVALUE = NULL;
    yypop_buffer_state(yyscanner);
  }

                                 
  yyfree(yyg->yy_buffer_stack, yyscanner);
  yyg->yy_buffer_stack = NULL;

                                          
  yyfree(yyg->yy_start_stack, yyscanner);
  yyg->yy_start_stack = NULL;

                                                                                 
                                                          
  yy_init_globals(yyscanner);

                                                 
  yyfree(yyscanner, yyscanner);
  yyscanner = NULL;
  return 0;
}

   
                              
   

#ifndef yytext_ptr
static void
yy_flex_strncpy(char *s1, const char *s2, int n, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  (void)yyg;

  int i;
  for (i = 0; i < n; ++i)
  {
    s1[i] = s2[i];
  }
}
#endif

#ifdef YY_NEED_STRLEN
static int
yy_flex_strlen(const char *s, yyscan_t yyscanner)
{
  int n;
  for (n = 0; s[n]; ++n)
    ;

  return n;
}
#endif

void *
yyalloc(yy_size_t size, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  (void)yyg;
  return malloc(size);
}

void *
yyrealloc(void *ptr, yy_size_t size, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  (void)yyg;

                                                             
                                                                
                                                                
                                                                
                                                                   
                                    
     
  return realloc(ptr, size);
}

void
yyfree(void *ptr, yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
  (void)yyg;
  free((char *)ptr);                                        
}

#define YYTABLES_NAME "yytables"

#line 929 "psqlscan.l"

                    

   
                                        
   
                                                                    
                                                                    
                                                       
   
PsqlScanState
psql_scan_create(const PsqlScanCallbacks *callbacks)
{
  PsqlScanState state;

  state = (PsqlScanStateData *)pg_malloc0(sizeof(PsqlScanStateData));

  state->callbacks = callbacks;

  yylex_init(&state->scanner);

  yyset_extra(state, state->scanner);

  psql_scan_reset(state);

  return state;
}

   
                                                                  
   
void
psql_scan_destroy(PsqlScanState state)
{
  psql_scan_finish(state);

  psql_scan_reset(state);

  yylex_destroy(state->scanner);

  free(state);
}

   
                                                       
   
                                                                         
                                                                            
              
   
void
psql_scan_set_passthrough(PsqlScanState state, void *passthrough)
{
  state->cb_passthrough = passthrough;
}

   
                                                     
   
                                                                       
                                                                        
                                                                     
                                                                         
                                                    
   
                                                                       
                                                                   
   
void
psql_scan_setup(PsqlScanState state, const char *line, int line_len, int encoding, bool std_strings)
{
                                   
  Assert(state->scanbufhandle == NULL);
  Assert(state->buffer_stack == NULL);

                                                      
  state->encoding = encoding;
  state->safe_encoding = pg_valid_server_encoding_id(encoding);

                                          
  state->std_strings = std_strings;

                                                                         
  state->scanbufhandle = psqlscan_prepare_buffer(state, line, line_len, &state->scanbuf);
  state->scanline = line;

                                                                 
  state->curline = state->scanbuf;
  state->refline = state->scanline;
}

   
                                            
   
                                                                          
                                                
   
                                                                   
   
                                                                         
                                                                           
                                                                          
                
   
                                                                     
                                                                    
                                                                      
                        
   
                                                                     
                                                                           
   
                                                                       
                                                                         
                                                                        
                                             
   
                                                                          
                                                                           
   
                                                                           
                              
   
PsqlScanResult
psql_scan(PsqlScanState state, PQExpBuffer query_buf, promptStatus_t *prompt)
{
  PsqlScanResult result;
  int lexresult;

                                
  Assert(state->scanbufhandle != NULL);

                                 
  state->output_buf = query_buf;

                        
  if (state->buffer_stack != NULL)
  {
    yy_switch_to_buffer(state->buffer_stack->buf, state->scanner);
  }
  else
  {
    yy_switch_to_buffer(state->scanbufhandle, state->scanner);
  }

                
  lexresult = yylex(NULL, state->scanner);

     
                                                                 
     
  switch (lexresult)
  {
  case LEXRES_EOL:                   
    switch (state->start_state)
    {
    case INITIAL:
    case xuiend:                                  
    case xusend:
      if (state->paren_depth > 0)
      {
        result = PSCAN_INCOMPLETE;
        *prompt = PROMPT_PAREN;
      }
      else if (query_buf->len > 0)
      {
        result = PSCAN_EOL;
        *prompt = PROMPT_CONTINUE;
      }
      else
      {
                                                  
        result = PSCAN_INCOMPLETE;
        *prompt = PROMPT_READY;
      }
      break;
    case xb:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_SINGLEQUOTE;
      break;
    case xc:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_COMMENT;
      break;
    case xd:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_DOUBLEQUOTE;
      break;
    case xh:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_SINGLEQUOTE;
      break;
    case xe:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_SINGLEQUOTE;
      break;
    case xq:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_SINGLEQUOTE;
      break;
    case xdolq:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_DOLLARQUOTE;
      break;
    case xui:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_DOUBLEQUOTE;
      break;
    case xus:
      result = PSCAN_INCOMPLETE;
      *prompt = PROMPT_SINGLEQUOTE;
      break;
    default:;
                          
      fprintf(stderr, "invalid YY_START\n");
      exit(1);
      break;
    }
    break;
  case LEXRES_SEMI:                
    result = PSCAN_SEMICOLON;
    *prompt = PROMPT_READY;
    break;
  case LEXRES_BACKSLASH:                
    result = PSCAN_BACKSLASH;
    *prompt = PROMPT_READY;
    break;
  default:;
                        
    fprintf(stderr, "invalid yylex result\n");
    exit(1);
    break;
  }

  return result;
}

   
                                                                        
                                                                        
                                                                      
                                                        
   
                                                                        
                                 
   
void
psql_scan_finish(PsqlScanState state)
{
                                                
  while (state->buffer_stack != NULL)
  {
    psqlscan_pop_buffer_stack(state);
  }

                                            
  if (state->scanbufhandle)
  {
    yy_delete_buffer(state->scanbufhandle, state->scanner);
  }
  state->scanbufhandle = NULL;
  if (state->scanbuf)
  {
    free(state->scanbuf);
  }
  state->scanbuf = NULL;
}

   
                                                                        
                                                                         
                                                                           
                                                                           
                                                                            
                            
   
                                                                      
                               
   
void
psql_scan_reset(PsqlScanState state)
{
  state->start_state = INITIAL;
  state->paren_depth = 0;
  state->xcdepth = 0;                           
  if (state->dolqstart)
  {
    free(state->dolqstart);
  }
  state->dolqstart = NULL;
}

   
                                                             
   
                                                                           
                                                                           
                                                                      
                                  
   
                                                                          
                               
   
                                                                         
                                                                              
                                      
   
void
psql_scan_reselect_sql_lexer(PsqlScanState state)
{
  state->start_state = INITIAL;
}

   
                                                                  
   
                                                                    
                                                                    
                 
   
bool
psql_scan_in_quote(PsqlScanState state)
{
  return state->start_state != INITIAL;
}

   
                                                          
   
                                                                          
   
void
psqlscan_push_new_buffer(PsqlScanState state, const char *newstr, const char *varname)
{
  StackElem *stackelem;

  stackelem = (StackElem *)pg_malloc(sizeof(StackElem));

     
                                                                           
                                                                      
                                                
     
  stackelem->varname = varname ? pg_strdup(varname) : NULL;

  stackelem->buf = psqlscan_prepare_buffer(state, newstr, strlen(newstr), &stackelem->bufstring);
  state->curline = stackelem->bufstring;
  if (state->safe_encoding)
  {
    stackelem->origstring = NULL;
    state->refline = stackelem->bufstring;
  }
  else
  {
    stackelem->origstring = pg_strdup(newstr);
    state->refline = stackelem->origstring;
  }
  stackelem->next = state->buffer_stack;
  state->buffer_stack = stackelem;
}

   
                                                          
   
                                                                    
                                                       
                                     
   
void
psqlscan_pop_buffer_stack(PsqlScanState state)
{
  StackElem *stackelem = state->buffer_stack;

  state->buffer_stack = stackelem->next;
  yy_delete_buffer(stackelem->buf, state->scanner);
  free(stackelem->bufstring);
  if (stackelem->origstring)
  {
    free(stackelem->origstring);
  }
  if (stackelem->varname)
  {
    free(stackelem->varname);
  }
  free(stackelem);
}

   
                                                            
   
void
psqlscan_select_top_buffer(PsqlScanState state)
{
  StackElem *stackelem = state->buffer_stack;

  if (stackelem != NULL)
  {
    yy_switch_to_buffer(stackelem->buf, state->scanner);
    state->curline = stackelem->bufstring;
    state->refline = stackelem->origstring ? stackelem->origstring : stackelem->bufstring;
  }
  else
  {
    yy_switch_to_buffer(state->scanbufhandle, state->scanner);
    state->curline = state->scanbuf;
    state->refline = state->scanline;
  }
}

   
                                                                 
                           
   
bool
psqlscan_var_is_current_source(PsqlScanState state, const char *varname)
{
  StackElem *stackelem;

  for (stackelem = state->buffer_stack; stackelem != NULL; stackelem = stackelem->next)
  {
    if (stackelem->varname && strcmp(stackelem->varname, varname) == 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                                        
                                                                     
                                                                         
   
                                                                          
   
YY_BUFFER_STATE
psqlscan_prepare_buffer(PsqlScanState state, const char *txt, int len, char **txtcopy)
{
  char *newtxt;

                                                          
  newtxt = pg_malloc(len + 2);
  *txtcopy = newtxt;
  newtxt[len] = newtxt[len + 1] = YY_END_OF_BUFFER_CHAR;

  if (state->safe_encoding)
  {
    memcpy(newtxt, txt, len);
  }
  else
  {
                                  
    int i = 0;

    while (i < len)
    {
      int thislen = PQmblen(txt + i, state->encoding);

                                               
      newtxt[i] = txt[i];
      i++;
      while (--thislen > 0 && i < len)
      {
        newtxt[i++] = (char)0xFF;
      }
    }
  }

  return yy_scan_buffer(newtxt, len + 2, state->scanner);
}

   
                                           
   
                                                                        
                                                                        
                                                                       
                                           
   
void
psqlscan_emit(PsqlScanState state, const char *txt, int len)
{
  PQExpBuffer output_buf = state->output_buf;

  if (state->safe_encoding)
  {
    appendBinaryPQExpBuffer(output_buf, txt, len);
  }
  else
  {
                                  
    const char *reference = state->refline;
    int i;

    reference += (txt - state->curline);

    for (i = 0; i < len; i++)
    {
      char ch = txt[i];

      if (ch == (char)0xFF)
      {
        ch = reference[i];
      }
      appendPQExpBufferChar(output_buf, ch);
    }
  }
}

   
                                                                             
   
                                                                       
                                                                           
   
char *
psqlscan_extract_substring(PsqlScanState state, const char *txt, int len)
{
  char *result = (char *)pg_malloc(len + 1);

  if (state->safe_encoding)
  {
    memcpy(result, txt, len);
  }
  else
  {
                                  
    const char *reference = state->refline;
    int i;

    reference += (txt - state->curline);

    for (i = 0; i < len; i++)
    {
      char ch = txt[i];

      if (ch == (char)0xFF)
      {
        ch = reference[i];
      }
      result[i] = ch;
    }
  }
  result[len] = '\0';
  return result;
}

   
                                                                   
   
                                                                         
                                                                          
                                                                       
                                                              
   
void
psqlscan_escape_variable(PsqlScanState state, const char *txt, int len, PsqlScanQuoteType quote)
{
  char *varname;
  char *value;

                        
  varname = psqlscan_extract_substring(state, txt + 2, len - 3);
  if (state->callbacks->get_variable)
  {
    value = state->callbacks->get_variable(varname, quote, state->cb_passthrough);
  }
  else
  {
    value = NULL;
  }
  free(varname);

  if (value)
  {
                                         
    appendPQExpBufferStr(state->output_buf, value);
    free(value);
  }
  else
  {
                                   
    psqlscan_emit(state, txt, len);
  }
}

void
psqlscan_test_variable(PsqlScanState state, const char *txt, int len)
{
  char *varname;
  char *value;

  varname = psqlscan_extract_substring(state, txt + 3, len - 4);
  if (state->callbacks->get_variable)
  {
    value = state->callbacks->get_variable(varname, PQUOTE_PLAIN, state->cb_passthrough);
  }
  else
  {
    value = NULL;
  }
  free(varname);

  if (value != NULL)
  {
    psqlscan_emit(state, "TRUE", 4);
    free(value);
  }
  else
  {
    psqlscan_emit(state, "FALSE", 5);
  }
}
