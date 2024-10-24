#line 2 "repl_scanner.c"

#line 4 "repl_scanner.c"

#define YY_INT_ALIGNED short int

                                         

#define yy_create_buffer replication_yy_create_buffer
#define yy_delete_buffer replication_yy_delete_buffer
#define yy_scan_buffer replication_yy_scan_buffer
#define yy_scan_string replication_yy_scan_string
#define yy_scan_bytes replication_yy_scan_bytes
#define yy_init_buffer replication_yy_init_buffer
#define yy_flush_buffer replication_yy_flush_buffer
#define yy_load_buffer_state replication_yy_load_buffer_state
#define yy_switch_to_buffer replication_yy_switch_to_buffer
#define yypush_buffer_state replication_yypush_buffer_state
#define yypop_buffer_state replication_yypop_buffer_state
#define yyensure_buffer_stack replication_yyensure_buffer_stack
#define yy_flex_debug replication_yy_flex_debug
#define yyin replication_yyin
#define yyleng replication_yyleng
#define yylex replication_yylex
#define yylineno replication_yylineno
#define yyout replication_yyout
#define yyrestart replication_yyrestart
#define yytext replication_yytext
#define yywrap replication_yywrap
#define yyalloc replication_yyalloc
#define yyrealloc replication_yyrealloc
#define yyfree replication_yyfree

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define replication_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer replication_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define replication_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer replication_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define replication_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer replication_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define replication_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string replication_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define replication_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes replication_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define replication_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer replication_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define replication_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer replication_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define replication_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state replication_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define replication_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer replication_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define replication_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state replication_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define replication_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state replication_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define replication_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack replication_yyensure_buffer_stack
#endif

#ifdef yylex
#define replication_yylex_ALREADY_DEFINED
#else
#define yylex replication_yylex
#endif

#ifdef yyrestart
#define replication_yyrestart_ALREADY_DEFINED
#else
#define yyrestart replication_yyrestart
#endif

#ifdef yylex_init
#define replication_yylex_init_ALREADY_DEFINED
#else
#define yylex_init replication_yylex_init
#endif

#ifdef yylex_init_extra
#define replication_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra replication_yylex_init_extra
#endif

#ifdef yylex_destroy
#define replication_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy replication_yylex_destroy
#endif

#ifdef yyget_debug
#define replication_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug replication_yyget_debug
#endif

#ifdef yyset_debug
#define replication_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug replication_yyset_debug
#endif

#ifdef yyget_extra
#define replication_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra replication_yyget_extra
#endif

#ifdef yyset_extra
#define replication_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra replication_yyset_extra
#endif

#ifdef yyget_in
#define replication_yyget_in_ALREADY_DEFINED
#else
#define yyget_in replication_yyget_in
#endif

#ifdef yyset_in
#define replication_yyset_in_ALREADY_DEFINED
#else
#define yyset_in replication_yyset_in
#endif

#ifdef yyget_out
#define replication_yyget_out_ALREADY_DEFINED
#else
#define yyget_out replication_yyget_out
#endif

#ifdef yyset_out
#define replication_yyset_out_ALREADY_DEFINED
#else
#define yyset_out replication_yyset_out
#endif

#ifdef yyget_leng
#define replication_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng replication_yyget_leng
#endif

#ifdef yyget_text
#define replication_yyget_text_ALREADY_DEFINED
#else
#define yyget_text replication_yyget_text
#endif

#ifdef yyget_lineno
#define replication_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno replication_yyget_lineno
#endif

#ifdef yyset_lineno
#define replication_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno replication_yyset_lineno
#endif

#ifdef yywrap
#define replication_yywrap_ALREADY_DEFINED
#else
#define yywrap replication_yywrap
#endif

#ifdef yyalloc
#define replication_yyalloc_ALREADY_DEFINED
#else
#define yyalloc replication_yyalloc
#endif

#ifdef yyrealloc
#define replication_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc replication_yyrealloc
#endif

#ifdef yyfree
#define replication_yyfree_ALREADY_DEFINED
#else
#define yyfree replication_yyfree
#endif

#ifdef yytext
#define replication_yytext_ALREADY_DEFINED
#else
#define yytext replication_yytext
#endif

