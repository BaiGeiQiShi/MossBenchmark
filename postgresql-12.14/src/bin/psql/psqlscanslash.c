#line 2 "psqlscanslash.c"
                                                                            
   
                   
                                                 
   
                                                                             
   
                                                          
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   
#include "postgres_fe.h"

#include "psqlscanslash.h"
#include "common/logging.h"
#include "fe_utils/conditional.h"

#include "libpq-fe.h"

#line 28 "psqlscanslash.c"

#define YY_INT_ALIGNED short int

                                         

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define slash_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer slash_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define slash_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer slash_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define slash_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer slash_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define slash_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string slash_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define slash_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes slash_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define slash_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer slash_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define slash_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer slash_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define slash_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state slash_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define slash_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer slash_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define slash_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state slash_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define slash_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state slash_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define slash_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack slash_yyensure_buffer_stack
#endif

#ifdef yylex
#define slash_yylex_ALREADY_DEFINED
#else
#define yylex slash_yylex
#endif

#ifdef yyrestart
#define slash_yyrestart_ALREADY_DEFINED
#else
#define yyrestart slash_yyrestart
#endif

#ifdef yylex_init
#define slash_yylex_init_ALREADY_DEFINED
#else
#define yylex_init slash_yylex_init
#endif

#ifdef yylex_init_extra
#define slash_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra slash_yylex_init_extra
#endif

#ifdef yylex_destroy
#define slash_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy slash_yylex_destroy
#endif

#ifdef yyget_debug
#define slash_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug slash_yyget_debug
#endif

#ifdef yyset_debug
#define slash_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug slash_yyset_debug
#endif

#ifdef yyget_extra
#define slash_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra slash_yyget_extra
#endif

#ifdef yyset_extra
#define slash_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra slash_yyset_extra
#endif

#ifdef yyget_in
#define slash_yyget_in_ALREADY_DEFINED
#else
#define yyget_in slash_yyget_in
#endif

#ifdef yyset_in
#define slash_yyset_in_ALREADY_DEFINED
#else
#define yyset_in slash_yyset_in
#endif

#ifdef yyget_out
#define slash_yyget_out_ALREADY_DEFINED
#else
#define yyget_out slash_yyget_out
#endif

#ifdef yyset_out
#define slash_yyset_out_ALREADY_DEFINED
#else
#define yyset_out slash_yyset_out
#endif

#ifdef yyget_leng
#define slash_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng slash_yyget_leng
#endif

#ifdef yyget_text
#define slash_yyget_text_ALREADY_DEFINED
#else
#define yyget_text slash_yyget_text
#endif

#ifdef yyget_lineno
#define slash_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno slash_yyget_lineno
#endif

#ifdef yyset_lineno
#define slash_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno slash_yyset_lineno
#endif

#ifdef yyget_column
#define slash_yyget_column_ALREADY_DEFINED
#else
#define yyget_column slash_yyget_column
#endif

#ifdef yyset_column
#define slash_yyset_column_ALREADY_DEFINED
#else
#define yyset_column slash_yyset_column
#endif

#ifdef yywrap
#define slash_yywrap_ALREADY_DEFINED
#else
#define yywrap slash_yywrap
#endif

#ifdef yyget_lval
#define slash_yyget_lval_ALREADY_DEFINED
#else
#define yyget_lval slash_yyget_lval
#endif

#ifdef yyset_lval
#define slash_yyset_lval_ALREADY_DEFINED
#else
#define yyset_lval slash_yyset_lval
#endif

#ifdef yyalloc
#define slash_yyalloc_ALREADY_DEFINED
#else
#define yyalloc slash_yyalloc
#endif

#ifdef yyrealloc
#define slash_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc slash_yyrealloc
#endif

#ifdef yyfree
#define slash_yyfree_ALREADY_DEFINED
#else
#define yyfree slash_yyfree
#endif

                                                                         

                               
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

                             

                                   

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

                      

