#line 2 "guc-file.c"

#line 4 "guc-file.c"

#define YY_INT_ALIGNED short int

                                         

#define yy_create_buffer GUC_yy_create_buffer
#define yy_delete_buffer GUC_yy_delete_buffer
#define yy_scan_buffer GUC_yy_scan_buffer
#define yy_scan_string GUC_yy_scan_string
#define yy_scan_bytes GUC_yy_scan_bytes
#define yy_init_buffer GUC_yy_init_buffer
#define yy_flush_buffer GUC_yy_flush_buffer
#define yy_load_buffer_state GUC_yy_load_buffer_state
#define yy_switch_to_buffer GUC_yy_switch_to_buffer
#define yypush_buffer_state GUC_yypush_buffer_state
#define yypop_buffer_state GUC_yypop_buffer_state
#define yyensure_buffer_stack GUC_yyensure_buffer_stack
#define yy_flex_debug GUC_yy_flex_debug
#define yyin GUC_yyin
#define yyleng GUC_yyleng
#define yylex GUC_yylex
#define yylineno GUC_yylineno
#define yyout GUC_yyout
#define yyrestart GUC_yyrestart
#define yytext GUC_yytext
#define yywrap GUC_yywrap
#define yyalloc GUC_yyalloc
#define yyrealloc GUC_yyrealloc
#define yyfree GUC_yyfree

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define GUC_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer GUC_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define GUC_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer GUC_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define GUC_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer GUC_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define GUC_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string GUC_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define GUC_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes GUC_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define GUC_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer GUC_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define GUC_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer GUC_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define GUC_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state GUC_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define GUC_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer GUC_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define GUC_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state GUC_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define GUC_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state GUC_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define GUC_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack GUC_yyensure_buffer_stack
#endif

#ifdef yylex
#define GUC_yylex_ALREADY_DEFINED
#else
#define yylex GUC_yylex
#endif

#ifdef yyrestart
#define GUC_yyrestart_ALREADY_DEFINED
#else
#define yyrestart GUC_yyrestart
#endif

#ifdef yylex_init
#define GUC_yylex_init_ALREADY_DEFINED
#else
#define yylex_init GUC_yylex_init
#endif

#ifdef yylex_init_extra
#define GUC_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra GUC_yylex_init_extra
#endif

#ifdef yylex_destroy
#define GUC_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy GUC_yylex_destroy
#endif

#ifdef yyget_debug
#define GUC_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug GUC_yyget_debug
#endif

#ifdef yyset_debug
#define GUC_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug GUC_yyset_debug
#endif

#ifdef yyget_extra
#define GUC_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra GUC_yyget_extra
#endif

#ifdef yyset_extra
#define GUC_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra GUC_yyset_extra
#endif

#ifdef yyget_in
#define GUC_yyget_in_ALREADY_DEFINED
#else
#define yyget_in GUC_yyget_in
#endif

#ifdef yyset_in
#define GUC_yyset_in_ALREADY_DEFINED
#else
#define yyset_in GUC_yyset_in
#endif

#ifdef yyget_out
#define GUC_yyget_out_ALREADY_DEFINED
#else
#define yyget_out GUC_yyget_out
#endif

#ifdef yyset_out
#define GUC_yyset_out_ALREADY_DEFINED
#else
#define yyset_out GUC_yyset_out
#endif

#ifdef yyget_leng
#define GUC_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng GUC_yyget_leng
#endif

#ifdef yyget_text
#define GUC_yyget_text_ALREADY_DEFINED
#else
#define yyget_text GUC_yyget_text
#endif

#ifdef yyget_lineno
#define GUC_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno GUC_yyget_lineno
#endif

#ifdef yyset_lineno
#define GUC_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno GUC_yyset_lineno
#endif

#ifdef yywrap
#define GUC_yywrap_ALREADY_DEFINED
#else
#define yywrap GUC_yywrap
#endif

#ifdef yyalloc
#define GUC_yyalloc_ALREADY_DEFINED
#else
#define yyalloc GUC_yyalloc
#endif

#ifdef yyrealloc
#define GUC_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc GUC_yyrealloc
#endif

#ifdef yyfree
#define GUC_yyfree_ALREADY_DEFINED
#else
#define yyfree GUC_yyfree
#endif

#ifdef yytext
#define GUC_yytext_ALREADY_DEFINED
#else
#define yytext GUC_yytext
#endif

#ifdef yyleng
#define GUC_yyleng_ALREADY_DEFINED
#else
#define yyleng GUC_yyleng
#endif