#ifdef yyleng
#define replication_yyleng_ALREADY_DEFINED
#else
#define yyleng replication_yyleng
#endif

#ifdef yyin
#define replication_yyin_ALREADY_DEFINED
#else
#define yyin replication_yyin
#endif

#ifdef yyout
#define replication_yyout_ALREADY_DEFINED
#else
#define yyout replication_yyout
#endif

#ifdef yy_flex_debug
#define replication_yy_flex_debug_ALREADY_DEFINED
#else
#define yy_flex_debug replication_yy_flex_debug
#endif

#ifdef yylineno
#define replication_yylineno_ALREADY_DEFINED
#else
#define yylineno replication_yylineno
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

#define YY_LESS_LINENO(n)
#define YY_LINENO_REWIND_TO(ptr)

                                                                               
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

                      

#define replication_yywrap() (              1)
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
#define YY_NUM_RULES 38
#define YY_END_OF_BUFFER 39
                                            
                                    
struct yy_trans_info
{
  flex_int32_t yy_verify;
  flex_int32_t yy_nxt;
};
static const flex_int16_t yy_accept[279] = {0, 0, 0, 0, 0, 0, 0, 39, 37, 26, 26, 33, 29, 27, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 35, 34, 32, 30, 26, 0, 27, 0, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 35, 32, 31, 28, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 9, 36, 36, 36, 36, 2, 36, 36, 36, 36, 36, 36,

    36, 36, 36, 36, 4, 20, 36, 36, 36, 36, 36, 25, 36, 36, 36, 36, 36, 5, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 6, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 19, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 8, 36, 36, 17, 7, 36, 36, 36, 36, 12, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 21, 36, 36, 36, 36, 36, 36, 36,

    36, 36, 36, 36, 36, 36, 36, 1, 36, 36, 36, 36, 36, 36, 18, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 24, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 10, 36, 36, 36, 22, 3, 36, 36, 36, 36, 36, 36, 36, 36, 36, 16, 36, 36, 23, 36, 13, 36, 36, 11, 36, 36, 36, 36, 36, 15, 36, 14, 0};

static const YY_CHAR yy_ec[256] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 4, 1, 5, 1, 1, 6, 1, 1, 1, 1, 1, 1, 1, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 18, 25, 26, 27, 28, 29, 30, 31, 32, 18, 1, 1, 1, 1, 33, 1, 34, 34, 34, 34,

    34, 34, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 1, 1, 1, 1, 1, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,

    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};

static const YY_CHAR yy_meta[35] = {0, 1, 1, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6};

static const flex_int16_t yy_base[286] = {0, 0, 0, 574, 573, 570, 569, 574, 579, 33, 35, 579, 579, 32, 34, 62, 40, 41, 46, 564, 565, 44, 55, 45, 56, 73, 46, 74, 75, 60, 51, 0, 579, 0, 565, 97, 0, 95, 563, 562, 559, 78, 78, 86, 88, 88, 98, 100, 101, 99, 106, 108, 110, 111, 115, 116, 117, 118, 113, 120, 135, 136, 0, 0, 579, 0, 137, 122, 125, 138, 139, 140, 144, 147, 50, 145, 152, 151, 148, 156, 160, 161, 162, 163, 165, 168, 170, 171, 172, 560, 173, 174, 175, 177, 559, 180, 183, 186, 187, 189, 191,

    188, 193, 202, 204, 558, 557, 207, 208, 210, 211, 212, 556, 213, 215, 217, 218, 219, 555, 223, 228, 232, 233, 234, 236, 239, 237, 241, 242, 244, 246, 249, 250, 247, 251, 253, 258, 255, 260, 263, 269, 554, 270, 266, 271, 274, 278, 282, 275, 286, 283, 289, 291, 293, 294, 553, 296, 297, 298, 300, 299, 301, 303, 304, 310, 305, 314, 320, 315, 316, 322, 324, 550, 325, 326, 542, 538, 330, 338, 335, 333, 334, 340, 336, 344, 346, 341, 345, 347, 363, 368, 349, 342, 521, 365, 369, 354, 372, 373, 375, 376,

    379, 380, 382, 381, 383, 386, 384, 520, 387, 388, 393, 398, 402, 399, 519, 403, 406, 408, 410, 411, 413, 414, 416, 418, 421, 422, 426, 419, 518, 429, 432, 434, 437, 438, 440, 441, 445, 444, 446, 447, 448, 451, 449, 453, 454, 517, 455, 459, 456, 516, 515, 467, 470, 474, 475, 476, 477, 478, 479, 480, 514, 481, 482, 512, 485, 511, 484, 486, 509, 487, 489, 494, 488, 497, 179, 502, 114, 579, 529, 535, 537, 541, 547, 553, 55};