#define slash_yywrap(yyscanner) (              1)
#define YY_SKIP_YYWRAP
typedef flex_uint8_t YY_CHAR;

typedef int yy_state_type;

#define yytext_ptr yytext_r

static const flex_int16_t yy_nxt[][22] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

    {19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20},

    {19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20},

    {19, 21, 22, 23, 21, 21, 21, 21, 21, 21,

        21, 22, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21},

    {19, 21, 22, 23, 21, 21, 21, 21, 21, 21, 21, 22, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21},

    {19, 24, 25, 26, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 27, 24},

    {19, 24, 25, 26, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,

        27, 24},

    {19, 28, 29, 30, 31, 32, 28, 28, 33, 28, 28, 29, 34, 28, 28, 28, 28, 28, 28, 28, 28, 28},

    {19, 28, 29, 30, 31, 32, 28, 28, 33, 28, 28, 29, 34, 28, 28, 28, 28, 28, 28, 28, 28, 28},

    {19, 35, 35, 35, 35, 36, 35, 35, 35, 35, 35, 37, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35

    },

    {19, 35, 35, 35, 35, 36, 35, 35, 35, 35, 35, 37, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35},

    {19, 38, 38, 38, 38, 38, 38, 38, 39, 38, 38, 38, 40, 38, 38, 38, 38, 38, 38, 38, 38, 38},

    {19, 38, 38, 38, 38, 38, 38, 38, 39, 38, 38, 38, 40, 38, 38, 38, 38, 38, 38, 38, 38, 38},

    {19, 41, 41, 41, 42, 41, 41, 41, 41, 41,

        41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41},

    {19, 41, 41, 41, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41},

    {19, 43, 44, 45, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43},

    {19, 43, 44, 45, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,

        43, 43},

    {19, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46},

    {19, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46},

    {-19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19

    },

    {19, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20},

    {19, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21, -21},

    {19, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22, -22},

    {19, -23, -23, -23, -23, -23, -23, -23, -23, -23,

        -23, -23, -23, -23, -23, -23, -23, -23, -23, -23, -23, -23},

    {19, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24},

    {19, -25, 48, 48, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25},

    {19, -26, 48, 48, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26,

        -26, -26},

    {19, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27, -27},

    {19, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28, -28},

    {19, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29, -29

    },

    {19, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30},

    {19, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31, -31},

    {19, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32},

    {19, -33, -33, -33, 49, 50, 51, 51, -33, -33,

        51, -33, -33, 51, 51, 51, 51, 51, 51, 52, -33, -33},

    {19, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34, -34},

    {19, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35, -35},

    {19, -36, -36, -36, -36, 53, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36, -36,

        -36, -36},

    {19, 54, 54, -37, 54, 54, 55, 54, 54, 54, 54, 54, 54, 56, 57, 58, 59, 60, 61, 54, 54, 54},

    {19, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38, -38},

    {19, -39, -39, -39, -39, 62, 63, 63, -39, -39, 63, -39, -39, 63, 63, 63, 63, 63, 63, -39, -39, -39

    },

    {19, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40},

    {19, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41, -41},

    {19, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42, -42},

    {19, -43, -43, -43, -43, -43, -43, -43, -43, -43,

        -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43},

    {19, -44, 64, 64, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44},

    {19, -45, 64, 64, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45},

    {19, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46,

        -46, -46},

    {19, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, 65, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47},

    {19, -48, 48, 48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48},

    {19, -49, -49, -49, -49, -49, 66, 66, -49, -49, 66, -49, -49, 66, 66, 66, 66, 66, 66, -49, -49, -49

    },

    {19, -50, -50, -50, -50, -50, 67, 67, -50, -50, 67, -50, -50, 67, 67, 67, 67, 67, 67, -50, -50, -50},

    {19, -51, -51, -51, -51, -51, 51, 51, -51, -51, 51, -51, -51, 51, 51, 51, 51, 51, 51, -51, -51, -51},

    {19, -52, -52, -52, -52, -52, -52, -52, -52, 68, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52},

    {19, -53, -53, -53, -53, -53, -53, -53, -53, -53,

        -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53},

    {19, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54},

    {19, -55, -55, -55, -55, -55, 69, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55},

    {19, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56,

        -56, -56},

    {19, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57},

    {19, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58},

    {19, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59

    },

    {19, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60},

    {19, -61, -61, -61, -61, -61, 70, 70, -61, -61, -61, -61, -61, 70, 70, -61, -61, -61, -61, -61, -61, -61},

    {19, -62, -62, -62, -62, -62, 71, 71, -62, -62, 71, -62, -62, 71, 71, 71, 71, 71, 71, -62, -62, -62},

    {19, -63, -63, -63, -63, -63, 63, 63, -63, -63,

        63, -63, -63, 63, 63, 63, 63, 63, 63, -63, -63, -63},

    {19, -64, 64, 64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64},

    {19, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65},

    {19, -66, -66, -66, 72, -66, 66, 66, -66, -66, 66, -66, -66, 66, 66, 66, 66, 66, 66, -66,

        -66, -66},

    {19, -67, -67, -67, -67, 73, 67, 67, -67, -67, 67, -67, -67, 67, 67, 67, 67, 67, 67, -67, -67, -67},

    {19, -68, -68, -68, -68, -68, 74, 74, -68, -68, 74, -68, -68, 74, 74, 74, 74, 74, 74, -68, -68, -68},

    {19, -69, -69, -69, -69, -69, 75, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69

    },

    {19, -70, -70, -70, -70, -70, 76, 76, -70, -70, -70, -70, -70, 76, 76, -70, -70, -70, -70, -70, -70, -70},

    {19, -71, -71, -71, -71, 77, 71, 71, -71, -71, 71, -71, -71, 71, 71, 71, 71, 71, 71, -71, -71, -71},

    {19, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72},

    {19, -73, -73, -73, -73, -73, -73, -73, -73, -73,

        -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73, -73},

    {19, -74, -74, -74, -74, -74, 74, 74, -74, -74, 74, -74, -74, 74, 74, 74, 74, 74, 74, -74, -74, 78},

    {19, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75, -75},

    {19, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76, -76,

        -76, -76},

    {19, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77, -77},

    {19, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78, -78},

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
#define YY_NUM_RULES 42
#define YY_END_OF_BUFFER 43
                                            
                                    
struct yy_trans_info
{
  flex_int32_t yy_verify;
  flex_int32_t yy_nxt;
};
static const flex_int16_t yy_accept[79] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 43, 1, 3, 2, 2, 6, 4, 4, 5, 19, 7, 7, 10, 8, 19, 9, 30, 20, 30, 35, 35, 31, 37, 36, 39, 38, 38, 41, 41, 4, 16, 15, 11, 18, 21, 29, 27, 24, 26, 22, 25, 23, 29, 34, 32, 38, 40, 16, 15, 17, 27, 28, 34, 13, 12, 17, 27, 28, 33, 14};