#ifdef yyin
#define GUC_yyin_ALREADY_DEFINED
#else
#define yyin GUC_yyin
#endif

#ifdef yyout
#define GUC_yyout_ALREADY_DEFINED
#else
#define yyout GUC_yyout
#endif

#ifdef yy_flex_debug
#define GUC_yy_flex_debug_ALREADY_DEFINED
#else
#define yy_flex_debug GUC_yy_flex_debug
#endif

#ifdef yylineno
#define GUC_yylineno_ALREADY_DEFINED
#else
#define yylineno GUC_yylineno
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

                      

#define GUC_yywrap() (              1)
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
#define YY_NUM_RULES 12
#define YY_END_OF_BUFFER 13
                                            
                                    
struct yy_trans_info
{
  flex_int32_t yy_verify;
  flex_int32_t yy_nxt;
};
static const flex_int16_t yy_accept[41] = {0, 0, 0, 13, 11, 2, 1, 3, 11, 11, 9, 8, 8, 10, 4, 2, 3, 0, 6, 0, 9, 8, 8, 9, 0, 8, 8, 7, 7, 4, 4, 0, 9, 8, 8, 7, 5, 5, 5, 5, 0};

static const YY_CHAR yy_ec[256] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 4, 1, 1, 1, 5, 1, 1, 1, 6, 1, 7, 8, 9, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 9, 1, 1, 12, 1, 1, 1, 13, 13, 13, 13, 14, 13, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 16, 1, 1, 17, 1, 13, 13, 13, 13,

    14, 13, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 18, 15, 15, 1, 1, 1, 1, 1, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,

    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19};

static const YY_CHAR yy_meta[20] = {0, 1, 1, 2, 1, 1, 1, 3, 3, 3, 4, 4, 1, 5, 6, 5, 1, 3, 5, 3};

static const flex_int16_t yy_base[48] = {0, 0, 0, 50, 148, 43, 148, 0, 15, 24, 30, 28, 22, 148, 40, 35, 0, 17, 25, 0, 15, 0, 10, 0, 52, 0, 54, 10, 66, 79, 0, 13, 15, 0, 0, 4, 90, 101, 0, 0, 148, 118, 124, 127, 131, 133, 137, 141};

static const flex_int16_t yy_def[48] = {0, 40, 1, 40, 40, 40, 40, 41, 42, 40, 43, 40, 11, 40, 44, 40, 41, 42, 40, 42, 43, 11, 11, 20, 40, 45, 40, 46, 40, 44, 29, 40, 40, 26, 26, 46, 47, 47, 37, 37, 0, 40, 40, 40, 40, 40, 40, 40};

static const flex_int16_t yy_nxt[168] = {0, 4, 5, 6, 7, 8, 9, 9, 10, 4, 11, 12, 13, 14, 14, 14, 4, 14, 14, 14, 18, 35, 18, 32, 32, 32, 32, 35, 25, 24, 17, 19, 20, 19, 21, 22, 20, 15, 22, 22, 25, 25, 25, 25, 24, 15, 26, 27, 28, 27, 40, 40, 40, 40, 40, 40, 40, 30, 31, 31, 40, 40, 32, 32, 33, 33, 40, 34, 34, 25, 40, 40, 25, 27, 27, 27, 27, 27, 40, 36, 36, 36, 40, 37, 36, 36, 27, 28, 27, 40, 40, 40, 40, 40, 40, 40, 30, 27, 27, 27, 40,

    40, 40, 40, 40, 40, 40, 39, 27, 27, 27, 40, 40, 40, 40, 40, 40, 40, 39, 16, 40, 16, 16, 16, 16, 17, 40, 17, 17, 17, 17, 23, 40, 23, 29, 29, 29, 29, 25, 25, 27, 27, 27, 27, 38, 38, 38, 38, 3, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40};

static const flex_int16_t yy_chk[168] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 35, 17, 31, 31, 32, 32, 27, 22, 20, 18, 8, 9, 17, 9, 9, 11, 15, 11, 11, 12, 11, 11, 11, 10, 5, 11, 14, 14, 14, 3, 0, 0, 0, 0, 0, 0, 14, 24, 24, 0, 0, 24, 24, 26, 26, 0, 26, 26, 26, 0, 0, 26, 28, 28, 28, 28, 28, 0, 28, 28, 28, 0, 28, 28, 28, 29, 29, 29, 0, 0, 0, 0, 0, 0, 0, 29, 36, 36, 36, 0,

    0, 0, 0, 0, 0, 0, 36, 37, 37, 37, 0, 0, 0, 0, 0, 0, 0, 37, 41, 0, 41, 41, 41, 41, 42, 0, 42, 42, 42, 42, 43, 0, 43, 44, 44, 44, 44, 45, 45, 46, 46, 46, 46, 47, 47, 47, 47, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40};

