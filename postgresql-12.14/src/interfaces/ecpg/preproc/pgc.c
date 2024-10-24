#line 2 "pgc.c"
                                                                            
   
         
                              
   
                                                           
   
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>
#include <limits.h>

#include "common/string.h"

#include "preproc_extern.h"
#include "preproc.h"

#line 29 "pgc.c"

#define YY_INT_ALIGNED short int

                                         

#define yy_create_buffer base_yy_create_buffer
#define yy_delete_buffer base_yy_delete_buffer
#define yy_scan_buffer base_yy_scan_buffer
#define yy_scan_string base_yy_scan_string
#define yy_scan_bytes base_yy_scan_bytes
#define yy_init_buffer base_yy_init_buffer
#define yy_flush_buffer base_yy_flush_buffer
#define yy_load_buffer_state base_yy_load_buffer_state
#define yy_switch_to_buffer base_yy_switch_to_buffer
#define yypush_buffer_state base_yypush_buffer_state
#define yypop_buffer_state base_yypop_buffer_state
#define yyensure_buffer_stack base_yyensure_buffer_stack
#define yy_flex_debug base_yy_flex_debug
#define yyin base_yyin
#define yyleng base_yyleng
#define yylex base_yylex
#define yylineno base_yylineno
#define yyout base_yyout
#define yyrestart base_yyrestart
#define yytext base_yytext
#define yywrap base_yywrap
#define yyalloc base_yyalloc
#define yyrealloc base_yyrealloc
#define yyfree base_yyfree

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define base_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer base_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define base_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer base_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define base_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer base_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define base_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string base_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define base_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes base_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define base_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer base_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define base_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer base_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define base_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state base_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define base_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer base_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define base_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state base_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define base_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state base_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define base_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack base_yyensure_buffer_stack
#endif

#ifdef yylex
#define base_yylex_ALREADY_DEFINED
#else
#define yylex base_yylex
#endif

#ifdef yyrestart
#define base_yyrestart_ALREADY_DEFINED
#else
#define yyrestart base_yyrestart
#endif

#ifdef yylex_init
#define base_yylex_init_ALREADY_DEFINED
#else
#define yylex_init base_yylex_init
#endif

#ifdef yylex_init_extra
#define base_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra base_yylex_init_extra
#endif

#ifdef yylex_destroy
#define base_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy base_yylex_destroy
#endif

#ifdef yyget_debug
#define base_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug base_yyget_debug
#endif

#ifdef yyset_debug
#define base_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug base_yyset_debug
#endif

#ifdef yyget_extra
#define base_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra base_yyget_extra
#endif

#ifdef yyset_extra
#define base_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra base_yyset_extra
#endif

#ifdef yyget_in
#define base_yyget_in_ALREADY_DEFINED
#else
#define yyget_in base_yyget_in
#endif

#ifdef yyset_in
#define base_yyset_in_ALREADY_DEFINED
#else
#define yyset_in base_yyset_in
#endif

#ifdef yyget_out
#define base_yyget_out_ALREADY_DEFINED
#else
#define yyget_out base_yyget_out
#endif

#ifdef yyset_out
#define base_yyset_out_ALREADY_DEFINED
#else
#define yyset_out base_yyset_out
#endif

#ifdef yyget_leng
#define base_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng base_yyget_leng
#endif

#ifdef yyget_text
#define base_yyget_text_ALREADY_DEFINED
#else
#define yyget_text base_yyget_text
#endif

#ifdef yyget_lineno
#define base_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno base_yyget_lineno
#endif

#ifdef yyset_lineno
#define base_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno base_yyset_lineno
#endif

#ifdef yywrap
#define base_yywrap_ALREADY_DEFINED
#else
#define yywrap base_yywrap
#endif

#ifdef yyalloc
#define base_yyalloc_ALREADY_DEFINED
#else
#define yyalloc base_yyalloc
#endif

#ifdef yyrealloc
#define base_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc base_yyrealloc
#endif

#ifdef yyfree
#define base_yyfree_ALREADY_DEFINED
#else
#define yyfree base_yyfree
#endif

#ifdef yytext
#define base_yytext_ALREADY_DEFINED
#else
#define yytext base_yytext
#endif

#ifdef yyleng
#define base_yyleng_ALREADY_DEFINED
#else
#define yyleng base_yyleng
#endif

#ifdef yyin
#define base_yyin_ALREADY_DEFINED
#else
#define yyin base_yyin
#endif

#ifdef yyout
#define base_yyout_ALREADY_DEFINED
#else
#define yyout base_yyout
#endif

#ifdef yy_flex_debug
#define base_yy_flex_debug_ALREADY_DEFINED
#else
#define yy_flex_debug base_yy_flex_debug
#endif

#ifdef yylineno
#define base_yylineno_ALREADY_DEFINED
#else
#define yylineno base_yylineno
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

                                                                          
                                                                      
                        
   
#define BEGIN (yy_start) = 1 + 2 *
                                                                           
                                                                  
                  
   
#define YY_START (((yy_start)-1) / 2)
#define YYSTATE YY_START
                                                        
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
                                                           
#define YY_NEW_FILE yyrestart(yyin)
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

extern int yyleng;

extern FILE *yyin, *yyout;

#define EOB_ACT_CONTINUE_SCAN 0
#define EOB_ACT_END_OF_FILE 1
#define EOB_ACT_LAST_MATCH 2

                                                                                     
                                                                                        
                                                                  
                                                                                
                                                                           
                                                                           
   
#define YY_LESS_LINENO(n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int yyl;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    for (yyl = n; yyl < yyleng; ++yyl)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      if (yytext[yyl] == '\n')                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
        --yylineno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)
#define YY_LINENO_REWIND_TO(dst)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    const char *p;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    for (p = yy_cp - 1; p >= (dst); --p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      if (*p == '\n')                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
        --yylineno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

                                                                               
#define yyless(n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    int yyless_macro_arg = (n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    YY_LESS_LINENO(yyless_macro_arg);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    *yy_cp = (yy_hold_char);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    YY_RESTORE_YY_MORE_OFFSET(yy_c_buf_p) = yy_cp = yy_bp + yyless_macro_arg - YY_MORE_ADJ;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    YY_DO_BEFORE_ACTION;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)
#define unput(c) yyunput(c, (yytext_ptr))

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

                             
static size_t yy_buffer_stack_top = 0;                                        
static size_t yy_buffer_stack_max = 0;                                    
static YY_BUFFER_STATE *yy_buffer_stack = NULL;                           

                                                                
                                                             
                    
   
                                          
   
#define YY_CURRENT_BUFFER ((yy_buffer_stack) ? (yy_buffer_stack)[(yy_buffer_stack_top)] : NULL)
                                                                                
                                                          
   
#define YY_CURRENT_BUFFER_LVALUE (yy_buffer_stack)[(yy_buffer_stack_top)]

                                                                  
static char yy_hold_char;
static int yy_n_chars;                                               
int yyleng;

                                            
static char *yy_c_buf_p = NULL;
static int yy_init = 0;                                     
static int yy_start = 0;                         

                                                                
                                                            
   
static int yy_did_buffer_switch_on_eof;

void
yyrestart(FILE *input_file);
void
yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
YY_BUFFER_STATE
yy_create_buffer(FILE *file, int size);
void
yy_delete_buffer(YY_BUFFER_STATE b);
void
yy_flush_buffer(YY_BUFFER_STATE b);
void
yypush_buffer_state(YY_BUFFER_STATE new_buffer);
void
yypop_buffer_state(void);

static void
yyensure_buffer_stack(void);
static void
yy_load_buffer_state(void);
static void
yy_init_buffer(YY_BUFFER_STATE b, FILE *file);
#define YY_FLUSH_BUFFER yy_flush_buffer(YY_CURRENT_BUFFER)

YY_BUFFER_STATE
yy_scan_buffer(char *base, yy_size_t size);
YY_BUFFER_STATE
yy_scan_string(const char *yy_str);
YY_BUFFER_STATE
yy_scan_bytes(const char *bytes, int len);

void *yyalloc(yy_size_t);
void *
yyrealloc(void *, yy_size_t);
void
yyfree(void *);

#define yy_new_buffer yy_create_buffer
#define yy_set_interactive(is_interactive)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!YY_CURRENT_BUFFER)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      yyensure_buffer_stack();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    YY_CURRENT_BUFFER_LVALUE->yy_is_interactive = is_interactive;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  }
#define yy_set_bol(at_bol)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!YY_CURRENT_BUFFER)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      yyensure_buffer_stack();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    YY_CURRENT_BUFFER_LVALUE->yy_at_bol = at_bol;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  }
#define YY_AT_BOL() (YY_CURRENT_BUFFER_LVALUE->yy_at_bol)

                      

#define base_yywrap() (              1)
#define YY_SKIP_YYWRAP
typedef flex_uint8_t YY_CHAR;

FILE *yyin = NULL, *yyout = NULL;

typedef int yy_state_type;

extern int yylineno;
int yylineno = 1;

extern char *yytext;
#ifdef yytext_ptr
#undef yytext_ptr
#endif
#define yytext_ptr yytext

static yy_state_type
yy_get_previous_state(void);
static yy_state_type
yy_try_NUL_trans(yy_state_type current_state);
static int
yy_get_next_buffer(void);
static void yynoreturn
yy_fatal_error(const char *msg);

                                                                  
                                          
   
#define YY_DO_BEFORE_ACTION                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  (yytext_ptr) = yy_bp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  yyleng = (int)(yy_cp - yy_bp);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  (yy_hold_char) = *yy_cp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  *yy_cp = '\0';                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  (yy_c_buf_p) = yy_cp;
#define YY_NUM_RULES 152
#define YY_END_OF_BUFFER 153
                                            
                                    
struct yy_trans_info
{
  flex_int32_t yy_verify;
  flex_int32_t yy_nxt;
};
static const flex_int16_t yy_accept[826] = {0, 0, 0, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 153, 151, 15, 12, 8, 8, 9, 9, 9, 9, 55, 51, 57, 52, 57, 14, 19, 35, 30, 26, 36, 36, 28, 42, 26, 35, 46, 46, 48, 53, 32, 142, 142, 140, 99, 99, 140, 140, 121, 99, 99, 121, 56, 121, 81, 93, 121, 21, 97, 98, 92, 95, 91, 96, 121, 94,

    71, 71, 89, 90, 121, 104, 121, 87, 87, 102, 103, 100, 121, 101, 79, 1, 1, 68, 49, 68, 66, 67, 68, 23, 67, 67, 67, 67, 71, 67, 67, 67, 67, 78, 78, 78, 78, 78, 78, 150, 150, 150, 150, 146, 146, 145, 144, 143, 127, 127, 15, 12, 12, 13, 8, 10, 7, 4, 10, 6, 5, 55, 54, 57, 14, 19, 19, 20, 35, 30, 30, 33, 31, 26, 26, 27, 36, 28, 28, 29, 38, 39, 38, 38, 38, 34, 46, 45, 47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 141, 0,

    0, 0, 0, 0, 113, 0, 0, 0, 0, 0, 0, 118, 109, 0, 88, 116, 110, 114, 111, 115, 105, 120, 72, 3, 0, 117, 72, 71, 75, 83, 107, 112, 106, 87, 87, 108, 1, 0, 68, 65, 68, 0, 0, 43, 69, 44, 1, 59, 2, 72, 71, 58, 60, 77, 62, 64, 61, 63, 78, 11, 24, 22, 0, 18, 0, 149, 0, 0, 0, 145, 143, 0, 0, 126, 12, 17, 13, 12, 4, 5, 19, 16, 20, 19, 30, 41, 31, 30, 26, 27, 26, 28, 29, 28, 39, 0, 0, 40, 47, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 119, 3, 0, 82, 73, 72, 76, 74, 83, 87, 68, 68, 44, 1, 1, 2, 72, 71, 77, 0, 0, 0, 50, 25, 148, 147, 12, 12, 12, 12, 19, 19, 19, 19, 30, 30, 30, 30, 26, 26, 26, 26, 28, 28, 28, 28, 39, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 86, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 87, 68, 0, 72, 77, 0, 0, 0, 77, 148, 148, 147, 147, 12, 12, 12, 12, 12, 19, 19, 16, 19, 19, 30, 30, 30, 30, 30, 26, 26, 26, 26, 26, 28, 28, 28, 28, 28, 0, 0, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0, 0, 135, 0, 0, 0, 0, 0, 0, 0, 86, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 87, 68, 68, 0, 72, 77, 0, 0, 0, 0, 12, 19, 30, 26, 28, 0, 37, 0, 0,

    0, 0, 0, 0, 135, 0, 137, 0, 131, 0, 0, 0, 0, 0, 0, 86, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 125, 0, 87, 68, 0, 0, 0, 0, 0, 0, 77, 77, 0, 0, 0, 0, 0, 0, 0, 139, 131, 133, 0, 0, 0, 0, 123, 0, 125, 0, 80, 70, 0, 0, 0, 0, 0, 77, 0, 0, 0, 0, 0, 0, 133, 0, 0, 0, 84, 84, 123, 129, 80, 80, 87, 87, 87, 87, 70, 77, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    84, 84, 84, 0, 84, 0, 129, 0, 0, 0, 87, 87, 87, 87, 87, 87, 70, 77, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 87, 87, 87, 87, 87, 87, 87, 87, 0, 0, 0, 0, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 134, 0, 0, 0, 0, 0, 0, 0, 0, 0, 87, 87, 87, 87, 87, 87, 87, 87, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 41, 0, 41, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 134, 0, 136, 0, 130, 0, 0, 0, 0, 0, 0, 87, 87, 87, 87, 87, 87, 0, 53, 0, 0, 0, 0, 0, 53, 53, 0, 0, 0, 0, 0, 0, 0, 32, 32, 0, 0, 0, 0, 0, 32, 32, 0, 138, 130, 132, 85, 85, 0, 0, 124, 87, 87, 87, 124, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 41, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 132, 85, 85, 85, 85, 122, 0, 122, 87, 53, 0, 0, 0,

    0, 0, 0, 0, 0, 32, 32, 0, 128, 128, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const YY_CHAR yy_ec[256] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 2, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 21, 21, 21, 21, 21, 21, 22, 22, 23, 24, 25, 26, 27, 28, 28, 29, 30, 31, 32, 33, 34, 35, 35, 36, 35, 35, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 35, 35, 47, 35, 35, 48, 49, 50, 51, 52, 28, 29, 30, 31, 32,

    33, 34, 35, 35, 53, 35, 35, 37, 38, 39, 40, 41, 42, 43, 44, 45, 54, 35, 35, 55, 35, 35, 56, 57, 58, 28, 1, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,

    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59};

static const YY_CHAR yy_meta[60] = {0, 1, 2, 3, 3, 4, 5, 4, 6, 7, 4, 8, 9, 9, 10, 7, 1, 11, 12, 13, 14, 14, 14, 15, 16, 17, 18, 19, 4, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 12, 22, 9, 4, 21, 21, 21, 21, 1, 4, 1, 21};

static const flex_int16_t yy_base[917] = {0, 0, 0, 2033, 2029, 0, 53, 2, 4, 2029, 2028, 0, 5, 2021, 2020, 2018, 1974, 1973, 1971, 1, 26, 20, 22, 0, 27, 1969, 1961, 1951, 1950, 110, 0, 40, 168, 201, 0, 260, 0, 14, 28, 62, 81, 319, 0, 378, 0, 1956, 5117, 0, 85, 0, 0, 26, 1941, 42, 1939, 0, 1946, 5117, 5117, 41, 0, 89, 0, 92, 171, 0, 0, 175, 418, 181, 1936, 0, 0, 1937, 438, 443, 5117, 446, 5117, 5117, 5117, 423, 27, 5117, 187, 193, 1911, 5117, 23, 425, 1910, 1919, 5117, 5117, 5117, 448, 82, 5117, 426, 461, 472,

    481, 486, 5117, 5117, 1902, 1900, 1895, 0, 52, 5117, 5117, 5117, 1860, 5117, 5117, 508, 514, 1890, 5117, 532, 505, 0, 0, 5117, 5117, 1897, 502, 1898, 511, 75, 154, 1872, 1863, 0, 1871, 1868, 1866, 1866, 1863, 1847, 0, 5117, 0, 5117, 550, 5117, 5117, 0, 5117, 560, 0, 492, 569, 1853, 0, 449, 5117, 0, 466, 5117, 0, 0, 5117, 5117, 0, 573, 589, 1837, 0, 592, 599, 5117, 1835, 595, 611, 1825, 0, 615, 622, 1816, 5117, 445, 0, 0, 0, 5117, 0, 5117, 1817, 627, 1804, 1787, 632, 639, 1798, 1781, 563, 643, 5117, 451,

    1778, 1778, 650, 502, 5117, 693, 496, 621, 1776, 466, 1768, 5117, 5117, 526, 5117, 5117, 5117, 5117, 5117, 5117, 1786, 5117, 641, 0, 1792, 5117, 650, 735, 667, 0, 5117, 5117, 5117, 0, 1761, 5117, 662, 525, 0, 0, 764, 570, 573, 5117, 655, 1783, 817, 5117, 0, 742, 764, 5117, 5117, 603, 0, 0, 0, 0, 0, 5117, 5117, 5117, 537, 5117, 1766, 5117, 1772, 1750, 763, 5117, 0, 792, 796, 5117, 800, 5117, 1758, 806, 0, 0, 835, 5117, 1757, 845, 852, 5117, 1755, 855, 862, 1754, 873, 878, 1751, 881, 549, 0, 0, 0, 1741, 884,

    1703, 889, 1709, 900, 1680, 797, 1682, 549, 1677, 926, 5117, 0, 1691, 0, 688, 722, 1663, 1662, 738, 1648, 1656, 1652, 1653, 1651, 5117, 0, 1678, 5117, 5117, 785, 792, 808, 0, 1649, 977, 1649, 1642, 0, 1030, 0, 887, 892, 879, 1603, 0, 972, 5117, 5117, 920, 929, 995, 1006, 1012, 1015, 1048, 1058, 1065, 1068, 1075, 1086, 1091, 1094, 1098, 1102, 1110, 1114, 1118, 1121, 1130, 1137, 5117, 0, 0, 5117, 1140, 1146, 1149, 1156, 1598, 1165, 1168, 1195, 1204, 1213, 1592, 1587, 1586, 587, 1585, 1568, 946, 1017, 937, 1022, 1039, 1555, 1544, 1533, 1535, 1515,

    1514, 1503, 1505, 723, 1503, 1506, 1176, 1265, 1203, 1211, 1036, 1235, 1259, 1284, 1293, 1224, 5117, 1232, 5117, 1296, 1301, 1321, 1325, 1342, 1345, 1349, 1361, 1365, 1371, 1381, 1391, 1401, 1411, 1407, 1423, 1427, 1439, 1443, 1449, 1459, 1469, 1479, 1489, 1485, 0, 0, 1495, 1501, 1504, 1505, 1517, 1521, 1533, 1566, 1578, 1587, 1498, 1251, 1377, 1492, 1483, 1482, 1262, 1471, 1142, 1497, 1445, 774, 1465, 1476, 1468, 1452, 423, 1451, 1445, 1434, 726, 1436, 807, 1440, 1317, 1423, 0, 821, 1369, 1399, 1157, 1641, 1590, 1698, 1599, 1594, 1611, 1651, 1668, 1671, 0, 5117, 1752, 1417,

    1756, 1759, 1762, 1416, 1433, 1550, 5117, 1554, 1527, 1421, 1402, 1419, 880, 1409, 1778, 1782, 1785, 0, 1382, 1362, 1374, 0, 1366, 1354, 1365, 1363, 1354, 1558, 1335, 1334, 1356, 1544, 1588, 1834, 0, 0, 1341, 1170, 1891, 0, 1826, 1324, 1815, 1884, 1317, 1632, 5117, 1582, 1623, 1820, 1306, 0, 1302, 1635, 1300, 1657, 1293, 1940, 1656, 1308, 1300, 5, 1966, 0, 2024, 0, 1861, 1692, 1917, 1894, 1864, 1719, 1921, 879, 1281, 1929, 1962, 1845, 1947, 2073, 0, 1261, 882, 895, 1242, 1938, 2127, 0, 2013, 1966, 0, 1261, 2045, 2061, 2078, 0, 1256, 892, 1228, 828,

    1976, 1985, 1988, 1218, 2020, 1208, 2050, 1210, 903, 1203, 1206, 1160, 1177, 839, 1170, 1161, 5117, 0, 2185, 1170, 2081, 2188, 2205, 1154, 2099, 1130, 1129, 942, 1122, 1105, 1095, 1093, 1076, 1045, 1041, 947, 1030, 1015, 977, 1005, 979, 970, 955, 2208, 2224, 2241, 2244, 5117, 2262, 2265, 2282, 2298, 2301, 2317, 2320, 2337, 2340, 5117, 2347, 2358, 2376, 2386, 2066, 2191, 918, 917, 914, 930, 946, 1010, 889, 882, 872, 2109, 2253, 836, 817, 768, 1037, 724, 663, 2396, 2406, 2416, 2426, 2436, 2454, 2464, 2474, 2484, 2494, 2510, 2513, 2529, 2532, 2549, 0, 662, 2552, 2568,

    2585, 2588, 2607, 2624, 2634, 2645, 2655, 2665, 2677, 2118, 2402, 5117, 2412, 2121, 624, 603, 582, 566, 1053, 548, 546, 2661, 2234, 417, 167, 2304, 2687, 2693, 2697, 2709, 2713, 2725, 2758, 2770, 0, 2781, 2786, 2791, 2803, 2813, 2839, 2849, 2819, 5117, 2859, 2875, 2879, 2895, 2898, 2915, 0, 2742, 5117, 2323, 2442, 2490, 2500, 67, 45, 2516, 2535, 2578, 13, 2613, 2934, 2944, 2954, 2964, 2974, 2990, 3000, 3022, 3010, 3038, 3048, 3058, 3068, 3078, 3096, 3106, 3116, 3126, 3136, 3152, 3162, 3172, 3183, 2617, 2671, 2703, 2719, 2746, 2750, 3, 2754, 2774, 3200, 3211, 3221, 3232,

    3249, 3259, 3277, 3280, 3297, 3313, 0, 3316, 2797, 2807, 3335, 3346, 3371, 3374, 3393, 3404, 3420, 3438, 3442, 3461, 3465, 3487, 3497, 3514, 5117, 3568, 3590, 3612, 3634, 3656, 3678, 3700, 3722, 3744, 3766, 3788, 3810, 3832, 3854, 3876, 3898, 3920, 3942, 3963, 3985, 1334, 4006, 4027, 4044, 4062, 4076, 4083, 4099, 4120, 4142, 4164, 4181, 4201, 4219, 4235, 173, 473, 536, 4249, 4267, 4286, 805, 4303, 4324, 4343, 4357, 4378, 4400, 4422, 4444, 4466, 1013, 1060, 1116, 4488, 4510, 617, 4531, 4552, 4574, 4596, 4618, 4640, 1163, 1170, 4662, 4684, 4701, 1231, 1257, 4722, 4744, 1269, 4765, 1288,

    1317, 4786, 4808, 4830, 4852, 4874, 4896, 4918, 4940, 4962, 4984, 5006, 5028, 5050, 5072, 5094};