static const YY_CHAR yy_ec[256] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 4, 1, 1, 1, 1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 8, 1, 1, 1, 1, 9, 1, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 1, 11, 1, 1, 10, 12, 7, 13, 7, 7,

    7, 14, 10, 10, 10, 10, 10, 10, 10, 15, 10, 10, 10, 16, 10, 17, 10, 10, 10, 18, 10, 10, 19, 20, 21, 1, 1, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
#line 1 "psqlscanslash.l"

#line 29 "psqlscanslash.l"
#include "fe_utils/psqlscan_int.h"

#define PQmblenBounded(s, e) strnlen(s, PQmblen(s, e))

   
                                                                             
                                                                           
   
typedef int YYSTYPE;

   
                                                                          
                  
   
#define YY_EXTRA_TYPE PsqlScanState

   
                                                                           
                                                                          
   
static enum slash_option_type option_type;
static char *option_quote;
static int unquoted_option_chars;
static int backtick_start_offset;

                                
#define LEXRES_EOL 0                   
#define LEXRES_OK 1                                           

static void
evaluate_backtick(PsqlScanState state);

#define ECHO psqlscan_emit(cur_state, yytext, yyleng)

   
                                                                         
                                                                            
                                                                        
                                          
   
extern int
slash_yyget_column(yyscan_t yyscanner);
extern void
slash_yyset_column(int column_no, yyscan_t yyscanner);

                     