static yy_state_type yy_last_accepting_state;
static char *yy_last_accepting_cpos;

extern int yy_flex_debug;
int yy_flex_debug = 0;

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *yytext;
#line 1 "guc-file.l"
                   
   
                                      
   
                                                                
   
                                     
   
#line 11 "guc-file.l"

#include "postgres.h"

#include <ctype.h>
#include <unistd.h>

#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/guc.h"

   
                                                                       
                                                                          
                                                                             
                                                                         
   
#undef fprintf
#define fprintf(file, fmt, msg) GUC_flex_fatal(msg)

enum
{
  GUC_ID = 1,
  GUC_STRING = 2,
  GUC_INTEGER = 3,
  GUC_REAL = 4,
  GUC_EQUALS = 5,
  GUC_UNQUOTED_STRING = 6,
  GUC_QUALIFIED_ID = 7,
  GUC_EOL = 99,
  GUC_ERROR = 100
};

static unsigned int ConfigFileLineno;
static const char *GUC_flex_fatal_errmsg;
static sigjmp_buf *GUC_flex_fatal_jmp;

static void
FreeConfigVariable(ConfigVariable *item);

static void
record_config_file_error(const char *errmsg, const char *config_file, int lineno, ConfigVariable **head_p, ConfigVariable **tail_p);

static int
GUC_flex_fatal(const char *msg);
static char *
GUC_scanstr(const char *s);

                     

#line 810 "guc-file.c"
#define YY_NO_INPUT 1
#line 812 "guc-file.c"

#define INITIAL 0

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
#line 94 "guc-file.l"

#line 1030 "guc-file.c"

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
          if (yy_current_state >= 41)
          {
            yy_c = yy_meta[yy_c];
          }
        }
        yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
        ++yy_cp;
      } while (yy_current_state != 40);
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
#line 96 "guc-file.l"
        ConfigFileLineno++;
        return GUC_EOL;
        YY_BREAK
      case 2:
        YY_RULE_SETUP
#line 97 "guc-file.l"
                            
        YY_BREAK
      case 3:
        YY_RULE_SETUP
#line 98 "guc-file.l"
                                                             
        YY_BREAK
      case 4:
        YY_RULE_SETUP
#line 100 "guc-file.l"
        return GUC_ID;
        YY_BREAK
      case 5:
        YY_RULE_SETUP
#line 101 "guc-file.l"
        return GUC_QUALIFIED_ID;
        YY_BREAK
      case 6:
        YY_RULE_SETUP
#line 102 "guc-file.l"
        return GUC_STRING;
        YY_BREAK
      case 7:
        YY_RULE_SETUP
#line 103 "guc-file.l"
        return GUC_UNQUOTED_STRING;
        YY_BREAK
      case 8:
        YY_RULE_SETUP
#line 104 "guc-file.l"
        return GUC_INTEGER;
        YY_BREAK
      case 9:
        YY_RULE_SETUP
#line 105 "guc-file.l"
        return GUC_REAL;
        YY_BREAK
      case 10:
        YY_RULE_SETUP
#line 106 "guc-file.l"
        return GUC_EQUALS;
        YY_BREAK
      case 11:
        YY_RULE_SETUP
#line 108 "guc-file.l"
        return GUC_ERROR;
        YY_BREAK
      case 12:
        YY_RULE_SETUP
#line 110 "guc-file.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 1144 "guc-file.c"
      case YY_STATE_EOF(INITIAL):
        yyterminate();

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
      if (yy_current_state >= 41)
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
    if (yy_current_state >= 41)
    {
      yy_c = yy_meta[yy_c];
    }
  }
  yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
  yy_is_jam = (yy_current_state == 40);

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

#line 110 "guc-file.l"

                    

   
                                                                     
                                                                         
                                                                        
                                                                          
                                                                          
                                                           
   