static const flex_int16_t yy_def[917] = {0, 826, 826, 827, 827, 828, 828, 6, 6, 829, 829, 830, 830, 831, 831, 832, 832, 832, 832, 833, 833, 832, 832, 834, 834, 829, 829, 832, 832, 825, 29, 835, 835, 825, 33, 825, 35, 836, 836, 837, 837, 825, 41, 825, 43, 825, 825, 838, 825, 839, 839, 825, 825, 825, 825, 840, 825, 825, 825, 825, 841, 825, 842, 825, 825, 843, 843, 825, 844, 825, 842, 845, 845, 846, 825, 825, 825, 847, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 848, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825,

    825, 825, 825, 825, 825, 825, 825, 849, 849, 825, 825, 825, 825, 825, 825, 825, 825, 850, 825, 848, 851, 850, 850, 825, 825, 850, 825, 850, 825, 852, 850, 850, 850, 853, 853, 853, 853, 853, 853, 854, 855, 825, 856, 825, 825, 825, 825, 857, 825, 858, 838, 825, 825, 825, 839, 825, 825, 859, 825, 825, 860, 840, 825, 825, 841, 825, 825, 825, 842, 825, 825, 825, 825, 825, 825, 825, 843, 825, 825, 825, 825, 825, 861, 862, 863, 825, 845, 825, 864, 825, 825, 825, 825, 825, 825, 825, 825, 847, 825, 825,

    825, 825, 825, 848, 825, 825, 206, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 865, 866, 825, 825, 825, 825, 867, 825, 825, 825, 849, 849, 825, 825, 848, 850, 850, 206, 206, 825, 825, 825, 868, 869, 825, 870, 825, 825, 825, 825, 871, 850, 850, 850, 850, 853, 825, 825, 825, 825, 825, 854, 825, 855, 856, 825, 825, 857, 825, 858, 825, 825, 825, 825, 872, 859, 860, 825, 825, 825, 873, 825, 825, 825, 874, 825, 825, 875, 825, 825, 876, 825, 877, 878, 879, 864, 880,

    825, 825, 825, 881, 825, 825, 825, 825, 825, 206, 825, 310, 310, 206, 310, 206, 206, 206, 310, 825, 825, 825, 825, 825, 825, 865, 866, 825, 825, 825, 825, 825, 867, 849, 310, 335, 868, 869, 869, 870, 825, 825, 871, 825, 882, 883, 825, 825, 825, 825, 884, 872, 872, 872, 885, 873, 873, 873, 886, 874, 874, 874, 887, 875, 875, 875, 888, 876, 876, 876, 825, 889, 890, 825, 880, 880, 880, 880, 825, 891, 881, 881, 881, 881, 825, 825, 825, 825, 825, 825, 825, 892, 310, 310, 310, 206, 206, 206, 206, 310,

    310, 825, 825, 825, 825, 825, 849, 892, 825, 825, 893, 883, 883, 883, 883, 825, 825, 825, 825, 884, 884, 884, 884, 872, 885, 885, 885, 885, 873, 886, 886, 886, 886, 874, 887, 887, 887, 887, 875, 888, 888, 888, 888, 876, 894, 895, 880, 880, 825, 891, 891, 891, 891, 891, 881, 881, 825, 825, 825, 825, 825, 825, 825, 825, 892, 896, 892, 310, 892, 206, 206, 206, 206, 310, 310, 825, 825, 825, 825, 825, 825, 849, 408, 335, 825, 825, 893, 897, 883, 825, 883, 884, 885, 886, 887, 888, 898, 825, 880, 825,

    891, 891, 881, 825, 825, 825, 825, 825, 825, 825, 825, 896, 825, 310, 892, 892, 892, 206, 206, 206, 206, 310, 310, 825, 825, 825, 825, 825, 825, 849, 335, 825, 825, 897, 534, 534, 534, 534, 899, 900, 880, 825, 891, 881, 825, 825, 825, 825, 825, 825, 206, 310, 825, 825, 825, 825, 825, 849, 825, 825, 534, 534, 534, 534, 899, 901, 880, 825, 891, 881, 825, 825, 825, 825, 825, 206, 825, 825, 825, 825, 580, 849, 849, 849, 849, 825, 825, 895, 880, 825, 902, 825, 891, 881, 825, 903, 825, 825, 825, 825,

    310, 825, 310, 310, 825, 825, 825, 825, 825, 825, 849, 849, 849, 849, 849, 849, 825, 587, 880, 825, 904, 891, 881, 825, 905, 825, 825, 825, 825, 825, 310, 825, 825, 825, 825, 849, 849, 849, 849, 849, 849, 849, 849, 880, 825, 906, 880, 825, 904, 904, 907, 904, 891, 881, 825, 908, 881, 825, 905, 905, 909, 905, 825, 825, 825, 825, 825, 310, 825, 825, 825, 825, 849, 849, 849, 849, 849, 849, 849, 849, 825, 880, 880, 825, 880, 880, 910, 904, 904, 825, 904, 904, 891, 825, 911, 891, 903, 825, 881, 881,

    825, 881, 881, 912, 905, 905, 825, 905, 905, 825, 825, 825, 825, 825, 825, 310, 825, 825, 825, 825, 849, 849, 849, 849, 849, 849, 910, 880, 910, 910, 913, 910, 910, 907, 902, 891, 891, 825, 891, 891, 914, 914, 881, 825, 912, 912, 915, 912, 912, 909, 903, 825, 825, 825, 825, 310, 825, 825, 825, 825, 849, 849, 849, 825, 910, 910, 910, 825, 910, 910, 910, 910, 891, 914, 914, 916, 914, 914, 914, 912, 912, 912, 825, 912, 912, 912, 912, 825, 310, 825, 310, 825, 825, 825, 825, 849, 913, 910, 914, 914,

    825, 914, 914, 914, 914, 915, 903, 912, 825, 825, 910, 916, 914, 912, 910, 914, 912, 910, 914, 912, 910, 914, 912, 914, 0, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825,

    825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825};