#line 1199 "psqlscanslash.c"
                                                                  
#define YY_NO_INPUT 1
   
                                                               
                                                                       
                                                                               
                                                                            
                                                                             
                                                                      
   
                                                    

   
                                                                      
   
#line 1215 "psqlscanslash.c"

#define INITIAL 0
#define xslashcmd 1
#define xslashargstart 2
#define xslasharg 3
#define xslashquote 4
#define xslashbackquote 5
#define xslashdquote 6
#define xslashwholeline 7
#define xslashend 8

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
#line 121 "psqlscanslash.l"

#line 125 "psqlscanslash.l"
                                                                      
    PsqlScanState cur_state = yyextra;
    PQExpBuffer output_buf = cur_state->output_buf;

       
                                                                       
                                                                         
                                                                           
                                                                         
                                                                           
       
    BEGIN(cur_state->start_state);

       
                                                                         
                                                                         
       

#line 1519 "psqlscanslash.c"

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
#line 144 "psqlscanslash.l"
        {
          ECHO;
        }
        YY_BREAK
         
                                                                   
         

                                                                      
      case 2:
                                  
        YY_RULE_SETUP
#line 153 "psqlscanslash.l"
        {
          yyless(0);
          cur_state->start_state = YY_START;
          return LEXRES_OK;
        }
        YY_BREAK
      case 3:
        YY_RULE_SETUP
#line 159 "psqlscanslash.l"
        {
          ECHO;
        }
        YY_BREAK

         
                                                                             
                                                                              
                            
         
      case 4:
                                  
        YY_RULE_SETUP
#line 170 "psqlscanslash.l"
        {
        }
        YY_BREAK
      case 5:
        YY_RULE_SETUP
#line 172 "psqlscanslash.l"
        {
          if (option_type == OT_FILEPIPE)
          {
                                              
            ECHO;
            BEGIN(xslashwholeline);
          }
          else
          {
                                                       
            yyless(0);
            BEGIN(xslasharg);
          }
        }
        YY_BREAK
      case 6:
        YY_RULE_SETUP
#line 187 "psqlscanslash.l"
        {
          yyless(0);
          BEGIN(xslasharg);
        }
        YY_BREAK

         
                                                                   
         
                                                                            
                                                                           
                                                                                
         
      case 7:
                                  
        YY_RULE_SETUP
#line 203 "psqlscanslash.l"
        {
             
                                                                 
                                                                     
             
                                                                 
                                                                  
                                                                     
                     
             
          yyless(0);
          cur_state->start_state = YY_START;
          return LEXRES_OK;
        }
        YY_BREAK
      case 8:
        YY_RULE_SETUP
#line 218 "psqlscanslash.l"
        {
          *option_quote = '\'';
          unquoted_option_chars = 0;
          BEGIN(xslashquote);
        }
        YY_BREAK
      case 9:
        YY_RULE_SETUP
#line 224 "psqlscanslash.l"
        {
          backtick_start_offset = output_buf->len;
          *option_quote = '`';
          unquoted_option_chars = 0;
          BEGIN(xslashbackquote);
        }
        YY_BREAK
      case 10:
        YY_RULE_SETUP
