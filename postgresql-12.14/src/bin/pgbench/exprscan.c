#line 2 "exprscan.c"

#line 4 "exprscan.c"

#define YY_INT_ALIGNED short int

                                         

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define expr_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer expr_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define expr_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer expr_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define expr_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer expr_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define expr_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string expr_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define expr_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes expr_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define expr_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer expr_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define expr_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer expr_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define expr_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state expr_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define expr_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer expr_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define expr_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state expr_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define expr_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state expr_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define expr_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack expr_yyensure_buffer_stack
#endif

#ifdef yylex
#define expr_yylex_ALREADY_DEFINED
#else
#define yylex expr_yylex
#endif

#ifdef yyrestart
#define expr_yyrestart_ALREADY_DEFINED
#else
#define yyrestart expr_yyrestart
#endif

#ifdef yylex_init
#define expr_yylex_init_ALREADY_DEFINED
#else
#define yylex_init expr_yylex_init
#endif

#ifdef yylex_init_extra
#define expr_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra expr_yylex_init_extra
#endif

#ifdef yylex_destroy
#define expr_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy expr_yylex_destroy
#endif

#ifdef yyget_debug
#define expr_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug expr_yyget_debug
#endif

#ifdef yyset_debug
#define expr_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug expr_yyset_debug
#endif

#ifdef yyget_extra
#define expr_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra expr_yyget_extra
#endif

#ifdef yyset_extra
#define expr_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra expr_yyset_extra
#endif

#ifdef yyget_in
#define expr_yyget_in_ALREADY_DEFINED
#else
#define yyget_in expr_yyget_in
#endif

#ifdef yyset_in
#define expr_yyset_in_ALREADY_DEFINED
#else
#define yyset_in expr_yyset_in
#endif

#ifdef yyget_out
#define expr_yyget_out_ALREADY_DEFINED
#else
#define yyget_out expr_yyget_out
#endif

#ifdef yyset_out
#define expr_yyset_out_ALREADY_DEFINED
#else
#define yyset_out expr_yyset_out
#endif

#ifdef yyget_leng
#define expr_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng expr_yyget_leng
#endif

#ifdef yyget_text
#define expr_yyget_text_ALREADY_DEFINED
#else
#define yyget_text expr_yyget_text
#endif

#ifdef yyget_lineno
#define expr_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno expr_yyget_lineno
#endif

#ifdef yyset_lineno
#define expr_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno expr_yyset_lineno
#endif

#ifdef yyget_column
#define expr_yyget_column_ALREADY_DEFINED
#else
#define yyget_column expr_yyget_column
#endif

#ifdef yyset_column
#define expr_yyset_column_ALREADY_DEFINED
#else
#define yyset_column expr_yyset_column
#endif

#ifdef yywrap
#define expr_yywrap_ALREADY_DEFINED
#else
#define yywrap expr_yywrap
#endif

#ifdef yyget_lval
#define expr_yyget_lval_ALREADY_DEFINED
#else
#define yyget_lval expr_yyget_lval
#endif

#ifdef yyset_lval
#define expr_yyset_lval_ALREADY_DEFINED
#else
#define yyset_lval expr_yyset_lval
#endif

#ifdef yyalloc
#define expr_yyalloc_ALREADY_DEFINED
#else
#define yyalloc expr_yyalloc
#endif

#ifdef yyrealloc
#define expr_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc expr_yyrealloc
#endif

#ifdef yyfree
#define expr_yyfree_ALREADY_DEFINED
#else
#define yyfree expr_yyfree
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

                      

#define expr_yywrap(yyscanner) (              1)
#define YY_SKIP_YYWRAP
typedef flex_uint8_t YY_CHAR;

typedef int yy_state_type;

#define yytext_ptr yytext_r

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
#define YY_NUM_RULES 51
#define YY_END_OF_BUFFER 52
                                            
                                    
struct yy_trans_info
{
  flex_int32_t yy_verify;
  flex_int32_t yy_nxt;
};
static const flex_int16_t yy_accept[129] = {0, 0, 0, 0, 0, 52, 1, 3, 5, 1, 50, 47, 49, 50, 22, 10, 21, 24, 25, 8, 6, 26, 7, 50, 9, 43, 43, 50, 18, 11, 19, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 50, 20, 23, 1, 1, 3, 4, 0, 47, 13, 45, 44, 43, 0, 43, 38, 16, 14, 12, 15, 17, 46, 46, 46, 46, 46, 46, 30, 46, 46, 28, 46, 46, 46, 48, 0, 2, 0, 0, 44, 0, 44, 43, 27, 46, 46, 37, 46, 46, 29, 46, 46, 46, 46, 0, 45, 43, 33, 36, 46,

    46, 46, 39, 35, 40, 34, 43, 41, 46, 46, 43, 31, 46, 43, 32, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 0};