static const flex_int16_t yy_def[286] = {0, 278, 1, 279, 279, 280, 280, 278, 278, 278, 278, 278, 278, 281, 282, 282, 15, 15, 15, 15, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 283, 278, 284, 278, 278, 285, 281, 281, 282, 15, 15, 282, 282, 282, 15, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 283, 284, 278, 285, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,

    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,

    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 0, 278, 278, 278, 278, 278, 278, 278};

static const flex_int16_t yy_nxt[614] = {0, 8, 9, 10, 11, 8, 12, 8, 13, 14, 15, 16, 17, 18, 19, 20, 20, 21, 20, 20, 22, 23, 24, 20, 25, 26, 27, 28, 29, 20, 30, 20, 20, 20, 14, 35, 35, 35, 35, 36, 37, 36, 40, 40, 40, 40, 40, 40, 40, 40, 40, 278, 278, 278, 49, 40, 46, 278, 278, 53, 61, 65, 278, 278, 47, 42, 43, 278, 40, 36, 40, 41, 40, 40, 40, 40, 40, 44, 48, 50, 278, 278, 278, 98, 57, 278, 60, 40, 58, 51, 54, 67, 59, 278, 55, 278, 40, 40, 52, 35, 35,

    56, 36, 37, 66, 278, 278, 278, 278, 68, 72, 71, 69, 278, 70, 278, 73, 278, 278, 75, 278, 278, 278, 278, 278, 278, 83, 278, 84, 278, 74, 91, 278, 79, 85, 76, 77, 80, 81, 82, 78, 86, 278, 278, 278, 278, 278, 278, 87, 92, 90, 278, 278, 88, 278, 278, 89, 96, 278, 278, 101, 93, 95, 278, 97, 100, 94, 278, 278, 278, 278, 103, 278, 104, 102, 278, 99, 278, 278, 278, 278, 278, 278, 110, 278, 108, 278, 278, 107, 106, 278, 105, 109, 278, 278, 278, 278, 119, 278, 112, 278,

    114, 116, 118, 111, 123, 113, 117, 115, 278, 124, 278, 120, 121, 278, 278, 122, 278, 278, 278, 278, 128, 278, 132, 278, 278, 278, 125, 133, 126, 278, 130, 137, 129, 127, 278, 136, 138, 131, 278, 278, 278, 134, 278, 278, 135, 278, 142, 278, 278, 140, 278, 143, 278, 278, 139, 278, 278, 278, 150, 278, 141, 278, 148, 152, 278, 144, 278, 146, 147, 278, 149, 154, 278, 145, 155, 278, 278, 278, 159, 151, 278, 278, 158, 161, 278, 153, 156, 157, 278, 278, 164, 160, 278, 167, 166, 278, 165, 278, 162, 278,

    278, 163, 278, 278, 278, 278, 278, 278, 172, 278, 278, 278, 179, 168, 169, 178, 278, 181, 170, 175, 278, 278, 278, 173, 176, 171, 278, 184, 278, 174, 278, 278, 278, 177, 180, 185, 278, 182, 183, 278, 278, 278, 278, 186, 278, 192, 278, 278, 278, 199, 278, 278, 278, 278, 205, 278, 187, 188, 189, 190, 278, 191, 198, 196, 193, 195, 194, 197, 204, 278, 200, 278, 201, 202, 278, 278, 203, 208, 278, 278, 206, 278, 278, 210, 207, 278, 278, 278, 278, 278, 278, 209, 278, 278, 278, 214, 221, 216, 211, 278,

    213, 215, 218, 220, 278, 278, 219, 212, 278, 278, 224, 225, 278, 226, 278, 217, 278, 278, 222, 278, 278, 230, 278, 223, 278, 278, 227, 278, 278, 232, 236, 235, 278, 228, 237, 278, 229, 239, 278, 231, 278, 234, 233, 278, 278, 238, 278, 278, 240, 242, 278, 278, 278, 278, 278, 278, 241, 278, 244, 278, 278, 278, 278, 243, 252, 278, 247, 245, 246, 249, 254, 251, 248, 278, 250, 256, 278, 257, 253, 255, 278, 278, 278, 278, 278, 278, 278, 278, 278, 258, 278, 278, 278, 278, 278, 278, 260, 259, 262, 265,

    278, 266, 267, 278, 264, 271, 261, 268, 278, 263, 269, 273, 272, 274, 275, 278, 270, 278, 278, 276, 278, 278, 278, 278, 278, 278, 278, 278, 277, 31, 31, 31, 31, 31, 31, 33, 33, 33, 33, 33, 33, 38, 38, 39, 278, 39, 39, 62, 278, 62, 62, 62, 62, 63, 63, 63, 278, 63, 63, 278, 278, 278, 278, 278, 278, 278, 278, 40, 278, 36, 64, 278, 45, 278, 34, 34, 32, 32, 7, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,

    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278};