#line 231 "psqlscanslash.l"
        {
          ECHO;
          *option_quote = '"';
          unquoted_option_chars = 0;
          BEGIN(xslashdquote);
        }
        YY_BREAK
      case 11:
        YY_RULE_SETUP
#line 238 "psqlscanslash.l"
        {
                                                   
          if (cur_state->callbacks->get_variable == NULL)
          {
            ECHO;
          }
          else
          {
            char *varname;
            char *value;

            varname = psqlscan_extract_substring(cur_state, yytext + 1, yyleng - 1);
            value = cur_state->callbacks->get_variable(varname, PQUOTE_PLAIN, cur_state->cb_passthrough);
            free(varname);

               
                                                              
                                                                 
                                                               
                                                                 
                                                                  
               
            if (value)
            {
              appendPQExpBufferStr(output_buf, value);
              free(value);
            }
            else
            {
              ECHO;
            }

            *option_quote = ':';
          }
          unquoted_option_chars = 0;
        }
        YY_BREAK
      case 12:
        YY_RULE_SETUP
#line 275 "psqlscanslash.l"
        {
          psqlscan_escape_variable(cur_state, yytext, yyleng, PQUOTE_SQL_LITERAL);
          *option_quote = ':';
          unquoted_option_chars = 0;
        }
        YY_BREAK
      case 13:
        YY_RULE_SETUP
#line 283 "psqlscanslash.l"
        {
          psqlscan_escape_variable(cur_state, yytext, yyleng, PQUOTE_SQL_IDENT);
          *option_quote = ':';
          unquoted_option_chars = 0;
        }
        YY_BREAK
      case 14:
        YY_RULE_SETUP
#line 290 "psqlscanslash.l"
        {
          psqlscan_test_variable(cur_state, yytext, yyleng);
        }
        YY_BREAK
      case 15:
        YY_RULE_SETUP
#line 294 "psqlscanslash.l"
        {
                                                   
          yyless(1);
          unquoted_option_chars++;
          ECHO;
        }
        YY_BREAK
      case 16:
        YY_RULE_SETUP
#line 301 "psqlscanslash.l"
        {
                                                   
          yyless(1);
          unquoted_option_chars++;
          ECHO;
        }
        YY_BREAK
      case 17:
        YY_RULE_SETUP
#line 308 "psqlscanslash.l"
        {
                                                   
          yyless(1);
          unquoted_option_chars++;
          ECHO;
        }
        YY_BREAK
      case 18:
        YY_RULE_SETUP
#line 315 "psqlscanslash.l"
        {
                                                   
          yyless(1);
          unquoted_option_chars++;
          ECHO;
        }
        YY_BREAK
      case 19:
        YY_RULE_SETUP
#line 322 "psqlscanslash.l"
        {
          unquoted_option_chars++;
          ECHO;
        }
        YY_BREAK

         
                                                                        
                   
         
      case 20:
        YY_RULE_SETUP
#line 335 "psqlscanslash.l"
        {
          BEGIN(xslasharg);
        }
        YY_BREAK
      case 21:
        YY_RULE_SETUP
#line 337 "psqlscanslash.l"
        {
          appendPQExpBufferChar(output_buf, '\'');
        }
        YY_BREAK
      case 22:
        YY_RULE_SETUP
#line 339 "psqlscanslash.l"
        {
          appendPQExpBufferChar(output_buf, '\n');
        }
        YY_BREAK
      case 23:
        YY_RULE_SETUP
#line 340 "psqlscanslash.l"
        {
          appendPQExpBufferChar(output_buf, '\t');
        }
        YY_BREAK
      case 24:
        YY_RULE_SETUP
#line 341 "psqlscanslash.l"
        {
          appendPQExpBufferChar(output_buf, '\b');
        }
        YY_BREAK
      case 25:
        YY_RULE_SETUP