void
ProcessConfigFile(GucContext context)
{
  int elevel;
  MemoryContext config_cxt;
  MemoryContext caller_cxt;

     
                                                                           
                                                 
     
  Assert((context == PGC_POSTMASTER && !IsUnderPostmaster) || context == PGC_SIGHUP);

     
                                                                          
                                    
     
  elevel = IsUnderPostmaster ? DEBUG2 : LOG;

     
                                                                      
                                                                           
                                                                            
                                       
     
  config_cxt = AllocSetContextCreate(CurrentMemoryContext, "config file processing", ALLOCSET_DEFAULT_SIZES);
  caller_cxt = MemoryContextSwitchTo(config_cxt);

     
                                                                           
     
  (void)ProcessConfigFileInternal(context, true, elevel);

                
  MemoryContextSwitchTo(caller_cxt);
  MemoryContextDelete(config_cxt);
}

   
                                                                            
                                                                              
                                                                               
                                                                               
                                
   
static ConfigVariable *
ProcessConfigFileInternal(GucContext context, bool applySettings, int elevel)
{
  bool error = false;
  bool applying = false;
  const char *ConfFileWithError;
  ConfigVariable *item, *head, *tail;
  int i;

                                                                         
  ConfFileWithError = ConfigFileName;
  head = tail = NULL;

  if (!ParseConfigFile(ConfigFileName, true, NULL, 0, 0, elevel, &head, &tail))
  {
                                                           
    error = true;
    goto bail_out;
  }

     
                                                                             
                                                                            
                                                                           
          
     
  if (DataDir)
  {
    if (!ParseConfigFile(PG_AUTOCONF_FILENAME, false, NULL, 0, 0, elevel, &head, &tail))
    {
                                                             
      error = true;
      ConfFileWithError = PG_AUTOCONF_FILENAME;
      goto bail_out;
    }
  }
  else
  {
       
                                                                      
                                                                     
                                                                  
                                                                        
                                                                           
                                                                      
       
    ConfigVariable *newlist = NULL;

       
                                                                       
       
    for (item = head; item; item = item->next)
    {
      if (!item->ignore && strcmp(item->name, "data_directory") == 0)
      {
        newlist = item;
      }
    }

    if (newlist)
    {
      newlist->next = NULL;
    }
    head = tail = newlist;

       
                                                            
       
                                                                         
                                                                         
                        
       
    if (head == NULL)
    {
      goto bail_out;
    }
  }

     
                                                                         
                                                                           
                                          
     
  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *gconf = guc_variables[i];

    gconf->status &= ~GUC_IS_IN_FILE;
  }

     
                                                                        
                                                                      
                                                                            
                                                                       
                                                                            
                                                                           
                                                                         
     
                                                                      
                                                                            
                                                         
     
  for (item = head; item; item = item->next)
  {
    struct config_generic *record;

                                                     
    if (item->ignore)
    {
      continue;
    }

       
                                                                           
                               
       
    record = find_option(item->name, false, elevel);

    if (record)
    {
                                                                  
      if (record->status & GUC_IS_IN_FILE)
      {
           
                                                                       
                                                                      
                                                          
           
        ConfigVariable *pitem;

        for (pitem = head; pitem != item; pitem = pitem->next)
        {
          if (!pitem->ignore && strcmp(pitem->name, item->name) == 0)
          {
            pitem->ignore = true;
          }
        }
      }
                                          
      record->status |= GUC_IS_IN_FILE;
    }
    else if (strchr(item->name, GUC_QUALIFIER_SEPARATOR) == NULL)
    {
                                                    
      ereport(elevel, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\" in file \"%s\" line %u", item->name, item->filename, item->sourceline)));
      item->errmsg = pstrdup("unrecognized configuration parameter");
      error = true;
      ConfFileWithError = item->filename;
    }
  }

     
                                                                             
              
     
  if (error)
  {
    goto bail_out;
  }

                                                                 
  applying = true;

     
                                                                       
                                                                          
                                                                             
                               
     
  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *gconf = guc_variables[i];
    GucStack *stack;

    if (gconf->reset_source != PGC_S_FILE || (gconf->status & GUC_IS_IN_FILE))
    {
      continue;
    }
    if (gconf->context < PGC_SIGHUP)
    {
                                                            
      gconf->status |= GUC_PENDING_RESTART;
      ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", gconf->name)));
      record_config_file_error(psprintf("parameter \"%s\" cannot be changed without restarting the server", gconf->name), NULL, 0, &head, &tail);
      error = true;
      continue;
    }

                                                                    
    if (!applySettings)
    {
      continue;
    }

       
                                                                          
                                    
       
    if (gconf->reset_source == PGC_S_FILE)
    {
      gconf->reset_source = PGC_S_DEFAULT;
    }
    if (gconf->source == PGC_S_FILE)
    {
      gconf->source = PGC_S_DEFAULT;
    }
    for (stack = gconf->stack; stack; stack = stack->prev)
    {
      if (stack->source == PGC_S_FILE)
      {
        stack->source = PGC_S_DEFAULT;
      }
    }

                                                                       
    if (set_config_option(gconf->name, NULL, context, PGC_S_DEFAULT, GUC_ACTION_SET, true, 0, false) > 0)
    {
                                         
      if (context == PGC_SIGHUP)
      {
        ereport(elevel, (errmsg("parameter \"%s\" removed from configuration file, reset to default", gconf->name)));
      }
    }
  }

     
                                                                  
                                                                        
                                                                        
     
                                                                          
                                                                            
                                                   
     
                                                                             
                                                                             
                                                                             
                                  
     
  if (context == PGC_SIGHUP && applySettings)
  {
    InitializeGUCOptionsFromEnvironment();
    pg_timezone_abbrev_initialize();
                                                                         
    SetConfigOption("client_encoding", GetDatabaseEncodingName(), PGC_BACKEND, PGC_S_DYNAMIC_DEFAULT);
  }

     
                                                
     
  for (item = head; item; item = item->next)
  {
    char *pre_value = NULL;
    int scres;

                                             
    if (item->ignore)
    {
      continue;
    }

                                                                      
    if (context == PGC_SIGHUP && applySettings && !IsUnderPostmaster)
    {
      const char *preval = GetConfigOption(item->name, true, false);

                                                                         
      if (!preval)
      {
        preval = "";
      }
                                                            
      pre_value = pstrdup(preval);
    }

    scres = set_config_option(item->name, item->value, context, PGC_S_FILE, GUC_ACTION_SET, applySettings, 0, false);
    if (scres > 0)
    {
                                                                  
      if (pre_value)
      {
        const char *post_value = GetConfigOption(item->name, true, false);

        if (!post_value)
        {
          post_value = "";
        }
        if (strcmp(pre_value, post_value) != 0)
        {
          ereport(elevel, (errmsg("parameter \"%s\" changed to \"%s\"", item->name, item->value)));
        }
      }
      item->applied = true;
    }
    else if (scres == 0)
    {
      error = true;
      item->errmsg = pstrdup("setting could not be applied");
      ConfFileWithError = item->filename;
    }
    else
    {
                                                                 
      item->applied = true;
    }

       
                                                                         
                                                                           
                                                                           
                     
       
    if (scres != 0 && applySettings)
    {
      set_config_sourcefile(item->name, item->filename, item->sourceline);
    }

    if (pre_value)
    {
      pfree(pre_value);
    }
  }

                                                                  
  if (applySettings)
  {
    PgReloadTime = GetCurrentTimestamp();
  }