static const flex_int16_t yy_chk[614] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 10, 10, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 16, 17, 21, 23, 26, 23, 18, 21, 74, 30, 26, 30, 285, 22, 24, 22, 16, 17, 29, 14, 15, 15, 15, 15, 15, 15, 15, 15, 18, 22, 24, 25, 27, 28, 74, 28, 42, 29, 41, 28, 25, 27, 42, 28, 43, 27, 44, 15, 45, 25, 35, 35,

    27, 37, 37, 41, 46, 49, 47, 48, 43, 47, 46, 44, 50, 45, 51, 48, 52, 53, 50, 58, 277, 54, 55, 56, 57, 56, 59, 57, 67, 49, 67, 68, 52, 58, 50, 50, 53, 54, 55, 51, 59, 60, 61, 66, 69, 70, 71, 60, 68, 66, 72, 75, 61, 73, 78, 61, 72, 77, 76, 77, 69, 71, 79, 73, 76, 70, 80, 81, 82, 83, 79, 84, 80, 78, 85, 75, 86, 87, 88, 90, 91, 92, 86, 93, 84, 275, 95, 83, 82, 96, 81, 85, 97, 98, 101, 99, 97, 100, 88, 102,

    91, 93, 96, 87, 101, 90, 95, 92, 103, 102, 104, 98, 99, 107, 108, 100, 109, 110, 111, 113, 108, 114, 113, 115, 116, 117, 103, 114, 104, 119, 110, 119, 109, 107, 120, 117, 120, 111, 121, 122, 123, 115, 124, 126, 116, 125, 124, 127, 128, 122, 129, 125, 130, 133, 121, 131, 132, 134, 132, 135, 123, 137, 130, 134, 136, 126, 138, 128, 129, 139, 131, 136, 143, 127, 137, 140, 142, 144, 142, 133, 145, 148, 140, 144, 146, 135, 138, 139, 147, 150, 147, 143, 149, 150, 149, 151, 148, 152, 145, 153,

    154, 146, 156, 157, 158, 160, 159, 161, 156, 162, 163, 165, 163, 151, 152, 162, 164, 165, 153, 159, 166, 168, 169, 157, 160, 154, 167, 168, 170, 158, 171, 173, 174, 161, 164, 169, 177, 166, 167, 180, 181, 179, 183, 170, 178, 179, 182, 186, 192, 186, 184, 187, 185, 188, 192, 191, 171, 173, 174, 177, 196, 178, 185, 183, 180, 182, 181, 184, 191, 189, 187, 194, 188, 189, 190, 195, 190, 196, 197, 198, 194, 199, 200, 198, 195, 201, 202, 204, 203, 205, 207, 197, 206, 209, 210, 202, 210, 204, 199, 211,

    201, 203, 206, 209, 212, 214, 207, 200, 213, 216, 213, 214, 217, 216, 218, 205, 219, 220, 211, 221, 222, 220, 223, 212, 224, 228, 217, 225, 226, 222, 226, 225, 227, 218, 227, 230, 219, 230, 231, 221, 232, 224, 223, 233, 234, 228, 235, 236, 231, 233, 238, 237, 239, 240, 241, 243, 232, 242, 235, 244, 245, 247, 249, 234, 243, 248, 238, 236, 237, 240, 245, 242, 239, 252, 241, 248, 253, 249, 244, 247, 254, 255, 256, 257, 258, 259, 260, 262, 263, 252, 267, 265, 268, 270, 273, 271, 254, 253, 256, 259,

    272, 260, 262, 274, 258, 268, 255, 263, 276, 257, 265, 271, 270, 272, 273, 269, 267, 266, 264, 274, 261, 251, 250, 246, 229, 215, 208, 193, 276, 279, 279, 279, 279, 279, 279, 280, 280, 280, 280, 280, 280, 281, 281, 282, 176, 282, 282, 283, 175, 283, 283, 283, 283, 284, 284, 284, 172, 284, 284, 155, 141, 118, 112, 106, 105, 94, 89, 40, 39, 38, 34, 20, 19, 7, 6, 5, 4, 3, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,

    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278};