static const flex_int16_t yy_nxt[5177] = {0, 825, 825, 72, 66, 50, 58, 50, 73, 50, 50, 58, 67, 825, 51, 50, 53, 50, 53, 52, 141, 54, 537, 54, 825, 50, 50, 50, 50, 66, 72, 69, 563, 69, 141, 73, 809, 67, 142, 143, 156, 142, 79, 80, 79, 157, 796, 164, 81, 59, 68, 50, 142, 143, 59, 142, 159, 50, 50, 207, 50, 160, 50, 50, 145, 145, 145, 51, 50, 70, 50, 70, 52, 82, 202, 68, 208, 794, 50, 50, 50, 50, 202, 145, 145, 145, 146, 152, 153, 153, 164, 166, 167, 167, 170, 171, 171, 217, 252, 235, 793,

    253, 154, 172, 50, 146, 168, 235, 218, 173, 50, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 76, 76, 76, 76, 77, 77, 77, 77, 76, 76, 76, 77, 79, 80, 79, 174, 175, 175, 81, 178, 179, 179, 255, 256, 172, 174, 175, 175, 172, 296, 176, 203, 203, 203, 180, 296, 204, 203, 203, 203, 176, 763, 204,

    82, 83, 84, 85, 84, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 102, 103, 104, 105, 106, 107, 83, 108, 108, 108, 108, 109, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 110, 83, 111, 83, 108, 108, 108, 108, 112, 113, 114, 108, 115, 116, 117, 116, 118, 119, 120, 121, 122, 123, 124, 125, 125, 122, 122, 125, 126, 127, 128, 129, 129, 129, 130, 125, 131, 132, 133, 123, 134, 135, 134, 134, 136, 134, 134, 134, 134, 134, 137, 134,

    134, 134, 134, 134, 134, 138, 139, 125, 115, 125, 122, 134, 134, 138, 139, 115, 123, 115, 134, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 148, 148, 148, 148, 147, 147, 147, 148, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,

    149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 149, 149, 149, 149, 150, 150, 150, 150, 149, 149, 149, 150, 182, 182, 190, 190, 190, 219, 163, 193, 194, 194, 197, 197, 197, 762, 220, 221, 172, 191, 200, 209, 200, 201, 195, 210, 214, 156, 183, 295, 295, 215, 157, 521, 199, 211, 184, 185, 216, 222, 201, 521, 210, 211, 159, 223, 223, 223, 192, 160, 224, 297, 306, 196, 307, 225, 192, 297, 152, 153, 153, 196, 226, 227, 308,

    228, 228, 228, 227, 323, 228, 228, 228, 154, 237, 237, 237, 244, 229, 238, 237, 237, 237, 229, 248, 238, 223, 223, 223, 245, 245, 245, 230, 250, 316, 251, 251, 251, 317, 318, 230, 241, 207, 241, 214, 241, 241, 347, 229, 215, 241, 241, 348, 241, 298, 241, 269, 269, 269, 208, 298, 241, 241, 241, 241, 242, 272, 272, 272, 197, 197, 197, 242, 371, 371, 275, 275, 275, 270, 166, 167, 167, 243, 761, 276, 389, 760, 241, 274, 243, 277, 199, 390, 241, 168, 281, 281, 281, 170, 171, 171, 174, 175, 175, 282,

    285, 285, 285, 316, 758, 283, 319, 317, 173, 286, 320, 176, 289, 289, 289, 287, 178, 179, 179, 344, 345, 286, 460, 292, 292, 292, 757, 290, 190, 190, 190, 180, 286, 193, 194, 194, 411, 411, 293, 460, 302, 302, 302, 191, 197, 197, 197, 756, 195, 286, 346, 203, 203, 203, 319, 303, 204, 755, 320, 321, 223, 223, 223, 237, 237, 237, 199, 329, 238, 330, 330, 330, 192, 229, 245, 245, 245, 196, 742, 727, 192, 331, 229, 331, 196, 196, 332, 332, 332, 393, 394, 395, 196, 310, 310, 311, 312, 310, 310, 310,

    310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 313, 310, 310, 310, 310, 310, 310, 310, 310, 310, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 310, 315, 310, 310, 310, 314, 314, 314, 310, 310, 310, 310, 227, 396, 228, 228, 228, 726, 478, 329, 397, 341, 341, 341, 269, 269, 269, 229, 335, 400, 335, 525, 335, 335, 229, 478, 401, 335, 335, 525, 335, 250, 336, 342, 342, 342, 270, 468, 335, 335, 335, 335, 514, 272, 272, 272, 229, 272, 272, 272,

    724, 275, 275, 275, 330, 330, 330, 353, 153, 153, 276, 332, 332, 332, 335, 274, 277, 229, 333, 274, 335, 339, 354, 339, 333, 339, 339, 332, 332, 332, 339, 339, 386, 339, 484, 339, 281, 281, 281, 531, 387, 339, 339, 339, 339, 282, 357, 167, 167, 386, 723, 283, 527, 285, 285, 285, 361, 171, 171, 629, 527, 358, 286, 289, 289, 289, 630, 339, 287, 722, 640, 362, 286, 339, 365, 175, 175, 641, 290, 292, 292, 292, 369, 179, 179, 376, 190, 190, 286, 366, 302, 302, 302, 513, 293, 344, 345, 370, 310, 286,

    377, 382, 194, 194, 409, 303, 410, 410, 410, 250, 721, 228, 228, 228, 720, 598, 383, 599, 612, 229, 613, 416, 416, 416, 229, 719, 346, 626, 614, 378, 418, 418, 418, 615, 196, 627, 600, 378, 393, 394, 395, 634, 196, 417, 626, 384, 715, 463, 463, 463, 714, 713, 419, 384, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 412, 412, 412, 716, 665, 310, 310, 310, 335, 673, 335, 716, 335, 335, 680, 414, 464, 335, 335, 717, 335, 665, 336, 421, 275, 275, 673,

    717, 335, 335, 335, 335, 422, 679, 353, 153, 153, 678, 423, 676, 353, 153, 153, 353, 153, 153, 466, 467, 415, 354, 393, 394, 395, 372, 335, 354, 676, 468, 424, 372, 335, 339, 313, 339, 677, 339, 339, 393, 394, 395, 339, 339, 718, 339, 675, 339, 426, 281, 281, 344, 345, 339, 339, 339, 339, 427, 357, 167, 167, 718, 674, 428, 469, 357, 167, 167, 357, 167, 167, 672, 373, 358, 671, 431, 285, 285, 373, 339, 358, 725, 346, 429, 432, 339, 361, 171, 171, 725, 433, 361, 171, 171, 361, 171, 171, 759, 436,

    289, 289, 362, 365, 175, 175, 759, 362, 437, 670, 434, 365, 175, 175, 438, 365, 175, 175, 366, 441, 292, 292, 369, 179, 179, 669, 366, 668, 442, 374, 439, 369, 179, 179, 443, 374, 667, 370, 369, 179, 179, 376, 190, 190, 466, 467, 370, 376, 190, 190, 376, 190, 190, 444, 666, 468, 377, 376, 190, 190, 313, 664, 377, 663, 658, 447, 451, 302, 302, 382, 194, 194, 377, 344, 345, 452, 445, 481, 481, 481, 648, 453, 445, 446, 383, 378, 562, 563, 448, 446, 469, 378, 643, 378, 378, 637, 382, 194, 194, 378,

    642, 378, 378, 638, 346, 382, 194, 194, 639, 378, 454, 383, 637, 384, 382, 194, 194, 564, 454, 482, 455, 384, 485, 485, 485, 416, 416, 416, 409, 383, 486, 486, 486, 418, 418, 418, 412, 412, 412, 636, 384, 635, 633, 229, 497, 456, 632, 417, 384, 384, 497, 414, 505, 505, 505, 419, 631, 384, 384, 628, 412, 412, 412, 463, 463, 463, 384, 466, 467, 483, 498, 483, 625, 483, 483, 414, 498, 621, 484, 483, 616, 483, 540, 336, 415, 412, 412, 412, 540, 483, 483, 483, 483, 611, 412, 412, 412, 421, 275, 275,

    488, 566, 421, 275, 275, 464, 422, 566, 415, 489, 490, 422, 423, 469, 600, 483, 537, 423, 481, 481, 481, 483, 421, 275, 275, 532, 421, 275, 275, 580, 588, 422, 579, 415, 577, 422, 588, 423, 576, 189, 491, 492, 415, 353, 153, 153, 426, 281, 281, 571, 426, 281, 281, 189, 189, 427, 568, 537, 424, 427, 529, 428, 426, 281, 281, 428, 426, 281, 281, 408, 558, 427, 357, 167, 167, 427, 557, 428, 506, 506, 506, 493, 431, 285, 285, 555, 532, 429, 533, 533, 533, 432, 431, 285, 285, 554, 553, 433, 552, 522,

    507, 432, 431, 285, 285, 551, 518, 433, 361, 171, 171, 432, 431, 285, 285, 518, 409, 433, 330, 330, 330, 432, 392, 434, 436, 289, 289, 494, 436, 289, 289, 229, 513, 437, 505, 505, 505, 437, 550, 438, 436, 289, 289, 438, 436, 289, 289, 466, 467, 437, 365, 175, 175, 437, 549, 438, 545, 542, 468, 495, 441, 292, 292, 313, 530, 439, 515, 516, 517, 442, 441, 292, 292, 528, 526, 443, 524, 523, 468, 442, 441, 292, 292, 313, 522, 443, 369, 179, 179, 442, 441, 292, 292, 469, 520, 443, 376, 190, 190, 442,

    519, 444, 376, 190, 190, 496, 451, 302, 302, 518, 513, 447, 511, 469, 510, 452, 509, 377, 451, 302, 302, 453, 451, 302, 302, 508, 504, 452, 548, 548, 548, 452, 500, 453, 451, 302, 302, 453, 480, 479, 378, 477, 476, 452, 499, 475, 378, 474, 378, 501, 454, 506, 506, 506, 378, 546, 546, 546, 454, 556, 556, 556, 454, 559, 559, 559, 454, 451, 302, 302, 454, 473, 472, 507, 454, 471, 452, 547, 454, 382, 194, 194, 453, 548, 548, 548, 454, 470, 382, 194, 194, 412, 412, 412, 455, 421, 275, 275, 502, 462,

    412, 412, 412, 383, 422, 532, 488, 560, 560, 560, 492, 454, 426, 281, 281, 414, 490, 461, 459, 454, 458, 427, 457, 384, 572, 572, 572, 493, 449, 345, 503, 384, 384, 546, 546, 546, 578, 578, 578, 415, 384, 534, 535, 412, 412, 534, 534, 534, 415, 244, 534, 534, 431, 285, 285, 547, 534, 537, 556, 556, 556, 432, 408, 534, 534, 534, 534, 494, 534, 436, 289, 289, 441, 292, 292, 586, 586, 586, 437, 407, 328, 442, 406, 405, 495, 404, 403, 496, 402, 534, 538, 534, 399, 590, 590, 590, 534, 534, 534, 412,

    412, 412, 591, 398, 392, 413, 413, 391, 592, 413, 413, 413, 413, 388, 414, 413, 413, 413, 413, 413, 572, 572, 572, 385, 413, 380, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 413, 379, 415, 188, 539, 539, 539, 539, 376, 190, 190, 539, 451, 302, 302, 451, 302, 302, 382, 194, 194, 452, 367, 377, 452, 363, 359, 501, 355, 351, 453, 350, 349, 383, 515, 516, 517, 541, 515, 516, 517, 515, 516, 517, 266, 244, 468, 544, 334, 328, 468, 313, 378, 468, 325,

    313, 454, 543, 313, 454, 378, 324, 384, 322, 454, 309, 308, 454, 305, 304, 384, 451, 302, 302, 301, 300, 573, 573, 573, 188, 452, 469, 376, 190, 190, 469, 453, 294, 469, 534, 535, 412, 412, 534, 534, 534, 291, 377, 534, 534, 569, 578, 578, 578, 534, 561, 288, 574, 284, 567, 575, 534, 534, 534, 534, 454, 534, 376, 190, 190, 595, 595, 595, 454, 278, 266, 378, 575, 264, 596, 263, 262, 377, 261, 378, 597, 260, 534, 538, 534, 382, 194, 194, 258, 534, 534, 534, 412, 412, 412, 382, 194, 194, 257, 413,

    383, 589, 413, 413, 413, 413, 378, 489, 490, 413, 383, 249, 570, 247, 378, 240, 236, 413, 451, 302, 302, 233, 573, 573, 573, 232, 231, 452, 213, 384, 601, 602, 603, 453, 594, 212, 205, 384, 491, 384, 415, 581, 581, 581, 188, 593, 186, 384, 607, 607, 607, 163, 161, 574, 158, 825, 575, 617, 617, 617, 75, 75, 454, 605, 605, 605, 74, 590, 590, 590, 454, 582, 583, 575, 74, 584, 591, 601, 602, 603, 604, 64, 592, 64, 63, 585, 605, 605, 605, 601, 602, 603, 584, 585, 587, 587, 587, 587, 587, 587,

    587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 606, 376, 190, 190, 587, 587, 587, 587, 605, 605, 605, 587, 412, 412, 412, 63, 377, 61, 61, 413, 56, 56, 413, 413, 413, 413, 48, 489, 490, 413, 48, 825, 619, 451, 302, 302, 825, 413, 607, 607, 607, 825, 452, 825, 825, 378, 825, 825, 453, 382, 194, 194, 825, 378, 710, 710, 710, 825, 491, 825, 415, 581, 581, 581, 383, 825, 595, 595, 595, 650, 590, 590, 622, 825, 825, 596, 825, 454, 651, 825, 623, 597, 825, 825, 652, 454, 825,

    660, 595, 595, 825, 608, 574, 384, 825, 609, 661, 710, 710, 710, 825, 384, 662, 825, 825, 610, 710, 710, 710, 754, 754, 754, 609, 610, 534, 535, 412, 412, 534, 534, 534, 618, 536, 534, 534, 536, 536, 536, 536, 534, 562, 563, 536, 618, 618, 618, 534, 534, 534, 534, 536, 534, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 618, 564, 534, 538, 534, 618, 618, 618, 618, 534, 534, 534, 618, 644, 645, 645, 451, 302, 302, 711, 711, 711, 646, 825, 825, 452, 825,

    825, 647, 825, 825, 453, 825, 654, 655, 655, 644, 645, 645, 825, 825, 712, 656, 825, 825, 646, 825, 653, 657, 825, 825, 647, 645, 645, 645, 825, 825, 378, 825, 825, 454, 591, 754, 754, 754, 378, 825, 681, 454, 683, 684, 684, 376, 190, 190, 825, 825, 384, 375, 825, 378, 711, 711, 711, 685, 384, 825, 687, 378, 825, 650, 590, 590, 650, 590, 590, 192, 825, 825, 651, 825, 825, 651, 712, 192, 652, 825, 825, 652, 825, 689, 690, 690, 686, 825, 825, 378, 825, 825, 651, 825, 686, 825, 825, 378, 691, 650,

    590, 590, 693, 694, 694, 764, 764, 764, 651, 825, 825, 695, 825, 825, 692, 825, 825, 696, 654, 655, 655, 694, 694, 694, 754, 754, 754, 656, 825, 825, 697, 825, 825, 657, 825, 825, 698, 825, 700, 701, 701, 382, 194, 194, 825, 825, 454, 381, 660, 595, 595, 825, 825, 702, 454, 825, 704, 661, 825, 660, 595, 595, 384, 662, 825, 196, 825, 825, 661, 825, 384, 825, 825, 196, 662, 825, 825, 706, 707, 707, 825, 825, 703, 825, 825, 384, 661, 660, 595, 595, 703, 825, 708, 384, 825, 825, 661, 376, 190, 190,

    825, 825, 709, 711, 711, 711, 728, 376, 190, 190, 825, 825, 377, 752, 752, 752, 728, 190, 190, 190, 825, 825, 377, 825, 825, 712, 648, 376, 190, 190, 825, 825, 191, 825, 825, 753, 728, 376, 190, 190, 825, 378, 447, 788, 788, 788, 728, 825, 825, 378, 825, 378, 377, 825, 825, 730, 645, 645, 825, 378, 825, 192, 825, 825, 731, 650, 590, 590, 448, 192, 732, 378, 825, 825, 734, 650, 590, 590, 825, 378, 652, 378, 825, 825, 734, 590, 590, 590, 825, 378, 652, 789, 790, 791, 735, 650, 590, 590, 825, 733,

    592, 792, 792, 792, 734, 825, 825, 733, 825, 825, 692, 650, 590, 590, 693, 694, 694, 764, 764, 764, 651, 825, 825, 695, 825, 825, 692, 825, 825, 696, 694, 694, 694, 737, 738, 738, 795, 795, 795, 697, 825, 825, 452, 825, 825, 698, 825, 825, 739, 825, 451, 302, 302, 382, 194, 194, 825, 825, 454, 452, 825, 825, 743, 825, 825, 741, 454, 825, 383, 382, 194, 194, 825, 825, 196, 825, 825, 740, 743, 788, 788, 788, 196, 825, 383, 740, 302, 302, 302, 382, 194, 194, 825, 825, 454, 744, 825, 384, 743, 825,

    825, 303, 454, 825, 455, 384, 825, 825, 382, 194, 194, 825, 825, 384, 764, 764, 764, 743, 788, 788, 788, 384, 825, 383, 825, 746, 655, 655, 825, 825, 196, 825, 825, 384, 747, 660, 595, 595, 196, 456, 748, 384, 825, 825, 750, 825, 660, 595, 595, 825, 662, 825, 384, 825, 825, 750, 595, 595, 595, 825, 384, 662, 752, 752, 752, 751, 660, 595, 595, 749, 825, 597, 789, 790, 791, 750, 825, 749, 660, 595, 595, 709, 825, 825, 753, 825, 825, 661, 730, 645, 645, 825, 825, 709, 376, 190, 190, 731, 730, 645,

    645, 825, 825, 765, 792, 792, 792, 731, 825, 377, 730, 645, 645, 765, 767, 768, 768, 825, 825, 731, 789, 790, 791, 731, 825, 765, 730, 645, 645, 769, 825, 825, 733, 825, 825, 731, 825, 825, 378, 825, 733, 771, 733, 752, 752, 752, 378, 792, 792, 792, 733, 795, 795, 795, 733, 795, 795, 795, 770, 730, 645, 645, 733, 825, 825, 753, 770, 825, 731, 825, 733, 689, 690, 690, 765, 810, 810, 810, 733, 825, 651, 825, 451, 302, 302, 825, 691, 451, 302, 302, 772, 773, 302, 302, 302, 825, 773, 453, 810, 810,

    810, 744, 453, 733, 451, 302, 302, 303, 810, 810, 810, 733, 825, 773, 451, 302, 302, 825, 825, 501, 382, 194, 194, 773, 825, 825, 454, 825, 825, 453, 825, 454, 825, 825, 454, 383, 196, 825, 825, 454, 775, 694, 694, 825, 196, 502, 825, 825, 454, 776, 775, 694, 694, 825, 825, 777, 454, 825, 454, 776, 746, 655, 655, 825, 384, 779, 454, 825, 825, 747, 825, 825, 384, 825, 825, 780, 746, 655, 655, 825, 782, 783, 783, 825, 778, 747, 825, 825, 825, 747, 825, 780, 778, 825, 778, 784, 746, 655, 655, 746,

    655, 655, 778, 825, 749, 747, 825, 825, 747, 825, 825, 786, 749, 825, 780, 825, 706, 707, 707, 825, 749, 825, 825, 825, 785, 661, 825, 825, 749, 825, 787, 708, 785, 825, 825, 730, 645, 645, 825, 825, 749, 825, 825, 749, 731, 730, 645, 645, 749, 825, 771, 749, 825, 825, 797, 730, 645, 645, 825, 825, 765, 825, 825, 825, 797, 645, 645, 645, 825, 825, 765, 825, 825, 825, 735, 730, 645, 645, 825, 733, 681, 825, 825, 825, 797, 825, 825, 733, 825, 733, 771, 730, 645, 645, 825, 825, 825, 733, 825, 733,

    797, 730, 645, 645, 825, 825, 765, 733, 825, 192, 731, 451, 302, 302, 825, 825, 771, 192, 825, 733, 452, 825, 772, 730, 645, 645, 453, 733, 825, 825, 825, 825, 731, 825, 825, 733, 825, 825, 765, 775, 694, 694, 825, 733, 825, 733, 825, 825, 776, 775, 694, 694, 825, 733, 779, 454, 825, 825, 776, 800, 801, 801, 825, 454, 779, 798, 825, 733, 776, 775, 694, 694, 825, 825, 802, 733, 825, 825, 776, 775, 694, 694, 825, 778, 804, 825, 825, 825, 776, 825, 825, 778, 825, 778, 779, 825, 825, 775, 694, 694,

    825, 778, 825, 803, 825, 825, 776, 746, 655, 655, 805, 803, 804, 778, 825, 825, 747, 746, 655, 655, 825, 778, 786, 778, 825, 825, 806, 746, 655, 655, 825, 778, 780, 825, 825, 825, 806, 694, 694, 694, 825, 778, 780, 825, 825, 825, 807, 825, 825, 778, 825, 749, 698, 746, 655, 655, 825, 825, 825, 749, 825, 749, 806, 746, 655, 655, 825, 825, 786, 749, 825, 749, 806, 746, 655, 655, 825, 825, 780, 749, 825, 196, 747, 825, 746, 655, 655, 825, 786, 196, 825, 825, 825, 747, 787, 825, 825, 749, 825, 780,

    825, 767, 768, 768, 825, 749, 825, 749, 825, 825, 731, 825, 730, 645, 645, 749, 769, 749, 825, 825, 825, 731, 775, 694, 694, 749, 808, 765, 749, 825, 825, 812, 825, 775, 694, 694, 749, 779, 825, 825, 825, 811, 812, 825, 825, 770, 825, 825, 779, 825, 694, 694, 694, 770, 825, 825, 733, 825, 825, 807, 775, 694, 694, 825, 733, 698, 778, 825, 825, 812, 825, 825, 825, 825, 778, 804, 825, 778, 775, 694, 694, 775, 694, 694, 825, 778, 825, 812, 825, 825, 776, 825, 825, 779, 196, 825, 804, 825, 775, 694,

    694, 825, 196, 825, 778, 825, 825, 776, 825, 805, 825, 825, 778, 779, 782, 783, 783, 746, 655, 655, 825, 825, 778, 747, 825, 778, 747, 825, 825, 784, 778, 825, 780, 778, 825, 825, 730, 645, 645, 825, 813, 825, 778, 825, 825, 731, 814, 800, 801, 801, 778, 765, 825, 825, 825, 825, 776, 825, 785, 825, 825, 749, 802, 815, 825, 825, 785, 825, 825, 749, 825, 825, 775, 694, 694, 746, 655, 655, 825, 825, 733, 776, 825, 825, 747, 825, 825, 779, 733, 825, 780, 803, 825, 825, 730, 645, 645, 825, 825, 803,

    825, 816, 817, 731, 825, 775, 694, 694, 825, 765, 825, 825, 825, 825, 776, 825, 778, 825, 825, 749, 779, 746, 655, 655, 778, 825, 825, 749, 825, 825, 747, 825, 819, 818, 825, 825, 780, 825, 733, 730, 645, 645, 825, 775, 694, 694, 733, 825, 731, 778, 825, 825, 776, 825, 765, 825, 825, 778, 779, 825, 820, 825, 746, 655, 655, 749, 730, 645, 645, 825, 821, 747, 825, 749, 825, 731, 825, 780, 825, 825, 825, 765, 822, 733, 825, 825, 825, 778, 775, 694, 694, 733, 825, 823, 825, 778, 825, 776, 746, 655,

    655, 825, 825, 779, 825, 825, 749, 747, 825, 825, 733, 825, 825, 780, 749, 775, 694, 694, 733, 824, 825, 825, 825, 825, 776, 825, 825, 825, 825, 825, 779, 825, 778, 825, 825, 825, 825, 825, 825, 825, 778, 825, 749, 825, 825, 825, 825, 825, 825, 825, 749, 825, 825, 825, 825, 825, 825, 825, 825, 778, 825, 825, 825, 825, 825, 825, 825, 778, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,

    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,

    62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140,

    140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 151, 151, 151, 151, 151, 151, 151, 825, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 155, 155, 155, 155, 155, 155, 155, 155, 155, 825, 155, 155, 825, 155, 155, 155, 155, 155, 155, 155, 155, 155, 162, 162, 162, 162, 825, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 165, 165,

    165, 165, 165, 165, 165, 825, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 169, 169, 169, 169, 169, 169, 169, 825, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 177, 177, 177, 177, 177, 177, 177, 825, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 187, 187, 187, 187, 187, 825, 187, 187, 187, 187, 187, 187, 187, 187, 187,

    187, 187, 187, 187, 187, 187, 187, 198, 198, 825, 825, 198, 825, 825, 825, 825, 825, 825, 825, 198, 825, 198, 825, 825, 825, 198, 198, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 234, 825, 825, 825, 825, 825, 825, 825, 234, 825, 825, 825, 825, 825, 234, 234, 239, 825, 825, 239, 825, 825, 239, 239, 825, 239, 825, 825, 825, 239, 239, 239, 246, 825, 825, 825, 825, 825, 825, 825, 246, 825, 825, 825, 825, 825, 246, 246, 254, 825, 825,

    254, 825, 254, 254, 259, 825, 825, 825, 825, 825, 825, 825, 259, 825, 825, 825, 825, 825, 259, 259, 265, 265, 265, 265, 825, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 825, 265, 825, 265, 265, 265, 267, 267, 267, 267, 825, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 267, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 268, 825, 268, 268, 268, 271, 825, 825, 825, 825, 825, 825, 825, 271, 825, 825, 825, 825, 825,

    271, 271, 273, 273, 825, 825, 273, 825, 825, 825, 825, 825, 825, 825, 273, 825, 273, 825, 825, 825, 273, 273, 279, 825, 825, 279, 825, 825, 279, 279, 825, 279, 825, 825, 825, 279, 279, 279, 280, 825, 825, 280, 825, 825, 280, 280, 825, 280, 825, 825, 825, 280, 280, 280, 299, 825, 825, 825, 825, 825, 825, 825, 299, 825, 825, 825, 825, 825, 299, 299, 326, 825, 825, 326, 825, 825, 326, 326, 825, 326, 825, 825, 825, 326, 326, 326, 327, 327, 327, 327, 327, 327, 327, 327, 327, 327, 327, 327, 327, 327,

    327, 327, 327, 327, 327, 327, 327, 327, 337, 825, 825, 825, 825, 825, 825, 825, 337, 825, 825, 825, 825, 825, 337, 337, 338, 338, 825, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 338, 340, 825, 825, 340, 825, 825, 340, 340, 825, 340, 825, 825, 825, 340, 340, 340, 343, 825, 825, 825, 825, 343, 343, 825, 343, 825, 825, 825, 825, 825, 343, 343, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352, 352,

    356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 356, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 364, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 368, 375, 375, 375, 375, 375, 375, 375, 375, 375, 375, 375, 375,

    375, 375, 375, 375, 375, 375, 375, 375, 375, 375, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 381, 413, 413, 825, 825, 413, 413, 825, 413, 413, 413, 413, 413, 413, 825, 825, 825, 825, 413, 413, 413, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 425, 430, 430, 430, 430,

    430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 430, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 435, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 450, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465, 465,

    465, 465, 465, 465, 465, 465, 487, 825, 825, 825, 825, 487, 487, 825, 487, 825, 825, 825, 825, 825, 487, 487, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 825, 512, 512, 512, 512, 512, 512, 512, 512, 512, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 536, 565, 565, 825, 825, 565, 565, 825, 565, 565, 565, 565, 565, 565, 825, 825, 825, 825, 565, 565, 565, 620, 620, 620, 620, 620, 620, 620, 825, 620, 620, 620, 620, 620, 620,

    620, 620, 620, 620, 620, 620, 620, 620, 624, 624, 624, 624, 624, 624, 624, 825, 624, 624, 624, 624, 624, 624, 624, 624, 624, 624, 624, 624, 624, 624, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 649, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 659, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 682, 688, 688, 688, 688,

    688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 688, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 699, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 705, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 729, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736, 736,

    736, 736, 736, 736, 736, 736, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 745, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 766, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 774, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 781, 799, 799, 799, 799, 799, 799,

    799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 799, 45, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825};