bail_out:
  if (error && applySettings)
  {
                                                       
    if (context == PGC_POSTMASTER)
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("configuration file \"%s\" contains errors", ConfFileWithError)));
    }
    else if (applying)
    {
      ereport(elevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("configuration file \"%s\" contains errors; unaffected changes were applied", ConfFileWithError)));
    }
    else
    {
      ereport(elevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("configuration file \"%s\" contains errors; no changes were applied", ConfFileWithError)));
    }
  }

                                                               
  return head;
}

   
                                                                           
                                                                             
                                                                             
   
static char *
AbsoluteConfigLocation(const char *location, const char *calling_file)
{
  char abs_path[MAXPGPATH];

  if (is_absolute_path(location))
  {
    return pstrdup(location);
  }
  else
  {
    if (calling_file != NULL)
    {
      strlcpy(abs_path, calling_file, sizeof(abs_path));
      get_parent_directory(abs_path);
      join_path_components(abs_path, abs_path, location);
      canonicalize_path(abs_path);
    }
    else
    {
      AssertState(DataDir);
      join_path_components(abs_path, DataDir, location);
      canonicalize_path(abs_path);
    }
    return pstrdup(abs_path);
  }
}

   
                                                                       
                                   
   
                                                                           
                                 
   
                                                                   
                                                           
   
                                                                            
                                                                           
                                                   
   