static yy_state_type yy_last_accepting_state;
static char *yy_last_accepting_cpos;

extern int yy_flex_debug;
int yy_flex_debug = 0;

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *yytext;
#line 1 "repl_scanner.l"
#line 2 "repl_scanner.l"
                                                                            
   
                  
                                                    
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "utils/builtins.h"
#include "parser/scansup.h"

                                                                             
#undef fprintf
#define fprintf(file, fmt, msg) fprintf_to_ereport(fmt, msg)

static void
fprintf_to_ereport(const char *fmt, const char *msg)
{
  ereport(ERROR, (errmsg_internal("%s", msg)));
}

                                                         
static YY_BUFFER_STATE scanbufhandle;

                                            
static int repl_pushed_back_token;

                                       
static StringInfoData litbuf;

static void
startlit(void);
static char *
litbufdup(void);
static void
addlit(char *ytext, int yleng);
static void
addlitchar(unsigned char ychar);

                     

#line 976 "repl_scanner.c"
#define YY_NO_INPUT 1
   
                     
                                                           
                                        
   

                  
                                            
   
                
                                                                         
   
#line 990 "repl_scanner.c"

#define INITIAL 0
#define xd 1
#define xq 2

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
#line 95 "repl_scanner.l"

#line 99 "repl_scanner.l"
                                                                   

                                                      
    if (repl_pushed_back_token)
    {
      int result = repl_pushed_back_token;

      repl_pushed_back_token = 0;
      return result;
    }

#line 1224 "repl_scanner.c"

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
          if (yy_current_state >= 279)
          {
            yy_c = yy_meta[yy_c];
          }
        }
        yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
        ++yy_cp;
      } while (yy_current_state != 278);
      yy_cp = (yy_last_accepting_cpos);
      yy_current_state = (yy_last_accepting_state);

    yy_find_action:
      yy_act = yy_accept[yy_current_state];

      YY_DO_BEFORE_ACTION;

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
#line 111 "repl_scanner.l"
        {
          return K_BASE_BACKUP;
        }
        YY_BREAK
      case 2:
        YY_RULE_SETUP
#line 112 "repl_scanner.l"
        {
          return K_FAST;
        }
        YY_BREAK
      case 3:
        YY_RULE_SETUP
#line 113 "repl_scanner.l"
        {
          return K_IDENTIFY_SYSTEM;
        }
        YY_BREAK
      case 4:
        YY_RULE_SETUP
#line 114 "repl_scanner.l"
        {
          return K_SHOW;
        }
        YY_BREAK
      case 5:
        YY_RULE_SETUP
#line 115 "repl_scanner.l"
        {
          return K_LABEL;
        }
        YY_BREAK
      case 6:
        YY_RULE_SETUP
#line 116 "repl_scanner.l"
        {
          return K_NOWAIT;
        }
        YY_BREAK
      case 7:
        YY_RULE_SETUP