static const flex_int16_t yy_chk[5177] = {0, 0, 0, 23, 19, 5, 11, 5, 23, 5, 5, 12, 19, 0, 5, 5, 7, 5, 8, 5, 37, 7, 562, 8, 0, 5, 5, 5, 5, 20, 24, 21, 562, 22, 38, 24, 794, 20, 37, 37, 51, 37, 31, 31, 31, 51, 763, 59, 31, 11, 19, 5, 38, 38, 12, 38, 53, 5, 6, 88, 6, 53, 6, 6, 39, 39, 39, 6, 6, 21, 6, 22, 6, 31, 82, 20, 88, 759, 6, 6, 6, 6, 82, 40, 40, 40, 39, 48, 48, 48, 59, 61, 61, 61, 63, 63, 63, 96, 130, 109, 758,

    130, 48, 63, 6, 40, 61, 109, 96, 63, 6, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 32, 32, 32, 64, 64, 64, 32, 67, 67, 67, 131, 131, 64, 69, 69, 69, 67, 861, 64, 84, 84, 84, 67, 861, 84, 85, 85, 85, 69, 725, 85,

    32, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,

    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,

    43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 68, 68, 74, 74, 74, 98, 74, 75, 75, 75, 77, 77, 77, 724, 98, 98, 75, 74, 81, 89, 89, 81, 75, 89, 95, 156, 68, 182, 182, 95, 156, 473, 77, 89, 68, 68, 95, 99, 81, 473, 89, 89, 159, 99, 99, 99, 74, 159, 100, 862, 200, 75, 200, 100, 74, 862, 152, 152, 152, 75, 100, 101, 210,

    101, 101, 101, 102, 210, 102, 102, 102, 152, 116, 116, 116, 121, 101, 116, 117, 117, 117, 102, 127, 117, 127, 127, 127, 121, 121, 121, 101, 129, 207, 129, 129, 129, 207, 207, 101, 120, 204, 120, 214, 120, 120, 263, 129, 214, 120, 120, 263, 120, 863, 120, 145, 145, 145, 204, 863, 120, 120, 120, 120, 238, 150, 150, 150, 197, 197, 197, 120, 295, 295, 153, 153, 153, 145, 166, 166, 166, 238, 721, 153, 308, 720, 120, 150, 120, 153, 197, 308, 120, 166, 167, 167, 167, 170, 170, 170, 174, 174, 174, 167,

    171, 171, 171, 242, 718, 167, 243, 242, 170, 171, 243, 174, 175, 175, 175, 171, 178, 178, 178, 254, 254, 175, 388, 179, 179, 179, 717, 175, 190, 190, 190, 178, 179, 193, 193, 193, 882, 882, 179, 388, 194, 194, 194, 190, 198, 198, 198, 716, 193, 194, 254, 203, 203, 203, 208, 194, 203, 715, 208, 208, 223, 223, 223, 237, 237, 237, 198, 227, 237, 227, 227, 227, 190, 223, 245, 245, 245, 193, 698, 681, 190, 229, 227, 229, 194, 193, 229, 229, 229, 315, 315, 315, 194, 206, 206, 206, 206, 206, 206, 206,

    206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 228, 316, 228, 228, 228, 680, 404, 250, 316, 250, 250, 250, 269, 269, 269, 228, 241, 319, 241, 477, 241, 241, 250, 404, 319, 241, 241, 477, 241, 251, 241, 251, 251, 251, 269, 468, 241, 241, 241, 241, 468, 272, 272, 272, 251, 273, 273, 273,

    678, 275, 275, 275, 330, 330, 330, 278, 278, 278, 275, 331, 331, 331, 241, 272, 275, 330, 867, 273, 241, 247, 278, 247, 867, 247, 247, 332, 332, 332, 247, 247, 306, 247, 484, 247, 281, 281, 281, 484, 306, 247, 247, 247, 247, 281, 284, 284, 284, 306, 677, 281, 479, 285, 285, 285, 288, 288, 288, 600, 479, 284, 285, 289, 289, 289, 600, 247, 285, 676, 614, 288, 289, 247, 291, 291, 291, 614, 289, 292, 292, 292, 294, 294, 294, 300, 300, 300, 292, 291, 302, 302, 302, 513, 292, 343, 343, 294, 513, 302,

    300, 304, 304, 304, 341, 302, 341, 341, 341, 342, 673, 342, 342, 342, 672, 574, 304, 574, 583, 341, 583, 349, 349, 349, 342, 671, 343, 598, 584, 300, 350, 350, 350, 584, 302, 598, 609, 300, 393, 393, 393, 609, 302, 349, 598, 304, 667, 391, 391, 391, 666, 665, 350, 304, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 310, 346, 346, 346, 668, 628, 310, 310, 310, 335, 636, 335, 668, 335, 335, 643, 346, 391, 335, 335, 669, 335, 628, 335, 351, 351, 351, 636,

    669, 335, 335, 335, 335, 351, 642, 352, 352, 352, 641, 351, 639, 353, 353, 353, 354, 354, 354, 392, 392, 346, 352, 394, 394, 394, 877, 335, 353, 639, 392, 354, 877, 335, 339, 392, 339, 640, 339, 339, 395, 395, 395, 339, 339, 670, 339, 638, 339, 355, 355, 355, 411, 411, 339, 339, 339, 339, 355, 356, 356, 356, 670, 637, 355, 392, 357, 357, 357, 358, 358, 358, 635, 878, 356, 634, 359, 359, 359, 878, 339, 357, 679, 411, 358, 359, 339, 360, 360, 360, 679, 359, 361, 361, 361, 362, 362, 362, 719, 363,

    363, 363, 360, 364, 364, 364, 719, 361, 363, 633, 362, 365, 365, 365, 363, 366, 366, 366, 364, 367, 367, 367, 368, 368, 368, 632, 365, 631, 367, 879, 366, 369, 369, 369, 367, 879, 630, 368, 370, 370, 370, 375, 375, 375, 465, 465, 369, 376, 376, 376, 377, 377, 377, 370, 629, 465, 375, 378, 378, 378, 465, 627, 376, 626, 624, 377, 380, 380, 380, 381, 381, 381, 378, 487, 487, 380, 889, 407, 407, 407, 620, 380, 889, 890, 381, 375, 538, 538, 378, 890, 465, 376, 616, 375, 377, 612, 382, 382, 382, 376,

    615, 378, 377, 612, 487, 383, 383, 383, 613, 378, 380, 382, 612, 381, 384, 384, 384, 538, 380, 407, 383, 381, 409, 409, 409, 416, 416, 416, 410, 384, 410, 410, 410, 418, 418, 418, 412, 412, 412, 611, 382, 610, 608, 410, 894, 384, 606, 416, 382, 383, 894, 412, 458, 458, 458, 418, 604, 383, 384, 599, 413, 413, 413, 463, 463, 463, 384, 408, 408, 408, 895, 408, 597, 408, 408, 413, 895, 592, 408, 408, 585, 408, 898, 408, 412, 414, 414, 414, 898, 408, 408, 408, 408, 582, 415, 415, 415, 420, 420, 420,

    414, 900, 421, 421, 421, 463, 420, 900, 413, 415, 415, 421, 420, 408, 575, 408, 561, 421, 481, 481, 481, 408, 422, 422, 422, 560, 423, 423, 423, 557, 901, 422, 555, 414, 553, 423, 901, 422, 551, 846, 415, 423, 415, 424, 424, 424, 425, 425, 425, 545, 426, 426, 426, 846, 846, 425, 542, 537, 424, 426, 481, 425, 427, 427, 427, 426, 428, 428, 428, 531, 530, 427, 429, 429, 429, 428, 529, 427, 459, 459, 459, 428, 430, 430, 430, 527, 485, 429, 485, 485, 485, 430, 431, 431, 431, 526, 525, 430, 524, 523,

    459, 431, 432, 432, 432, 521, 520, 431, 434, 434, 434, 432, 433, 433, 433, 519, 486, 432, 486, 486, 486, 433, 514, 434, 435, 435, 435, 433, 436, 436, 436, 486, 512, 435, 505, 505, 505, 436, 511, 435, 437, 437, 437, 436, 438, 438, 438, 467, 467, 437, 439, 439, 439, 438, 510, 437, 504, 500, 467, 438, 440, 440, 440, 467, 482, 439, 469, 469, 469, 440, 441, 441, 441, 480, 478, 440, 476, 475, 469, 441, 442, 442, 442, 469, 474, 441, 444, 444, 444, 442, 443, 443, 443, 467, 472, 442, 447, 447, 447, 443,

    471, 444, 448, 448, 448, 443, 450, 450, 450, 470, 466, 447, 464, 469, 462, 450, 461, 448, 451, 451, 451, 450, 452, 452, 452, 460, 457, 451, 509, 509, 509, 452, 449, 451, 453, 453, 453, 452, 406, 405, 447, 403, 402, 453, 448, 401, 448, 400, 447, 453, 450, 506, 506, 506, 448, 508, 508, 508, 450, 528, 528, 528, 451, 532, 532, 532, 452, 454, 454, 454, 451, 399, 398, 506, 452, 397, 454, 508, 453, 455, 455, 455, 454, 548, 548, 548, 453, 396, 456, 456, 456, 489, 489, 489, 455, 492, 492, 492, 454, 390,

    491, 491, 491, 456, 492, 533, 489, 533, 533, 533, 492, 454, 493, 493, 493, 491, 489, 389, 387, 454, 386, 493, 385, 455, 549, 549, 549, 493, 379, 344, 456, 455, 456, 546, 546, 546, 554, 554, 554, 489, 456, 488, 488, 488, 488, 488, 488, 488, 491, 337, 488, 488, 494, 494, 494, 546, 488, 488, 556, 556, 556, 494, 336, 488, 488, 488, 488, 494, 488, 495, 495, 495, 496, 496, 496, 559, 559, 559, 495, 334, 327, 496, 324, 323, 495, 322, 321, 496, 320, 488, 488, 488, 318, 568, 568, 568, 488, 488, 488, 490,

    490, 490, 568, 317, 313, 490, 490, 309, 568, 490, 490, 490, 490, 307, 490, 490, 490, 490, 490, 490, 572, 572, 572, 305, 490, 303, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 490, 301, 490, 299, 490, 490, 490, 490, 499, 499, 499, 490, 501, 501, 501, 502, 502, 502, 503, 503, 503, 501, 293, 499, 502, 290, 287, 501, 283, 277, 502, 268, 267, 503, 515, 515, 515, 499, 516, 516, 516, 517, 517, 517, 265, 246, 515, 503, 235, 225, 516, 515, 499, 517, 221,

    516, 501, 502, 517, 502, 499, 211, 503, 209, 501, 202, 201, 502, 196, 195, 503, 543, 543, 543, 192, 191, 550, 550, 550, 189, 543, 515, 541, 541, 541, 516, 543, 180, 517, 534, 534, 534, 534, 534, 534, 534, 176, 541, 534, 534, 543, 578, 578, 578, 534, 534, 173, 550, 168, 541, 550, 534, 534, 534, 534, 543, 534, 567, 567, 567, 571, 571, 571, 543, 154, 140, 541, 550, 139, 571, 138, 137, 567, 136, 541, 571, 135, 534, 534, 534, 544, 544, 544, 133, 534, 534, 534, 539, 539, 539, 570, 570, 570, 132, 539,

    544, 567, 539, 539, 539, 539, 567, 539, 539, 539, 570, 128, 544, 126, 567, 118, 113, 539, 569, 569, 569, 107, 573, 573, 573, 106, 105, 569, 91, 544, 576, 576, 576, 569, 570, 90, 86, 544, 539, 570, 539, 558, 558, 558, 73, 569, 70, 570, 579, 579, 579, 56, 54, 573, 52, 45, 573, 586, 586, 586, 28, 27, 569, 577, 577, 577, 26, 590, 590, 590, 569, 558, 558, 573, 25, 558, 590, 601, 601, 601, 576, 18, 590, 17, 16, 558, 602, 602, 602, 603, 603, 603, 558, 558, 563, 563, 563, 563, 563, 563,

    563, 563, 563, 563, 563, 563, 563, 563, 563, 563, 563, 563, 563, 577, 589, 589, 589, 563, 563, 563, 563, 605, 605, 605, 563, 565, 565, 565, 15, 589, 14, 13, 565, 10, 9, 565, 565, 565, 565, 4, 565, 565, 565, 3, 0, 589, 593, 593, 593, 0, 565, 607, 607, 607, 0, 593, 0, 0, 589, 0, 0, 593, 594, 594, 594, 0, 589, 663, 663, 663, 0, 565, 0, 565, 580, 580, 580, 594, 0, 595, 595, 595, 621, 621, 621, 593, 0, 0, 595, 0, 593, 621, 0, 594, 595, 0, 0, 621, 593, 0,

    625, 625, 625, 0, 580, 580, 594, 0, 580, 625, 674, 674, 674, 0, 594, 625, 0, 0, 580, 710, 710, 710, 714, 714, 714, 580, 580, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 587, 619, 619, 619, 622, 622, 622, 664, 664, 664, 619, 0, 0, 622, 0,

    0, 619, 0, 0, 622, 0, 623, 623, 623, 644, 644, 644, 0, 0, 664, 623, 0, 0, 644, 0, 622, 623, 0, 0, 644, 645, 645, 645, 0, 0, 619, 0, 0, 622, 645, 723, 723, 723, 619, 0, 645, 622, 646, 646, 646, 647, 647, 647, 0, 0, 623, 646, 0, 644, 675, 675, 675, 646, 623, 0, 647, 644, 0, 649, 649, 649, 650, 650, 650, 645, 0, 0, 649, 0, 0, 650, 675, 645, 649, 0, 0, 650, 0, 651, 651, 651, 646, 0, 0, 647, 0, 0, 651, 0, 646, 0, 0, 647, 651, 652,

    652, 652, 653, 653, 653, 726, 726, 726, 652, 0, 0, 653, 0, 0, 652, 0, 0, 653, 654, 654, 654, 655, 655, 655, 754, 754, 754, 654, 0, 0, 655, 0, 0, 654, 0, 0, 655, 0, 656, 656, 656, 657, 657, 657, 0, 0, 653, 656, 659, 659, 659, 0, 0, 656, 653, 0, 657, 659, 0, 660, 660, 660, 654, 659, 0, 655, 0, 0, 660, 0, 654, 0, 0, 655, 660, 0, 0, 661, 661, 661, 0, 0, 656, 0, 0, 657, 661, 662, 662, 662, 656, 0, 661, 657, 0, 0, 662, 682, 682, 682,

    0, 0, 662, 711, 711, 711, 682, 683, 683, 683, 0, 0, 682, 713, 713, 713, 683, 684, 684, 684, 0, 0, 683, 0, 0, 711, 684, 685, 685, 685, 0, 0, 684, 0, 0, 713, 685, 686, 686, 686, 0, 682, 685, 755, 755, 755, 686, 0, 0, 682, 0, 683, 686, 0, 0, 687, 687, 687, 0, 683, 0, 684, 0, 0, 687, 688, 688, 688, 686, 684, 687, 685, 0, 0, 688, 689, 689, 689, 0, 685, 688, 686, 0, 0, 689, 690, 690, 690, 0, 686, 689, 756, 756, 756, 690, 691, 691, 691, 0, 687,

    690, 757, 757, 757, 691, 0, 0, 687, 0, 0, 691, 692, 692, 692, 693, 693, 693, 760, 760, 760, 692, 0, 0, 693, 0, 0, 692, 0, 0, 693, 694, 694, 694, 695, 695, 695, 761, 761, 761, 694, 0, 0, 695, 0, 0, 694, 0, 0, 695, 0, 696, 696, 696, 699, 699, 699, 0, 0, 693, 696, 0, 0, 699, 0, 0, 696, 693, 0, 699, 700, 700, 700, 0, 0, 694, 0, 0, 695, 700, 762, 762, 762, 694, 0, 700, 695, 701, 701, 701, 702, 702, 702, 0, 0, 696, 701, 0, 699, 702, 0,

    0, 701, 696, 0, 702, 699, 0, 0, 703, 703, 703, 0, 0, 700, 764, 764, 764, 703, 788, 788, 788, 700, 0, 703, 0, 704, 704, 704, 0, 0, 701, 0, 0, 702, 704, 705, 705, 705, 701, 703, 704, 702, 0, 0, 705, 0, 706, 706, 706, 0, 705, 0, 703, 0, 0, 706, 707, 707, 707, 0, 703, 706, 722, 722, 722, 707, 708, 708, 708, 704, 0, 707, 789, 789, 789, 708, 0, 704, 709, 709, 709, 708, 0, 0, 722, 0, 0, 709, 727, 727, 727, 0, 0, 709, 728, 728, 728, 727, 729, 729,

    729, 0, 0, 727, 790, 790, 790, 729, 0, 728, 730, 730, 730, 729, 731, 731, 731, 0, 0, 730, 791, 791, 791, 731, 0, 730, 732, 732, 732, 731, 0, 0, 727, 0, 0, 732, 0, 0, 728, 0, 727, 732, 729, 752, 752, 752, 728, 792, 792, 792, 729, 793, 793, 793, 730, 795, 795, 795, 731, 733, 733, 733, 730, 0, 0, 752, 731, 0, 733, 0, 732, 734, 734, 734, 733, 796, 796, 796, 732, 0, 734, 0, 736, 736, 736, 0, 734, 737, 737, 737, 733, 736, 738, 738, 738, 0, 737, 736, 809, 809,

    809, 738, 737, 733, 739, 739, 739, 738, 810, 810, 810, 733, 0, 739, 740, 740, 740, 0, 0, 739, 743, 743, 743, 740, 0, 0, 736, 0, 0, 740, 0, 737, 0, 0, 736, 743, 738, 0, 0, 737, 741, 741, 741, 0, 738, 740, 0, 0, 739, 741, 742, 742, 742, 0, 0, 741, 739, 0, 740, 742, 745, 745, 745, 0, 743, 742, 740, 0, 0, 745, 0, 0, 743, 0, 0, 745, 746, 746, 746, 0, 747, 747, 747, 0, 741, 746, 0, 0, 0, 747, 0, 746, 741, 0, 742, 747, 748, 748, 748, 749,

    749, 749, 742, 0, 745, 748, 0, 0, 749, 0, 0, 748, 745, 0, 749, 0, 750, 750, 750, 0, 746, 0, 0, 0, 747, 750, 0, 0, 746, 0, 749, 750, 747, 0, 0, 765, 765, 765, 0, 0, 748, 0, 0, 749, 765, 766, 766, 766, 748, 0, 765, 749, 0, 0, 766, 767, 767, 767, 0, 0, 766, 0, 0, 0, 767, 768, 768, 768, 0, 0, 767, 0, 0, 0, 768, 769, 769, 769, 0, 765, 768, 0, 0, 0, 769, 0, 0, 765, 0, 766, 769, 770, 770, 770, 0, 0, 0, 766, 0, 767,

    770, 771, 771, 771, 0, 0, 770, 767, 0, 768, 771, 773, 773, 773, 0, 0, 771, 768, 0, 769, 773, 0, 770, 772, 772, 772, 773, 769, 0, 0, 0, 0, 772, 0, 0, 770, 0, 0, 772, 774, 774, 774, 0, 770, 0, 771, 0, 0, 774, 775, 775, 775, 0, 771, 774, 773, 0, 0, 775, 776, 776, 776, 0, 773, 775, 772, 0, 772, 776, 777, 777, 777, 0, 0, 776, 772, 0, 0, 777, 778, 778, 778, 0, 774, 777, 0, 0, 0, 778, 0, 0, 774, 0, 775, 778, 0, 0, 779, 779, 779,

    0, 775, 0, 776, 0, 0, 779, 780, 780, 780, 778, 776, 779, 777, 0, 0, 780, 781, 781, 781, 0, 777, 780, 778, 0, 0, 781, 782, 782, 782, 0, 778, 781, 0, 0, 0, 782, 783, 783, 783, 0, 779, 782, 0, 0, 0, 783, 0, 0, 779, 0, 780, 783, 784, 784, 784, 0, 0, 0, 780, 0, 781, 784, 785, 785, 785, 0, 0, 784, 781, 0, 782, 785, 786, 786, 786, 0, 0, 785, 782, 0, 783, 786, 0, 787, 787, 787, 0, 786, 783, 0, 0, 0, 787, 785, 0, 0, 784, 0, 787,

    0, 797, 797, 797, 0, 784, 0, 785, 0, 0, 797, 0, 798, 798, 798, 785, 797, 786, 0, 0, 0, 798, 799, 799, 799, 786, 787, 798, 787, 0, 0, 799, 0, 800, 800, 800, 787, 799, 0, 0, 0, 798, 800, 0, 0, 797, 0, 0, 800, 0, 801, 801, 801, 797, 0, 0, 798, 0, 0, 801, 802, 802, 802, 0, 798, 801, 799, 0, 0, 802, 0, 0, 0, 0, 799, 802, 0, 800, 803, 803, 803, 804, 804, 804, 0, 800, 0, 803, 0, 0, 804, 0, 0, 803, 801, 0, 804, 0, 805, 805,

    805, 0, 801, 0, 802, 0, 0, 805, 0, 803, 0, 0, 802, 805, 806, 806, 806, 808, 808, 808, 0, 0, 803, 806, 0, 804, 808, 0, 0, 806, 803, 0, 808, 804, 0, 0, 811, 811, 811, 0, 805, 0, 805, 0, 0, 811, 808, 812, 812, 812, 805, 811, 0, 0, 0, 0, 812, 0, 806, 0, 0, 808, 812, 811, 0, 0, 806, 0, 0, 808, 0, 0, 813, 813, 813, 814, 814, 814, 0, 0, 811, 813, 0, 0, 814, 0, 0, 813, 811, 0, 814, 812, 0, 0, 815, 815, 815, 0, 0, 812,

    0, 813, 814, 815, 0, 816, 816, 816, 0, 815, 0, 0, 0, 0, 816, 0, 813, 0, 0, 814, 816, 817, 817, 817, 813, 0, 0, 814, 0, 0, 817, 0, 816, 815, 0, 0, 817, 0, 815, 818, 818, 818, 0, 819, 819, 819, 815, 0, 818, 816, 0, 0, 819, 0, 818, 0, 0, 816, 819, 0, 817, 0, 820, 820, 820, 817, 821, 821, 821, 0, 818, 820, 0, 817, 0, 821, 0, 820, 0, 0, 0, 821, 819, 818, 0, 0, 0, 819, 822, 822, 822, 818, 0, 820, 0, 819, 0, 822, 823, 823,

    823, 0, 0, 822, 0, 0, 820, 823, 0, 0, 821, 0, 0, 823, 820, 824, 824, 824, 821, 822, 0, 0, 0, 0, 824, 0, 0, 0, 0, 0, 824, 0, 822, 0, 0, 0, 0, 0, 0, 0, 822, 0, 823, 0, 0, 0, 0, 0, 0, 0, 823, 0, 0, 0, 0, 0, 0, 0, 0, 824, 0, 0, 0, 0, 0, 0, 0, 824, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 826, 827, 827, 827, 827, 827, 827, 827, 827, 827, 827,

    827, 827, 827, 827, 827, 827, 827, 827, 827, 827, 827, 827, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 828, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 829, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 830, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831, 831,

    832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 832, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 833, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 834, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 835, 836, 836, 836, 836, 836, 836, 836, 836, 836, 836, 836, 836,

    836, 836, 836, 836, 836, 836, 836, 836, 836, 836, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 837, 838, 838, 838, 838, 838, 838, 838, 0, 838, 838, 838, 838, 838, 838, 838, 838, 838, 838, 838, 838, 838, 838, 839, 839, 839, 839, 839, 839, 839, 839, 839, 0, 839, 839, 0, 839, 839, 839, 839, 839, 839, 839, 839, 839, 840, 840, 840, 840, 0, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 840, 841, 841,

    841, 841, 841, 841, 841, 0, 841, 841, 841, 841, 841, 841, 841, 841, 841, 841, 841, 841, 841, 841, 842, 842, 842, 842, 842, 842, 842, 0, 842, 842, 842, 842, 842, 842, 842, 842, 842, 842, 842, 842, 842, 842, 843, 843, 843, 843, 843, 843, 843, 0, 843, 843, 843, 843, 843, 843, 843, 843, 843, 843, 843, 843, 843, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 844, 845, 845, 845, 845, 845, 0, 845, 845, 845, 845, 845, 845, 845, 845, 845,

    845, 845, 845, 845, 845, 845, 845, 847, 847, 0, 0, 847, 0, 0, 0, 0, 0, 0, 0, 847, 0, 847, 0, 0, 0, 847, 847, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 848, 849, 0, 0, 0, 0, 0, 0, 0, 849, 0, 0, 0, 0, 0, 849, 849, 850, 0, 0, 850, 0, 0, 850, 850, 0, 850, 0, 0, 0, 850, 850, 850, 851, 0, 0, 0, 0, 0, 0, 0, 851, 0, 0, 0, 0, 0, 851, 851, 852, 0, 0,

    852, 0, 852, 852, 853, 0, 0, 0, 0, 0, 0, 0, 853, 0, 0, 0, 0, 0, 853, 853, 854, 854, 854, 854, 0, 854, 854, 854, 854, 854, 854, 854, 854, 854, 854, 854, 0, 854, 0, 854, 854, 854, 855, 855, 855, 855, 0, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 855, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 856, 0, 856, 856, 856, 857, 0, 0, 0, 0, 0, 0, 0, 857, 0, 0, 0, 0, 0,

    857, 857, 858, 858, 0, 0, 858, 0, 0, 0, 0, 0, 0, 0, 858, 0, 858, 0, 0, 0, 858, 858, 859, 0, 0, 859, 0, 0, 859, 859, 0, 859, 0, 0, 0, 859, 859, 859, 860, 0, 0, 860, 0, 0, 860, 860, 0, 860, 0, 0, 0, 860, 860, 860, 864, 0, 0, 0, 0, 0, 0, 0, 864, 0, 0, 0, 0, 0, 864, 864, 865, 0, 0, 865, 0, 0, 865, 865, 0, 865, 0, 0, 0, 865, 865, 865, 866, 866, 866, 866, 866, 866, 866, 866, 866, 866, 866, 866, 866, 866,

    866, 866, 866, 866, 866, 866, 866, 866, 868, 0, 0, 0, 0, 0, 0, 0, 868, 0, 0, 0, 0, 0, 868, 868, 869, 869, 0, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 869, 870, 0, 0, 870, 0, 0, 870, 870, 0, 870, 0, 0, 0, 870, 870, 870, 871, 0, 0, 0, 0, 871, 871, 0, 871, 0, 0, 0, 0, 0, 871, 871, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872, 872,

    873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 873, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 874, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 875, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 876, 880, 880, 880, 880, 880, 880, 880, 880, 880, 880, 880, 880,

    880, 880, 880, 880, 880, 880, 880, 880, 880, 880, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 881, 883, 883, 0, 0, 883, 883, 0, 883, 883, 883, 883, 883, 883, 0, 0, 0, 0, 883, 883, 883, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 884, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 885, 886, 886, 886, 886,

    886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 886, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 887, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 891, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892, 892,

    892, 892, 892, 892, 892, 892, 893, 0, 0, 0, 0, 893, 893, 0, 893, 0, 0, 0, 0, 0, 893, 893, 896, 896, 896, 896, 896, 896, 896, 896, 896, 896, 896, 896, 0, 896, 896, 896, 896, 896, 896, 896, 896, 896, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 897, 899, 899, 0, 0, 899, 899, 0, 899, 899, 899, 899, 899, 899, 0, 0, 0, 0, 899, 899, 899, 902, 902, 902, 902, 902, 902, 902, 0, 902, 902, 902, 902, 902, 902,

    902, 902, 902, 902, 902, 902, 902, 902, 903, 903, 903, 903, 903, 903, 903, 0, 903, 903, 903, 903, 903, 903, 903, 903, 903, 903, 903, 903, 903, 903, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 904, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 905, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 906, 907, 907, 907, 907,

    907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 907, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 908, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 909, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911, 911,

    911, 911, 911, 911, 911, 911, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 912, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 913, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 914, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 915, 916, 916, 916, 916, 916, 916,

    916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 916, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825, 825};

                                                      