bool
ParseConfigFile(const char *config_file, bool strict, const char *calling_file, int calling_lineno, int depth, int elevel, ConfigVariable **head_p, ConfigVariable **tail_p)
{
  char *abs_path;
  bool OK = true;
  FILE *fp;

     
                                                                            
                                                                        
     
  if (strspn(config_file, " \t\r\n") == strlen(config_file))
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("empty configuration file name: \"%s\"", config_file)));
    record_config_file_error("empty configuration file name", calling_file, calling_lineno, head_p, tail_p);
    return false;
  }

     
                                                                            
                                                                            
                                                                
     
  if (depth > 10)
  {
    ereport(elevel, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("could not open configuration file \"%s\": maximum nesting depth exceeded", config_file)));
    record_config_file_error("nesting depth exceeded", calling_file, calling_lineno, head_p, tail_p);
    return false;
  }

  abs_path = AbsoluteConfigLocation(config_file, calling_file);

     
                                                                             
                                                                          
                                                                           
                                                                       
     
  if (calling_file && strcmp(abs_path, calling_file) == 0)
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("configuration file recursion in \"%s\"", calling_file)));
    record_config_file_error("configuration file recursion", calling_file, calling_lineno, head_p, tail_p);
    pfree(abs_path);
    return false;
  }

  fp = AllocateFile(abs_path, "r");
  if (!fp)
  {
    if (strict)
    {
      ereport(elevel, (errcode_for_file_access(), errmsg("could not open configuration file \"%s\": %m", abs_path)));
      record_config_file_error(psprintf("could not open file \"%s\"", abs_path), calling_file, calling_lineno, head_p, tail_p);
      OK = false;
    }
    else
    {
      ereport(LOG, (errmsg("skipping missing configuration file \"%s\"", abs_path)));
    }
    goto cleanup;
  }

  OK = ParseConfigFp(fp, abs_path, depth, elevel, head_p, tail_p);

cleanup:
  if (fp)
  {
    FreeFile(fp);
  }
  pfree(abs_path);

  return OK;
}

   
                                                                   
                        
   
static void
record_config_file_error(const char *errmsg, const char *config_file, int lineno, ConfigVariable **head_p, ConfigVariable **tail_p)
{
  ConfigVariable *item;

  item = palloc(sizeof *item);
  item->name = NULL;
  item->value = NULL;
  item->errmsg = pstrdup(errmsg);
  item->filename = config_file ? pstrdup(config_file) : NULL;
  item->sourceline = lineno;
  item->ignore = true;
  item->applied = false;
  item->next = NULL;
  if (*head_p == NULL)
  {
    *head_p = item;
  }
  else
  {
    (*tail_p)->next = item;
  }
  *tail_p = item;
}

   
                                                                              
                                                                              
                                                                             
                                                                 
   
                                                                     
   
static int
GUC_flex_fatal(const char *msg)
{
  GUC_flex_fatal_errmsg = msg;
  siglongjmp(*GUC_flex_fatal_jmp, 1);
  return 0;                          
}

   
                                                                       
                                   
   
                     
                                                                          
                                                                         
                                                              
                                      
                            
                                                                    
   
                                                                             
                                                                            
                                                                              
                                                                       
   
                                                                          
                                                                           
                                                      
   
                                                                         
                                                                       
   
                                                                      
                                                                      
                                                                         
                                                                       
                      
   