#line 342 "psqlscanslash.l"
        {
          appendPQExpBufferChar(output_buf, '\r');
        }
        YY_BREAK
      case 26:
        YY_RULE_SETUP
#line 343 "psqlscanslash.l"
        {
          appendPQExpBufferChar(output_buf, '\f');
        }
        YY_BREAK
      case 27:
        YY_RULE_SETUP
#line 345 "psqlscanslash.l"
        {
                          
          appendPQExpBufferChar(output_buf, (char)strtol(yytext + 1, NULL, 8));
        }
        YY_BREAK
      case 28:
        YY_RULE_SETUP
#line 351 "psqlscanslash.l"
        {
                        
          appendPQExpBufferChar(output_buf, (char)strtol(yytext + 2, NULL, 16));
        }
        YY_BREAK
      case 29:
        YY_RULE_SETUP
#line 357 "psqlscanslash.l"
        {
          psqlscan_emit(cur_state, yytext + 1, 1);
        }
        YY_BREAK
      case 30:
                                   
        YY_RULE_SETUP
#line 359 "psqlscanslash.l"
        {
          ECHO;
        }
        YY_BREAK

         
                                                                          
                                                                     
         
      case 31:
        YY_RULE_SETUP
#line 369 "psqlscanslash.l"
        {
                                                                     
          if (cur_state->cb_passthrough == NULL || conditional_active((ConditionalStack)cur_state->cb_passthrough))
          {
            evaluate_backtick(cur_state);
          }
          BEGIN(xslasharg);
        }
        YY_BREAK
      case 32:
        YY_RULE_SETUP
#line 377 "psqlscanslash.l"
        {
                                                   
          if (cur_state->callbacks->get_variable == NULL)
          {
            ECHO;
          }
          else
          {
            char *varname;
            char *value;

            varname = psqlscan_extract_substring(cur_state, yytext + 1, yyleng - 1);
            value = cur_state->callbacks->get_variable(varname, PQUOTE_PLAIN, cur_state->cb_passthrough);
            free(varname);

            if (value)
            {
              appendPQExpBufferStr(output_buf, value);
              free(value);
            }
            else
            {
              ECHO;
            }
          }
        }
        YY_BREAK
      case 33:
        YY_RULE_SETUP
#line 404 "psqlscanslash.l"
        {
          psqlscan_escape_variable(cur_state, yytext, yyleng, PQUOTE_SHELL_ARG);
        }
        YY_BREAK
      case 34:
        YY_RULE_SETUP
#line 409 "psqlscanslash.l"
        {
                                                   
          yyless(1);
          ECHO;
        }
        YY_BREAK
      case 35:
                                   
        YY_RULE_SETUP
#line 415 "psqlscanslash.l"
        {
          ECHO;
        }
        YY_BREAK

                                                                          
      case 36:
        YY_RULE_SETUP
#line 422 "psqlscanslash.l"
        {
          ECHO;
          BEGIN(xslasharg);
        }
        YY_BREAK
      case 37:
                                   
        YY_RULE_SETUP
#line 427 "psqlscanslash.l"
        {
          ECHO;
        }
        YY_BREAK

                                                   
                                           
      case 38:
                                   
        YY_RULE_SETUP
#line 435 "psqlscanslash.l"
        {
          if (output_buf->len > 0)
          {
            ECHO;
          }
        }
        YY_BREAK
      case 39:
        YY_RULE_SETUP
#line 440 "psqlscanslash.l"
        {
          ECHO;
        }
        YY_BREAK

                                                                            
      case 40:
        YY_RULE_SETUP
#line 447 "psqlscanslash.l"
        {
          cur_state->start_state = YY_START;
          return LEXRES_OK;
        }
        YY_BREAK
      case 41:
                                   
        YY_RULE_SETUP