static const flex_int32_t yy_rule_can_match_eol[153] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    1,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
};

static yy_state_type yy_last_accepting_state;
static char *yy_last_accepting_cpos;

extern int yy_flex_debug;
int yy_flex_debug = 0;

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *yytext;
#line 1 "pgc.l"

#line 30 "pgc.l"

                     

extern YYSTYPE base_yylval;

static int xcdepth = 0;                                                     
static char *dolqstart = NULL;                                       

   
                                                                       
                                                                        
                                                                      
                                                                  
   
static char *literalbuf = NULL;                        
static int literallen;                                     
static int literalalloc;                                           

                                                               
static int parenths_open;

                                                                                    
static bool include_next;

#define startlit() (literalbuf[0] = '\0', literallen = 0)
static void
addlit(char *ytext, int yleng);
static void
addlitchar(unsigned char);
static int
process_integer_literal(const char *token, YYSTYPE *lval);
static void
parse_include(void);
static bool
ecpg_isspace(char ch);
static bool
isdefine(void);
static bool
isinformixdefine(void);

char *token_start;
static int state_before;

struct _yy_buffer
{
  YY_BUFFER_STATE buffer;
  long lineno;
  char *filename;
  struct _yy_buffer *next;
} *yy_buffer = NULL;

static char *old;

#define MAX_NESTED_IF 128
static short preproc_tos;
static short ifcond;
static struct _if_value
{
  short condition;
  short else_branch;
} stacked_if_value[MAX_NESTED_IF];

#line 2248 "pgc.c"
#define YY_NO_INPUT 1
   
                                                               
                                                                       
                                                                               
                                                                            
                                                                             
                                                                      
   
                                                                  
                                                          
                     
                            
                                         
                                             
                                                           
                                     
                                    
                                           
                                 
                                                                      
                                     
                                 
                                                 
                                             
                                                     
                                                                      
   
                                                                           
                                                    
   

                                                           

   
                                                                          
                                                                        
                                                                            
                                                                            
                                                                             
   
                                                                          
                                                                             
                                                                         
                               
   
                                                                     
   
                                                                          
             
   
   
                                                                  
                                                                       
                                                                          
                                                                  
   
              
   
                        
                        
                                                 
                  
                                            
   
                                         
                                                                         
                                                                     
                                                               
                                                
   
                                                                          
                                    
   
                
                                                                         
   
                     
                                                                         
                                                    
   
                                            
                                        
                                 
                    
   
                                                                              
                                                                          
                                                                              
                                                                            
                                                                             
                                                                           
                             
                                                                       
                                                                          
                                                                               
                                                                          
                                                                          
                                                                              
               
                                                                        
   
                                                              
   
                                                                                
                                                                             
                                                                              
                                                                            
                                                                           
                                                       
   
   
                                                                          
                                                                         
                                                                        
                                                                        
                                                                             
   
                                                                         
                        
   
                                              
                                                          
                                              
   
                                                                                 
   
                                                                         
                                                          
   
                                       
                                                 
   
                                                                              
                                                                                
                 
                                                                   
                                                                            
                           
                                                                            
                                          
                                                                      
                                                           
   
                                
                             
                                                  
                                                        
                                                                 
                                                                       
                                                                          
   
#line 2401 "pgc.c"

#define INITIAL 0
#define xb 1
#define xcc 2
#define xcsql 3
#define xd 4
#define xdc 5
#define xh 6
#define xn 7
#define xq 8
#define xe 9
#define xqc 10
#define xdolq 11
#define xui 12
#define xus 13
#define xcond 14
#define xskip 15
#define C 16
#define SQL 17
#define incl 18
#define def 19
#define def_ident 20
#define undef 21

#ifndef YY_NO_UNISTD_H
                                                                        
                                                                              
                                                        
   
#include <unistd.h>
#endif

#ifndef YY_EXTRA_TYPE
#define YY_EXTRA_TYPE void *
#endif

static int
yy_init_globals(void);

                                
                                                                       

int
yylex_destroy(void);

int
yyget_debug(void);

void
yyset_debug(int debug_flag);

YY_EXTRA_TYPE
yyget_extra(void);

void
yyset_extra(YY_EXTRA_TYPE user_defined);

FILE *
yyget_in(void);

void
yyset_in(FILE *_in_str);

FILE *
yyget_out(void);

void
yyset_out(FILE *_out_str);

int
yyget_leng(void);

char *
yyget_text(void);

int
yyget_lineno(void);

void
yyset_lineno(int _line_number);

                                                                        
              
   

#ifndef YY_SKIP_YYWRAP
#ifdef __cplusplus
extern "C" int
yywrap(void);
#else
extern int
yywrap(void);
#endif
#endif

#ifndef YY_NO_UNPUT

static void
yyunput(int c, char *buf_ptr);

#endif

#ifndef yytext_ptr
static void
yy_flex_strncpy(char *, const char *, int);
#endif

#ifdef YY_NEED_STRLEN
static int
yy_flex_strlen(const char *);
#endif

#ifndef YY_NO_INPUT
#ifdef __cplusplus
static int
yyinput(void);
#else
static int
input(void);
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
#define YY_FATAL_ERROR(msg) yy_fatal_error(msg)
#endif

                                                        

                                                                       
                          
   
#ifndef YY_DECL
#define YY_DECL_IS_OURS 1

extern int
yylex(void);

#define YY_DECL int yylex(void)
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

  if (!(yy_init))
  {
    (yy_init) = 1;

#ifdef YY_USER_INIT
    YY_USER_INIT;
#endif

    if (!(yy_start))
    {
      (yy_start) = 1;                        
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
      yyensure_buffer_stack();
      YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE);
    }

    yy_load_buffer_state();
  }

  {
#line 400 "pgc.l"

#line 404 "pgc.l"
                                                              
    token_start = NULL;

#line 2648 "pgc.c"

    while (              1) /* loops until end-of-file is reached */
    {
      yy_cp = (yy_c_buf_p);

                              
      *yy_cp = (yy_hold_char);

                                                                   
                          
         
      yy_bp = yy_cp;

      yy_current_state = (yy_start);
    yy_match:
      do
      {
        YY_CHAR yy_c = yy_ec[YY_SC_TO_UI(*yy_cp)];
        if (yy_accept[yy_current_state])
        {
          (yy_last_accepting_state) = yy_current_state;
          (yy_last_accepting_cpos) = yy_cp;
        }
        while (yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state)
        {
          yy_current_state = (int)yy_def[yy_current_state];
          if (yy_current_state >= 826)
          {
            yy_c = yy_meta[yy_c];
          }
        }
        yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
        ++yy_cp;
      } while (yy_current_state != 825);
      yy_cp = (yy_last_accepting_cpos);
      yy_current_state = (yy_last_accepting_state);

    yy_find_action:
      yy_act = yy_accept[yy_current_state];

      YY_DO_BEFORE_ACTION;

      if (yy_act != YY_END_OF_BUFFER && yy_rule_can_match_eol[yy_act])
      {
        int yyl;
        for (yyl = 0; yyl < yyleng; ++yyl)
        {
          if (yytext[yyl] == '\n')
          {

            yylineno++;
          }
        };
      }

    do_action:                                                     

      switch (yy_act)
      {                                       
      case 0:                   
                                                     
        *yy_cp = (yy_hold_char);
        yy_cp = (yy_last_accepting_cpos);
        yy_current_state = (yy_last_accepting_state);
        goto yy_find_action;

      case 1:
                                  
        YY_RULE_SETUP
#line 409 "pgc.l"
        {
                      
        }
        YY_BREAK
      case 2:
        YY_RULE_SETUP
#line 413 "pgc.l"
        {
          token_start = yytext;
          state_before = YYSTATE;
          xcdepth = 0;
          BEGIN(xcsql);
                                                                  
          yyless(2);
          fputs("/*", yyout);
        }
        YY_BREAK
                 
      case 3:
        YY_RULE_SETUP
#line 424 "pgc.l"
        {
          token_start = yytext;
          state_before = YYSTATE;
          xcdepth = 0;
          BEGIN(xcc);
                                                                  
          yyless(2);
          fputs("/*", yyout);
        }
        YY_BREAK
      case 4:
        YY_RULE_SETUP
#line 433 "pgc.l"
        {
          ECHO;
        }
        YY_BREAK
      case 5:
        YY_RULE_SETUP
#line 434 "pgc.l"
        {
          xcdepth++;
                                                                  
          yyless(2);
          fputs("/_*", yyout);
        }
        YY_BREAK
      case 6:
        YY_RULE_SETUP
#line 440 "pgc.l"
        {
          if (xcdepth <= 0)
          {
            ECHO;
            BEGIN(state_before);
            token_start = NULL;
          }
          else
          {
            xcdepth--;
            fputs("*_/", yyout);
          }
        }
        YY_BREAK
      case 7:
        YY_RULE_SETUP
#line 453 "pgc.l"
        {
          ECHO;
          BEGIN(state_before);
          token_start = NULL;
        }
        YY_BREAK

      case 8:
                                  
        YY_RULE_SETUP
#line 460 "pgc.l"
        {
          ECHO;
        }
        YY_BREAK
      case 9:
        YY_RULE_SETUP
#line 464 "pgc.l"
        {
          ECHO;
        }
        YY_BREAK
      case 10:
        YY_RULE_SETUP
#line 468 "pgc.l"
        {
          ECHO;
        }
        YY_BREAK
      case YY_STATE_EOF(xcc):
      case YY_STATE_EOF(xcsql):
#line 472 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated /* comment");
      }
        YY_BREAK
                         

      case 11:
        YY_RULE_SETUP
#line 478 "pgc.l"
        {
          token_start = yytext;
          BEGIN(xb);
          startlit();
          addlitchar('b');
        }
        YY_BREAK
                 
      case 12:
                           
#line 487 "pgc.l"
      case 13:
                                   
        YY_RULE_SETUP
#line 487 "pgc.l"
        {
          yyless(1);
          BEGIN(SQL);
          if (literalbuf[strspn(literalbuf, "01") + 1] != '\0')
          {
            mmerror(PARSE_ERROR, ET_ERROR, "invalid bit string literal");
          }
          base_yylval.str = mm_strdup(literalbuf);
          return BCONST;
        }
        YY_BREAK
      case 14:
                           
#line 496 "pgc.l"
      case 15:
                                   
        YY_RULE_SETUP
#line 496 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 16:
                           
#line 500 "pgc.l"
      case 17:
                                   
        YY_RULE_SETUP
#line 500 "pgc.l"
        {
                      
        }
        YY_BREAK
      case YY_STATE_EOF(xb):
#line 503 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated bit string literal");
      }
        YY_BREAK
      case 18:
        YY_RULE_SETUP
#line 505 "pgc.l"
        {
          token_start = yytext;
          BEGIN(xh);
          startlit();
          addlitchar('x');
        }
        YY_BREAK
      case 19:
                           
#line 512 "pgc.l"
      case 20:
                                   
        YY_RULE_SETUP
#line 512 "pgc.l"
        {
          yyless(1);
          BEGIN(SQL);
          base_yylval.str = mm_strdup(literalbuf);
          return XCONST;
        }
        YY_BREAK
      case YY_STATE_EOF(xh):
#line 519 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated hexadecimal string literal");
      }
        YY_BREAK
      case 21:
        YY_RULE_SETUP