#line 117 "repl_scanner.l"
        {
          return K_PROGRESS;
        }
        YY_BREAK
      case 8:
        YY_RULE_SETUP
#line 118 "repl_scanner.l"
        {
          return K_MAX_RATE;
        }
        YY_BREAK
      case 9:
        YY_RULE_SETUP
#line 119 "repl_scanner.l"
        {
          return K_WAL;
        }
        YY_BREAK
      case 10:
        YY_RULE_SETUP
#line 120 "repl_scanner.l"
        {
          return K_TABLESPACE_MAP;
        }
        YY_BREAK
      case 11:
        YY_RULE_SETUP
#line 121 "repl_scanner.l"
        {
          return K_NOVERIFY_CHECKSUMS;
        }
        YY_BREAK
      case 12:
        YY_RULE_SETUP
#line 122 "repl_scanner.l"
        {
          return K_TIMELINE;
        }
        YY_BREAK
      case 13:
        YY_RULE_SETUP
#line 123 "repl_scanner.l"
        {
          return K_START_REPLICATION;
        }
        YY_BREAK
      case 14:
        YY_RULE_SETUP
#line 124 "repl_scanner.l"
        {
          return K_CREATE_REPLICATION_SLOT;
        }
        YY_BREAK
      case 15:
        YY_RULE_SETUP
#line 125 "repl_scanner.l"
        {
          return K_DROP_REPLICATION_SLOT;
        }
        YY_BREAK
      case 16:
        YY_RULE_SETUP
#line 126 "repl_scanner.l"
        {
          return K_TIMELINE_HISTORY;
        }
        YY_BREAK
      case 17:
        YY_RULE_SETUP
#line 127 "repl_scanner.l"
        {
          return K_PHYSICAL;
        }
        YY_BREAK
      case 18:
        YY_RULE_SETUP
#line 128 "repl_scanner.l"
        {
          return K_RESERVE_WAL;
        }
        YY_BREAK
      case 19:
        YY_RULE_SETUP
#line 129 "repl_scanner.l"
        {
          return K_LOGICAL;
        }
        YY_BREAK
      case 20:
        YY_RULE_SETUP
#line 130 "repl_scanner.l"
        {
          return K_SLOT;
        }
        YY_BREAK
      case 21:
        YY_RULE_SETUP
#line 131 "repl_scanner.l"
        {
          return K_TEMPORARY;
        }
        YY_BREAK
      case 22:
        YY_RULE_SETUP
#line 132 "repl_scanner.l"
        {
          return K_EXPORT_SNAPSHOT;
        }
        YY_BREAK
      case 23:
        YY_RULE_SETUP
#line 133 "repl_scanner.l"
        {
          return K_NOEXPORT_SNAPSHOT;
        }
        YY_BREAK
      case 24:
        YY_RULE_SETUP
#line 134 "repl_scanner.l"
        {
          return K_USE_SNAPSHOT;
        }
        YY_BREAK
      case 25:
        YY_RULE_SETUP
#line 135 "repl_scanner.l"
        {
          return K_WAIT;
        }
        YY_BREAK
      case 26:
                                   
        YY_RULE_SETUP
#line 137 "repl_scanner.l"
        {                 
        }
        YY_BREAK
      case 27:
        YY_RULE_SETUP
#line 139 "repl_scanner.l"
        {
          yylval.uintval = strtoul(yytext, NULL, 10);
          return UCONST;
        }
        YY_BREAK
      case 28:
        YY_RULE_SETUP
#line 144 "repl_scanner.l"
        {
          uint32 hi, lo;
          if (sscanf(yytext, "%X/%X", &hi, &lo) != 2)
          {
            yyerror("invalid streaming start location");
          }
          yylval.recptr = ((uint64)hi) << 32 | lo;
          return RECPTR;
        }
        YY_BREAK
      case 29:
        YY_RULE_SETUP
#line 153 "repl_scanner.l"
        {
          BEGIN(xq);
          startlit();
        }
        YY_BREAK
      case 30:
        YY_RULE_SETUP