#line 452 "psqlscanslash.l"
        {
          yyless(0);
          cur_state->start_state = YY_START;
          return LEXRES_OK;
        }
        YY_BREAK

      case YY_STATE_EOF(INITIAL):
      case YY_STATE_EOF(xslashcmd):
      case YY_STATE_EOF(xslashargstart):
      case YY_STATE_EOF(xslasharg):
      case YY_STATE_EOF(xslashquote):
      case YY_STATE_EOF(xslashbackquote):
      case YY_STATE_EOF(xslashdquote):
      case YY_STATE_EOF(xslashwholeline):
      case YY_STATE_EOF(xslashend):
#line 460 "psqlscanslash.l"
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
      case 42:
        YY_RULE_SETUP
#line 475 "psqlscanslash.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 2013 "psqlscanslash.c"

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

      default:
        YY_FATAL_ERROR("fatal flex scanner internal error--no action found");
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

#line 475 "psqlscanslash.l"

                    

   
                                                                             
                                                                            
                                                    
   
                                                                          
                   
   
char *
psql_scan_slash_command(PsqlScanState state)
{
  PQExpBufferData mybuf;

                                
  Assert(state->scanbufhandle != NULL);

                                                          
  initPQExpBuffer(&mybuf);

                                 
  state->output_buf = &mybuf;

                        
  if (state->buffer_stack != NULL)
  {
    yy_switch_to_buffer(state->buffer_stack->buf, state->scanner);
  }
  else
  {
    yy_switch_to_buffer(state->scanbufhandle, state->scanner);
  }

     
                                                                    
                                                                 
     
  state->start_state = xslashcmd;

                
  yylex(NULL, state->scanner);

                                                         

     
                                                                             
                                
     
  psql_scan_reselect_sql_lexer(state);

  return mybuf.data;
}

   
                                                                           
                                                                   
   
                                                                        
                                                                           
            
   
                                                                          
                                               
   
                                                                             
                                                           
   
                                                                             
                                                                         
                                                                           
                                                 
   
char *
psql_scan_slash_option(PsqlScanState state, enum slash_option_type type, char *quote, bool semicolon)
{
  PQExpBufferData mybuf;
  int lexresult PG_USED_FOR_ASSERTS_ONLY;
  int final_state;
  char local_quote;

                                
  Assert(state->scanbufhandle != NULL);

  if (quote == NULL)
  {
    quote = &local_quote;
  }
  *quote = 0;

                                                          
  initPQExpBuffer(&mybuf);

                                                          
  option_type = type;
  option_quote = quote;
  unquoted_option_chars = 0;

                                 
  state->output_buf = &mybuf;

                        
  if (state->buffer_stack != NULL)
  {
    yy_switch_to_buffer(state->buffer_stack->buf, state->scanner);
  }
  else
  {
    yy_switch_to_buffer(state->scanbufhandle, state->scanner);
  }

                             
  if (type == OT_WHOLE_LINE)
  {
    state->start_state = xslashwholeline;
  }
  else
  {
    state->start_state = xslashargstart;
  }

                
  lexresult = yylex(NULL, state->scanner);

                                        
  final_state = state->start_state;

     
                                                                             
                                
     
  psql_scan_reselect_sql_lexer(state);

     
                                                                       
                                                                             
                                                                    
     
  Assert(lexresult == LEXRES_EOL || lexresult == LEXRES_OK);

  switch (final_state)
  {
  case xslashargstart:
                   
    break;
  case xslasharg:
                                                              
    if (semicolon)
    {
      while (unquoted_option_chars-- > 0 && mybuf.len > 0 && mybuf.data[mybuf.len - 1] == ';')
      {
        mybuf.data[--mybuf.len] = '\0';
      }
    }

       
                                                                     
                                                                      
       
    if (type == OT_SQLID || type == OT_SQLIDHACK)
    {
      dequote_downcase_identifier(mybuf.data, (type != OT_SQLIDHACK), state->encoding);
                                                    
      mybuf.len = strlen(mybuf.data);
    }
    break;
  case xslashquote:
  case xslashbackquote:
  case xslashdquote:
                                         
    pg_log_error("unterminated quoted string");
    termPQExpBuffer(&mybuf);
    return NULL;
  case xslashwholeline:
                     
    break;
  default:
                        
    fprintf(stderr, "invalid YY_START\n");
    exit(1);
  }

     
                                                                       
                                    
     
  if (mybuf.len == 0 && *quote == 0)
  {
    termPQExpBuffer(&mybuf);
    return NULL;
  }

                                         
  return mybuf.data;
}

   
                                                         
   
void
psql_scan_slash_command_end(PsqlScanState state)
{
                                
  Assert(state->scanbufhandle != NULL);

                                 
  state->output_buf = NULL;                               

                        
  if (state->buffer_stack != NULL)
  {
    yy_switch_to_buffer(state->buffer_stack->buf, state->scanner);
  }
  else
  {
    yy_switch_to_buffer(state->scanbufhandle, state->scanner);
  }

                             
  state->start_state = xslashend;

                
  yylex(NULL, state->scanner);

                                                         

     
                                                                       
                                             
     
  psql_scan_reselect_sql_lexer(state);
}

   
                                     
   
int
psql_scan_get_paren_depth(PsqlScanState state)
{
  return state->paren_depth;
}

   
                           
   
void
psql_scan_set_paren_depth(PsqlScanState state, int depth)
{
  Assert(depth >= 0);
  state->paren_depth = depth;
}

   
                                                      
   
                                                                   
                   
   
                                                                         
                                                                         
                                                                           
   
                                                                       
                                                                        
                                                                        
                                                                          
                                                                    
   