#line 521 "pgc.l"
        {
          token_start = yytext;
          state_before = YYSTATE;
          BEGIN(xqc);
          startlit();
        }
        YY_BREAK

      case 22:
        YY_RULE_SETUP
#line 529 "pgc.l"
        {
                                 
                                               
             
          token_start = yytext;
          state_before = YYSTATE;
          BEGIN(xn);
          startlit();
        }
        YY_BREAK
      case 23:
        YY_RULE_SETUP
#line 539 "pgc.l"
        {
          token_start = yytext;
          state_before = YYSTATE;
          BEGIN(xq);
          startlit();
        }
        YY_BREAK
      case 24:
        YY_RULE_SETUP
#line 545 "pgc.l"
        {
          token_start = yytext;
          state_before = YYSTATE;
          BEGIN(xe);
          startlit();
        }
        YY_BREAK
      case 25:
        YY_RULE_SETUP
#line 551 "pgc.l"
        {
          token_start = yytext;
          state_before = YYSTATE;
          BEGIN(xus);
          startlit();
          addlit(yytext, yyleng);
        }
        YY_BREAK
                 
      case 26:
                           
#line 561 "pgc.l"
      case 27:
                                   
        YY_RULE_SETUP
#line 561 "pgc.l"
        {
          yyless(1);
          BEGIN(state_before);
          base_yylval.str = mm_strdup(literalbuf);
          return SCONST;
        }
        YY_BREAK
      case 28:
                           
#line 568 "pgc.l"
      case 29:
                                   
        YY_RULE_SETUP
#line 568 "pgc.l"
        {
          yyless(1);
          BEGIN(state_before);
          base_yylval.str = mm_strdup(literalbuf);
          return ECONST;
        }
        YY_BREAK
      case 30:
                           
#line 575 "pgc.l"
      case 31:
                                   
        YY_RULE_SETUP
#line 575 "pgc.l"
        {
          yyless(1);
          BEGIN(state_before);
          base_yylval.str = mm_strdup(literalbuf);
          return NCONST;
        }
        YY_BREAK
      case 32:
                                   
        YY_RULE_SETUP
#line 581 "pgc.l"
        {
          addlit(yytext, yyleng);
          BEGIN(state_before);
          base_yylval.str = mm_strdup(literalbuf);
          return UCONST;
        }
        YY_BREAK
      case 33:
        YY_RULE_SETUP
#line 587 "pgc.l"
        {
          addlitchar('\'');
        }
        YY_BREAK
      case 34:
        YY_RULE_SETUP
#line 588 "pgc.l"
        {
          addlitchar('\\');
          addlitchar('\'');
        }
        YY_BREAK
      case 35:
                                   
        YY_RULE_SETUP
#line 592 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 36:
                                   
        YY_RULE_SETUP
#line 593 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 37:
        YY_RULE_SETUP
#line 596 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 38:
                                   
        YY_RULE_SETUP
#line 599 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 39:
        YY_RULE_SETUP
#line 602 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 40:
        YY_RULE_SETUP
#line 605 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 41:
                                   
        YY_RULE_SETUP
#line 608 "pgc.l"
        {
                      
        }
        YY_BREAK
      case 42:
        YY_RULE_SETUP
#line 611 "pgc.l"
        {
                                                         
          addlitchar(yytext[0]);
        }
        YY_BREAK
      case YY_STATE_EOF(xq):
      case YY_STATE_EOF(xqc):
      case YY_STATE_EOF(xe):
      case YY_STATE_EOF(xn):
      case YY_STATE_EOF(xus):
#line 615 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated quoted string");
      }
        YY_BREAK

      case 43:
        YY_RULE_SETUP
#line 618 "pgc.l"
        {
          token_start = yytext;
          if (dolqstart)
          {
            free(dolqstart);
          }
          dolqstart = mm_strdup(yytext);
          BEGIN(xdolq);
          startlit();
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 44:
        YY_RULE_SETUP
#line 627 "pgc.l"
        {
                                                  
          yyless(1);
                                       
          return yytext[0];
        }
        YY_BREAK
                 
      case 45:
        YY_RULE_SETUP
#line 635 "pgc.l"
        {
          if (strcmp(yytext, dolqstart) == 0)
          {
            addlit(yytext, yyleng);
            free(dolqstart);
            dolqstart = NULL;
            BEGIN(SQL);
            base_yylval.str = mm_strdup(literalbuf);
            return DOLCONST;
          }
          else
          {
               
                                                                  
                                                                   
                                                                  
               
            addlit(yytext, yyleng - 1);
            yyless(yyleng - 1);
          }
        }
        YY_BREAK
      case 46:
                                   
        YY_RULE_SETUP
#line 656 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 47:
        YY_RULE_SETUP
#line 659 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 48:
        YY_RULE_SETUP
#line 662 "pgc.l"
        {
                                           
          addlitchar(yytext[0]);
        }
        YY_BREAK
      case YY_STATE_EOF(xdolq):
#line 666 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated dollar-quoted string");
      }
        YY_BREAK

      case 49:
        YY_RULE_SETUP
#line 669 "pgc.l"
        {
          state_before = YYSTATE;
          BEGIN(xd);
          startlit();
        }
        YY_BREAK
      case 50:
        YY_RULE_SETUP
#line 674 "pgc.l"
        {
          state_before = YYSTATE;
          BEGIN(xui);
          startlit();
          addlit(yytext, yyleng);
        }
        YY_BREAK
                 
      case 51:
        YY_RULE_SETUP
#line 682 "pgc.l"
        {
          BEGIN(state_before);
          if (literallen == 0)
          {
            mmerror(PARSE_ERROR, ET_ERROR, "zero-length delimited identifier");
          }
                                                                                                          
          base_yylval.str = mm_strdup(literalbuf);
          return CSTRING;
        }
        YY_BREAK
      case 52:
        YY_RULE_SETUP
#line 690 "pgc.l"
        {
          BEGIN(state_before);
          base_yylval.str = mm_strdup(literalbuf);
          return CSTRING;
        }
        YY_BREAK
      case 53:
                                   
        YY_RULE_SETUP
#line 695 "pgc.l"
        {
          BEGIN(state_before);
          if (literallen == 2)           
          {
            mmerror(PARSE_ERROR, ET_ERROR, "zero-length delimited identifier");
          }
                                                                                                          
          addlit(yytext, yyleng);
          base_yylval.str = mm_strdup(literalbuf);
          return UIDENT;
        }
        YY_BREAK
      case 54:
        YY_RULE_SETUP
#line 704 "pgc.l"
        {
          addlitchar('"');
        }
        YY_BREAK
      case 55:
                                   
        YY_RULE_SETUP
#line 707 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case YY_STATE_EOF(xd):
      case YY_STATE_EOF(xui):
#line 710 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated quoted identifier");
      }
        YY_BREAK
      case 56:
        YY_RULE_SETUP
#line 711 "pgc.l"
        {
          state_before = YYSTATE;
          BEGIN(xdc);
          startlit();
        }
        YY_BREAK
      case 57:
                                   
        YY_RULE_SETUP
#line 716 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case YY_STATE_EOF(xdc):
#line 719 "pgc.l"
      {
        mmfatal(PARSE_ERROR, "unterminated quoted string");
      }
        YY_BREAK

      case 58:
        YY_RULE_SETUP
#line 722 "pgc.l"
        {
          return TYPECAST;
        }
        YY_BREAK
      case 59:
        YY_RULE_SETUP
#line 726 "pgc.l"
        {
          return DOT_DOT;
        }
        YY_BREAK
      case 60:
        YY_RULE_SETUP
#line 730 "pgc.l"
        {
          return COLON_EQUALS;
        }
        YY_BREAK
      case 61:
        YY_RULE_SETUP
#line 734 "pgc.l"
        {
          return EQUALS_GREATER;
        }
        YY_BREAK
      case 62:
        YY_RULE_SETUP
#line 738 "pgc.l"
        {
          return LESS_EQUALS;
        }
        YY_BREAK
      case 63:
        YY_RULE_SETUP
#line 742 "pgc.l"
        {
          return GREATER_EQUALS;
        }
        YY_BREAK
      case 64:
        YY_RULE_SETUP
#line 746 "pgc.l"
        {
                                                                  
          return NOT_EQUALS;
        }
        YY_BREAK
      case 65:
        YY_RULE_SETUP
#line 751 "pgc.l"
        {
                                                                  
          return NOT_EQUALS;
        }
        YY_BREAK
      case 66:
        YY_RULE_SETUP
#line 756 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            unput(':');
          }
          else
          {
            return yytext[0];
          }
        }
        YY_BREAK
      case 67:
        YY_RULE_SETUP
#line 766 "pgc.l"
        {
             
                                                  
                                                    
                                        
             
          if (yytext[0] == ';' && struct_level == 0)
          {
            BEGIN(C);
          }
          return yytext[0];
        }
        YY_BREAK
      case 68:
        YY_RULE_SETUP
#line 777 "pgc.l"
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
               
                                                               
                                                           
                                                           
                                                
               
            if (nchars == 1 && strchr(",()[].;:+-*/%^<>=", yytext[0]))
            {
              return yytext[0];
            }
               
                                                                
                                                                
                                                               
                                           
               
            if (nchars == 2)
            {
              if (yytext[0] == '=' && yytext[1] == '>')
              {
                return EQUALS_GREATER;
              }
              if (yytext[0] == '>' && yytext[1] == '=')
              {
                return GREATER_EQUALS;
              }
              if (yytext[0] == '<' && yytext[1] == '=')
              {
                return LESS_EQUALS;
              }
              if (yytext[0] == '<' && yytext[1] == '>')
              {
                return NOT_EQUALS;
              }
              if (yytext[0] == '!' && yytext[1] == '=')
              {
                return NOT_EQUALS;
              }
            }
          }

          base_yylval.str = mm_strdup(yytext);
          return Op;
        }
        YY_BREAK
      case 69:
        YY_RULE_SETUP
#line 874 "pgc.l"
        {
          base_yylval.ival = atol(yytext + 1);
          return PARAM;
        }
        YY_BREAK
      case 70:
        YY_RULE_SETUP
#line 879 "pgc.l"
        {
          base_yylval.str = mm_strdup(yytext);
          return IP;
        }
        YY_BREAK
                   

      case 71:
        YY_RULE_SETUP
#line 886 "pgc.l"
        {
          return process_integer_literal(yytext, &base_yylval);
        }
        YY_BREAK
      case 72:
        YY_RULE_SETUP
#line 889 "pgc.l"
        {
          base_yylval.str = mm_strdup(yytext);
          return FCONST;
        }
        YY_BREAK
      case 73:
        YY_RULE_SETUP
#line 893 "pgc.l"
        {
                                                       
          yyless(yyleng - 2);
          return process_integer_literal(yytext, &base_yylval);
        }
        YY_BREAK
      case 74:
        YY_RULE_SETUP
#line 898 "pgc.l"
        {
          base_yylval.str = mm_strdup(yytext);
          return FCONST;
        }
        YY_BREAK
      case 75:
        YY_RULE_SETUP
#line 902 "pgc.l"
        {
             
                                                              
                                                   
             
          yyless(yyleng - 1);
          return process_integer_literal(yytext, &base_yylval);
        }
        YY_BREAK
      case 76:
        YY_RULE_SETUP
#line 910 "pgc.l"
        {
                                                             
          yyless(yyleng - 2);
          return process_integer_literal(yytext, &base_yylval);
        }
        YY_BREAK
                     

      case 77:
                                   
        YY_RULE_SETUP
#line 918 "pgc.l"
        {
          base_yylval.str = mm_strdup(yytext + 1);
          return CVARIABLE;
        }
        YY_BREAK
      case 78:
        YY_RULE_SETUP
#line 923 "pgc.l"
        {
          if (!isdefine())
          {
            int kwvalue;

                                            
            kwvalue = ScanECPGKeywordLookup(yytext);
            if (kwvalue >= 0)
            {
              return kwvalue;
            }

                                    
            kwvalue = ScanCKeywordLookup(yytext);
            if (kwvalue >= 0)
            {
              return kwvalue;
            }

               
                                                               
               
                                                                  
                                                                 
                                                                     
                                             
               
            base_yylval.str = mm_strdup(yytext);
            return IDENT;
          }
        }
        YY_BREAK
      case 79:
        YY_RULE_SETUP
#line 951 "pgc.l"
        {
          return yytext[0];
        }
        YY_BREAK
                 
         
                                   
         
      case 80:
                                   
        YY_RULE_SETUP
#line 960 "pgc.l"
        {
          BEGIN(SQL);
          return SQL_START;
        }
        YY_BREAK
      case 81:
        YY_RULE_SETUP