bool
ParseConfigFp(FILE *fp, const char *config_file, int depth, int elevel, ConfigVariable **head_p, ConfigVariable **tail_p)
{
  volatile bool OK = true;
  unsigned int save_ConfigFileLineno = ConfigFileLineno;
  sigjmp_buf *save_GUC_flex_fatal_jmp = GUC_flex_fatal_jmp;
  sigjmp_buf flex_fatal_jmp;
  volatile YY_BUFFER_STATE lex_buffer = NULL;
  int errorcount;
  int token;

  if (sigsetjmp(flex_fatal_jmp, 1) == 0)
  {
    GUC_flex_fatal_jmp = &flex_fatal_jmp;
  }
  else
  {
       
                                                                       
                                                                          
                                                                  
       
    elog(elevel, "%s at file \"%s\" line %u", GUC_flex_fatal_errmsg, config_file, ConfigFileLineno);
    record_config_file_error(GUC_flex_fatal_errmsg, config_file, ConfigFileLineno, head_p, tail_p);
    OK = false;
    goto cleanup;
  }

     
           
     
  ConfigFileLineno = 1;
  errorcount = 0;

  lex_buffer = yy_create_buffer(fp, YY_BUF_SIZE);
  yy_switch_to_buffer(lex_buffer);

                                                
  while ((token = yylex()))
  {
    char *opt_name = NULL;
    char *opt_value = NULL;
    ConfigVariable *item;

    if (token == GUC_EOL)                            
    {
      continue;
    }

                                            
    if (token != GUC_ID && token != GUC_QUALIFIED_ID)
    {
      goto parse_error;
    }
    opt_name = pstrdup(yytext);

                                                                 
    token = yylex();
    if (token == GUC_EQUALS)
    {
      token = yylex();
    }

                                           
    if (token != GUC_ID && token != GUC_STRING && token != GUC_INTEGER && token != GUC_REAL && token != GUC_UNQUOTED_STRING)
    {
      goto parse_error;
    }
    if (token == GUC_STRING)                               
    {
      opt_value = GUC_scanstr(yytext);
    }
    else
    {
      opt_value = pstrdup(yytext);
    }

                                                       
    token = yylex();
    if (token != GUC_EOL)
    {
      if (token != 0)
      {
        goto parse_error;
      }
                                                                      
      ConfigFileLineno++;
    }

                                               
    if (guc_name_compare(opt_name, "include_dir") == 0)
    {
         
                                                                 
                                
         
      if (!ParseConfigDirectory(opt_value, config_file, ConfigFileLineno - 1, depth + 1, elevel, head_p, tail_p))
      {
        OK = false;
      }
      yy_switch_to_buffer(lex_buffer);
      pfree(opt_name);
      pfree(opt_value);
    }
    else if (guc_name_compare(opt_name, "include_if_exists") == 0)
    {
         
                                                                       
                                
         
      if (!ParseConfigFile(opt_value, false, config_file, ConfigFileLineno - 1, depth + 1, elevel, head_p, tail_p))
      {
        OK = false;
      }
      yy_switch_to_buffer(lex_buffer);
      pfree(opt_name);
      pfree(opt_value);
    }
    else if (guc_name_compare(opt_name, "include") == 0)
    {
         
                                                                       
                      
         
      if (!ParseConfigFile(opt_value, true, config_file, ConfigFileLineno - 1, depth + 1, elevel, head_p, tail_p))
      {
        OK = false;
      }
      yy_switch_to_buffer(lex_buffer);
      pfree(opt_name);
      pfree(opt_value);
    }
    else
    {
                                             
      item = palloc(sizeof *item);
      item->name = opt_name;
      item->value = opt_value;
      item->errmsg = NULL;
      item->filename = pstrdup(config_file);
      item->sourceline = ConfigFileLineno - 1;
      item->ignore = false;
      item->applied = false;
      item->next = NULL;
      if (*head_p == NULL)
      {
        *head_p = item;
      }
      else
      {
        (*tail_p)->next = item;
      }
      *tail_p = item;
    }

                                                                
    if (token == 0)
    {
      break;
    }
    continue;

  parse_error:
                                                          
    if (opt_name)
    {
      pfree(opt_name);
    }
    if (opt_value)
    {
      pfree(opt_value);
    }

                          
    if (token == GUC_EOL || token == 0)
    {
      ereport(elevel, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("syntax error in file \"%s\" line %u, near end of line", config_file, ConfigFileLineno - 1)));
      record_config_file_error("syntax error", config_file, ConfigFileLineno - 1, head_p, tail_p);
    }
    else
    {
      ereport(elevel, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("syntax error in file \"%s\" line %u, near token \"%s\"", config_file, ConfigFileLineno, yytext)));
      record_config_file_error("syntax error", config_file, ConfigFileLineno, head_p, tail_p);
    }
    OK = false;
    errorcount++;

       
                                                                        
                                                                       
                                                                           
                                                                        
                                                          
       
    if (errorcount >= 100 || elevel <= DEBUG1)
    {
      ereport(elevel, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many syntax errors found, abandoning file \"%s\"", config_file)));
      break;
    }

                                           
    while (token != GUC_EOL && token != 0)
    {
      token = yylex();
    }
                                  
    if (token == 0)
    {
      break;
    }
  }

cleanup:
  yy_delete_buffer(lex_buffer);
                                                                          
  ConfigFileLineno = save_ConfigFileLineno;
  GUC_flex_fatal_jmp = save_GUC_flex_fatal_jmp;
  return OK;
}

   
                                                                           
   
                                                                            
   
                                                                   
                                                           
   
                                          
   