static const YY_CHAR yy_ec[256] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 2, 2, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 5, 1, 6, 1, 7, 8, 1, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 1, 28, 29, 30, 1, 1, 31, 32, 33, 34, 35, 36, 32, 37, 38, 32, 32, 39, 32, 40, 41, 32, 32, 42, 43, 44, 45, 32, 46, 32, 32, 32, 1, 47, 1, 1, 32, 1, 31, 32, 33, 34,

    35, 36, 32, 37, 38, 32, 32, 39, 32, 40, 41, 32, 32, 42, 43, 44, 45, 32, 46, 32, 32, 32, 1, 48, 1, 49, 1, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,

    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};

static const YY_CHAR yy_meta[50] = {0, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 4, 1, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 1, 1, 6, 6, 6, 6, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 1, 1, 1};

static const flex_int16_t yy_base[138] = {0, 0, 3, 50, 0, 204, 156, 6, 205, 97, 205, 7, 205, 173, 205, 205, 205, 205, 205, 205, 205, 205, 205, 0, 205, 0, 1, 0, 0, 205, 2, 161, 0, 169, 0, 168, 155, 0, 155, 0, 159, 9, 205, 205, 148, 99, 15, 205, 191, 19, 205, 158, 157, 172, 10, 171, 0, 205, 205, 205, 205, 205, 0, 155, 145, 144, 152, 146, 144, 139, 143, 0, 146, 135, 143, 205, 172, 205, 169, 13, 128, 32, 0, 14, 0, 120, 110, 0, 100, 97, 101, 101, 99, 103, 97, 92, 0, 29, 0, 0, 101,

    95, 88, 0, 0, 0, 0, 19, 0, 93, 92, 103, 0, 74, 88, 0, 89, 91, 92, 93, 97, 100, 101, 104, 102, 111, 110, 7, 205, 146, 153, 9, 157, 160, 163, 166, 171, 174};

static const flex_int16_t yy_def[138] = {0, 129, 129, 128, 3, 128, 130, 128, 128, 130, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 131, 128, 132, 132, 133, 128, 128, 128, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 128, 128, 128, 130, 130, 128, 128, 128, 128, 128, 131, 135, 26, 136, 26, 133, 128, 128, 128, 128, 128, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 128, 128, 128, 128, 137, 135, 136, 81, 26, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 137, 95, 26, 134, 134, 134,

    134, 134, 134, 134, 134, 134, 26, 134, 134, 134, 26, 134, 134, 26, 134, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 0, 128, 128, 128, 128, 128, 128, 128, 128, 128};

static const flex_int16_t yy_nxt[255] = {0, 128, 7, 8, 7, 7, 8, 7, 46, 49, 46, 49, 75, 76, 51, 52, 52, 46, 128, 46, 55, 49, 81, 49, 81, 95, 53, 95, 57, 58, 59, 60, 61, 53, 97, 54, 54, 72, 53, 65, 66, 69, 73, 111, 128, 70, 128, 9, 53, 107, 9, 10, 11, 12, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 27, 28, 29, 30, 31, 32, 33, 32, 34, 35, 32, 36, 32, 37, 38, 32, 32, 39, 32, 40, 41, 42, 43, 47,

    48, 77, 78, 128, 116, 128, 53, 53, 117, 53, 53, 53, 115, 118, 120, 53, 119, 121, 53, 53, 53, 114, 53, 122, 123, 124, 125, 126, 53, 53, 113, 112, 110, 109, 127, 108, 106, 105, 104, 103, 102, 101, 100, 45, 99, 45, 6, 6, 6, 6, 6, 6, 6, 44, 98, 44, 44, 44, 44, 44, 53, 53, 54, 53, 56, 56, 56, 62, 62, 62, 80, 77, 80, 82, 75, 82, 96, 94, 96, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 53, 54, 79, 47, 45, 74, 71, 68, 67, 64,

    63, 50, 45, 128, 5, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};