#line 961 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            BEGIN(SQL);
            return SQL_START;
          }
          else
          {
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 82:
                                   
        YY_RULE_SETUP
#line 971 "pgc.l"
        {
          ECHO;
        }
        YY_BREAK
      case 83:
        YY_RULE_SETUP
#line 972 "pgc.l"
        {
          char *endptr;

          errno = 0;
          base_yylval.ival = strtoul((char *)yytext, &endptr, 16);
          if (*endptr != '\0' || errno == ERANGE)
          {
            errno = 0;
            base_yylval.str = mm_strdup(yytext);
            return SCONST;
          }
          return ICONST;
        }
        YY_BREAK
      case 84:
                                   
        YY_RULE_SETUP
#line 985 "pgc.l"
        {
          if (system_includes)
          {
            include_next = false;
            BEGIN(incl);
          }
          else
          {
            base_yylval.str = mm_strdup(yytext);
            return CPP_LINE;
          }
        }
        YY_BREAK
      case 85:
                                   
        YY_RULE_SETUP
#line 997 "pgc.l"
        {
          if (system_includes)
          {
            include_next = true;
            BEGIN(incl);
          }
          else
          {
            base_yylval.str = mm_strdup(yytext);
            return CPP_LINE;
          }
        }
        YY_BREAK
      case 86:
                                   
        YY_RULE_SETUP
#line 1009 "pgc.l"
        {
          base_yylval.str = mm_strdup(yytext);
          return CPP_LINE;
        }
        YY_BREAK
      case 87:
        YY_RULE_SETUP
#line 1013 "pgc.l"
        {
             
                                            
                                                      
                                                                   
             
          if (braces_open == 0 && parenths_open == 0)
          {
            if (current_function)
            {
              free(current_function);
            }
            current_function = mm_strdup(yytext);
          }
                                                           
                                                                                
          if ((!INFORMIX_MODE || !isinformixdefine()) && !isdefine())
          {
            int kwvalue;

            kwvalue = ScanCKeywordLookup(yytext);
            if (kwvalue >= 0)
            {
              return kwvalue;
            }
            else
            {
              base_yylval.str = mm_strdup(yytext);
              return IDENT;
            }
          }
        }
        YY_BREAK
      case 88:
        YY_RULE_SETUP
#line 1041 "pgc.l"
        {
          mmerror(PARSE_ERROR, ET_ERROR, "nested           comments");
        }
        YY_BREAK
      case 89:
        YY_RULE_SETUP
#line 1042 "pgc.l"
        {
          return ':';
        }
        YY_BREAK
      case 90:
        YY_RULE_SETUP
#line 1043 "pgc.l"
        {
          return ';';
        }
        YY_BREAK
      case 91:
        YY_RULE_SETUP
#line 1044 "pgc.l"
        {
          return ',';
        }
        YY_BREAK
      case 92:
        YY_RULE_SETUP
#line 1045 "pgc.l"
        {
          return '*';
        }
        YY_BREAK
      case 93:
        YY_RULE_SETUP
#line 1046 "pgc.l"
        {
          return '%';
        }
        YY_BREAK
      case 94:
        YY_RULE_SETUP
#line 1047 "pgc.l"
        {
          return '/';
        }
        YY_BREAK
      case 95:
        YY_RULE_SETUP
#line 1048 "pgc.l"
        {
          return '+';
        }
        YY_BREAK
      case 96:
        YY_RULE_SETUP
#line 1049 "pgc.l"
        {
          return '-';
        }
        YY_BREAK
      case 97:
        YY_RULE_SETUP
#line 1050 "pgc.l"
        {
          parenths_open++;
          return '(';
        }
        YY_BREAK
      case 98:
        YY_RULE_SETUP
#line 1051 "pgc.l"
        {
          parenths_open--;
          return ')';
        }
        YY_BREAK
      case 99:
                                   
        YY_RULE_SETUP
#line 1052 "pgc.l"
        {
          ECHO;
        }
        YY_BREAK
      case 100:
        YY_RULE_SETUP
#line 1053 "pgc.l"
        {
          return '{';
        }
        YY_BREAK
      case 101:
        YY_RULE_SETUP
#line 1054 "pgc.l"
        {
          return '}';
        }
        YY_BREAK
      case 102:
        YY_RULE_SETUP
#line 1055 "pgc.l"
        {
          return '[';
        }
        YY_BREAK
      case 103:
        YY_RULE_SETUP
#line 1056 "pgc.l"
        {
          return ']';
        }
        YY_BREAK
      case 104:
        YY_RULE_SETUP
#line 1057 "pgc.l"
        {
          return '=';
        }
        YY_BREAK
      case 105:
        YY_RULE_SETUP
#line 1058 "pgc.l"
        {
          return S_MEMBER;
        }
        YY_BREAK
      case 106:
        YY_RULE_SETUP
#line 1059 "pgc.l"
        {
          return S_RSHIFT;
        }
        YY_BREAK
      case 107:
        YY_RULE_SETUP
#line 1060 "pgc.l"
        {
          return S_LSHIFT;
        }
        YY_BREAK
      case 108:
        YY_RULE_SETUP
#line 1061 "pgc.l"
        {
          return S_OR;
        }
        YY_BREAK
      case 109:
        YY_RULE_SETUP
#line 1062 "pgc.l"
        {
          return S_AND;
        }
        YY_BREAK
      case 110:
        YY_RULE_SETUP
#line 1063 "pgc.l"
        {
          return S_INC;
        }
        YY_BREAK
      case 111:
        YY_RULE_SETUP
#line 1064 "pgc.l"
        {
          return S_DEC;
        }
        YY_BREAK
      case 112:
        YY_RULE_SETUP
#line 1065 "pgc.l"
        {
          return S_EQUAL;
        }
        YY_BREAK
      case 113:
        YY_RULE_SETUP
#line 1066 "pgc.l"
        {
          return S_NEQUAL;
        }
        YY_BREAK
      case 114:
        YY_RULE_SETUP
#line 1067 "pgc.l"
        {
          return S_ADD;
        }
        YY_BREAK
      case 115:
        YY_RULE_SETUP
#line 1068 "pgc.l"
        {
          return S_SUB;
        }
        YY_BREAK
      case 116:
        YY_RULE_SETUP
#line 1069 "pgc.l"
        {
          return S_MUL;
        }
        YY_BREAK
      case 117:
        YY_RULE_SETUP
#line 1070 "pgc.l"
        {
          return S_DIV;
        }
        YY_BREAK
      case 118:
        YY_RULE_SETUP
#line 1071 "pgc.l"
        {
          return S_MOD;
        }
        YY_BREAK
      case 119:
        YY_RULE_SETUP
#line 1072 "pgc.l"
        {
          return S_MEMPOINT;
        }
        YY_BREAK
      case 120:
        YY_RULE_SETUP
#line 1073 "pgc.l"
        {
          return S_DOTPOINT;
        }
        YY_BREAK
      case 121:
        YY_RULE_SETUP
#line 1074 "pgc.l"
        {
          return S_ANYTHING;
        }
        YY_BREAK
      case 122:
                                    
        YY_RULE_SETUP
#line 1075 "pgc.l"
        {
          BEGIN(def_ident);
        }
        YY_BREAK
      case 123:
                                    
        YY_RULE_SETUP
#line 1076 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            BEGIN(def_ident);
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 124:
                                    
        YY_RULE_SETUP
#line 1088 "pgc.l"
        {
          BEGIN(undef);
        }
        YY_BREAK
      case 125:
                                    
        YY_RULE_SETUP
#line 1089 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            BEGIN(undef);
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 126:
                                    
        YY_RULE_SETUP
#line 1101 "pgc.l"
        {
          struct _defines *ptr, *ptr2 = NULL;
          int i;

             
                                                                    
                                                                    
             
          for (i = strlen(yytext) - 2; i > 0 && ecpg_isspace(yytext[i]); i--)
            ;
          yytext[i + 1] = '\0';

          for (ptr = defines; ptr != NULL; ptr2 = ptr, ptr = ptr->next)
          {
            if (strcmp(yytext, ptr->olddef) == 0)
            {
              if (ptr2 == NULL)
              {
                defines = ptr->next;
              }
              else
              {
                ptr2->next = ptr->next;
              }
              free(ptr->newdef);
              free(ptr->olddef);
              free(ptr);
              break;
            }
          }

          BEGIN(C);
        }
        YY_BREAK
      case 127:
                                    
        YY_RULE_SETUP
#line 1133 "pgc.l"
        {
          mmfatal(PARSE_ERROR, "missing identifier in EXEC SQL UNDEF command");
          yyterminate();
        }
        YY_BREAK
      case 128:
                                    
        YY_RULE_SETUP
#line 1137 "pgc.l"
        {
          BEGIN(incl);
        }
        YY_BREAK
      case 129:
                                    
        YY_RULE_SETUP
#line 1138 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            BEGIN(incl);
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 130:
                                    
        YY_RULE_SETUP
#line 1150 "pgc.l"
        {
          ifcond = true;
          BEGIN(xcond);
        }
        YY_BREAK
      case 131:
                                    
        YY_RULE_SETUP
#line 1151 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            ifcond = true;
            BEGIN(xcond);
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 132:
                                    
        YY_RULE_SETUP
#line 1164 "pgc.l"
        {
          ifcond = false;
          BEGIN(xcond);
        }
        YY_BREAK
      case 133:
                                    
        YY_RULE_SETUP
#line 1165 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            ifcond = false;
            BEGIN(xcond);
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 134:
                                    
        YY_RULE_SETUP
#line 1178 "pgc.l"
        {                
          if (preproc_tos == 0)
          {
            mmfatal(PARSE_ERROR, "missing matching \"EXEC SQL IFDEF\" / \"EXEC SQL IFNDEF\"");
          }
          else if (stacked_if_value[preproc_tos].else_branch)
          {
            mmfatal(PARSE_ERROR, "missing \"EXEC SQL ENDIF;\"");
          }
          else
          {
            preproc_tos--;
          }

          ifcond = true;
          BEGIN(xcond);
        }
        YY_BREAK
      case 135:
                                    
        YY_RULE_SETUP
#line 1189 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            if (preproc_tos == 0)
            {
              mmfatal(PARSE_ERROR, "missing matching \"EXEC SQL IFDEF\" / \"EXEC SQL IFNDEF\"");
            }
            else if (stacked_if_value[preproc_tos].else_branch)
            {
              mmfatal(PARSE_ERROR, "missing \"EXEC SQL ENDIF;\"");
            }
            else
            {
              preproc_tos--;
            }

            ifcond = true;
            BEGIN(xcond);
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 136:
                                    
        YY_RULE_SETUP
#line 1210 "pgc.l"
        {                                                                            
          if (stacked_if_value[preproc_tos].else_branch)
          {
            mmfatal(PARSE_ERROR, "more than one EXEC SQL ELSE");
          }
          else
          {
            stacked_if_value[preproc_tos].else_branch = true;
            stacked_if_value[preproc_tos].condition = (stacked_if_value[preproc_tos - 1].condition && !stacked_if_value[preproc_tos].condition);

            if (stacked_if_value[preproc_tos].condition)
            {
              BEGIN(C);
            }
            else
            {
              BEGIN(xskip);
            }
          }
        }
        YY_BREAK
      case 137:
                                    
        YY_RULE_SETUP
#line 1226 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            if (stacked_if_value[preproc_tos].else_branch)
            {
              mmfatal(PARSE_ERROR, "more than one EXEC SQL ELSE");
            }
            else
            {
              stacked_if_value[preproc_tos].else_branch = true;
              stacked_if_value[preproc_tos].condition = (stacked_if_value[preproc_tos - 1].condition && !stacked_if_value[preproc_tos].condition);

              if (stacked_if_value[preproc_tos].condition)
              {
                BEGIN(C);
              }
              else
              {
                BEGIN(xskip);
              }
            }
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 138:
                                    
        YY_RULE_SETUP
#line 1251 "pgc.l"
        {
          if (preproc_tos == 0)
          {
            mmfatal(PARSE_ERROR, "unmatched EXEC SQL ENDIF");
          }
          else
          {
            preproc_tos--;
          }

          if (stacked_if_value[preproc_tos].condition)
          {
            BEGIN(C);
          }
          else
          {
            BEGIN(xskip);
          }
        }
        YY_BREAK
      case 139:
                                    
        YY_RULE_SETUP
#line 1262 "pgc.l"
        {
                                           
          if (INFORMIX_MODE)
          {
            if (preproc_tos == 0)
            {
              mmfatal(PARSE_ERROR, "unmatched EXEC SQL ENDIF");
            }
            else
            {
              preproc_tos--;
            }

            if (stacked_if_value[preproc_tos].condition)
            {
              BEGIN(C);
            }
            else
            {
              BEGIN(xskip);
            }
          }
          else
          {
            yyless(1);
            return S_ANYTHING;
          }
        }
        YY_BREAK
      case 140:
        YY_RULE_SETUP
#line 1283 "pgc.l"
        {             
        }
        YY_BREAK
      case 141:
                                    
        YY_RULE_SETUP
#line 1285 "pgc.l"
        {
          if (preproc_tos >= MAX_NESTED_IF - 1)
          {
            mmfatal(PARSE_ERROR, "too many nested EXEC SQL IFDEF conditions");
          }
          else
          {
            struct _defines *defptr;
            unsigned int i;

               
                                                                      
                                                                      
               
            for (i = strlen(yytext) - 2; i > 0 && ecpg_isspace(yytext[i]); i--)
              ;
            yytext[i + 1] = '\0';

            for (defptr = defines; defptr != NULL && strcmp(yytext, defptr->olddef) != 0; defptr = defptr->next)
                        ;

            preproc_tos++;
            stacked_if_value[preproc_tos].else_branch = false;
            stacked_if_value[preproc_tos].condition = (defptr ? ifcond : !ifcond) && stacked_if_value[preproc_tos - 1].condition;
          }

          if (stacked_if_value[preproc_tos].condition)
          {
            BEGIN(C);
          }
          else
          {
            BEGIN(xskip);
          }
        }
        YY_BREAK
      case 142:
                                    
        YY_RULE_SETUP
#line 1321 "pgc.l"
        {
          mmfatal(PARSE_ERROR, "missing identifier in EXEC SQL IFDEF command");
          yyterminate();
        }
        YY_BREAK
      case 143:
        YY_RULE_SETUP
#line 1325 "pgc.l"
        {
          old = mm_strdup(yytext);
          BEGIN(def);
          startlit();
        }
        YY_BREAK
      case 144:
                                    
        YY_RULE_SETUP
#line 1330 "pgc.l"
        {
          mmfatal(PARSE_ERROR, "missing identifier in EXEC SQL DEFINE command");
          yyterminate();
        }
        YY_BREAK
      case 145:
                                    
        YY_RULE_SETUP
#line 1334 "pgc.l"
        {
          struct _defines *ptr, *this;

          for (ptr = defines; ptr != NULL; ptr = ptr->next)
          {
            if (strcmp(old, ptr->olddef) == 0)
            {
              free(ptr->newdef);
              ptr->newdef = mm_strdup(literalbuf);
            }
          }
          if (ptr == NULL)
          {
            this = (struct _defines *)mm_alloc(sizeof(struct _defines));

                                    
            this->olddef = old;
            this->newdef = mm_strdup(literalbuf);
            this->next = defines;
            this->used = NULL;
            defines = this;
          }

          BEGIN(C);
        }
        YY_BREAK
      case 146:
                                    
        YY_RULE_SETUP
#line 1359 "pgc.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 147:
                                    
        YY_RULE_SETUP
#line 1360 "pgc.l"
        {
          parse_include();
        }
        YY_BREAK
      case 148:
                                    
        YY_RULE_SETUP
#line 1361 "pgc.l"
        {
          parse_include();
        }
        YY_BREAK
      case 149:
                                    
        YY_RULE_SETUP
#line 1362 "pgc.l"
        {
          parse_include();
        }
        YY_BREAK
      case 150:
                                    
        YY_RULE_SETUP
#line 1363 "pgc.l"
        {
          mmfatal(PARSE_ERROR, "syntax error in EXEC SQL INCLUDE command");
          yyterminate();
        }
        YY_BREAK
      case YY_STATE_EOF(INITIAL):
      case YY_STATE_EOF(xcond):
      case YY_STATE_EOF(xskip):
      case YY_STATE_EOF(C):
      case YY_STATE_EOF(SQL):
      case YY_STATE_EOF(incl):
      case YY_STATE_EOF(def):
      case YY_STATE_EOF(def_ident):
      case YY_STATE_EOF(undef):
#line 1368 "pgc.l"
      {
        if (yy_buffer == NULL)
        {
          if (preproc_tos > 0)
          {
            preproc_tos = 0;
            mmfatal(PARSE_ERROR, "missing \"EXEC SQL ENDIF;\"");
          }
          yyterminate();
        }
        else
        {
          struct _yy_buffer *yb = yy_buffer;
          int i;
          struct _defines *ptr;

          for (ptr = defines; ptr; ptr = ptr->next)
          {
            if (ptr->used == yy_buffer)
            {
              ptr->used = NULL;
              break;
            }
          }

          if (yyin != NULL)
          {
            fclose(yyin);
          }

          yy_delete_buffer(YY_CURRENT_BUFFER);
          yy_switch_to_buffer(yy_buffer->buffer);

          yylineno = yy_buffer->lineno;

                                                                           
          i = strcmp(input_filename, yy_buffer->filename);

          free(input_filename);
          input_filename = yy_buffer->filename;

          yy_buffer = yy_buffer->next;
          free(yb);

          if (i != 0)
          {
            output_line_number();
          }
        }
      }
        YY_BREAK
      case 151:
                                    
        YY_RULE_SETUP
#line 1414 "pgc.l"
        {
          mmfatal(PARSE_ERROR, "internal error: unreachable state; please report this to <pgsql-bugs@lists.postgresql.org>");
        }
        YY_BREAK
      case 152:
        YY_RULE_SETUP
#line 1416 "pgc.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 4360 "pgc.c"

      case YY_END_OF_BUFFER:
      {
                                                                
        int yy_amount_of_matched_text = (int)(yy_cp - (yytext_ptr)) - 1;

                                                      
        *yy_cp = (yy_hold_char);
        YY_RESTORE_YY_MORE_OFFSET

        if (YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_NEW)
        {
                                                              
                                                          
                                                          
                                                     
                                                           
                                                                 
                                                             
                                                                
             
          (yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
          YY_CURRENT_BUFFER_LVALUE->yy_input_file = yyin;
          YY_CURRENT_BUFFER_LVALUE->yy_buffer_status = YY_BUFFER_NORMAL;
        }

                                                                      
                                                                 
                                                                
                                                            
                                                              
                       
           
        if ((yy_c_buf_p) <= &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)])
        {                             
          yy_state_type yy_next_state;

          (yy_c_buf_p) = (yytext_ptr) + yy_amount_of_matched_text;

          yy_current_state = yy_get_previous_state();

                                                        
                                           
                                                        
                                                        
                                                           
                                                           
                                    
             

          yy_next_state = yy_try_NUL_trans(yy_current_state);

          yy_bp = (yytext_ptr) + YY_MORE_ADJ;

          if (yy_next_state)
          {
                                  
            yy_cp = ++(yy_c_buf_p);
            yy_current_state = yy_next_state;
            goto yy_match;
          }

          else
          {
            yy_cp = (yy_last_accepting_cpos);
            yy_current_state = (yy_last_accepting_state);
            goto yy_find_action;
          }
        }

        else
        {
          switch (yy_get_next_buffer())
          {
          case EOB_ACT_END_OF_FILE:
          {
            (yy_did_buffer_switch_on_eof) = 0;

            if (yywrap())
            {
                                                   
                                                     
                                           
                                                  
                                                   
                                                      
                                                     
                                            
                 
              (yy_c_buf_p) = (yytext_ptr) + YY_MORE_ADJ;

              yy_act = YY_STATE_EOF(YY_START);
              goto do_action;
            }

            else
            {
              if (!(yy_did_buffer_switch_on_eof))
              {
                YY_NEW_FILE;
              }
            }
            break;
          }

          case EOB_ACT_CONTINUE_SCAN:
            (yy_c_buf_p) = (yytext_ptr) + yy_amount_of_matched_text;

            yy_current_state = yy_get_previous_state();

            yy_cp = (yy_c_buf_p);
            yy_bp = (yytext_ptr) + YY_MORE_ADJ;
            goto yy_match;

          case EOB_ACT_LAST_MATCH:
            (yy_c_buf_p) = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)];

            yy_current_state = yy_get_previous_state();

            yy_cp = (yy_c_buf_p);
            yy_bp = (yytext_ptr) + YY_MORE_ADJ;
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
yy_get_next_buffer(void)
{
  char *dest = YY_CURRENT_BUFFER_LVALUE->yy_ch_buf;
  char *source = (yytext_ptr);
  int number_to_move, i;
  int ret_val;

  if ((yy_c_buf_p) > &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars) + 1])
  {
    YY_FATAL_ERROR("fatal flex scanner internal error--end of buffer missed");
  }

  if (YY_CURRENT_BUFFER_LVALUE->yy_fill_buffer == 0)
  {                                                       
    if ((yy_c_buf_p) - (yytext_ptr)-YY_MORE_ADJ == 1)
    {
                                                    
                                    
         
      return EOB_ACT_END_OF_FILE;
    }

    else
    {
                                                      
                     
         
      return EOB_ACT_LAST_MATCH;
    }
  }

                              

                                                 
  number_to_move = (int)((yy_c_buf_p) - (yytext_ptr)-1);

  for (i = 0; i < number_to_move; ++i)
  {
    *(dest++) = *(source++);
  }

  if (YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_EOF_PENDING)
  {
                                                                
                         
       
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars) = 0;
  }

  else
  {
    int num_to_read = YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;

    while (num_to_read <= 0)
    {                                               

                                                      
      YY_BUFFER_STATE b = YY_CURRENT_BUFFER_LVALUE;

      int yy_c_buf_p_offset = (int)((yy_c_buf_p)-b->yy_ch_buf);

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
                                                  
            yyrealloc((void *)b->yy_ch_buf, (yy_size_t)(b->yy_buf_size + 2));
      }
      else
      {
                                             
        b->yy_ch_buf = NULL;
      }

      if (!b->yy_ch_buf)
      {
        YY_FATAL_ERROR("fatal error - scanner input buffer overflow");
      }

      (yy_c_buf_p) = &b->yy_ch_buf[yy_c_buf_p_offset];

      num_to_read = YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;
    }

    if (num_to_read > YY_READ_BUF_SIZE)
    {
      num_to_read = YY_READ_BUF_SIZE;
    }

                            
    YY_INPUT((&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move]), (yy_n_chars), num_to_read);

    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
  }

  if ((yy_n_chars) == 0)
  {
    if (number_to_move == YY_MORE_ADJ)
    {
      ret_val = EOB_ACT_END_OF_FILE;
      yyrestart(yyin);
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

  if (((yy_n_chars) + number_to_move) > YY_CURRENT_BUFFER_LVALUE->yy_buf_size)
  {
                                                                  
    int new_size = (yy_n_chars) + number_to_move + ((yy_n_chars) >> 1);
    YY_CURRENT_BUFFER_LVALUE->yy_ch_buf = (char *)yyrealloc((void *)YY_CURRENT_BUFFER_LVALUE->yy_ch_buf, (yy_size_t)new_size);
    if (!YY_CURRENT_BUFFER_LVALUE->yy_ch_buf)
    {
      YY_FATAL_ERROR("out of dynamic memory in yy_get_next_buffer()");
    }
                                     
    YY_CURRENT_BUFFER_LVALUE->yy_buf_size = (int)(new_size - 2);
  }

  (yy_n_chars) += number_to_move;
  YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] = YY_END_OF_BUFFER_CHAR;
  YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars) + 1] = YY_END_OF_BUFFER_CHAR;

  (yytext_ptr) = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[0];

  return ret_val;
}

                                                                                