#line 158 "repl_scanner.l"
        {
          yyless(1);
          BEGIN(INITIAL);
          yylval.str = litbufdup();
          return SCONST;
        }
        YY_BREAK
      case 31:
        YY_RULE_SETUP
#line 165 "repl_scanner.l"
        {
          addlitchar('\'');
        }
        YY_BREAK
      case 32:
                                   
        YY_RULE_SETUP
#line 169 "repl_scanner.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 33:
        YY_RULE_SETUP
#line 173 "repl_scanner.l"
        {
          BEGIN(xd);
          startlit();
        }
        YY_BREAK
      case 34:
        YY_RULE_SETUP
#line 178 "repl_scanner.l"
        {
          int len;
          yyless(1);
          BEGIN(INITIAL);
          yylval.str = litbufdup();
          len = strlen(yylval.str);
          truncate_identifier(yylval.str, len, true);
          return IDENT;
        }
        YY_BREAK
      case 35:
                                   
        YY_RULE_SETUP
#line 188 "repl_scanner.l"
        {
          addlit(yytext, yyleng);
        }
        YY_BREAK
      case 36:
        YY_RULE_SETUP
#line 192 "repl_scanner.l"
        {
          int len = strlen(yytext);

          yylval.str = downcase_truncate_identifier(yytext, len, true);
          return IDENT;
        }
        YY_BREAK
      case 37:
        YY_RULE_SETUP
#line 199 "repl_scanner.l"
        {
                                                                   
          return yytext[0];
        }
        YY_BREAK
      case YY_STATE_EOF(xq):
      case YY_STATE_EOF(xd):
#line 204 "repl_scanner.l"
      {
        yyerror("unterminated quoted string");
      }
        YY_BREAK
      case YY_STATE_EOF(INITIAL):
#line 207 "repl_scanner.l"
      {
        yyterminate();
      }
        YY_BREAK
      case 38:
        YY_RULE_SETUP
#line 211 "repl_scanner.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 1524 "repl_scanner.c"

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
      if (yy_current_state >= 279)
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
    if (yy_current_state >= 279)
    {
      yy_c = yy_meta[yy_c];
    }
  }
  yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
  yy_is_jam = (yy_current_state == 278);

  return yy_is_jam ? 0 : yy_current_state;
}

#ifndef YY_NO_UNPUT

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

#line 211 "repl_scanner.l"

                    

static void
startlit(void)
{
  initStringInfo(&litbuf);
}

static char *
litbufdup(void)
{
  return litbuf.data;
}

static void
addlit(char *ytext, int yleng)
{
  appendBinaryStringInfo(&litbuf, ytext, yleng);
}

static void
addlitchar(unsigned char ychar)
{
  appendStringInfoChar(&litbuf, ychar);
}

void
yyerror(const char *message)
{
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg_internal("%s", message)));
}

void
replication_scanner_init(const char *str)
{
  Size slen = strlen(str);
  char *scanbuf;

     
                                        
     
  if (YY_CURRENT_BUFFER)
  {
    yy_delete_buffer(YY_CURRENT_BUFFER);
  }

     
                                                                 
     
  scanbuf = (char *)palloc(slen + 2);
  memcpy(scanbuf, str, slen);
  scanbuf[slen] = scanbuf[slen + 1] = YY_END_OF_BUFFER_CHAR;
  scanbufhandle = yy_scan_buffer(scanbuf, slen + 2);

                                          
  BEGIN(INITIAL);
  repl_pushed_back_token = 0;
}

void
replication_scanner_finish(void)
{
  yy_delete_buffer(scanbufhandle);
  scanbufhandle = NULL;
}

   
                                                                        
   
                                                                           
                                                                       
                                                                         
                                                             
   
bool
replication_scanner_is_replication_command(void)
{
  int first_token = replication_yylex();

  switch (first_token)
  {
  case K_IDENTIFY_SYSTEM:
  case K_BASE_BACKUP:
  case K_START_REPLICATION:
  case K_CREATE_REPLICATION_SLOT:
  case K_DROP_REPLICATION_SLOT:
  case K_TIMELINE_HISTORY:
  case K_SHOW:
                                                               
    repl_pushed_back_token = first_token;
    return true;
  default:
                                                       
    return false;
  }
}