static const flex_int16_t yy_chk[255] = {0, 0, 1, 1, 1, 2, 2, 2, 7, 11, 7, 11, 41, 41, 131, 25, 26, 46, 0, 46, 26, 49, 54, 49, 54, 79, 127, 79, 28, 28, 28, 30, 30, 83, 83, 25, 26, 39, 107, 34, 34, 37, 39, 107, 81, 37, 81, 1, 97, 97, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 9,

    9, 45, 45, 95, 114, 95, 114, 116, 116, 117, 118, 119, 113, 117, 119, 120, 118, 120, 121, 122, 124, 111, 123, 121, 122, 123, 124, 125, 126, 125, 110, 109, 102, 101, 126, 100, 94, 93, 92, 91, 90, 89, 88, 9, 86, 45, 129, 129, 129, 129, 129, 129, 129, 130, 85, 130, 130, 130, 130, 130, 132, 132, 80, 132, 133, 133, 133, 134, 134, 134, 135, 78, 135, 136, 76, 136, 137, 74, 137, 73, 72, 70, 69, 68, 67, 66, 65, 64, 63, 55, 53, 52, 51, 48, 44, 40, 38, 36, 35, 33,

    31, 13, 6, 5, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
#line 1 "exprscan.l"
#line 2 "exprscan.l"
                                                                            
   
              
                                                    
   
                                            
   
                                                                          
                                                                       
   
                                                                       
   
                                                               
   
                                                                             
   
                                                                         
                                                                        
   
                              
   
                                                                            
   

#include "fe_utils/psqlscan_int.h"

                                                             
static const char *expr_source = NULL;
static int expr_lineno = 0;
static int expr_start_offset = 0;
static const char *expr_command = NULL;

                                                        
static bool last_was_newline = false;

   
                                                                         
                                                                            
                                                                        
                                          
   
extern int
expr_yyget_column(yyscan_t yyscanner);
extern void
expr_yyset_column(int column_no, yyscan_t yyscanner);

                     

#line 790 "exprscan.c"
                                                                  
#define YY_NO_INPUT 1
                       
                                                                  
                              
                               
                      

#line 799 "exprscan.c"

#define INITIAL 0
#define EXPR 1

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
#line 93 "exprscan.l"

#line 97 "exprscan.l"
                                                                      
    PsqlScanState cur_state = yyextra;

       
                                                                       
                                                                         
                                                                           
                                                                         
                                                                           
       
    BEGIN(cur_state->start_state);

                                
    last_was_newline = false;

                       

#line 1095 "exprscan.c"

    while (              1) /* loops until end-of-file is reached */
    {
      yy_cp = yyg->yy_c_buf_p;

                              
      *yy_cp = yyg->yy_hold_char;

                                                                   
                          
         
      yy_bp = yy_cp;

      yy_current_state = yyg->yy_start;
    yy_match:
      do
      {
        YY_CHAR yy_c = yy_ec[YY_SC_TO_UI(*yy_cp)];
        if (yy_accept[yy_current_state])
        {
          yyg->yy_last_accepting_state = yy_current_state;
          yyg->yy_last_accepting_cpos = yy_cp;
        }
        while (yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state)
        {
          yy_current_state = (int)yy_def[yy_current_state];
          if (yy_current_state >= 129)
          {
            yy_c = yy_meta[yy_c];
          }
        }
        yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
        ++yy_cp;
      } while (yy_current_state != 128);
      yy_cp = yyg->yy_last_accepting_cpos;
      yy_current_state = yyg->yy_last_accepting_state;

    yy_find_action:
      yy_act = yy_accept[yy_current_state];

      YY_DO_BEFORE_ACTION;

    do_action:                                                     

      switch (yy_act)
      {                                       
      case 0:                   
                                                     
        *yy_cp = yyg->yy_hold_char;
        yy_cp = yyg->yy_last_accepting_cpos;
        yy_current_state = yyg->yy_last_accepting_state;
        goto yy_find_action;

      case 1:
        YY_RULE_SETUP
#line 115 "exprscan.l"
        {
                                                
          psqlscan_emit(cur_state, yytext, yyleng);
          return 1;
        }
        YY_BREAK
         
                                                                             
                                                  
         
      case 2:
                                  
        YY_RULE_SETUP
#line 125 "exprscan.l"
        {
                                                                
          int wordlen = yyleng - 2;
          if (yytext[wordlen] == '\r')
          {
            wordlen--;
          }
          Assert(yytext[wordlen] == '\\');
          psqlscan_emit(cur_state, yytext, wordlen);
          return 1;
        }
        YY_BREAK
      case 3:
        YY_RULE_SETUP
#line 135 "exprscan.l"
        {             
        }
        YY_BREAK
      case 4:
                                  
        YY_RULE_SETUP
#line 137 "exprscan.l"
        {             
        }
        YY_BREAK
      case 5:
                                  
        YY_RULE_SETUP
#line 139 "exprscan.l"
        {
                                     
          last_was_newline = true;
          return 0;
        }
        YY_BREAK
                        

      case 6:
        YY_RULE_SETUP
#line 149 "exprscan.l"
        {
          return '+';
        }
        YY_BREAK
      case 7:
        YY_RULE_SETUP
#line 150 "exprscan.l"
        {
          return '-';
        }
        YY_BREAK
      case 8:
        YY_RULE_SETUP
#line 151 "exprscan.l"
        {
          return '*';
        }
        YY_BREAK
      case 9:
        YY_RULE_SETUP
#line 152 "exprscan.l"
        {
          return '/';
        }
        YY_BREAK
      case 10:
        YY_RULE_SETUP
#line 153 "exprscan.l"
        {
          return '%';
        }                                
        YY_BREAK
      case 11:
        YY_RULE_SETUP
#line 154 "exprscan.l"
        {
          return '=';
        }
        YY_BREAK
      case 12:
        YY_RULE_SETUP
#line 155 "exprscan.l"
        {
          return NE_OP;
        }
        YY_BREAK
      case 13:
        YY_RULE_SETUP
#line 156 "exprscan.l"
        {
          return NE_OP;
        }                                
        YY_BREAK
      case 14:
        YY_RULE_SETUP
#line 157 "exprscan.l"
        {
          return LE_OP;
        }
        YY_BREAK
      case 15:
        YY_RULE_SETUP
#line 158 "exprscan.l"
        {
          return GE_OP;
        }
        YY_BREAK
      case 16:
        YY_RULE_SETUP
#line 159 "exprscan.l"
        {
          return LS_OP;
        }
        YY_BREAK
      case 17:
        YY_RULE_SETUP
#line 160 "exprscan.l"
        {
          return RS_OP;
        }
        YY_BREAK
      case 18:
        YY_RULE_SETUP
#line 161 "exprscan.l"
        {
          return '<';
        }
        YY_BREAK
      case 19:
        YY_RULE_SETUP
#line 162 "exprscan.l"
        {
          return '>';
        }
        YY_BREAK
      case 20:
        YY_RULE_SETUP
#line 163 "exprscan.l"
        {
          return '|';
        }
        YY_BREAK
      case 21:
        YY_RULE_SETUP
#line 164 "exprscan.l"
        {
          return '&';
        }
        YY_BREAK
      case 22:
        YY_RULE_SETUP
#line 165 "exprscan.l"
        {
          return '#';
        }
        YY_BREAK
      case 23:
        YY_RULE_SETUP
#line 166 "exprscan.l"
        {
          return '~';
        }
        YY_BREAK
      case 24:
        YY_RULE_SETUP
#line 168 "exprscan.l"
        {
          return '(';
        }
        YY_BREAK
      case 25:
        YY_RULE_SETUP
#line 169 "exprscan.l"
        {
          return ')';
        }
        YY_BREAK
      case 26:
        YY_RULE_SETUP
#line 170 "exprscan.l"
        {
          return ',';
        }
        YY_BREAK
      case 27:
        YY_RULE_SETUP
#line 172 "exprscan.l"
        {
          return AND_OP;
        }
        YY_BREAK
      case 28:
        YY_RULE_SETUP
#line 173 "exprscan.l"
        {
          return OR_OP;
        }
        YY_BREAK
      case 29:
        YY_RULE_SETUP
#line 174 "exprscan.l"
        {
          return NOT_OP;
        }
        YY_BREAK
      case 30:
        YY_RULE_SETUP
#line 175 "exprscan.l"
        {
          return IS_OP;
        }
        YY_BREAK
      case 31:
        YY_RULE_SETUP
#line 176 "exprscan.l"
        {
          return ISNULL_OP;
        }
        YY_BREAK
      case 32:
        YY_RULE_SETUP
#line 177 "exprscan.l"
        {
          return NOTNULL_OP;
        }
        YY_BREAK
      case 33:
        YY_RULE_SETUP
#line 179 "exprscan.l"
        {
          return CASE_KW;
        }
        YY_BREAK
      case 34:
        YY_RULE_SETUP
#line 180 "exprscan.l"
        {
          return WHEN_KW;
        }
        YY_BREAK
      case 35:
        YY_RULE_SETUP
#line 181 "exprscan.l"
        {
          return THEN_KW;
        }
        YY_BREAK
      case 36:
        YY_RULE_SETUP
#line 182 "exprscan.l"
        {
          return ELSE_KW;
        }
        YY_BREAK
      case 37:
        YY_RULE_SETUP
#line 183 "exprscan.l"
        {
          return END_KW;
        }
        YY_BREAK
      case 38:
        YY_RULE_SETUP
#line 185 "exprscan.l"
        {
          yylval->str = pg_strdup(yytext + 1);
          return VARIABLE;
        }
        YY_BREAK
      case 39:
        YY_RULE_SETUP
#line 190 "exprscan.l"
        {
          return NULL_CONST;
        }
        YY_BREAK
      case 40:
        YY_RULE_SETUP
#line 191 "exprscan.l"
        {
          yylval->bval = true;
          return BOOLEAN_CONST;
        }
        YY_BREAK
      case 41:
        YY_RULE_SETUP
#line 195 "exprscan.l"
        {
          yylval->bval = false;
          return BOOLEAN_CONST;
        }
        YY_BREAK
      case 42:
        YY_RULE_SETUP
#line 199 "exprscan.l"
        {
             
                                                            
                                                                  
                                                                    
                                 
             
          return MAXINT_PLUS_ONE_CONST;
        }
        YY_BREAK
      case 43:
        YY_RULE_SETUP
#line 208 "exprscan.l"
        {
          if (!strtoint64(yytext, true, &yylval->ival))
          {
            expr_yyerror_more(yyscanner, "bigint constant overflow", strdup(yytext));
          }
          return INTEGER_CONST;
        }
        YY_BREAK
      case 44:
        YY_RULE_SETUP
#line 214 "exprscan.l"
        {
          if (!strtodouble(yytext, true, &yylval->dval))
          {
            expr_yyerror_more(yyscanner, "double constant overflow", strdup(yytext));
          }
          return DOUBLE_CONST;
        }
        YY_BREAK
      case 45:
        YY_RULE_SETUP
#line 220 "exprscan.l"
        {
          if (!strtodouble(yytext, true, &yylval->dval))
          {
            expr_yyerror_more(yyscanner, "double constant overflow", strdup(yytext));
          }
          return DOUBLE_CONST;
        }
        YY_BREAK
      case 46:
        YY_RULE_SETUP
#line 226 "exprscan.l"
        {
          yylval->str = pg_strdup(yytext);
          return FUNCTION;
        }
        YY_BREAK
      case 47:
        YY_RULE_SETUP
#line 231 "exprscan.l"
        {             
        }
        YY_BREAK
      case 48:
                                   
        YY_RULE_SETUP
#line 233 "exprscan.l"
        {             
        }
        YY_BREAK
      case 49:
                                   
        YY_RULE_SETUP
#line 235 "exprscan.l"
        {
                                     
          last_was_newline = true;
          return 0;
        }
        YY_BREAK
      case 50:
        YY_RULE_SETUP
#line 241 "exprscan.l"
        {
             
                                                                  
                                                 
             
          expr_yyerror_more(yyscanner, "unexpected character", pg_strdup(yytext));
                                                     
          return 0;
        }
        YY_BREAK

      case YY_STATE_EOF(INITIAL):
      case YY_STATE_EOF(EXPR):
#line 254 "exprscan.l"
      {
        if (cur_state->buffer_stack == NULL)
        {
          return 0;                           
        }

           
                                                              
                                 
           
        psqlscan_pop_buffer_stack(cur_state);
        psqlscan_select_top_buffer(cur_state);
      }
        YY_BREAK
      case 51:
        YY_RULE_SETUP
#line 266 "exprscan.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 1494 "exprscan.c"

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
            yy_cp = yyg->yy_last_accepting_cpos;
            yy_current_state = yyg->yy_last_accepting_state;
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
    YY_CHAR yy_c = (*yy_cp ? yy_ec[YY_SC_TO_UI(*yy_cp)] : 1);
    if (yy_accept[yy_current_state])
    {
      yyg->yy_last_accepting_state = yy_current_state;
      yyg->yy_last_accepting_cpos = yy_cp;
    }
    while (yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state)
    {
      yy_current_state = (int)yy_def[yy_current_state];
      if (yy_current_state >= 129)
      {
        yy_c = yy_meta[yy_c];
      }
    }
    yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
  }

  return yy_current_state;
}

                                                                    
   
            
                                                   
   
static yy_state_type
yy_try_NUL_trans(yy_state_type yy_current_state, yyscan_t yyscanner)
{
  int yy_is_jam;
  struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;                                                     
  char *yy_cp = yyg->yy_c_buf_p;

  YY_CHAR yy_c = 1;
  if (yy_accept[yy_current_state])
  {
    yyg->yy_last_accepting_state = yy_current_state;
    yyg->yy_last_accepting_cpos = yy_cp;
  }
  while (yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state)
  {
    yy_current_state = (int)yy_def[yy_current_state];
    if (yy_current_state >= 129)
    {
      yy_c = yy_meta[yy_c];
    }
  }
  yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
  yy_is_jam = (yy_current_state == 128);

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

#line 266 "exprscan.l"

                    

void
expr_yyerror_more(yyscan_t yyscanner, const char *message, const char *more)
{
  PsqlScanState state = yyget_extra(yyscanner);
  int error_detection_offset = expr_scanner_offset(state) - 1;
  YYSTYPE lval;
  char *full_line;

     
                                                                           
                                                                           
                                                                 
     
  if (!last_was_newline)
  {
    while (yylex(&lval, yyscanner))
                ;
  }

                                                          
  full_line = expr_scanner_get_substring(state, expr_start_offset, expr_scanner_offset(state), true);

  syntax_error(expr_source, expr_lineno, full_line, expr_command, message, more, error_detection_offset - expr_start_offset);
}

void
expr_yyerror(yyscan_t yyscanner, const char *message)
{
  expr_yyerror_more(yyscanner, message, NULL);
}

   
                                                                         
                                                                  
                                                           
   
bool
expr_lex_one_word(PsqlScanState state, PQExpBuffer word_buf, int *offset)
{
  int lexresult;
  YYSTYPE lval;

                                
  Assert(state->scanbufhandle != NULL);

                                 
  state->output_buf = word_buf;
  resetPQExpBuffer(word_buf);

                        
  if (state->buffer_stack != NULL)
  {
    yy_switch_to_buffer(state->buffer_stack->buf, state->scanner);
  }
  else
  {
    yy_switch_to_buffer(state->scanbufhandle, state->scanner);
  }

                       
  state->start_state = INITIAL;

                
  lexresult = yylex(&lval, state->scanner);

     
                                                                            
                                  
     
  if (lexresult)
  {
    *offset = expr_scanner_offset(state) - word_buf->len;
  }
  else
  {
    *offset = -1;
  }

     
                                                                             
                                
     
  psql_scan_reselect_sql_lexer(state);

  return (bool)lexresult;
}

   
                                                    
   
                                                                
                                                                       
   
yyscan_t
expr_scanner_init(PsqlScanState state, const char *source, int lineno, int start_offset, const char *command)
{
                               
  expr_source = source;
  expr_lineno = lineno;
  expr_start_offset = start_offset;
  expr_command = command;

                                
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

                       
  state->start_state = EXPR;

  return state->scanner;
}

   
                                
   
void
expr_scanner_finish(yyscan_t yyscanner)
{
  PsqlScanState state = yyget_extra(yyscanner);

     
                                                       
     
  psql_scan_reselect_sql_lexer(state);
}

   
                                                                  
   
                                                                          
                                                                         
                                                                           
                                                        
   
int
expr_scanner_offset(PsqlScanState state)
{
  return strlen(state->scanbuf);
}

   
                                                                   
                                                                   
               
   
char *
expr_scanner_get_substring(PsqlScanState state, int start_offset, int end_offset, bool chomp)
{
  char *result;
  const char *scanptr = state->scanbuf + start_offset;
  int slen = end_offset - start_offset;

  Assert(slen >= 0);
  Assert(end_offset <= strlen(state->scanbuf));

  if (chomp)
  {
    while (slen > 0 && (scanptr[slen - 1] == '\n' || scanptr[slen - 1] == '\r'))
    {
      slen--;
    }
  }

  result = (char *)pg_malloc(slen + 1);
  memcpy(result, scanptr, slen);
  result[slen] = '\0';

  return result;
}

   
                                                               
                                                             
   
int
expr_scanner_get_lineno(PsqlScanState state, int offset)
{
  int lineno = 1;
  const char *p = state->scanbuf;

  while (*p && offset > 0)
  {
    if (*p == '\n')
    {
      lineno++;
    }
    p++, offset--;
  }
  return lineno;
}