static yy_state_type
yy_get_previous_state(void)
{
  yy_state_type yy_current_state;
  char *yy_cp;

  yy_current_state = (yy_start);

  for (yy_cp = (yytext_ptr) + YY_MORE_ADJ; yy_cp < (yy_c_buf_p); ++yy_cp)
  {
    YY_CHAR yy_c = (*yy_cp ? yy_ec[YY_SC_TO_UI(*yy_cp)] : 1);
    if (yy_accept[yy_current_state])
    {
      (yy_last_accepting_state) = yy_current_state;
      (yy_last_accepting_cpos) = yy_cp;
    }
    while (yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state)
    {
      yy_current_state = (int)yy_def[yy_current_state];
      if (yy_current_state >= 826)
      {
        yy_c = yy_meta[yy_c];
      }
    }
    yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
  }

  return yy_current_state;
}

                                                                    
   
            
                                                   
   
static yy_state_type
yy_try_NUL_trans(yy_state_type yy_current_state)
{
  int yy_is_jam;
  char *yy_cp = (yy_c_buf_p);

  YY_CHAR yy_c = 1;
  if (yy_accept[yy_current_state])
  {
    (yy_last_accepting_state) = yy_current_state;
    (yy_last_accepting_cpos) = yy_cp;
  }
  while (yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state)
  {
    yy_current_state = (int)yy_def[yy_current_state];
    if (yy_current_state >= 826)
    {
      yy_c = yy_meta[yy_c];
    }
  }
  yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
  yy_is_jam = (yy_current_state == 825);

  return yy_is_jam ? 0 : yy_current_state;
}

#ifndef YY_NO_UNPUT

static void
yyunput(int c, char *yy_bp)
{
  char *yy_cp;

  yy_cp = (yy_c_buf_p);

                                         
  *yy_cp = (yy_hold_char);

  if (yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2)
  {                                           
                           
    int number_to_move = (yy_n_chars) + 2;
    char *dest = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[YY_CURRENT_BUFFER_LVALUE->yy_buf_size + 2];
    char *source = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move];

    while (source > YY_CURRENT_BUFFER_LVALUE->yy_ch_buf)
    {
      *--dest = *--source;
    }

    yy_cp += (int)(dest - source);
    yy_bp += (int)(dest - source);
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars) = (int)YY_CURRENT_BUFFER_LVALUE->yy_buf_size;

    if (yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2)
    {
      YY_FATAL_ERROR("flex scanner push-back overflow");
    }
  }

  *--yy_cp = (char)c;

  if (c == '\n')
  {
    --yylineno;
  }

  (yytext_ptr) = yy_bp;
  (yy_hold_char) = *yy_cp;
  (yy_c_buf_p) = yy_cp;
}

#endif

#ifndef YY_NO_INPUT
#ifdef __cplusplus
static int
yyinput(void)
#else
static int
input(void)
#endif

{
  int c;

  *(yy_c_buf_p) = (yy_hold_char);

  if (*(yy_c_buf_p) == YY_END_OF_BUFFER_CHAR)
  {
                                                                 
                                                               
                                                                
       
    if ((yy_c_buf_p) < &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)])
    {
                                  
      *(yy_c_buf_p) = '\0';
    }

    else
    {                      
      int offset = (int)((yy_c_buf_p) - (yytext_ptr));
      ++(yy_c_buf_p);

      switch (yy_get_next_buffer())
      {
      case EOB_ACT_LAST_MATCH:
                                           
                                         
                                           
                                         
                                         
                                            
                                             
                                   
           

                                  
        yyrestart(yyin);

                       

      case EOB_ACT_END_OF_FILE:
      {
        if (yywrap())
        {
          return 0;
        }

        if (!(yy_did_buffer_switch_on_eof))
        {
          YY_NEW_FILE;
        }
#ifdef __cplusplus
        return yyinput();
#else
        return input();
#endif
      }

      case EOB_ACT_CONTINUE_SCAN:
        (yy_c_buf_p) = (yytext_ptr) + offset;
        break;
      }
    }
  }

  c = *(unsigned char *)(yy_c_buf_p);                            
  *(yy_c_buf_p) = '\0';                                    
  (yy_hold_char) = *++(yy_c_buf_p);

  if (c == '\n')
  {

    yylineno++;
  };

  return c;
}
#endif                         

                                                    
                                        
   
                                                                          
   
void
yyrestart(FILE *input_file)
{

  if (!YY_CURRENT_BUFFER)
  {
    yyensure_buffer_stack();
    YY_CURRENT_BUFFER_LVALUE = yy_create_buffer(yyin, YY_BUF_SIZE);
  }

  yy_init_buffer(YY_CURRENT_BUFFER, input_file);
  yy_load_buffer_state();
}

                                        
                                           
   
   
void
yy_switch_to_buffer(YY_BUFFER_STATE new_buffer)
{

                                                                  
          
                            
                                       
     
  yyensure_buffer_stack();
  if (YY_CURRENT_BUFFER == new_buffer)
  {
    return;
  }

  if (YY_CURRENT_BUFFER)
  {
                                               
    *(yy_c_buf_p) = (yy_hold_char);
    YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
  }

  YY_CURRENT_BUFFER_LVALUE = new_buffer;
  yy_load_buffer_state();

                                                              
                                                            
                                                            
                                    
     
  (yy_did_buffer_switch_on_eof) = 1;
}

static void
yy_load_buffer_state(void)
{
  (yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
  (yytext_ptr) = (yy_c_buf_p) = YY_CURRENT_BUFFER_LVALUE->yy_buf_pos;
  yyin = YY_CURRENT_BUFFER_LVALUE->yy_input_file;
  (yy_hold_char) = *(yy_c_buf_p);
}

                                                   
                                  
                                                                                      
   
                                       
   
YY_BUFFER_STATE
yy_create_buffer(FILE *file, int size)
{
  YY_BUFFER_STATE b;

  b = (YY_BUFFER_STATE)yyalloc(sizeof(struct yy_buffer_state));
  if (!b)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_create_buffer()");
  }

  b->yy_buf_size = size;

                                                                         
                                                   
     
  b->yy_ch_buf = (char *)yyalloc((yy_size_t)(b->yy_buf_size + 2));
  if (!b->yy_ch_buf)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_create_buffer()");
  }

  b->yy_is_our_buffer = 1;

  yy_init_buffer(b, file);

  return b;
}

                        
                                                     
   
   
void
yy_delete_buffer(YY_BUFFER_STATE b)
{

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
    yyfree((void *)b->yy_ch_buf);
  }

  yyfree((void *)b);
}

                                          
                                                                        
                                           
   
static void
yy_init_buffer(YY_BUFFER_STATE b, FILE *file)

{
  int oerrno = errno;

  yy_flush_buffer(b);

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
yy_flush_buffer(YY_BUFFER_STATE b)
{
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
    yy_load_buffer_state();
  }
}

                                                               
                                                             
                  
                                     
   
   
void
yypush_buffer_state(YY_BUFFER_STATE new_buffer)
{
  if (new_buffer == NULL)
  {
    return;
  }

  yyensure_buffer_stack();

                                                      
  if (YY_CURRENT_BUFFER)
  {
                                               
    *(yy_c_buf_p) = (yy_hold_char);
    YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
    YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
  }

                                                        
  if (YY_CURRENT_BUFFER)
  {
    (yy_buffer_stack_top)++;
  }
  YY_CURRENT_BUFFER_LVALUE = new_buffer;

                                        
  yy_load_buffer_state();
  (yy_did_buffer_switch_on_eof) = 1;
}

                                                          
                                          
   
   
void
yypop_buffer_state(void)
{
  if (!YY_CURRENT_BUFFER)
  {
    return;
  }

  yy_delete_buffer(YY_CURRENT_BUFFER);
  YY_CURRENT_BUFFER_LVALUE = NULL;
  if ((yy_buffer_stack_top) > 0)
  {
    --(yy_buffer_stack_top);
  }

  if (YY_CURRENT_BUFFER)
  {
    yy_load_buffer_state();
    (yy_did_buffer_switch_on_eof) = 1;
  }
}

                                             
                                            
   
static void
yyensure_buffer_stack(void)
{
  yy_size_t num_to_alloc;

  if (!(yy_buffer_stack))
  {

                                                                            
                                                                         
                                           
       
    num_to_alloc = 1;                                                        
    (yy_buffer_stack) = (struct yy_buffer_state **)yyalloc(num_to_alloc * sizeof(struct yy_buffer_state *));
    if (!(yy_buffer_stack))
    {
      YY_FATAL_ERROR("out of dynamic memory in yyensure_buffer_stack()");
    }

    memset((yy_buffer_stack), 0, num_to_alloc * sizeof(struct yy_buffer_state *));

    (yy_buffer_stack_max) = num_to_alloc;
    (yy_buffer_stack_top) = 0;
    return;
  }

  if ((yy_buffer_stack_top) >= ((yy_buffer_stack_max)) - 1)
  {

                                                             
    yy_size_t grow_size = 8                          ;

    num_to_alloc = (yy_buffer_stack_max) + grow_size;
    (yy_buffer_stack) = (struct yy_buffer_state **)yyrealloc((yy_buffer_stack), num_to_alloc * sizeof(struct yy_buffer_state *));
    if (!(yy_buffer_stack))
    {
      YY_FATAL_ERROR("out of dynamic memory in yyensure_buffer_stack()");
    }

                                 
    memset((yy_buffer_stack) + (yy_buffer_stack_max), 0, grow_size * sizeof(struct yy_buffer_state *));
    (yy_buffer_stack_max) = num_to_alloc;
  }
}

                                                                                          
                                    
                                                         
   
                                                    
   
YY_BUFFER_STATE
yy_scan_buffer(char *base, yy_size_t size)
{
  YY_BUFFER_STATE b;

  if (size < 2 || base[size - 2] != YY_END_OF_BUFFER_CHAR || base[size - 1] != YY_END_OF_BUFFER_CHAR)
  {
                                                  
    return NULL;
  }

  b = (YY_BUFFER_STATE)yyalloc(sizeof(struct yy_buffer_state));
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

  yy_switch_to_buffer(b);

  return b;
}

                                                                                 
                                  
                                                
   
                                                    
                                                                         
                                  
   
YY_BUFFER_STATE
yy_scan_string(const char *yystr)
{

  return yy_scan_bytes(yystr, (int)strlen(yystr));
}

                                                                                        
                                    
                                          
                                                                                 
   
                                                    
   
YY_BUFFER_STATE
yy_scan_bytes(const char *yybytes, int _yybytes_len)
{
  YY_BUFFER_STATE b;
  char *buf;
  yy_size_t n;
  int i;

                                                                       
  n = (yy_size_t)(_yybytes_len + 2);
  buf = (char *)yyalloc(n);
  if (!buf)
  {
    YY_FATAL_ERROR("out of dynamic memory in yy_scan_bytes()");
  }

  for (i = 0; i < _yybytes_len; ++i)
  {
    buf[i] = yybytes[i];
  }

  buf[_yybytes_len] = buf[_yybytes_len + 1] = YY_END_OF_BUFFER_CHAR;

  b = yy_scan_buffer(buf, n);
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
yy_fatal_error(const char *msg)
{
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
    yytext[yyleng] = (yy_hold_char);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (yy_c_buf_p) = yytext + yyless_macro_arg;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    (yy_hold_char) = *(yy_c_buf_p);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    *(yy_c_buf_p) = '\0';                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    yyleng = yyless_macro_arg;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

                                                              

                                 
   
   
int
yyget_lineno(void)
{

  return yylineno;
}

                          
   
   
FILE *
yyget_in(void)
{
  return yyin;
}

                           
   
   
FILE *
yyget_out(void)
{
  return yyout;
}

                                         
   
   
int
yyget_leng(void)
{
  return yyleng;
}

                           
   
   

char *
yyget_text(void)
{
  return yytext;
}

                                 
                                   
   
   
void
yyset_lineno(int _line_number)
{

  yylineno = _line_number;
}

                                                            
                 
                                     
   
                            
   
void
yyset_in(FILE *_in_str)
{
  yyin = _in_str;
}

void
yyset_out(FILE *_out_str)
{
  yyout = _out_str;
}

int
yyget_debug(void)
{
  return yy_flex_debug;
}

void
yyset_debug(int _bdebug)
{
  yy_flex_debug = _bdebug;
}

static int
yy_init_globals(void)
{
                                                                  
                                                                           
     

                                                              
  yylineno = 1;

  (yy_buffer_stack) = NULL;
  (yy_buffer_stack_top) = 0;
  (yy_buffer_stack_max) = 0;
  (yy_c_buf_p) = NULL;
  (yy_init) = 0;
  (yy_start) = 0;

                       
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
yylex_destroy(void)
{

                                                      
  while (YY_CURRENT_BUFFER)
  {
    yy_delete_buffer(YY_CURRENT_BUFFER);
    YY_CURRENT_BUFFER_LVALUE = NULL;
    yypop_buffer_state();
  }

                                 
  yyfree((yy_buffer_stack));
  (yy_buffer_stack) = NULL;

                                                                                      
                                                     
  yy_init_globals();

  return 0;
}

   
                              
   

#ifndef yytext_ptr
static void
yy_flex_strncpy(char *s1, const char *s2, int n)
{

  int i;
  for (i = 0; i < n; ++i)
  {
    s1[i] = s2[i];
  }
}
#endif

#ifdef YY_NEED_STRLEN
static int
yy_flex_strlen(const char *s)
{
  int n;
  for (n = 0; s[n]; ++n)
    ;

  return n;
}
#endif

void *
yyalloc(yy_size_t size)
{
  return malloc(size);
}

void *
yyrealloc(void *ptr, yy_size_t size)
{

                                                             
                                                                
                                                                
                                                                
                                                                   
                                    
     
  return realloc(ptr, size);
}

void
yyfree(void *ptr)
{
  free((char *)ptr);                                        
}

#define YYTABLES_NAME "yytables"

#line 1416 "pgc.l"

                    

void
lex_init(void)
{
  braces_open = 0;
  parenths_open = 0;
  current_function = NULL;

  preproc_tos = 0;
  yylineno = 1;
  ifcond = true;
  stacked_if_value[preproc_tos].condition = ifcond;
  stacked_if_value[preproc_tos].else_branch = false;

                                                                     
  if (literalbuf == NULL)
  {
    literalalloc = 1024;
    literalbuf = (char *)mm_alloc(literalalloc);
  }
  startlit();

  BEGIN(C);
}

static void
addlit(char *ytext, int yleng)
{
                                
  if ((literallen + yleng) >= literalalloc)
  {
    do
    {
      literalalloc *= 2;
    } while ((literallen + yleng) >= literalalloc);
    literalbuf = (char *)realloc(literalbuf, literalalloc);
  }
                                          
  memcpy(literalbuf + literallen, ytext, yleng);
  literallen += yleng;
  literalbuf[literallen] = '\0';
}

static void
addlitchar(unsigned char ychar)
{
                                
  if ((literallen + 1) >= literalalloc)
  {
    literalalloc *= 2;
    literalbuf = (char *)realloc(literalbuf, literalalloc);
  }
                                          
  literalbuf[literallen] = ychar;
  literallen += 1;
  literalbuf[literallen] = '\0';
}

   
                                                                              
                                  
   
static int
process_integer_literal(const char *token, YYSTYPE *lval)
{
  int val;
  char *endptr;

  errno = 0;
  val = strtoint(token, &endptr, 10);
  if (*endptr != '\0' || errno == ERANGE)
  {
                                                                         
    lval->str = mm_strdup(token);
    return FCONST;
  }
  lval->ival = val;
  return ICONST;
}

static void
parse_include(void)
{
                                 
  struct _yy_buffer *yb;
  struct _include_path *ip;
  char inc_file[MAXPGPATH];
  unsigned int i;

  yb = mm_alloc(sizeof(struct _yy_buffer));

  yb->buffer = YY_CURRENT_BUFFER;
  yb->lineno = yylineno;
  yb->filename = input_filename;
  yb->next = yy_buffer;

  yy_buffer = yb;

     
                                                                     
                                                                   
     
  for (i = strlen(yytext) - 2; i > 0 && ecpg_isspace(yytext[i]); i--)
    ;

  if (yytext[i] == ';')
  {
    i--;
  }

  yytext[i + 1] = '\0';

  yyin = NULL;

                                                                         
                                                                                         
  if (yytext[0] == '"' && yytext[i] == '"' && ((compat != ECPG_COMPAT_INFORMIX && compat != ECPG_COMPAT_INFORMIX_SE) || yytext[1] == '/'))
  {
    yytext[i] = '\0';
    memmove(yytext, yytext + 1, strlen(yytext));

    strlcpy(inc_file, yytext, sizeof(inc_file));
    yyin = fopen(inc_file, "r");
    if (!yyin)
    {
      if (strlen(inc_file) <= 2 || strcmp(inc_file + strlen(inc_file) - 2, ".h") != 0)
      {
        strcat(inc_file, ".h");
        yyin = fopen(inc_file, "r");
      }
    }
  }
  else
  {
    if ((yytext[0] == '"' && yytext[i] == '"') || (yytext[0] == '<' && yytext[i] == '>'))
    {
      yytext[i] = '\0';
      memmove(yytext, yytext + 1, strlen(yytext));
    }

    for (ip = include_paths; yyin == NULL && ip != NULL; ip = ip->next)
    {
      if (strlen(ip->path) + strlen(yytext) + 4 > MAXPGPATH)
      {
        fprintf(stderr, _("Error: include path \"%s/%s\" is too long on line %d, skipping\n"), ip->path, yytext, yylineno);
        continue;
      }
      snprintf(inc_file, sizeof(inc_file), "%s/%s", ip->path, yytext);
      yyin = fopen(inc_file, "r");
      if (!yyin)
      {
        if (strcmp(inc_file + strlen(inc_file) - 2, ".h") != 0)
        {
          strcat(inc_file, ".h");
          yyin = fopen(inc_file, "r");
        }
      }
                                                                                
      if (yyin && include_next)
      {
        fclose(yyin);
        yyin = NULL;
        include_next = false;
      }
    }
  }
  if (!yyin)
  {
    mmfatal(NO_INCLUDE_FILE, "could not open include file \"%s\" on line %d", yytext, yylineno);
  }

  input_filename = mm_strdup(inc_file);
  yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
  yylineno = 1;
  output_line_number();

  BEGIN(C);
}

   
                                                                            
   
static bool
ecpg_isspace(char ch)
{
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f')
  {
    return true;
  }
  return false;
}

static bool
isdefine(void)
{
  struct _defines *ptr;

                       
  for (ptr = defines; ptr; ptr = ptr->next)
  {
    if (strcmp(yytext, ptr->olddef) == 0 && ptr->used == NULL)
    {
      struct _yy_buffer *yb;

      yb = mm_alloc(sizeof(struct _yy_buffer));

      yb->buffer = YY_CURRENT_BUFFER;
      yb->lineno = yylineno;
      yb->filename = mm_strdup(input_filename);
      yb->next = yy_buffer;

      ptr->used = yy_buffer = yb;

      yy_scan_string(ptr->newdef);
      return true;
    }
  }

  return false;
}

static bool
isinformixdefine(void)
{
  const char *new = NULL;

  if (strcmp(yytext, "dec_t") == 0)
  {
    new = "decimal";
  }
  else if (strcmp(yytext, "intrvl_t") == 0)
  {
    new = "interval";
  }
  else if (strcmp(yytext, "dtime_t") == 0)
  {
    new = "timestamp";
  }

  if (new)
  {
    struct _yy_buffer *yb;

    yb = mm_alloc(sizeof(struct _yy_buffer));

    yb->buffer = YY_CURRENT_BUFFER;
    yb->lineno = yylineno;
    yb->filename = mm_strdup(input_filename);
    yb->next = yy_buffer;
    yy_buffer = yb;

    yy_scan_string(new);
    return true;
  }

  return false;
}