void
dequote_downcase_identifier(char *str, bool downcase, int encoding)
{
  bool inquotes = false;
  char *cp = str;

  while (*cp)
  {
    if (*cp == '"')
    {
      if (inquotes && cp[1] == '"')
      {
                                                     
        cp++;
      }
      else
      {
        inquotes = !inquotes;
      }
                                     
      memmove(cp, cp + 1, strlen(cp));
                             
    }
    else
    {
      if (downcase && !inquotes)
      {
        *cp = pg_tolower((unsigned char)*cp);
      }
      cp += PQmblenBounded(cp, encoding);
    }
  }
}

   
                                                                  
   
                                                                            
                                                                 
   
static void
evaluate_backtick(PsqlScanState state)
{
  PQExpBuffer output_buf = state->output_buf;
  char *cmd = output_buf->data + backtick_start_offset;
  PQExpBufferData cmd_output;
  FILE *fd;
  bool error = false;
  char buf[512];
  size_t result;

  initPQExpBuffer(&cmd_output);

  fd = popen(cmd, "r");
  if (!fd)
  {
    pg_log_error("%s: %m", cmd);
    error = true;
  }

  if (!error)
  {
    do
    {
      result = fread(buf, 1, sizeof(buf), fd);
      if (ferror(fd))
      {
        pg_log_error("%s: %m", cmd);
        error = true;
        break;
      }
      appendBinaryPQExpBuffer(&cmd_output, buf, result);
    } while (!feof(fd));
  }

  if (fd && pclose(fd) == -1)
  {
    pg_log_error("%s: %m", cmd);
    error = true;
  }

  if (PQExpBufferDataBroken(cmd_output))
  {
    pg_log_error("%s: out of memory", cmd);
    error = true;
  }

                                                    
  output_buf->len = backtick_start_offset;
  output_buf->data[output_buf->len] = '\0';

                                                  
  if (!error)
  {
                                                   
    if (cmd_output.len > 0 && cmd_output.data[cmd_output.len - 1] == '\n')
    {
      cmd_output.len--;
    }
    appendBinaryPQExpBuffer(output_buf, cmd_output.data, cmd_output.len);
  }

  termPQExpBuffer(&cmd_output);
}