bool
ParseConfigDirectory(const char *includedir, const char *calling_file, int calling_lineno, int depth, int elevel, ConfigVariable **head_p, ConfigVariable **tail_p)
{
  char *directory;
  DIR *d;
  struct dirent *de;
  char **filenames;
  int num_filenames;
  int size_filenames;
  bool status;

     
                                                                        
                                                                          
                                                           
     
  if (strspn(includedir, " \t\r\n") == strlen(includedir))
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("empty configuration directory name: \"%s\"", includedir)));
    record_config_file_error("empty configuration directory name", calling_file, calling_lineno, head_p, tail_p);
    return false;
  }

     
                                                                      
                                                                 
     

  directory = AbsoluteConfigLocation(includedir, calling_file);
  d = AllocateDir(directory);
  if (d == NULL)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not open configuration directory \"%s\": %m", directory)));
    record_config_file_error(psprintf("could not open directory \"%s\"", directory), calling_file, calling_lineno, head_p, tail_p);
    status = false;
    goto cleanup;
  }

     
                                                                          
                                            
     
  size_filenames = 32;
  filenames = (char **)palloc(size_filenames * sizeof(char *));
  num_filenames = 0;

  while ((de = ReadDir(d, directory)) != NULL)
  {
    struct stat st;
    char filename[MAXPGPATH];

       
                                                                         
                                                                         
                                                                         
       
    if (strlen(de->d_name) < 6)
    {
      continue;
    }
    if (de->d_name[0] == '.')
    {
      continue;
    }
    if (strcmp(de->d_name + strlen(de->d_name) - 5, ".conf") != 0)
    {
      continue;
    }

    join_path_components(filename, directory, de->d_name);
    canonicalize_path(filename);
    if (stat(filename, &st) == 0)
    {
      if (!S_ISDIR(st.st_mode))
      {
                                                                    
        if (num_filenames >= size_filenames)
        {
          size_filenames += 32;
          filenames = (char **)repalloc(filenames, size_filenames * sizeof(char *));
        }
        filenames[num_filenames] = pstrdup(filename);
        num_filenames++;
      }
    }
    else
    {
         
                                                                         
                                                                       
                                    
         
      ereport(elevel, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", filename)));
      record_config_file_error(psprintf("could not stat file \"%s\"", filename), calling_file, calling_lineno, head_p, tail_p);
      status = false;
      goto cleanup;
    }
  }

  if (num_filenames > 0)
  {
    int i;

    qsort(filenames, num_filenames, sizeof(char *), pg_qsort_strcmp);
    for (i = 0; i < num_filenames; i++)
    {
      if (!ParseConfigFile(filenames[i], true, calling_file, calling_lineno, depth, elevel, head_p, tail_p))
      {
        status = false;
        goto cleanup;
      }
    }
  }
  status = true;

cleanup:
  if (d)
  {
    FreeDir(d);
  }
  pfree(directory);
  return status;
}

   
                                                                      
   
void
FreeConfigVariables(ConfigVariable *list)
{
  ConfigVariable *item;

  item = list;
  while (item)
  {
    ConfigVariable *next = item->next;

    FreeConfigVariable(item);
    item = next;
  }
}

   
                                
   
static void
FreeConfigVariable(ConfigVariable *item)
{
  if (item->name)
  {
    pfree(item->name);
  }
  if (item->value)
  {
    pfree(item->value);
  }
  if (item->errmsg)
  {
    pfree(item->errmsg);
  }
  if (item->filename)
  {
    pfree(item->filename);
  }
  pfree(item);
}

   
            
   
                                                                            
                                       
   
                                                                           
           
   
static char *
GUC_scanstr(const char *s)
{
  char *newStr;
  int len, i, j;

  Assert(s != NULL && s[0] == '\'');
  len = strlen(s);
  Assert(len >= 2);
  Assert(s[len - 1] == '\'');

                                                                     
  s++, len--;

                                                                     
  newStr = palloc(len);

  for (i = 0, j = 0; i < len; i++)
  {
    if (s[i] == '\\')
    {
      i++;
      switch (s[i])
      {
      case 'b':
        newStr[j] = '\b';
        break;
      case 'f':
        newStr[j] = '\f';
        break;
      case 'n':
        newStr[j] = '\n';
        break;
      case 'r':
        newStr[j] = '\r';
        break;
      case 't':
        newStr[j] = '\t';
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      {
        int k;
        long octVal = 0;

        for (k = 0; s[i + k] >= '0' && s[i + k] <= '7' && k < 3; k++)
        {
          octVal = (octVal << 3) + (s[i + k] - '0');
        }
        i += k - 1;
        newStr[j] = ((char)octVal);
      }
      break;
      default:
        newStr[j] = s[i];
        break;
      }             
    }
    else if (s[i] == '\'' && s[i + 1] == '\'')
    {
                                                
      newStr[j] = s[++i];
    }
    else
    {
      newStr[j] = s[i];
    }
    j++;
  }

                                                                
  Assert(j > 0 && j <= len);
  newStr[--j] = '\0';

  return newStr;
}
