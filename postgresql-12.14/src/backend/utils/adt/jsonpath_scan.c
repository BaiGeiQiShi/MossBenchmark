#line 2 "jsonpath_scan.c"

#line 4 "jsonpath_scan.c"

#define YY_INT_ALIGNED short int

                                         

#define yy_create_buffer jsonpath_yy_create_buffer
#define yy_delete_buffer jsonpath_yy_delete_buffer
#define yy_scan_buffer jsonpath_yy_scan_buffer
#define yy_scan_string jsonpath_yy_scan_string
#define yy_scan_bytes jsonpath_yy_scan_bytes
#define yy_init_buffer jsonpath_yy_init_buffer
#define yy_flush_buffer jsonpath_yy_flush_buffer
#define yy_load_buffer_state jsonpath_yy_load_buffer_state
#define yy_switch_to_buffer jsonpath_yy_switch_to_buffer
#define yypush_buffer_state jsonpath_yypush_buffer_state
#define yypop_buffer_state jsonpath_yypop_buffer_state
#define yyensure_buffer_stack jsonpath_yyensure_buffer_stack
#define yy_flex_debug jsonpath_yy_flex_debug
#define yyin jsonpath_yyin
#define yyleng jsonpath_yyleng
#define yylex jsonpath_yylex
#define yylineno jsonpath_yylineno
#define yyout jsonpath_yyout
#define yyrestart jsonpath_yyrestart
#define yytext jsonpath_yytext
#define yywrap jsonpath_yywrap
#define yyalloc jsonpath_yyalloc
#define yyrealloc jsonpath_yyrealloc
#define yyfree jsonpath_yyfree

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yy_create_buffer
#define jsonpath_yy_create_buffer_ALREADY_DEFINED
#else
#define yy_create_buffer jsonpath_yy_create_buffer
#endif

#ifdef yy_delete_buffer
#define jsonpath_yy_delete_buffer_ALREADY_DEFINED
#else
#define yy_delete_buffer jsonpath_yy_delete_buffer
#endif

#ifdef yy_scan_buffer
#define jsonpath_yy_scan_buffer_ALREADY_DEFINED
#else
#define yy_scan_buffer jsonpath_yy_scan_buffer
#endif

#ifdef yy_scan_string
#define jsonpath_yy_scan_string_ALREADY_DEFINED
#else
#define yy_scan_string jsonpath_yy_scan_string
#endif

#ifdef yy_scan_bytes
#define jsonpath_yy_scan_bytes_ALREADY_DEFINED
#else
#define yy_scan_bytes jsonpath_yy_scan_bytes
#endif

#ifdef yy_init_buffer
#define jsonpath_yy_init_buffer_ALREADY_DEFINED
#else
#define yy_init_buffer jsonpath_yy_init_buffer
#endif

#ifdef yy_flush_buffer
#define jsonpath_yy_flush_buffer_ALREADY_DEFINED
#else
#define yy_flush_buffer jsonpath_yy_flush_buffer
#endif

#ifdef yy_load_buffer_state
#define jsonpath_yy_load_buffer_state_ALREADY_DEFINED
#else
#define yy_load_buffer_state jsonpath_yy_load_buffer_state
#endif

#ifdef yy_switch_to_buffer
#define jsonpath_yy_switch_to_buffer_ALREADY_DEFINED
#else
#define yy_switch_to_buffer jsonpath_yy_switch_to_buffer
#endif

#ifdef yypush_buffer_state
#define jsonpath_yypush_buffer_state_ALREADY_DEFINED
#else
#define yypush_buffer_state jsonpath_yypush_buffer_state
#endif

#ifdef yypop_buffer_state
#define jsonpath_yypop_buffer_state_ALREADY_DEFINED
#else
#define yypop_buffer_state jsonpath_yypop_buffer_state
#endif

#ifdef yyensure_buffer_stack
#define jsonpath_yyensure_buffer_stack_ALREADY_DEFINED
#else
#define yyensure_buffer_stack jsonpath_yyensure_buffer_stack
#endif

#ifdef yylex
#define jsonpath_yylex_ALREADY_DEFINED
#else
#define yylex jsonpath_yylex
#endif

#ifdef yyrestart
#define jsonpath_yyrestart_ALREADY_DEFINED
#else
#define yyrestart jsonpath_yyrestart
#endif

#ifdef yylex_init
#define jsonpath_yylex_init_ALREADY_DEFINED
#else
#define yylex_init jsonpath_yylex_init
#endif

#ifdef yylex_init_extra
#define jsonpath_yylex_init_extra_ALREADY_DEFINED
#else
#define yylex_init_extra jsonpath_yylex_init_extra
#endif

#ifdef yylex_destroy
#define jsonpath_yylex_destroy_ALREADY_DEFINED
#else
#define yylex_destroy jsonpath_yylex_destroy
#endif

#ifdef yyget_debug
#define jsonpath_yyget_debug_ALREADY_DEFINED
#else
#define yyget_debug jsonpath_yyget_debug
#endif

#ifdef yyset_debug
#define jsonpath_yyset_debug_ALREADY_DEFINED
#else
#define yyset_debug jsonpath_yyset_debug
#endif

#ifdef yyget_extra
#define jsonpath_yyget_extra_ALREADY_DEFINED
#else
#define yyget_extra jsonpath_yyget_extra
#endif

#ifdef yyset_extra
#define jsonpath_yyset_extra_ALREADY_DEFINED
#else
#define yyset_extra jsonpath_yyset_extra
#endif

#ifdef yyget_in
#define jsonpath_yyget_in_ALREADY_DEFINED
#else
#define yyget_in jsonpath_yyget_in
#endif

#ifdef yyset_in
#define jsonpath_yyset_in_ALREADY_DEFINED
#else
#define yyset_in jsonpath_yyset_in
#endif

#ifdef yyget_out
#define jsonpath_yyget_out_ALREADY_DEFINED
#else
#define yyget_out jsonpath_yyget_out
#endif

#ifdef yyset_out
#define jsonpath_yyset_out_ALREADY_DEFINED
#else
#define yyset_out jsonpath_yyset_out
#endif

#ifdef yyget_leng
#define jsonpath_yyget_leng_ALREADY_DEFINED
#else
#define yyget_leng jsonpath_yyget_leng
#endif

#ifdef yyget_text
#define jsonpath_yyget_text_ALREADY_DEFINED
#else
#define yyget_text jsonpath_yyget_text
#endif

#ifdef yyget_lineno
#define jsonpath_yyget_lineno_ALREADY_DEFINED
#else
#define yyget_lineno jsonpath_yyget_lineno
#endif

#ifdef yyset_lineno
#define jsonpath_yyset_lineno_ALREADY_DEFINED
#else
#define yyset_lineno jsonpath_yyset_lineno
#endif

#ifdef yywrap
#define jsonpath_yywrap_ALREADY_DEFINED
#else
#define yywrap jsonpath_yywrap
#endif

#ifdef yyget_lval
#define jsonpath_yyget_lval_ALREADY_DEFINED
#else
#define yyget_lval jsonpath_yyget_lval
#endif

#ifdef yyset_lval
#define jsonpath_yyset_lval_ALREADY_DEFINED
#else
#define yyset_lval jsonpath_yyset_lval
#endif

#ifdef yyalloc
#define jsonpath_yyalloc_ALREADY_DEFINED
#else
#define yyalloc jsonpath_yyalloc
#endif

#ifdef yyrealloc
#define jsonpath_yyrealloc_ALREADY_DEFINED
#else
#define yyrealloc jsonpath_yyrealloc
#endif

#ifdef yyfree
#define jsonpath_yyfree_ALREADY_DEFINED
#else
#define yyfree jsonpath_yyfree
#endif

#ifdef yytext
#define jsonpath_yytext_ALREADY_DEFINED
#else
#define yytext jsonpath_yytext
#endif

#ifdef yyleng
#define jsonpath_yyleng_ALREADY_DEFINED
#else
#define yyleng jsonpath_yyleng
#endif

#ifdef yyin
#define jsonpath_yyin_ALREADY_DEFINED
#else
#define yyin jsonpath_yyin
#endif

#ifdef yyout
#define jsonpath_yyout_ALREADY_DEFINED
#else
#define yyout jsonpath_yyout
#endif

#ifdef yy_flex_debug
#define jsonpath_yy_flex_debug_ALREADY_DEFINED
#else
#define yy_flex_debug jsonpath_yy_flex_debug
#endif

#ifdef yylineno
#define jsonpath_yylineno_ALREADY_DEFINED
#else
#define yylineno jsonpath_yylineno
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

                      

#define jsonpath_yywrap() (              1)
#define YY_SKIP_YYWRAP
typedef flex_uint8_t YY_CHAR;

FILE *yyin = NULL, *yyout = NULL;

typedef const struct yy_trans_info *yy_state_type;

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
#define YY_NUM_RULES 48
#define YY_END_OF_BUFFER 49
struct yy_trans_info
{
  flex_int16_t yy_verify;
  flex_int16_t yy_nxt;
};
static const struct yy_trans_info yy_transition[8135] = {
    {0, 0},
    {0, 7879},
    {0, 0},
    {0, 7877},
    {1, 2580},
    {2, 2580},
    {3, 2580},
    {4, 2580},
    {5, 2580},
    {6, 2580},
    {7, 2580},
    {8, 2580},
    {9, 2838},
    {10, 2838},
    {11, 2580},
    {12, 2838},
    {13, 2838},
    {14, 2580},
    {15, 2580},
    {16, 2580},
    {17, 2580},
    {18, 2580},
    {19, 2580},
    {20, 2580},
    {21, 2580},
    {22, 2580},
    {23, 2580},
    {24, 2580},
    {25, 2580},
    {26, 2580},
    {27, 2580},
    {28, 2580},
    {29, 2580},
    {30, 2580},
    {31, 2580},
    {32, 2838},
    {33, 2642},
    {34, 2644},
    {35, 2672},
    {36, 2857},
    {37, 2672},
    {38, 2854},
    {39, 2580},
    {40, 2672},
    {41, 2672},
    {42, 3115},
    {43, 2672},
    {44, 2672},
    {45, 2672},
    {46, 2672},

    {47, 3117},
    {48, 3119},
    {49, 3377},
    {50, 3377},
    {51, 3377},
    {52, 3377},
    {53, 3377},
    {54, 3377},
    {55, 3377},
    {56, 3377},
    {57, 3377},
    {58, 2672},
    {59, 2580},
    {60, 3181},
    {61, 3183},
    {62, 3439},
    {63, 2672},
    {64, 2672},
    {65, 2580},
    {66, 2580},
    {67, 2580},
    {68, 2580},
    {69, 2580},
    {70, 2580},
    {71, 2580},
    {72, 2580},
    {73, 2580},
    {74, 2580},
    {75, 2580},
    {76, 2580},
    {77, 2580},
    {78, 2580},
    {79, 2580},
    {80, 2580},
    {81, 2580},
    {82, 2580},
    {83, 2580},
    {84, 2580},
    {85, 2580},
    {86, 2580},
    {87, 2580},
    {88, 2580},
    {89, 2580},
    {90, 2580},
    {91, 2672},
    {92, 3441},
    {93, 2672},
    {94, 2580},
    {95, 2580},
    {96, 2580},

    {97, 2580},
    {98, 2580},
    {99, 2580},
    {100, 2580},
    {101, 2580},
    {102, 2580},
    {103, 2580},
    {104, 2580},
    {105, 2580},
    {106, 2580},
    {107, 2580},
    {108, 2580},
    {109, 2580},
    {110, 2580},
    {111, 2580},
    {112, 2580},
    {113, 2580},
    {114, 2580},
    {115, 2580},
    {116, 2580},
    {117, 2580},
    {118, 2580},
    {119, 2580},
    {120, 2580},
    {121, 2580},
    {122, 2580},
    {123, 2672},
    {124, 3635},
    {125, 2672},
    {126, 2580},
    {127, 2580},
    {128, 2580},
    {129, 2580},
    {130, 2580},
    {131, 2580},
    {132, 2580},
    {133, 2580},
    {134, 2580},
    {135, 2580},
    {136, 2580},
    {137, 2580},
    {138, 2580},
    {139, 2580},
    {140, 2580},
    {141, 2580},
    {142, 2580},
    {143, 2580},
    {144, 2580},
    {145, 2580},
    {146, 2580},

    {147, 2580},
    {148, 2580},
    {149, 2580},
    {150, 2580},
    {151, 2580},
    {152, 2580},
    {153, 2580},
    {154, 2580},
    {155, 2580},
    {156, 2580},
    {157, 2580},
    {158, 2580},
    {159, 2580},
    {160, 2580},
    {161, 2580},
    {162, 2580},
    {163, 2580},
    {164, 2580},
    {165, 2580},
    {166, 2580},
    {167, 2580},
    {168, 2580},
    {169, 2580},
    {170, 2580},
    {171, 2580},
    {172, 2580},
    {173, 2580},
    {174, 2580},
    {175, 2580},
    {176, 2580},
    {177, 2580},
    {178, 2580},
    {179, 2580},
    {180, 2580},
    {181, 2580},
    {182, 2580},
    {183, 2580},
    {184, 2580},
    {185, 2580},
    {186, 2580},
    {187, 2580},
    {188, 2580},
    {189, 2580},
    {190, 2580},
    {191, 2580},
    {192, 2580},
    {193, 2580},
    {194, 2580},
    {195, 2580},
    {196, 2580},

    {197, 2580},
    {198, 2580},
    {199, 2580},
    {200, 2580},
    {201, 2580},
    {202, 2580},
    {203, 2580},
    {204, 2580},
    {205, 2580},
    {206, 2580},
    {207, 2580},
    {208, 2580},
    {209, 2580},
    {210, 2580},
    {211, 2580},
    {212, 2580},
    {213, 2580},
    {214, 2580},
    {215, 2580},
    {216, 2580},
    {217, 2580},
    {218, 2580},
    {219, 2580},
    {220, 2580},
    {221, 2580},
    {222, 2580},
    {223, 2580},
    {224, 2580},
    {225, 2580},
    {226, 2580},
    {227, 2580},
    {228, 2580},
    {229, 2580},
    {230, 2580},
    {231, 2580},
    {232, 2580},
    {233, 2580},
    {234, 2580},
    {235, 2580},
    {236, 2580},
    {237, 2580},
    {238, 2580},
    {239, 2580},
    {240, 2580},
    {241, 2580},
    {242, 2580},
    {243, 2580},
    {244, 2580},
    {245, 2580},
    {246, 2580},

    {247, 2580},
    {248, 2580},
    {249, 2580},
    {250, 2580},
    {251, 2580},
    {252, 2580},
    {253, 2580},
    {254, 2580},
    {255, 2580},
    {256, 2580},
    {0, 0},
    {0, 7619},
    {1, 2322},
    {2, 2322},
    {3, 2322},
    {4, 2322},
    {5, 2322},
    {6, 2322},
    {7, 2322},
    {8, 2322},
    {9, 2580},
    {10, 2580},
    {11, 2322},
    {12, 2580},
    {13, 2580},
    {14, 2322},
    {15, 2322},
    {16, 2322},
    {17, 2322},
    {18, 2322},
    {19, 2322},
    {20, 2322},
    {21, 2322},
    {22, 2322},
    {23, 2322},
    {24, 2322},
    {25, 2322},
    {26, 2322},
    {27, 2322},
    {28, 2322},
    {29, 2322},
    {30, 2322},
    {31, 2322},
    {32, 2580},
    {33, 2384},
    {34, 2386},
    {35, 2414},
    {36, 2599},
    {37, 2414},
    {38, 2596},

    {39, 2322},
    {40, 2414},
    {41, 2414},
    {42, 2857},
    {43, 2414},
    {44, 2414},
    {45, 2414},
    {46, 2414},
    {47, 2859},
    {48, 2861},
    {49, 3119},
    {50, 3119},
    {51, 3119},
    {52, 3119},
    {53, 3119},
    {54, 3119},
    {55, 3119},
    {56, 3119},
    {57, 3119},
    {58, 2414},
    {59, 2322},
    {60, 2923},
    {61, 2925},
    {62, 3181},
    {63, 2414},
    {64, 2414},
    {65, 2322},
    {66, 2322},
    {67, 2322},
    {68, 2322},
    {69, 2322},
    {70, 2322},
    {71, 2322},
    {72, 2322},
    {73, 2322},
    {74, 2322},
    {75, 2322},
    {76, 2322},
    {77, 2322},
    {78, 2322},
    {79, 2322},
    {80, 2322},
    {81, 2322},
    {82, 2322},
    {83, 2322},
    {84, 2322},
    {85, 2322},
    {86, 2322},
    {87, 2322},
    {88, 2322},

    {89, 2322},
    {90, 2322},
    {91, 2414},
    {92, 3183},
    {93, 2414},
    {94, 2322},
    {95, 2322},
    {96, 2322},
    {97, 2322},
    {98, 2322},
    {99, 2322},
    {100, 2322},
    {101, 2322},
    {102, 2322},
    {103, 2322},
    {104, 2322},
    {105, 2322},
    {106, 2322},
    {107, 2322},
    {108, 2322},
    {109, 2322},
    {110, 2322},
    {111, 2322},
    {112, 2322},
    {113, 2322},
    {114, 2322},
    {115, 2322},
    {116, 2322},
    {117, 2322},
    {118, 2322},
    {119, 2322},
    {120, 2322},
    {121, 2322},
    {122, 2322},
    {123, 2414},
    {124, 3377},
    {125, 2414},
    {126, 2322},
    {127, 2322},
    {128, 2322},
    {129, 2322},
    {130, 2322},
    {131, 2322},
    {132, 2322},
    {133, 2322},
    {134, 2322},
    {135, 2322},
    {136, 2322},
    {137, 2322},
    {138, 2322},

    {139, 2322},
    {140, 2322},
    {141, 2322},
    {142, 2322},
    {143, 2322},
    {144, 2322},
    {145, 2322},
    {146, 2322},
    {147, 2322},
    {148, 2322},
    {149, 2322},
    {150, 2322},
    {151, 2322},
    {152, 2322},
    {153, 2322},
    {154, 2322},
    {155, 2322},
    {156, 2322},
    {157, 2322},
    {158, 2322},
    {159, 2322},
    {160, 2322},
    {161, 2322},
    {162, 2322},
    {163, 2322},
    {164, 2322},
    {165, 2322},
    {166, 2322},
    {167, 2322},
    {168, 2322},
    {169, 2322},
    {170, 2322},
    {171, 2322},
    {172, 2322},
    {173, 2322},
    {174, 2322},
    {175, 2322},
    {176, 2322},
    {177, 2322},
    {178, 2322},
    {179, 2322},
    {180, 2322},
    {181, 2322},
    {182, 2322},
    {183, 2322},
    {184, 2322},
    {185, 2322},
    {186, 2322},
    {187, 2322},
    {188, 2322},

    {189, 2322},
    {190, 2322},
    {191, 2322},
    {192, 2322},
    {193, 2322},
    {194, 2322},
    {195, 2322},
    {196, 2322},
    {197, 2322},
    {198, 2322},
    {199, 2322},
    {200, 2322},
    {201, 2322},
    {202, 2322},
    {203, 2322},
    {204, 2322},
    {205, 2322},
    {206, 2322},
    {207, 2322},
    {208, 2322},
    {209, 2322},
    {210, 2322},
    {211, 2322},
    {212, 2322},
    {213, 2322},
    {214, 2322},
    {215, 2322},
    {216, 2322},
    {217, 2322},
    {218, 2322},
    {219, 2322},
    {220, 2322},
    {221, 2322},
    {222, 2322},
    {223, 2322},
    {224, 2322},
    {225, 2322},
    {226, 2322},
    {227, 2322},
    {228, 2322},
    {229, 2322},
    {230, 2322},
    {231, 2322},
    {232, 2322},
    {233, 2322},
    {234, 2322},
    {235, 2322},
    {236, 2322},
    {237, 2322},
    {238, 2322},

    {239, 2322},
    {240, 2322},
    {241, 2322},
    {242, 2322},
    {243, 2322},
    {244, 2322},
    {245, 2322},
    {246, 2322},
    {247, 2322},
    {248, 2322},
    {249, 2322},
    {250, 2322},
    {251, 2322},
    {252, 2322},
    {253, 2322},
    {254, 2322},
    {255, 2322},
    {256, 2322},
    {0, 0},
    {0, 7361},
    {1, 3151},
    {2, 3151},
    {3, 3151},
    {4, 3151},
    {5, 3151},
    {6, 3151},
    {7, 3151},
    {8, 3151},
    {9, 3151},
    {10, 3151},
    {11, 3151},
    {12, 3151},
    {13, 3151},
    {14, 3151},
    {15, 3151},
    {16, 3151},
    {17, 3151},
    {18, 3151},
    {19, 3151},
    {20, 3151},
    {21, 3151},
    {22, 3151},
    {23, 3151},
    {24, 3151},
    {25, 3151},
    {26, 3151},
    {27, 3151},
    {28, 3151},
    {29, 3151},
    {30, 3151},

    {31, 3151},
    {32, 3151},
    {33, 3151},
    {34, 3121},
    {35, 3151},
    {36, 3151},
    {37, 3151},
    {38, 3151},
    {39, 3151},
    {40, 3151},
    {41, 3151},
    {42, 3151},
    {43, 3151},
    {44, 3151},
    {45, 3151},
    {46, 3151},
    {47, 3151},
    {48, 3151},
    {49, 3151},
    {50, 3151},
    {51, 3151},
    {52, 3151},
    {53, 3151},
    {54, 3151},
    {55, 3151},
    {56, 3151},
    {57, 3151},
    {58, 3151},
    {59, 3151},
    {60, 3151},
    {61, 3151},
    {62, 3151},
    {63, 3151},
    {64, 3151},
    {65, 3151},
    {66, 3151},
    {67, 3151},
    {68, 3151},
    {69, 3151},
    {70, 3151},
    {71, 3151},
    {72, 3151},
    {73, 3151},
    {74, 3151},
    {75, 3151},
    {76, 3151},
    {77, 3151},
    {78, 3151},
    {79, 3151},
    {80, 3151},

    {81, 3151},
    {82, 3151},
    {83, 3151},
    {84, 3151},
    {85, 3151},
    {86, 3151},
    {87, 3151},
    {88, 3151},
    {89, 3151},
    {90, 3151},
    {91, 3151},
    {92, 3409},
    {93, 3151},
    {94, 3151},
    {95, 3151},
    {96, 3151},
    {97, 3151},
    {98, 3151},
    {99, 3151},
    {100, 3151},
    {101, 3151},
    {102, 3151},
    {103, 3151},
    {104, 3151},
    {105, 3151},
    {106, 3151},
    {107, 3151},
    {108, 3151},
    {109, 3151},
    {110, 3151},
    {111, 3151},
    {112, 3151},
    {113, 3151},
    {114, 3151},
    {115, 3151},
    {116, 3151},
    {117, 3151},
    {118, 3151},
    {119, 3151},
    {120, 3151},
    {121, 3151},
    {122, 3151},
    {123, 3151},
    {124, 3151},
    {125, 3151},
    {126, 3151},
    {127, 3151},
    {128, 3151},
    {129, 3151},
    {130, 3151},

    {131, 3151},
    {132, 3151},
    {133, 3151},
    {134, 3151},
    {135, 3151},
    {136, 3151},
    {137, 3151},
    {138, 3151},
    {139, 3151},
    {140, 3151},
    {141, 3151},
    {142, 3151},
    {143, 3151},
    {144, 3151},
    {145, 3151},
    {146, 3151},
    {147, 3151},
    {148, 3151},
    {149, 3151},
    {150, 3151},
    {151, 3151},
    {152, 3151},
    {153, 3151},
    {154, 3151},
    {155, 3151},
    {156, 3151},
    {157, 3151},
    {158, 3151},
    {159, 3151},
    {160, 3151},
    {161, 3151},
    {162, 3151},
    {163, 3151},
    {164, 3151},
    {165, 3151},
    {166, 3151},
    {167, 3151},
    {168, 3151},
    {169, 3151},
    {170, 3151},
    {171, 3151},
    {172, 3151},
    {173, 3151},
    {174, 3151},
    {175, 3151},
    {176, 3151},
    {177, 3151},
    {178, 3151},
    {179, 3151},
    {180, 3151},

    {181, 3151},
    {182, 3151},
    {183, 3151},
    {184, 3151},
    {185, 3151},
    {186, 3151},
    {187, 3151},
    {188, 3151},
    {189, 3151},
    {190, 3151},
    {191, 3151},
    {192, 3151},
    {193, 3151},
    {194, 3151},
    {195, 3151},
    {196, 3151},
    {197, 3151},
    {198, 3151},
    {199, 3151},
    {200, 3151},
    {201, 3151},
    {202, 3151},
    {203, 3151},
    {204, 3151},
    {205, 3151},
    {206, 3151},
    {207, 3151},
    {208, 3151},
    {209, 3151},
    {210, 3151},
    {211, 3151},
    {212, 3151},
    {213, 3151},
    {214, 3151},
    {215, 3151},
    {216, 3151},
    {217, 3151},
    {218, 3151},
    {219, 3151},
    {220, 3151},
    {221, 3151},
    {222, 3151},
    {223, 3151},
    {224, 3151},
    {225, 3151},
    {226, 3151},
    {227, 3151},
    {228, 3151},
    {229, 3151},
    {230, 3151},

    {231, 3151},
    {232, 3151},
    {233, 3151},
    {234, 3151},
    {235, 3151},
    {236, 3151},
    {237, 3151},
    {238, 3151},
    {239, 3151},
    {240, 3151},
    {241, 3151},
    {242, 3151},
    {243, 3151},
    {244, 3151},
    {245, 3151},
    {246, 3151},
    {247, 3151},
    {248, 3151},
    {249, 3151},
    {250, 3151},
    {251, 3151},
    {252, 3151},
    {253, 3151},
    {254, 3151},
    {255, 3151},
    {256, 3151},
    {0, 0},
    {0, 7103},
    {1, 2893},
    {2, 2893},
    {3, 2893},
    {4, 2893},
    {5, 2893},
    {6, 2893},
    {7, 2893},
    {8, 2893},
    {9, 2893},
    {10, 2893},
    {11, 2893},
    {12, 2893},
    {13, 2893},
    {14, 2893},
    {15, 2893},
    {16, 2893},
    {17, 2893},
    {18, 2893},
    {19, 2893},
    {20, 2893},
    {21, 2893},
    {22, 2893},

    {23, 2893},
    {24, 2893},
    {25, 2893},
    {26, 2893},
    {27, 2893},
    {28, 2893},
    {29, 2893},
    {30, 2893},
    {31, 2893},
    {32, 2893},
    {33, 2893},
    {34, 2863},
    {35, 2893},
    {36, 2893},
    {37, 2893},
    {38, 2893},
    {39, 2893},
    {40, 2893},
    {41, 2893},
    {42, 2893},
    {43, 2893},
    {44, 2893},
    {45, 2893},
    {46, 2893},
    {47, 2893},
    {48, 2893},
    {49, 2893},
    {50, 2893},
    {51, 2893},
    {52, 2893},
    {53, 2893},
    {54, 2893},
    {55, 2893},
    {56, 2893},
    {57, 2893},
    {58, 2893},
    {59, 2893},
    {60, 2893},
    {61, 2893},
    {62, 2893},
    {63, 2893},
    {64, 2893},
    {65, 2893},
    {66, 2893},
    {67, 2893},
    {68, 2893},
    {69, 2893},
    {70, 2893},
    {71, 2893},
    {72, 2893},

    {73, 2893},
    {74, 2893},
    {75, 2893},
    {76, 2893},
    {77, 2893},
    {78, 2893},
    {79, 2893},
    {80, 2893},
    {81, 2893},
    {82, 2893},
    {83, 2893},
    {84, 2893},
    {85, 2893},
    {86, 2893},
    {87, 2893},
    {88, 2893},
    {89, 2893},
    {90, 2893},
    {91, 2893},
    {92, 3151},
    {93, 2893},
    {94, 2893},
    {95, 2893},
    {96, 2893},
    {97, 2893},
    {98, 2893},
    {99, 2893},
    {100, 2893},
    {101, 2893},
    {102, 2893},
    {103, 2893},
    {104, 2893},
    {105, 2893},
    {106, 2893},
    {107, 2893},
    {108, 2893},
    {109, 2893},
    {110, 2893},
    {111, 2893},
    {112, 2893},
    {113, 2893},
    {114, 2893},
    {115, 2893},
    {116, 2893},
    {117, 2893},
    {118, 2893},
    {119, 2893},
    {120, 2893},
    {121, 2893},
    {122, 2893},

    {123, 2893},
    {124, 2893},
    {125, 2893},
    {126, 2893},
    {127, 2893},
    {128, 2893},
    {129, 2893},
    {130, 2893},
    {131, 2893},
    {132, 2893},
    {133, 2893},
    {134, 2893},
    {135, 2893},
    {136, 2893},
    {137, 2893},
    {138, 2893},
    {139, 2893},
    {140, 2893},
    {141, 2893},
    {142, 2893},
    {143, 2893},
    {144, 2893},
    {145, 2893},
    {146, 2893},
    {147, 2893},
    {148, 2893},
    {149, 2893},
    {150, 2893},
    {151, 2893},
    {152, 2893},
    {153, 2893},
    {154, 2893},
    {155, 2893},
    {156, 2893},
    {157, 2893},
    {158, 2893},
    {159, 2893},
    {160, 2893},
    {161, 2893},
    {162, 2893},
    {163, 2893},
    {164, 2893},
    {165, 2893},
    {166, 2893},
    {167, 2893},
    {168, 2893},
    {169, 2893},
    {170, 2893},
    {171, 2893},
    {172, 2893},

    {173, 2893},
    {174, 2893},
    {175, 2893},
    {176, 2893},
    {177, 2893},
    {178, 2893},
    {179, 2893},
    {180, 2893},
    {181, 2893},
    {182, 2893},
    {183, 2893},
    {184, 2893},
    {185, 2893},
    {186, 2893},
    {187, 2893},
    {188, 2893},
    {189, 2893},
    {190, 2893},
    {191, 2893},
    {192, 2893},
    {193, 2893},
    {194, 2893},
    {195, 2893},
    {196, 2893},
    {197, 2893},
    {198, 2893},
    {199, 2893},
    {200, 2893},
    {201, 2893},
    {202, 2893},
    {203, 2893},
    {204, 2893},
    {205, 2893},
    {206, 2893},
    {207, 2893},
    {208, 2893},
    {209, 2893},
    {210, 2893},
    {211, 2893},
    {212, 2893},
    {213, 2893},
    {214, 2893},
    {215, 2893},
    {216, 2893},
    {217, 2893},
    {218, 2893},
    {219, 2893},
    {220, 2893},
    {221, 2893},
    {222, 2893},

    {223, 2893},
    {224, 2893},
    {225, 2893},
    {226, 2893},
    {227, 2893},
    {228, 2893},
    {229, 2893},
    {230, 2893},
    {231, 2893},
    {232, 2893},
    {233, 2893},
    {234, 2893},
    {235, 2893},
    {236, 2893},
    {237, 2893},
    {238, 2893},
    {239, 2893},
    {240, 2893},
    {241, 2893},
    {242, 2893},
    {243, 2893},
    {244, 2893},
    {245, 2893},
    {246, 2893},
    {247, 2893},
    {248, 2893},
    {249, 2893},
    {250, 2893},
    {251, 2893},
    {252, 2893},
    {253, 2893},
    {254, 2893},
    {255, 2893},
    {256, 2893},
    {0, 0},
    {0, 6845},
    {1, 3151},
    {2, 3151},
    {3, 3151},
    {4, 3151},
    {5, 3151},
    {6, 3151},
    {7, 3151},
    {8, 3151},
    {9, 3409},
    {10, 3409},
    {11, 3151},
    {12, 3409},
    {13, 3409},
    {14, 3151},

    {15, 3151},
    {16, 3151},
    {17, 3151},
    {18, 3151},
    {19, 3151},
    {20, 3151},
    {21, 3151},
    {22, 3151},
    {23, 3151},
    {24, 3151},
    {25, 3151},
    {26, 3151},
    {27, 3151},
    {28, 3151},
    {29, 3151},
    {30, 3151},
    {31, 3151},
    {32, 3409},
    {33, 2607},
    {34, 2607},
    {35, 2607},
    {36, 2607},
    {37, 2607},
    {38, 2607},
    {39, 3151},
    {40, 2607},
    {41, 2607},
    {42, 2607},
    {43, 2607},
    {44, 2607},
    {45, 2607},
    {46, 2607},
    {47, 2627},
    {48, 3151},
    {49, 3151},
    {50, 3151},
    {51, 3151},
    {52, 3151},
    {53, 3151},
    {54, 3151},
    {55, 3151},
    {56, 3151},
    {57, 3151},
    {58, 2607},
    {59, 3151},
    {60, 2607},
    {61, 2607},
    {62, 2607},
    {63, 2607},
    {64, 2607},

    {65, 3151},
    {66, 3151},
    {67, 3151},
    {68, 3151},
    {69, 3151},
    {70, 3151},
    {71, 3151},
    {72, 3151},
    {73, 3151},
    {74, 3151},
    {75, 3151},
    {76, 3151},
    {77, 3151},
    {78, 3151},
    {79, 3151},
    {80, 3151},
    {81, 3151},
    {82, 3151},
    {83, 3151},
    {84, 3151},
    {85, 3151},
    {86, 3151},
    {87, 3151},
    {88, 3151},
    {89, 3151},
    {90, 3151},
    {91, 2607},
    {92, 2893},
    {93, 2607},
    {94, 3151},
    {95, 3151},
    {96, 3151},
    {97, 3151},
    {98, 3151},
    {99, 3151},
    {100, 3151},
    {101, 3151},
    {102, 3151},
    {103, 3151},
    {104, 3151},
    {105, 3151},
    {106, 3151},
    {107, 3151},
    {108, 3151},
    {109, 3151},
    {110, 3151},
    {111, 3151},
    {112, 3151},
    {113, 3151},
    {114, 3151},

    {115, 3151},
    {116, 3151},
    {117, 3151},
    {118, 3151},
    {119, 3151},
    {120, 3151},
    {121, 3151},
    {122, 3151},
    {123, 2607},
    {124, 2607},
    {125, 2607},
    {126, 3151},
    {127, 3151},
    {128, 3151},
    {129, 3151},
    {130, 3151},
    {131, 3151},
    {132, 3151},
    {133, 3151},
    {134, 3151},
    {135, 3151},
    {136, 3151},
    {137, 3151},
    {138, 3151},
    {139, 3151},
    {140, 3151},
    {141, 3151},
    {142, 3151},
    {143, 3151},
    {144, 3151},
    {145, 3151},
    {146, 3151},
    {147, 3151},
    {148, 3151},
    {149, 3151},
    {150, 3151},
    {151, 3151},
    {152, 3151},
    {153, 3151},
    {154, 3151},
    {155, 3151},
    {156, 3151},
    {157, 3151},
    {158, 3151},
    {159, 3151},
    {160, 3151},
    {161, 3151},
    {162, 3151},
    {163, 3151},
    {164, 3151},

    {165, 3151},
    {166, 3151},
    {167, 3151},
    {168, 3151},
    {169, 3151},
    {170, 3151},
    {171, 3151},
    {172, 3151},
    {173, 3151},
    {174, 3151},
    {175, 3151},
    {176, 3151},
    {177, 3151},
    {178, 3151},
    {179, 3151},
    {180, 3151},
    {181, 3151},
    {182, 3151},
    {183, 3151},
    {184, 3151},
    {185, 3151},
    {186, 3151},
    {187, 3151},
    {188, 3151},
    {189, 3151},
    {190, 3151},
    {191, 3151},
    {192, 3151},
    {193, 3151},
    {194, 3151},
    {195, 3151},
    {196, 3151},
    {197, 3151},
    {198, 3151},
    {199, 3151},
    {200, 3151},
    {201, 3151},
    {202, 3151},
    {203, 3151},
    {204, 3151},
    {205, 3151},
    {206, 3151},
    {207, 3151},
    {208, 3151},
    {209, 3151},
    {210, 3151},
    {211, 3151},
    {212, 3151},
    {213, 3151},
    {214, 3151},

    {215, 3151},
    {216, 3151},
    {217, 3151},
    {218, 3151},
    {219, 3151},
    {220, 3151},
    {221, 3151},
    {222, 3151},
    {223, 3151},
    {224, 3151},
    {225, 3151},
    {226, 3151},
    {227, 3151},
    {228, 3151},
    {229, 3151},
    {230, 3151},
    {231, 3151},
    {232, 3151},
    {233, 3151},
    {234, 3151},
    {235, 3151},
    {236, 3151},
    {237, 3151},
    {238, 3151},
    {239, 3151},
    {240, 3151},
    {241, 3151},
    {242, 3151},
    {243, 3151},
    {244, 3151},
    {245, 3151},
    {246, 3151},
    {247, 3151},
    {248, 3151},
    {249, 3151},
    {250, 3151},
    {251, 3151},
    {252, 3151},
    {253, 3151},
    {254, 3151},
    {255, 3151},
    {256, 3151},
    {0, 0},
    {0, 6587},
    {1, 2893},
    {2, 2893},
    {3, 2893},
    {4, 2893},
    {5, 2893},
    {6, 2893},

    {7, 2893},
    {8, 2893},
    {9, 3151},
    {10, 3151},
    {11, 2893},
    {12, 3151},
    {13, 3151},
    {14, 2893},
    {15, 2893},
    {16, 2893},
    {17, 2893},
    {18, 2893},
    {19, 2893},
    {20, 2893},
    {21, 2893},
    {22, 2893},
    {23, 2893},
    {24, 2893},
    {25, 2893},
    {26, 2893},
    {27, 2893},
    {28, 2893},
    {29, 2893},
    {30, 2893},
    {31, 2893},
    {32, 3151},
    {33, 2349},
    {34, 2349},
    {35, 2349},
    {36, 2349},
    {37, 2349},
    {38, 2349},
    {39, 2893},
    {40, 2349},
    {41, 2349},
    {42, 2349},
    {43, 2349},
    {44, 2349},
    {45, 2349},
    {46, 2349},
    {47, 2369},
    {48, 2893},
    {49, 2893},
    {50, 2893},
    {51, 2893},
    {52, 2893},
    {53, 2893},
    {54, 2893},
    {55, 2893},
    {56, 2893},

    {57, 2893},
    {58, 2349},
    {59, 2893},
    {60, 2349},
    {61, 2349},
    {62, 2349},
    {63, 2349},
    {64, 2349},
    {65, 2893},
    {66, 2893},
    {67, 2893},
    {68, 2893},
    {69, 2893},
    {70, 2893},
    {71, 2893},
    {72, 2893},
    {73, 2893},
    {74, 2893},
    {75, 2893},
    {76, 2893},
    {77, 2893},
    {78, 2893},
    {79, 2893},
    {80, 2893},
    {81, 2893},
    {82, 2893},
    {83, 2893},
    {84, 2893},
    {85, 2893},
    {86, 2893},
    {87, 2893},
    {88, 2893},
    {89, 2893},
    {90, 2893},
    {91, 2349},
    {92, 2635},
    {93, 2349},
    {94, 2893},
    {95, 2893},
    {96, 2893},
    {97, 2893},
    {98, 2893},
    {99, 2893},
    {100, 2893},
    {101, 2893},
    {102, 2893},
    {103, 2893},
    {104, 2893},
    {105, 2893},
    {106, 2893},

    {107, 2893},
    {108, 2893},
    {109, 2893},
    {110, 2893},
    {111, 2893},
    {112, 2893},
    {113, 2893},
    {114, 2893},
    {115, 2893},
    {116, 2893},
    {117, 2893},
    {118, 2893},
    {119, 2893},
    {120, 2893},
    {121, 2893},
    {122, 2893},
    {123, 2349},
    {124, 2349},
    {125, 2349},
    {126, 2893},
    {127, 2893},
    {128, 2893},
    {129, 2893},
    {130, 2893},
    {131, 2893},
    {132, 2893},
    {133, 2893},
    {134, 2893},
    {135, 2893},
    {136, 2893},
    {137, 2893},
    {138, 2893},
    {139, 2893},
    {140, 2893},
    {141, 2893},
    {142, 2893},
    {143, 2893},
    {144, 2893},
    {145, 2893},
    {146, 2893},
    {147, 2893},
    {148, 2893},
    {149, 2893},
    {150, 2893},
    {151, 2893},
    {152, 2893},
    {153, 2893},
    {154, 2893},
    {155, 2893},
    {156, 2893},

    {157, 2893},
    {158, 2893},
    {159, 2893},
    {160, 2893},
    {161, 2893},
    {162, 2893},
    {163, 2893},
    {164, 2893},
    {165, 2893},
    {166, 2893},
    {167, 2893},
    {168, 2893},
    {169, 2893},
    {170, 2893},
    {171, 2893},
    {172, 2893},
    {173, 2893},
    {174, 2893},
    {175, 2893},
    {176, 2893},
    {177, 2893},
    {178, 2893},
    {179, 2893},
    {180, 2893},
    {181, 2893},
    {182, 2893},
    {183, 2893},
    {184, 2893},
    {185, 2893},
    {186, 2893},
    {187, 2893},
    {188, 2893},
    {189, 2893},
    {190, 2893},
    {191, 2893},
    {192, 2893},
    {193, 2893},
    {194, 2893},
    {195, 2893},
    {196, 2893},
    {197, 2893},
    {198, 2893},
    {199, 2893},
    {200, 2893},
    {201, 2893},
    {202, 2893},
    {203, 2893},
    {204, 2893},
    {205, 2893},
    {206, 2893},

    {207, 2893},
    {208, 2893},
    {209, 2893},
    {210, 2893},
    {211, 2893},
    {212, 2893},
    {213, 2893},
    {214, 2893},
    {215, 2893},
    {216, 2893},
    {217, 2893},
    {218, 2893},
    {219, 2893},
    {220, 2893},
    {221, 2893},
    {222, 2893},
    {223, 2893},
    {224, 2893},
    {225, 2893},
    {226, 2893},
    {227, 2893},
    {228, 2893},
    {229, 2893},
    {230, 2893},
    {231, 2893},
    {232, 2893},
    {233, 2893},
    {234, 2893},
    {235, 2893},
    {236, 2893},
    {237, 2893},
    {238, 2893},
    {239, 2893},
    {240, 2893},
    {241, 2893},
    {242, 2893},
    {243, 2893},
    {244, 2893},
    {245, 2893},
    {246, 2893},
    {247, 2893},
    {248, 2893},
    {249, 2893},
    {250, 2893},
    {251, 2893},
    {252, 2893},
    {253, 2893},
    {254, 2893},
    {255, 2893},
    {256, 2893},

    {0, 0},
    {0, 6329},
    {1, 2119},
    {2, 2119},
    {3, 2119},
    {4, 2119},
    {5, 2119},
    {6, 2119},
    {7, 2119},
    {8, 2119},
    {9, 2119},
    {10, 2119},
    {11, 2119},
    {12, 2119},
    {13, 2119},
    {14, 2119},
    {15, 2119},
    {16, 2119},
    {17, 2119},
    {18, 2119},
    {19, 2119},
    {20, 2119},
    {21, 2119},
    {22, 2119},
    {23, 2119},
    {24, 2119},
    {25, 2119},
    {26, 2119},
    {27, 2119},
    {28, 2119},
    {29, 2119},
    {30, 2119},
    {31, 2119},
    {32, 2119},
    {33, 2119},
    {34, 2113},
    {35, 2119},
    {36, 2119},
    {37, 2119},
    {38, 2119},
    {39, 2119},
    {40, 2119},
    {41, 2119},
    {42, 2119},
    {43, 2119},
    {44, 2119},
    {45, 2119},
    {46, 2119},
    {47, 2119},
    {48, 2119},

    {49, 2119},
    {50, 2119},
    {51, 2119},
    {52, 2119},
    {53, 2119},
    {54, 2119},
    {55, 2119},
    {56, 2119},
    {57, 2119},
    {58, 2119},
    {59, 2119},
    {60, 2119},
    {61, 2119},
    {62, 2119},
    {63, 2119},
    {64, 2119},
    {65, 2119},
    {66, 2119},
    {67, 2119},
    {68, 2119},
    {69, 2119},
    {70, 2119},
    {71, 2119},
    {72, 2119},
    {73, 2119},
    {74, 2119},
    {75, 2119},
    {76, 2119},
    {77, 2119},
    {78, 2119},
    {79, 2119},
    {80, 2119},
    {81, 2119},
    {82, 2119},
    {83, 2119},
    {84, 2119},
    {85, 2119},
    {86, 2119},
    {87, 2119},
    {88, 2119},
    {89, 2119},
    {90, 2119},
    {91, 2119},
    {92, 2377},
    {93, 2119},
    {94, 2119},
    {95, 2119},
    {96, 2119},
    {97, 2119},
    {98, 2119},

    {99, 2119},
    {100, 2119},
    {101, 2119},
    {102, 2119},
    {103, 2119},
    {104, 2119},
    {105, 2119},
    {106, 2119},
    {107, 2119},
    {108, 2119},
    {109, 2119},
    {110, 2119},
    {111, 2119},
    {112, 2119},
    {113, 2119},
    {114, 2119},
    {115, 2119},
    {116, 2119},
    {117, 2119},
    {118, 2119},
    {119, 2119},
    {120, 2119},
    {121, 2119},
    {122, 2119},
    {123, 2119},
    {124, 2119},
    {125, 2119},
    {126, 2119},
    {127, 2119},
    {128, 2119},
    {129, 2119},
    {130, 2119},
    {131, 2119},
    {132, 2119},
    {133, 2119},
    {134, 2119},
    {135, 2119},
    {136, 2119},
    {137, 2119},
    {138, 2119},
    {139, 2119},
    {140, 2119},
    {141, 2119},
    {142, 2119},
    {143, 2119},
    {144, 2119},
    {145, 2119},
    {146, 2119},
    {147, 2119},
    {148, 2119},

    {149, 2119},
    {150, 2119},
    {151, 2119},
    {152, 2119},
    {153, 2119},
    {154, 2119},
    {155, 2119},
    {156, 2119},
    {157, 2119},
    {158, 2119},
    {159, 2119},
    {160, 2119},
    {161, 2119},
    {162, 2119},
    {163, 2119},
    {164, 2119},
    {165, 2119},
    {166, 2119},
    {167, 2119},
    {168, 2119},
    {169, 2119},
    {170, 2119},
    {171, 2119},
    {172, 2119},
    {173, 2119},
    {174, 2119},
    {175, 2119},
    {176, 2119},
    {177, 2119},
    {178, 2119},
    {179, 2119},
    {180, 2119},
    {181, 2119},
    {182, 2119},
    {183, 2119},
    {184, 2119},
    {185, 2119},
    {186, 2119},
    {187, 2119},
    {188, 2119},
    {189, 2119},
    {190, 2119},
    {191, 2119},
    {192, 2119},
    {193, 2119},
    {194, 2119},
    {195, 2119},
    {196, 2119},
    {197, 2119},
    {198, 2119},

    {199, 2119},
    {200, 2119},
    {201, 2119},
    {202, 2119},
    {203, 2119},
    {204, 2119},
    {205, 2119},
    {206, 2119},
    {207, 2119},
    {208, 2119},
    {209, 2119},
    {210, 2119},
    {211, 2119},
    {212, 2119},
    {213, 2119},
    {214, 2119},
    {215, 2119},
    {216, 2119},
    {217, 2119},
    {218, 2119},
    {219, 2119},
    {220, 2119},
    {221, 2119},
    {222, 2119},
    {223, 2119},
    {224, 2119},
    {225, 2119},
    {226, 2119},
    {227, 2119},
    {228, 2119},
    {229, 2119},
    {230, 2119},
    {231, 2119},
    {232, 2119},
    {233, 2119},
    {234, 2119},
    {235, 2119},
    {236, 2119},
    {237, 2119},
    {238, 2119},
    {239, 2119},
    {240, 2119},
    {241, 2119},
    {242, 2119},
    {243, 2119},
    {244, 2119},
    {245, 2119},
    {246, 2119},
    {247, 2119},
    {248, 2119},

    {249, 2119},
    {250, 2119},
    {251, 2119},
    {252, 2119},
    {253, 2119},
    {254, 2119},
    {255, 2119},
    {256, 2119},
    {0, 0},
    {0, 6071},
    {1, 1861},
    {2, 1861},
    {3, 1861},
    {4, 1861},
    {5, 1861},
    {6, 1861},
    {7, 1861},
    {8, 1861},
    {9, 1861},
    {10, 1861},
    {11, 1861},
    {12, 1861},
    {13, 1861},
    {14, 1861},
    {15, 1861},
    {16, 1861},
    {17, 1861},
    {18, 1861},
    {19, 1861},
    {20, 1861},
    {21, 1861},
    {22, 1861},
    {23, 1861},
    {24, 1861},
    {25, 1861},
    {26, 1861},
    {27, 1861},
    {28, 1861},
    {29, 1861},
    {30, 1861},
    {31, 1861},
    {32, 1861},
    {33, 1861},
    {34, 1855},
    {35, 1861},
    {36, 1861},
    {37, 1861},
    {38, 1861},
    {39, 1861},
    {40, 1861},

    {41, 1861},
    {42, 1861},
    {43, 1861},
    {44, 1861},
    {45, 1861},
    {46, 1861},
    {47, 1861},
    {48, 1861},
    {49, 1861},
    {50, 1861},
    {51, 1861},
    {52, 1861},
    {53, 1861},
    {54, 1861},
    {55, 1861},
    {56, 1861},
    {57, 1861},
    {58, 1861},
    {59, 1861},
    {60, 1861},
    {61, 1861},
    {62, 1861},
    {63, 1861},
    {64, 1861},
    {65, 1861},
    {66, 1861},
    {67, 1861},
    {68, 1861},
    {69, 1861},
    {70, 1861},
    {71, 1861},
    {72, 1861},
    {73, 1861},
    {74, 1861},
    {75, 1861},
    {76, 1861},
    {77, 1861},
    {78, 1861},
    {79, 1861},
    {80, 1861},
    {81, 1861},
    {82, 1861},
    {83, 1861},
    {84, 1861},
    {85, 1861},
    {86, 1861},
    {87, 1861},
    {88, 1861},
    {89, 1861},
    {90, 1861},

    {91, 1861},
    {92, 2119},
    {93, 1861},
    {94, 1861},
    {95, 1861},
    {96, 1861},
    {97, 1861},
    {98, 1861},
    {99, 1861},
    {100, 1861},
    {101, 1861},
    {102, 1861},
    {103, 1861},
    {104, 1861},
    {105, 1861},
    {106, 1861},
    {107, 1861},
    {108, 1861},
    {109, 1861},
    {110, 1861},
    {111, 1861},
    {112, 1861},
    {113, 1861},
    {114, 1861},
    {115, 1861},
    {116, 1861},
    {117, 1861},
    {118, 1861},
    {119, 1861},
    {120, 1861},
    {121, 1861},
    {122, 1861},
    {123, 1861},
    {124, 1861},
    {125, 1861},
    {126, 1861},
    {127, 1861},
    {128, 1861},
    {129, 1861},
    {130, 1861},
    {131, 1861},
    {132, 1861},
    {133, 1861},
    {134, 1861},
    {135, 1861},
    {136, 1861},
    {137, 1861},
    {138, 1861},
    {139, 1861},
    {140, 1861},

    {141, 1861},
    {142, 1861},
    {143, 1861},
    {144, 1861},
    {145, 1861},
    {146, 1861},
    {147, 1861},
    {148, 1861},
    {149, 1861},
    {150, 1861},
    {151, 1861},
    {152, 1861},
    {153, 1861},
    {154, 1861},
    {155, 1861},
    {156, 1861},
    {157, 1861},
    {158, 1861},
    {159, 1861},
    {160, 1861},
    {161, 1861},
    {162, 1861},
    {163, 1861},
    {164, 1861},
    {165, 1861},
    {166, 1861},
    {167, 1861},
    {168, 1861},
    {169, 1861},
    {170, 1861},
    {171, 1861},
    {172, 1861},
    {173, 1861},
    {174, 1861},
    {175, 1861},
    {176, 1861},
    {177, 1861},
    {178, 1861},
    {179, 1861},
    {180, 1861},
    {181, 1861},
    {182, 1861},
    {183, 1861},
    {184, 1861},
    {185, 1861},
    {186, 1861},
    {187, 1861},
    {188, 1861},
    {189, 1861},
    {190, 1861},

    {191, 1861},
    {192, 1861},
    {193, 1861},
    {194, 1861},
    {195, 1861},
    {196, 1861},
    {197, 1861},
    {198, 1861},
    {199, 1861},
    {200, 1861},
    {201, 1861},
    {202, 1861},
    {203, 1861},
    {204, 1861},
    {205, 1861},
    {206, 1861},
    {207, 1861},
    {208, 1861},
    {209, 1861},
    {210, 1861},
    {211, 1861},
    {212, 1861},
    {213, 1861},
    {214, 1861},
    {215, 1861},
    {216, 1861},
    {217, 1861},
    {218, 1861},
    {219, 1861},
    {220, 1861},
    {221, 1861},
    {222, 1861},
    {223, 1861},
    {224, 1861},
    {225, 1861},
    {226, 1861},
    {227, 1861},
    {228, 1861},
    {229, 1861},
    {230, 1861},
    {231, 1861},
    {232, 1861},
    {233, 1861},
    {234, 1861},
    {235, 1861},
    {236, 1861},
    {237, 1861},
    {238, 1861},
    {239, 1861},
    {240, 1861},

    {241, 1861},
    {242, 1861},
    {243, 1861},
    {244, 1861},
    {245, 1861},
    {246, 1861},
    {247, 1861},
    {248, 1861},
    {249, 1861},
    {250, 1861},
    {251, 1861},
    {252, 1861},
    {253, 1861},
    {254, 1861},
    {255, 1861},
    {256, 1861},
    {0, 0},
    {0, 5813},
    {1, 2411},
    {2, 2411},
    {3, 2411},
    {4, 2411},
    {5, 2411},
    {6, 2411},
    {7, 2411},
    {8, 2411},
    {9, 2411},
    {10, 2411},
    {11, 2411},
    {12, 2411},
    {13, 2411},
    {14, 2411},
    {15, 2411},
    {16, 2411},
    {17, 2411},
    {18, 2411},
    {19, 2411},
    {20, 2411},
    {21, 2411},
    {22, 2411},
    {23, 2411},
    {24, 2411},
    {25, 2411},
    {26, 2411},
    {27, 2411},
    {28, 2411},
    {29, 2411},
    {30, 2411},
    {31, 2411},
    {32, 2411},

    {33, 2411},
    {34, 2411},
    {35, 2411},
    {36, 2411},
    {37, 2411},
    {38, 2411},
    {39, 2411},
    {40, 2411},
    {41, 2411},
    {42, 2132},
    {43, 2411},
    {44, 2411},
    {45, 2411},
    {46, 2411},
    {47, 2411},
    {48, 2411},
    {49, 2411},
    {50, 2411},
    {51, 2411},
    {52, 2411},
    {53, 2411},
    {54, 2411},
    {55, 2411},
    {56, 2411},
    {57, 2411},
    {58, 2411},
    {59, 2411},
    {60, 2411},
    {61, 2411},
    {62, 2411},
    {63, 2411},
    {64, 2411},
    {65, 2411},
    {66, 2411},
    {67, 2411},
    {68, 2411},
    {69, 2411},
    {70, 2411},
    {71, 2411},
    {72, 2411},
    {73, 2411},
    {74, 2411},
    {75, 2411},
    {76, 2411},
    {77, 2411},
    {78, 2411},
    {79, 2411},
    {80, 2411},
    {81, 2411},
    {82, 2411},

    {83, 2411},
    {84, 2411},
    {85, 2411},
    {86, 2411},
    {87, 2411},
    {88, 2411},
    {89, 2411},
    {90, 2411},
    {91, 2411},
    {92, 2411},
    {93, 2411},
    {94, 2411},
    {95, 2411},
    {96, 2411},
    {97, 2411},
    {98, 2411},
    {99, 2411},
    {100, 2411},
    {101, 2411},
    {102, 2411},
    {103, 2411},
    {104, 2411},
    {105, 2411},
    {106, 2411},
    {107, 2411},
    {108, 2411},
    {109, 2411},
    {110, 2411},
    {111, 2411},
    {112, 2411},
    {113, 2411},
    {114, 2411},
    {115, 2411},
    {116, 2411},
    {117, 2411},
    {118, 2411},
    {119, 2411},
    {120, 2411},
    {121, 2411},
    {122, 2411},
    {123, 2411},
    {124, 2411},
    {125, 2411},
    {126, 2411},
    {127, 2411},
    {128, 2411},
    {129, 2411},
    {130, 2411},
    {131, 2411},
    {132, 2411},

    {133, 2411},
    {134, 2411},
    {135, 2411},
    {136, 2411},
    {137, 2411},
    {138, 2411},
    {139, 2411},
    {140, 2411},
    {141, 2411},
    {142, 2411},
    {143, 2411},
    {144, 2411},
    {145, 2411},
    {146, 2411},
    {147, 2411},
    {148, 2411},
    {149, 2411},
    {150, 2411},
    {151, 2411},
    {152, 2411},
    {153, 2411},
    {154, 2411},
    {155, 2411},
    {156, 2411},
    {157, 2411},
    {158, 2411},
    {159, 2411},
    {160, 2411},
    {161, 2411},
    {162, 2411},
    {163, 2411},
    {164, 2411},
    {165, 2411},
    {166, 2411},
    {167, 2411},
    {168, 2411},
    {169, 2411},
    {170, 2411},
    {171, 2411},
    {172, 2411},
    {173, 2411},
    {174, 2411},
    {175, 2411},
    {176, 2411},
    {177, 2411},
    {178, 2411},
    {179, 2411},
    {180, 2411},
    {181, 2411},
    {182, 2411},

    {183, 2411},
    {184, 2411},
    {185, 2411},
    {186, 2411},
    {187, 2411},
    {188, 2411},
    {189, 2411},
    {190, 2411},
    {191, 2411},
    {192, 2411},
    {193, 2411},
    {194, 2411},
    {195, 2411},
    {196, 2411},
    {197, 2411},
    {198, 2411},
    {199, 2411},
    {200, 2411},
    {201, 2411},
    {202, 2411},
    {203, 2411},
    {204, 2411},
    {205, 2411},
    {206, 2411},
    {207, 2411},
    {208, 2411},
    {209, 2411},
    {210, 2411},
    {211, 2411},
    {212, 2411},
    {213, 2411},
    {214, 2411},
    {215, 2411},
    {216, 2411},
    {217, 2411},
    {218, 2411},
    {219, 2411},
    {220, 2411},
    {221, 2411},
    {222, 2411},
    {223, 2411},
    {224, 2411},
    {225, 2411},
    {226, 2411},
    {227, 2411},
    {228, 2411},
    {229, 2411},
    {230, 2411},
    {231, 2411},
    {232, 2411},

    {233, 2411},
    {234, 2411},
    {235, 2411},
    {236, 2411},
    {237, 2411},
    {238, 2411},
    {239, 2411},
    {240, 2411},
    {241, 2411},
    {242, 2411},
    {243, 2411},
    {244, 2411},
    {245, 2411},
    {246, 2411},
    {247, 2411},
    {248, 2411},
    {249, 2411},
    {250, 2411},
    {251, 2411},
    {252, 2411},
    {253, 2411},
    {254, 2411},
    {255, 2411},
    {256, 2411},
    {0, 0},
    {0, 5555},
    {1, 2153},
    {2, 2153},
    {3, 2153},
    {4, 2153},
    {5, 2153},
    {6, 2153},
    {7, 2153},
    {8, 2153},
    {9, 2153},
    {10, 2153},
    {11, 2153},
    {12, 2153},
    {13, 2153},
    {14, 2153},
    {15, 2153},
    {16, 2153},
    {17, 2153},
    {18, 2153},
    {19, 2153},
    {20, 2153},
    {21, 2153},
    {22, 2153},
    {23, 2153},
    {24, 2153},

    {25, 2153},
    {26, 2153},
    {27, 2153},
    {28, 2153},
    {29, 2153},
    {30, 2153},
    {31, 2153},
    {32, 2153},
    {33, 2153},
    {34, 2153},
    {35, 2153},
    {36, 2153},
    {37, 2153},
    {38, 2153},
    {39, 2153},
    {40, 2153},
    {41, 2153},
    {42, 1874},
    {43, 2153},
    {44, 2153},
    {45, 2153},
    {46, 2153},
    {47, 2153},
    {48, 2153},
    {49, 2153},
    {50, 2153},
    {51, 2153},
    {52, 2153},
    {53, 2153},
    {54, 2153},
    {55, 2153},
    {56, 2153},
    {57, 2153},
    {58, 2153},
    {59, 2153},
    {60, 2153},
    {61, 2153},
    {62, 2153},
    {63, 2153},
    {64, 2153},
    {65, 2153},
    {66, 2153},
    {67, 2153},
    {68, 2153},
    {69, 2153},
    {70, 2153},
    {71, 2153},
    {72, 2153},
    {73, 2153},
    {74, 2153},

    {75, 2153},
    {76, 2153},
    {77, 2153},
    {78, 2153},
    {79, 2153},
    {80, 2153},
    {81, 2153},
    {82, 2153},
    {83, 2153},
    {84, 2153},
    {85, 2153},
    {86, 2153},
    {87, 2153},
    {88, 2153},
    {89, 2153},
    {90, 2153},
    {91, 2153},
    {92, 2153},
    {93, 2153},
    {94, 2153},
    {95, 2153},
    {96, 2153},
    {97, 2153},
    {98, 2153},
    {99, 2153},
    {100, 2153},
    {101, 2153},
    {102, 2153},
    {103, 2153},
    {104, 2153},
    {105, 2153},
    {106, 2153},
    {107, 2153},
    {108, 2153},
    {109, 2153},
    {110, 2153},
    {111, 2153},
    {112, 2153},
    {113, 2153},
    {114, 2153},
    {115, 2153},
    {116, 2153},
    {117, 2153},
    {118, 2153},
    {119, 2153},
    {120, 2153},
    {121, 2153},
    {122, 2153},
    {123, 2153},
    {124, 2153},

    {125, 2153},
    {126, 2153},
    {127, 2153},
    {128, 2153},
    {129, 2153},
    {130, 2153},
    {131, 2153},
    {132, 2153},
    {133, 2153},
    {134, 2153},
    {135, 2153},
    {136, 2153},
    {137, 2153},
    {138, 2153},
    {139, 2153},
    {140, 2153},
    {141, 2153},
    {142, 2153},
    {143, 2153},
    {144, 2153},
    {145, 2153},
    {146, 2153},
    {147, 2153},
    {148, 2153},
    {149, 2153},
    {150, 2153},
    {151, 2153},
    {152, 2153},
    {153, 2153},
    {154, 2153},
    {155, 2153},
    {156, 2153},
    {157, 2153},
    {158, 2153},
    {159, 2153},
    {160, 2153},
    {161, 2153},
    {162, 2153},
    {163, 2153},
    {164, 2153},
    {165, 2153},
    {166, 2153},
    {167, 2153},
    {168, 2153},
    {169, 2153},
    {170, 2153},
    {171, 2153},
    {172, 2153},
    {173, 2153},
    {174, 2153},

    {175, 2153},
    {176, 2153},
    {177, 2153},
    {178, 2153},
    {179, 2153},
    {180, 2153},
    {181, 2153},
    {182, 2153},
    {183, 2153},
    {184, 2153},
    {185, 2153},
    {186, 2153},
    {187, 2153},
    {188, 2153},
    {189, 2153},
    {190, 2153},
    {191, 2153},
    {192, 2153},
    {193, 2153},
    {194, 2153},
    {195, 2153},
    {196, 2153},
    {197, 2153},
    {198, 2153},
    {199, 2153},
    {200, 2153},
    {201, 2153},
    {202, 2153},
    {203, 2153},
    {204, 2153},
    {205, 2153},
    {206, 2153},
    {207, 2153},
    {208, 2153},
    {209, 2153},
    {210, 2153},
    {211, 2153},
    {212, 2153},
    {213, 2153},
    {214, 2153},
    {215, 2153},
    {216, 2153},
    {217, 2153},
    {218, 2153},
    {219, 2153},
    {220, 2153},
    {221, 2153},
    {222, 2153},
    {223, 2153},
    {224, 2153},

    {225, 2153},
    {226, 2153},
    {227, 2153},
    {228, 2153},
    {229, 2153},
    {230, 2153},
    {231, 2153},
    {232, 2153},
    {233, 2153},
    {234, 2153},
    {235, 2153},
    {236, 2153},
    {237, 2153},
    {238, 2153},
    {239, 2153},
    {240, 2153},
    {241, 2153},
    {242, 2153},
    {243, 2153},
    {244, 2153},
    {245, 2153},
    {246, 2153},
    {247, 2153},
    {248, 2153},
    {249, 2153},
    {250, 2153},
    {251, 2153},
    {252, 2153},
    {253, 2153},
    {254, 2153},
    {255, 2153},
    {256, 2153},
    {0, 47},
    {0, 5297},
    {1, 2153},
    {2, 2153},
    {3, 2153},
    {4, 2153},
    {5, 2153},
    {6, 2153},
    {7, 2153},
    {8, 2153},
    {0, 0},
    {0, 0},
    {11, 2153},
    {0, 0},
    {0, 0},
    {14, 2153},
    {15, 2153},
    {16, 2153},

    {17, 2153},
    {18, 2153},
    {19, 2153},
    {20, 2153},
    {21, 2153},
    {22, 2153},
    {23, 2153},
    {24, 2153},
    {25, 2153},
    {26, 2153},
    {27, 2153},
    {28, 2153},
    {29, 2153},
    {30, 2153},
    {31, 2153},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, 2153},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 2153},
    {49, 2153},
    {50, 2153},
    {51, 2153},
    {52, 2153},
    {53, 2153},
    {54, 2153},
    {55, 2153},
    {56, 2153},
    {57, 2153},
    {0, 0},
    {59, 2153},
    {0, 0},
    {0, 26},
    {0, 5235},
    {0, 45},
    {0, 5233},
    {65, 2153},
    {66, 2153},

    {67, 2153},
    {68, 2153},
    {69, 2153},
    {70, 2153},
    {71, 2153},
    {72, 2153},
    {73, 2153},
    {74, 2153},
    {75, 2153},
    {76, 2153},
    {77, 2153},
    {78, 2153},
    {79, 2153},
    {80, 2153},
    {81, 2153},
    {82, 2153},
    {83, 2153},
    {84, 2153},
    {85, 2153},
    {86, 2153},
    {87, 2153},
    {88, 2153},
    {89, 2153},
    {90, 2153},
    {0, 37},
    {0, 5205},
    {0, 0},
    {94, 2153},
    {95, 2153},
    {96, 2153},
    {97, 2153},
    {98, 2153},
    {99, 2153},
    {100, 2153},
    {101, 2153},
    {102, 2153},
    {103, 2153},
    {104, 2153},
    {105, 2153},
    {106, 2153},
    {107, 2153},
    {108, 2153},
    {109, 2153},
    {110, 2153},
    {111, 2153},
    {112, 2153},
    {113, 2153},
    {114, 2153},
    {115, 2153},
    {116, 2153},

    {117, 2153},
    {118, 2153},
    {119, 2153},
    {120, 2153},
    {121, 2153},
    {122, 2153},
    {61, 1574},
    {0, 0},
    {0, 0},
    {126, 2153},
    {127, 2153},
    {128, 2153},
    {129, 2153},
    {130, 2153},
    {131, 2153},
    {132, 2153},
    {133, 2153},
    {134, 2153},
    {135, 2153},
    {136, 2153},
    {137, 2153},
    {138, 2153},
    {139, 2153},
    {140, 2153},
    {141, 2153},
    {142, 2153},
    {143, 2153},
    {144, 2153},
    {145, 2153},
    {146, 2153},
    {147, 2153},
    {148, 2153},
    {149, 2153},
    {150, 2153},
    {151, 2153},
    {152, 2153},
    {153, 2153},
    {154, 2153},
    {155, 2153},
    {156, 2153},
    {157, 2153},
    {158, 2153},
    {159, 2153},
    {160, 2153},
    {161, 2153},
    {162, 2153},
    {163, 2153},
    {164, 2153},
    {165, 2153},
    {166, 2153},

    {167, 2153},
    {168, 2153},
    {169, 2153},
    {170, 2153},
    {171, 2153},
    {172, 2153},
    {173, 2153},
    {174, 2153},
    {175, 2153},
    {176, 2153},
    {177, 2153},
    {178, 2153},
    {179, 2153},
    {180, 2153},
    {181, 2153},
    {182, 2153},
    {183, 2153},
    {184, 2153},
    {185, 2153},
    {186, 2153},
    {187, 2153},
    {188, 2153},
    {189, 2153},
    {190, 2153},
    {191, 2153},
    {192, 2153},
    {193, 2153},
    {194, 2153},
    {195, 2153},
    {196, 2153},
    {197, 2153},
    {198, 2153},
    {199, 2153},
    {200, 2153},
    {201, 2153},
    {202, 2153},
    {203, 2153},
    {204, 2153},
    {205, 2153},
    {206, 2153},
    {207, 2153},
    {208, 2153},
    {209, 2153},
    {210, 2153},
    {211, 2153},
    {212, 2153},
    {213, 2153},
    {214, 2153},
    {215, 2153},
    {216, 2153},

    {217, 2153},
    {218, 2153},
    {219, 2153},
    {220, 2153},
    {221, 2153},
    {222, 2153},
    {223, 2153},
    {224, 2153},
    {225, 2153},
    {226, 2153},
    {227, 2153},
    {228, 2153},
    {229, 2153},
    {230, 2153},
    {231, 2153},
    {232, 2153},
    {233, 2153},
    {234, 2153},
    {235, 2153},
    {236, 2153},
    {237, 2153},
    {238, 2153},
    {239, 2153},
    {240, 2153},
    {241, 2153},
    {242, 2153},
    {243, 2153},
    {244, 2153},
    {245, 2153},
    {246, 2153},
    {247, 2153},
    {248, 2153},
    {249, 2153},
    {250, 2153},
    {251, 2153},
    {252, 2153},
    {253, 2153},
    {254, 2153},
    {255, 2153},
    {256, 2153},
    {0, 38},
    {0, 5039},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {9, 2153},
    {10, 2153},
    {0, 0},
    {12, 2153},
    {13, 2153},
    {0, 0},
    {0, 37},
    {0, 5023},
    {0, 0},
    {0, 37},
    {0, 5020},
    {1, 2153},
    {2, 2153},
    {3, 2153},
    {4, 2153},
    {5, 2153},
    {6, 2153},
    {7, 2153},
    {8, 2153},
    {0, 0},
    {0, 0},
    {11, 2153},
    {0, 0},
    {32, 2153},
    {14, 2153},
    {15, 2153},
    {16, 2153},
    {17, 2153},
    {18, 2153},
    {19, 2153},
    {20, 2153},
    {21, 2153},
    {22, 2153},
    {23, 2153},
    {24, 2153},
    {25, 2153},
    {26, 2153},
    {27, 2153},
    {28, 2153},
    {29, 2153},
    {30, 2153},
    {31, 2153},
    {0, 0},
    {0, 0},
    {34, 1361},
    {38, 1366},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, 2153},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 2153},
    {49, 2153},
    {50, 2153},
    {51, 2153},
    {52, 2153},
    {53, 2153},
    {54, 2153},
    {55, 2153},
    {56, 2153},
    {57, 2153},
    {0, 0},
    {59, 2153},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 2153},
    {66, 2153},
    {67, 2153},
    {68, 2153},
    {69, 2153},
    {70, 2153},
    {71, 2153},
    {72, 2153},
    {73, 2153},
    {74, 2153},
    {75, 2153},
    {76, 2153},
    {77, 2153},
    {78, 2153},
    {79, 2153},
    {80, 2153},
    {81, 2153},
    {82, 2153},
    {83, 2153},
    {84, 2153},
    {85, 2153},
    {86, 2153},
    {87, 2153},
    {88, 2153},
    {89, 2153},

    {90, 2153},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, 2153},
    {95, 2153},
    {96, 2153},
    {97, 2153},
    {98, 2153},
    {99, 2153},
    {100, 2153},
    {101, 2153},
    {102, 2153},
    {103, 2153},
    {104, 2153},
    {105, 2153},
    {106, 2153},
    {107, 2153},
    {108, 2153},
    {109, 2153},
    {110, 2153},
    {111, 2153},
    {112, 2153},
    {113, 2153},
    {114, 2153},
    {115, 2153},
    {116, 2153},
    {117, 2153},
    {118, 2153},
    {119, 2153},
    {120, 2153},
    {121, 2153},
    {122, 2153},
    {0, 0},
    {0, 0},
    {0, 0},
    {126, 2153},
    {127, 2153},
    {128, 2153},
    {129, 2153},
    {130, 2153},
    {131, 2153},
    {132, 2153},
    {133, 2153},
    {134, 2153},
    {135, 2153},
    {136, 2153},
    {137, 2153},
    {138, 2153},
    {139, 2153},

    {140, 2153},
    {141, 2153},
    {142, 2153},
    {143, 2153},
    {144, 2153},
    {145, 2153},
    {146, 2153},
    {147, 2153},
    {148, 2153},
    {149, 2153},
    {150, 2153},
    {151, 2153},
    {152, 2153},
    {153, 2153},
    {154, 2153},
    {155, 2153},
    {156, 2153},
    {157, 2153},
    {158, 2153},
    {159, 2153},
    {160, 2153},
    {161, 2153},
    {162, 2153},
    {163, 2153},
    {164, 2153},
    {165, 2153},
    {166, 2153},
    {167, 2153},
    {168, 2153},
    {169, 2153},
    {170, 2153},
    {171, 2153},
    {172, 2153},
    {173, 2153},
    {174, 2153},
    {175, 2153},
    {176, 2153},
    {177, 2153},
    {178, 2153},
    {179, 2153},
    {180, 2153},
    {181, 2153},
    {182, 2153},
    {183, 2153},
    {184, 2153},
    {185, 2153},
    {186, 2153},
    {187, 2153},
    {188, 2153},
    {189, 2153},

    {190, 2153},
    {191, 2153},
    {192, 2153},
    {193, 2153},
    {194, 2153},
    {195, 2153},
    {196, 2153},
    {197, 2153},
    {198, 2153},
    {199, 2153},
    {200, 2153},
    {201, 2153},
    {202, 2153},
    {203, 2153},
    {204, 2153},
    {205, 2153},
    {206, 2153},
    {207, 2153},
    {208, 2153},
    {209, 2153},
    {210, 2153},
    {211, 2153},
    {212, 2153},
    {213, 2153},
    {214, 2153},
    {215, 2153},
    {216, 2153},
    {217, 2153},
    {218, 2153},
    {219, 2153},
    {220, 2153},
    {221, 2153},
    {222, 2153},
    {223, 2153},
    {224, 2153},
    {225, 2153},
    {226, 2153},
    {227, 2153},
    {228, 2153},
    {229, 2153},
    {230, 2153},
    {231, 2153},
    {232, 2153},
    {233, 2153},
    {234, 2153},
    {235, 2153},
    {236, 2153},
    {237, 2153},
    {238, 2153},
    {239, 2153},

    {240, 2153},
    {241, 2153},
    {242, 2153},
    {243, 2153},
    {244, 2153},
    {245, 2153},
    {246, 2153},
    {247, 2153},
    {248, 2153},
    {249, 2153},
    {250, 2153},
    {251, 2153},
    {252, 2153},
    {253, 2153},
    {254, 2153},
    {255, 2153},
    {256, 2153},
    {0, 37},
    {0, 4762},
    {0, 37},
    {0, 4760},
    {0, 42},
    {0, 4758},
    {1, 1614},
    {2, 1614},
    {3, 1614},
    {4, 1614},
    {5, 1614},
    {6, 1614},
    {7, 1614},
    {8, 1614},
    {0, 0},
    {0, 0},
    {11, 1614},
    {0, 0},
    {0, 0},
    {14, 1614},
    {15, 1614},
    {16, 1614},
    {17, 1614},
    {18, 1614},
    {19, 1614},
    {20, 1614},
    {21, 1614},
    {22, 1614},
    {23, 1614},
    {24, 1614},
    {25, 1614},
    {26, 1614},
    {27, 1614},

    {28, 1614},
    {29, 1614},
    {30, 1614},
    {31, 1614},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {42, 1109},
    {39, 1614},
    {42, 1109},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {46, 2149},
    {0, 0},
    {48, 1614},
    {49, 1614},
    {50, 1614},
    {51, 1614},
    {52, 1614},
    {53, 1614},
    {54, 1614},
    {55, 1614},
    {56, 1614},
    {57, 1614},
    {0, 0},
    {59, 1614},
    {0, 0},
    {0, 28},
    {0, 4696},
    {0, 37},
    {0, 4694},
    {65, 1614},
    {66, 1614},
    {67, 1614},
    {68, 1614},
    {69, 2208},
    {70, 1614},
    {71, 1614},
    {72, 1614},
    {73, 1614},
    {74, 1614},
    {75, 1614},
    {76, 1614},
    {77, 1614},

    {78, 1614},
    {79, 1614},
    {80, 1614},
    {81, 1614},
    {82, 1614},
    {83, 1614},
    {84, 1614},
    {85, 1614},
    {86, 1614},
    {87, 1614},
    {88, 1614},
    {89, 1614},
    {90, 1614},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, 1614},
    {95, 1614},
    {96, 1614},
    {97, 1614},
    {98, 1614},
    {99, 1614},
    {100, 1614},
    {101, 2208},
    {102, 1614},
    {103, 1614},
    {104, 1614},
    {105, 1614},
    {106, 1614},
    {107, 1614},
    {108, 1614},
    {109, 1614},
    {110, 1614},
    {111, 1614},
    {112, 1614},
    {113, 1614},
    {114, 1614},
    {115, 1614},
    {116, 1614},
    {117, 1614},
    {118, 1614},
    {119, 1614},
    {120, 1614},
    {121, 1614},
    {122, 1614},
    {61, 1047},
    {62, 1049},
    {61, 1062},
    {126, 1614},
    {127, 1614},

    {128, 1614},
    {129, 1614},
    {130, 1614},
    {131, 1614},
    {132, 1614},
    {133, 1614},
    {134, 1614},
    {135, 1614},
    {136, 1614},
    {137, 1614},
    {138, 1614},
    {139, 1614},
    {140, 1614},
    {141, 1614},
    {142, 1614},
    {143, 1614},
    {144, 1614},
    {145, 1614},
    {146, 1614},
    {147, 1614},
    {148, 1614},
    {149, 1614},
    {150, 1614},
    {151, 1614},
    {152, 1614},
    {153, 1614},
    {154, 1614},
    {155, 1614},
    {156, 1614},
    {157, 1614},
    {158, 1614},
    {159, 1614},
    {160, 1614},
    {161, 1614},
    {162, 1614},
    {163, 1614},
    {164, 1614},
    {165, 1614},
    {166, 1614},
    {167, 1614},
    {168, 1614},
    {169, 1614},
    {170, 1614},
    {171, 1614},
    {172, 1614},
    {173, 1614},
    {174, 1614},
    {175, 1614},
    {176, 1614},
    {177, 1614},

    {178, 1614},
    {179, 1614},
    {180, 1614},
    {181, 1614},
    {182, 1614},
    {183, 1614},
    {184, 1614},
    {185, 1614},
    {186, 1614},
    {187, 1614},
    {188, 1614},
    {189, 1614},
    {190, 1614},
    {191, 1614},
    {192, 1614},
    {193, 1614},
    {194, 1614},
    {195, 1614},
    {196, 1614},
    {197, 1614},
    {198, 1614},
    {199, 1614},
    {200, 1614},
    {201, 1614},
    {202, 1614},
    {203, 1614},
    {204, 1614},
    {205, 1614},
    {206, 1614},
    {207, 1614},
    {208, 1614},
    {209, 1614},
    {210, 1614},
    {211, 1614},
    {212, 1614},
    {213, 1614},
    {214, 1614},
    {215, 1614},
    {216, 1614},
    {217, 1614},
    {218, 1614},
    {219, 1614},
    {220, 1614},
    {221, 1614},
    {222, 1614},
    {223, 1614},
    {224, 1614},
    {225, 1614},
    {226, 1614},
    {227, 1614},

    {228, 1614},
    {229, 1614},
    {230, 1614},
    {231, 1614},
    {232, 1614},
    {233, 1614},
    {234, 1614},
    {235, 1614},
    {236, 1614},
    {237, 1614},
    {238, 1614},
    {239, 1614},
    {240, 1614},
    {241, 1614},
    {242, 1614},
    {243, 1614},
    {244, 1614},
    {245, 1614},
    {246, 1614},
    {247, 1614},
    {248, 1614},
    {249, 1614},
    {250, 1614},
    {251, 1614},
    {252, 1614},
    {253, 1614},
    {254, 1614},
    {255, 1614},
    {256, 1614},
    {0, 42},
    {0, 4500},
    {1, 1356},
    {2, 1356},
    {3, 1356},
    {4, 1356},
    {5, 1356},
    {6, 1356},
    {7, 1356},
    {8, 1356},
    {0, 0},
    {0, 0},
    {11, 1356},
    {0, 0},
    {0, 0},
    {14, 1356},
    {15, 1356},
    {16, 1356},
    {17, 1356},
    {18, 1356},
    {19, 1356},

    {20, 1356},
    {21, 1356},
    {22, 1356},
    {23, 1356},
    {24, 1356},
    {25, 1356},
    {26, 1356},
    {27, 1356},
    {28, 1356},
    {29, 1356},
    {30, 1356},
    {31, 1356},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, 1356},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {46, 1891},
    {0, 0},
    {48, 2208},
    {49, 2208},
    {50, 2208},
    {51, 2208},
    {52, 2208},
    {53, 2208},
    {54, 2208},
    {55, 2208},
    {56, 2208},
    {57, 2208},
    {0, 0},
    {59, 1356},
    {0, 0},
    {0, 34},
    {0, 4438},
    {0, 46},
    {0, 4436},
    {65, 1356},
    {66, 1356},
    {67, 1356},
    {68, 1356},
    {69, 1950},

    {70, 1356},
    {71, 1356},
    {72, 1356},
    {73, 1356},
    {74, 1356},
    {75, 1356},
    {76, 1356},
    {77, 1356},
    {78, 1356},
    {79, 1356},
    {80, 1356},
    {81, 1356},
    {82, 1356},
    {83, 1356},
    {84, 1356},
    {85, 1356},
    {86, 1356},
    {87, 1356},
    {88, 1356},
    {89, 1356},
    {90, 1356},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, 1356},
    {95, 1356},
    {96, 1356},
    {97, 1356},
    {98, 1356},
    {99, 1356},
    {100, 1356},
    {101, 1950},
    {102, 1356},
    {103, 1356},
    {104, 1356},
    {105, 1356},
    {106, 1356},
    {107, 1356},
    {108, 1356},
    {109, 1356},
    {110, 1356},
    {111, 1356},
    {112, 1356},
    {113, 1356},
    {114, 1356},
    {115, 1356},
    {116, 1356},
    {117, 1356},
    {118, 1356},
    {119, 1356},

    {120, 1356},
    {121, 1356},
    {122, 1356},
    {61, 808},
    {0, 0},
    {0, 0},
    {126, 1356},
    {127, 1356},
    {128, 1356},
    {129, 1356},
    {130, 1356},
    {131, 1356},
    {132, 1356},
    {133, 1356},
    {134, 1356},
    {135, 1356},
    {136, 1356},
    {137, 1356},
    {138, 1356},
    {139, 1356},
    {140, 1356},
    {141, 1356},
    {142, 1356},
    {143, 1356},
    {144, 1356},
    {145, 1356},
    {146, 1356},
    {147, 1356},
    {148, 1356},
    {149, 1356},
    {150, 1356},
    {151, 1356},
    {152, 1356},
    {153, 1356},
    {154, 1356},
    {155, 1356},
    {156, 1356},
    {157, 1356},
    {158, 1356},
    {159, 1356},
    {160, 1356},
    {161, 1356},
    {162, 1356},
    {163, 1356},
    {164, 1356},
    {165, 1356},
    {166, 1356},
    {167, 1356},
    {168, 1356},
    {169, 1356},

    {170, 1356},
    {171, 1356},
    {172, 1356},
    {173, 1356},
    {174, 1356},
    {175, 1356},
    {176, 1356},
    {177, 1356},
    {178, 1356},
    {179, 1356},
    {180, 1356},
    {181, 1356},
    {182, 1356},
    {183, 1356},
    {184, 1356},
    {185, 1356},
    {186, 1356},
    {187, 1356},
    {188, 1356},
    {189, 1356},
    {190, 1356},
    {191, 1356},
    {192, 1356},
    {193, 1356},
    {194, 1356},
    {195, 1356},
    {196, 1356},
    {197, 1356},
    {198, 1356},
    {199, 1356},
    {200, 1356},
    {201, 1356},
    {202, 1356},
    {203, 1356},
    {204, 1356},
    {205, 1356},
    {206, 1356},
    {207, 1356},
    {208, 1356},
    {209, 1356},
    {210, 1356},
    {211, 1356},
    {212, 1356},
    {213, 1356},
    {214, 1356},
    {215, 1356},
    {216, 1356},
    {217, 1356},
    {218, 1356},
    {219, 1356},

    {220, 1356},
    {221, 1356},
    {222, 1356},
    {223, 1356},
    {224, 1356},
    {225, 1356},
    {226, 1356},
    {227, 1356},
    {228, 1356},
    {229, 1356},
    {230, 1356},
    {231, 1356},
    {232, 1356},
    {233, 1356},
    {234, 1356},
    {235, 1356},
    {236, 1356},
    {237, 1356},
    {238, 1356},
    {239, 1356},
    {240, 1356},
    {241, 1356},
    {242, 1356},
    {243, 1356},
    {244, 1356},
    {245, 1356},
    {246, 1356},
    {247, 1356},
    {248, 1356},
    {249, 1356},
    {250, 1356},
    {251, 1356},
    {252, 1356},
    {253, 1356},
    {254, 1356},
    {255, 1356},
    {256, 1356},
    {0, 37},
    {0, 4242},
    {0, 18},
    {0, 4240},
    {0, 4},
    {0, 4238},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 4},
    {0, 4218},
    {0, 19},
    {0, 4216},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 20},
    {0, 4210},
    {1, 2176},
    {2, 2176},
    {3, 2176},
    {4, 2176},
    {5, 2176},
    {6, 2176},
    {7, 2176},
    {8, 2176},
    {9, 2176},
    {10, 2176},
    {11, 2176},
    {12, 2176},
    {13, 2176},
    {14, 2176},
    {15, 2176},
    {16, 2176},
    {17, 2176},
    {18, 2176},
    {19, 2176},
    {20, 2176},
    {21, 2176},
    {22, 2176},
    {23, 2176},
    {24, 2176},
    {25, 2176},
    {26, 2176},
    {27, 2176},
    {28, 2176},
    {29, 2176},

    {30, 2176},
    {31, 2176},
    {32, 2176},
    {33, 2176},
    {42, 801},
    {35, 2176},
    {36, 2176},
    {37, 2176},
    {38, 2176},
    {39, 2176},
    {40, 2176},
    {41, 2176},
    {42, 2176},
    {43, 2176},
    {44, 2176},
    {45, 2176},
    {46, 2176},
    {47, 2176},
    {48, 2176},
    {49, 2176},
    {50, 2176},
    {51, 2176},
    {52, 2176},
    {53, 2176},
    {54, 2176},
    {55, 2176},
    {56, 2176},
    {57, 2176},
    {58, 2176},
    {59, 2176},
    {60, 2176},
    {61, 2176},
    {62, 2176},
    {63, 2176},
    {64, 2176},
    {65, 2176},
    {66, 2176},
    {67, 2176},
    {68, 2176},
    {69, 2176},
    {70, 2176},
    {71, 2176},
    {72, 2176},
    {73, 2176},
    {74, 2176},
    {75, 2176},
    {76, 2176},
    {77, 2176},
    {78, 2176},
    {79, 2176},

    {80, 2176},
    {81, 2176},
    {82, 2176},
    {83, 2176},
    {84, 2176},
    {85, 2176},
    {86, 2176},
    {87, 2176},
    {88, 2176},
    {89, 2176},
    {90, 2176},
    {91, 2176},
    {124, 640},
    {93, 2176},
    {94, 2176},
    {95, 2176},
    {96, 2176},
    {97, 2176},
    {98, 2176},
    {99, 2176},
    {100, 2176},
    {101, 2176},
    {102, 2176},
    {103, 2176},
    {104, 2176},
    {105, 2176},
    {106, 2176},
    {107, 2176},
    {108, 2176},
    {109, 2176},
    {110, 2176},
    {111, 2176},
    {112, 2176},
    {113, 2176},
    {114, 2176},
    {115, 2176},
    {116, 2176},
    {117, 2176},
    {118, 2176},
    {119, 2176},
    {120, 2176},
    {121, 2176},
    {122, 2176},
    {123, 2176},
    {124, 2176},
    {125, 2176},
    {126, 2176},
    {127, 2176},
    {128, 2176},
    {129, 2176},

    {130, 2176},
    {131, 2176},
    {132, 2176},
    {133, 2176},
    {134, 2176},
    {135, 2176},
    {136, 2176},
    {137, 2176},
    {138, 2176},
    {139, 2176},
    {140, 2176},
    {141, 2176},
    {142, 2176},
    {143, 2176},
    {144, 2176},
    {145, 2176},
    {146, 2176},
    {147, 2176},
    {148, 2176},
    {149, 2176},
    {150, 2176},
    {151, 2176},
    {152, 2176},
    {153, 2176},
    {154, 2176},
    {155, 2176},
    {156, 2176},
    {157, 2176},
    {158, 2176},
    {159, 2176},
    {160, 2176},
    {161, 2176},
    {162, 2176},
    {163, 2176},
    {164, 2176},
    {165, 2176},
    {166, 2176},
    {167, 2176},
    {168, 2176},
    {169, 2176},
    {170, 2176},
    {171, 2176},
    {172, 2176},
    {173, 2176},
    {174, 2176},
    {175, 2176},
    {176, 2176},
    {177, 2176},
    {178, 2176},
    {179, 2176},

    {180, 2176},
    {181, 2176},
    {182, 2176},
    {183, 2176},
    {184, 2176},
    {185, 2176},
    {186, 2176},
    {187, 2176},
    {188, 2176},
    {189, 2176},
    {190, 2176},
    {191, 2176},
    {192, 2176},
    {193, 2176},
    {194, 2176},
    {195, 2176},
    {196, 2176},
    {197, 2176},
    {198, 2176},
    {199, 2176},
    {200, 2176},
    {201, 2176},
    {202, 2176},
    {203, 2176},
    {204, 2176},
    {205, 2176},
    {206, 2176},
    {207, 2176},
    {208, 2176},
    {209, 2176},
    {210, 2176},
    {211, 2176},
    {212, 2176},
    {213, 2176},
    {214, 2176},
    {215, 2176},
    {216, 2176},
    {217, 2176},
    {218, 2176},
    {219, 2176},
    {220, 2176},
    {221, 2176},
    {222, 2176},
    {223, 2176},
    {224, 2176},
    {225, 2176},
    {226, 2176},
    {227, 2176},
    {228, 2176},
    {229, 2176},

    {230, 2176},
    {231, 2176},
    {232, 2176},
    {233, 2176},
    {234, 2176},
    {235, 2176},
    {236, 2176},
    {237, 2176},
    {238, 2176},
    {239, 2176},
    {240, 2176},
    {241, 2176},
    {242, 2176},
    {243, 2176},
    {244, 2176},
    {245, 2176},
    {246, 2176},
    {247, 2176},
    {248, 2176},
    {249, 2176},
    {250, 2176},
    {251, 2176},
    {252, 2176},
    {253, 2176},
    {254, 2176},
    {255, 2176},
    {256, 2176},
    {0, 17},
    {0, 3952},
    {1, 382},
    {2, 382},
    {3, 382},
    {4, 382},
    {5, 382},
    {6, 382},
    {7, 382},
    {8, 382},
    {9, 382},
    {0, 0},
    {11, 382},
    {12, 382},
    {13, 382},
    {14, 382},
    {15, 382},
    {16, 382},
    {17, 382},
    {18, 382},
    {19, 382},
    {20, 382},
    {21, 382},

    {22, 382},
    {23, 382},
    {24, 382},
    {25, 382},
    {26, 382},
    {27, 382},
    {28, 382},
    {29, 382},
    {30, 382},
    {31, 382},
    {32, 382},
    {33, 382},
    {34, 382},
    {35, 382},
    {36, 382},
    {37, 382},
    {38, 382},
    {39, 382},
    {40, 382},
    {41, 382},
    {42, 382},
    {43, 382},
    {44, 382},
    {45, 382},
    {46, 382},
    {47, 382},
    {48, 382},
    {49, 382},
    {50, 382},
    {51, 382},
    {52, 382},
    {53, 382},
    {54, 382},
    {55, 382},
    {56, 382},
    {57, 382},
    {58, 382},
    {59, 382},
    {60, 382},
    {61, 382},
    {62, 382},
    {63, 382},
    {64, 382},
    {65, 382},
    {66, 382},
    {67, 382},
    {68, 382},
    {69, 382},
    {70, 382},
    {71, 382},

    {72, 382},
    {73, 382},
    {74, 382},
    {75, 382},
    {76, 382},
    {77, 382},
    {78, 382},
    {79, 382},
    {80, 382},
    {81, 382},
    {82, 382},
    {83, 382},
    {84, 382},
    {85, 382},
    {86, 382},
    {87, 382},
    {88, 382},
    {89, 382},
    {90, 382},
    {91, 382},
    {92, 382},
    {93, 382},
    {94, 382},
    {95, 382},
    {96, 382},
    {97, 382},
    {98, 518},
    {99, 382},
    {100, 382},
    {101, 382},
    {102, 520},
    {103, 382},
    {104, 382},
    {105, 382},
    {106, 382},
    {107, 382},
    {108, 382},
    {109, 382},
    {110, 522},
    {111, 382},
    {112, 382},
    {113, 382},
    {114, 524},
    {115, 382},
    {116, 531},
    {117, 2176},
    {118, 533},
    {119, 382},
    {120, 2214},
    {121, 382},

    {122, 382},
    {123, 382},
    {124, 382},
    {125, 382},
    {126, 382},
    {127, 382},
    {128, 382},
    {129, 382},
    {130, 382},
    {131, 382},
    {132, 382},
    {133, 382},
    {134, 382},
    {135, 382},
    {136, 382},
    {137, 382},
    {138, 382},
    {139, 382},
    {140, 382},
    {141, 382},
    {142, 382},
    {143, 382},
    {144, 382},
    {145, 382},
    {146, 382},
    {147, 382},
    {148, 382},
    {149, 382},
    {150, 382},
    {151, 382},
    {152, 382},
    {153, 382},
    {154, 382},
    {155, 382},
    {156, 382},
    {157, 382},
    {158, 382},
    {159, 382},
    {160, 382},
    {161, 382},
    {162, 382},
    {163, 382},
    {164, 382},
    {165, 382},
    {166, 382},
    {167, 382},
    {168, 382},
    {169, 382},
    {170, 382},
    {171, 382},

    {172, 382},
    {173, 382},
    {174, 382},
    {175, 382},
    {176, 382},
    {177, 382},
    {178, 382},
    {179, 382},
    {180, 382},
    {181, 382},
    {182, 382},
    {183, 382},
    {184, 382},
    {185, 382},
    {186, 382},
    {187, 382},
    {188, 382},
    {189, 382},
    {190, 382},
    {191, 382},
    {192, 382},
    {193, 382},
    {194, 382},
    {195, 382},
    {196, 382},
    {197, 382},
    {198, 382},
    {199, 382},
    {200, 382},
    {201, 382},
    {202, 382},
    {203, 382},
    {204, 382},
    {205, 382},
    {206, 382},
    {207, 382},
    {208, 382},
    {209, 382},
    {210, 382},
    {211, 382},
    {212, 382},
    {213, 382},
    {214, 382},
    {215, 382},
    {216, 382},
    {217, 382},
    {218, 382},
    {219, 382},
    {220, 382},
    {221, 382},

    {222, 382},
    {223, 382},
    {224, 382},
    {225, 382},
    {226, 382},
    {227, 382},
    {228, 382},
    {229, 382},
    {230, 382},
    {231, 382},
    {232, 382},
    {233, 382},
    {234, 382},
    {235, 382},
    {236, 382},
    {237, 382},
    {238, 382},
    {239, 382},
    {240, 382},
    {241, 382},
    {242, 382},
    {243, 382},
    {244, 382},
    {245, 382},
    {246, 382},
    {247, 382},
    {248, 382},
    {249, 382},
    {250, 382},
    {251, 382},
    {252, 382},
    {253, 382},
    {254, 382},
    {255, 382},
    {256, 382},
    {0, 1},
    {0, 3694},
    {1, 2060},
    {2, 2060},
    {3, 2060},
    {4, 2060},
    {5, 2060},
    {6, 2060},
    {7, 2060},
    {8, 2060},
    {0, 0},
    {0, 0},
    {11, 2060},
    {0, 23},
    {0, 3681},

    {14, 2060},
    {15, 2060},
    {16, 2060},
    {17, 2060},
    {18, 2060},
    {19, 2060},
    {20, 2060},
    {21, 2060},
    {22, 2060},
    {23, 2060},
    {24, 2060},
    {25, 2060},
    {26, 2060},
    {27, 2060},
    {28, 2060},
    {29, 2060},
    {30, 2060},
    {31, 2060},
    {0, 32},
    {0, 3661},
    {0, 36},
    {0, 3659},
    {0, 24},
    {0, 3657},
    {0, 0},
    {39, 2060},
    {0, 27},
    {0, 3653},
    {0, 39},
    {0, 3651},
    {0, 29},
    {0, 3649},
    {0, 31},
    {0, 3647},
    {48, 2060},
    {49, 2060},
    {50, 2060},
    {51, 2060},
    {52, 2060},
    {53, 2060},
    {54, 2060},
    {55, 2060},
    {56, 2060},
    {57, 2060},
    {0, 0},
    {59, 2060},
    {47, 266},
    {0, 30},
    {0, 3632},
    {0, 33},

    {0, 3630},
    {65, 2060},
    {66, 2060},
    {67, 2060},
    {68, 2060},
    {69, 2060},
    {70, 2060},
    {71, 2060},
    {72, 2060},
    {73, 2060},
    {74, 2060},
    {75, 2060},
    {76, 2060},
    {77, 2060},
    {78, 2060},
    {79, 2060},
    {80, 2060},
    {81, 2060},
    {82, 2060},
    {83, 2060},
    {84, 2060},
    {85, 2060},
    {86, 2060},
    {87, 2060},
    {88, 2060},
    {89, 2060},
    {90, 2060},
    {0, 25},
    {0, 3602},
    {0, 0},
    {94, 2060},
    {95, 2060},
    {96, 2060},
    {97, 2060},
    {98, 2060},
    {99, 2060},
    {100, 2060},
    {101, 2060},
    {102, 2060},
    {103, 2060},
    {104, 2060},
    {105, 2060},
    {106, 2060},
    {107, 2060},
    {108, 2060},
    {109, 2060},
    {110, 2060},
    {111, 2060},
    {112, 2060},
    {113, 2060},

    {114, 2060},
    {115, 2060},
    {116, 2060},
    {117, 2060},
    {118, 2060},
    {119, 2060},
    {120, 2060},
    {121, 2060},
    {122, 2060},
    {0, 16},
    {0, 3570},
    {0, 0},
    {126, 2060},
    {127, 2060},
    {128, 2060},
    {129, 2060},
    {130, 2060},
    {131, 2060},
    {132, 2060},
    {133, 2060},
    {134, 2060},
    {135, 2060},
    {136, 2060},
    {137, 2060},
    {138, 2060},
    {139, 2060},
    {140, 2060},
    {141, 2060},
    {142, 2060},
    {143, 2060},
    {144, 2060},
    {145, 2060},
    {146, 2060},
    {147, 2060},
    {148, 2060},
    {149, 2060},
    {150, 2060},
    {151, 2060},
    {152, 2060},
    {153, 2060},
    {154, 2060},
    {155, 2060},
    {156, 2060},
    {157, 2060},
    {158, 2060},
    {159, 2060},
    {160, 2060},
    {161, 2060},
    {162, 2060},
    {163, 2060},

    {164, 2060},
    {165, 2060},
    {166, 2060},
    {167, 2060},
    {168, 2060},
    {169, 2060},
    {170, 2060},
    {171, 2060},
    {172, 2060},
    {173, 2060},
    {174, 2060},
    {175, 2060},
    {176, 2060},
    {177, 2060},
    {178, 2060},
    {179, 2060},
    {180, 2060},
    {181, 2060},
    {182, 2060},
    {183, 2060},
    {184, 2060},
    {185, 2060},
    {186, 2060},
    {187, 2060},
    {188, 2060},
    {189, 2060},
    {190, 2060},
    {191, 2060},
    {192, 2060},
    {193, 2060},
    {194, 2060},
    {195, 2060},
    {196, 2060},
    {197, 2060},
    {198, 2060},
    {199, 2060},
    {200, 2060},
    {201, 2060},
    {202, 2060},
    {203, 2060},
    {204, 2060},
    {205, 2060},
    {206, 2060},
    {207, 2060},
    {208, 2060},
    {209, 2060},
    {210, 2060},
    {211, 2060},
    {212, 2060},
    {213, 2060},

    {214, 2060},
    {215, 2060},
    {216, 2060},
    {217, 2060},
    {218, 2060},
    {219, 2060},
    {220, 2060},
    {221, 2060},
    {222, 2060},
    {223, 2060},
    {224, 2060},
    {225, 2060},
    {226, 2060},
    {227, 2060},
    {228, 2060},
    {229, 2060},
    {230, 2060},
    {231, 2060},
    {232, 2060},
    {233, 2060},
    {234, 2060},
    {235, 2060},
    {236, 2060},
    {237, 2060},
    {238, 2060},
    {239, 2060},
    {240, 2060},
    {241, 2060},
    {242, 2060},
    {243, 2060},
    {244, 2060},
    {245, 2060},
    {246, 2060},
    {247, 2060},
    {248, 2060},
    {249, 2060},
    {250, 2060},
    {251, 2060},
    {252, 2060},
    {253, 2060},
    {254, 2060},
    {255, 2060},
    {256, 2060},
    {0, 2},
    {0, 3436},
    {0, 5},
    {0, 3434},
    {0, 6},
    {0, 3432},
    {0, 7},

    {0, 3430},
    {0, 8},
    {0, 3428},
    {9, 2060},
    {10, 2060},
    {0, 0},
    {12, 2060},
    {13, 2060},
    {0, 9},
    {0, 3421},
    {0, 10},
    {0, 3419},
    {0, 3},
    {0, 3417},
    {0, 21},
    {0, 3415},
    {0, 12},
    {0, 3413},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {32, 2060},
    {0, 22},
    {0, 3402},
    {1, 2060},
    {2, 2060},
    {3, 2060},
    {4, 2060},
    {5, 2060},
    {6, 2060},
    {7, 2060},
    {8, 2060},
    {9, 2060},
    {10, 2060},
    {11, 2060},
    {12, 2060},
    {13, 2060},
    {14, 2060},
    {15, 2060},
    {16, 2060},
    {17, 2060},
    {18, 2060},
    {19, 2060},
    {20, 2060},
    {21, 2060},

    {22, 2060},
    {23, 2060},
    {24, 2060},
    {25, 2060},
    {26, 2060},
    {27, 2060},
    {28, 2060},
    {29, 2060},
    {30, 2060},
    {31, 2060},
    {32, 2060},
    {33, 2060},
    {34, 2060},
    {35, 2060},
    {36, 2060},
    {37, 2060},
    {38, 2060},
    {39, 2060},
    {40, 2060},
    {41, 2060},
    {0, 0},
    {43, 2060},
    {44, 2060},
    {45, 2060},
    {46, 2060},
    {47, 2060},
    {48, 2060},
    {49, 2060},
    {50, 2060},
    {51, 2060},
    {52, 2060},
    {53, 2060},
    {54, 2060},
    {55, 2060},
    {56, 2060},
    {57, 2060},
    {58, 2060},
    {59, 2060},
    {60, 2060},
    {61, 2060},
    {62, 2060},
    {63, 2060},
    {64, 2060},
    {65, 2060},
    {66, 2060},
    {67, 2060},
    {68, 2060},
    {69, 2060},
    {70, 2060},
    {71, 2060},

    {72, 2060},
    {73, 2060},
    {74, 2060},
    {75, 2060},
    {76, 2060},
    {77, 2060},
    {78, 2060},
    {79, 2060},
    {80, 2060},
    {81, 2060},
    {82, 2060},
    {83, 2060},
    {84, 2060},
    {85, 2060},
    {86, 2060},
    {87, 2060},
    {88, 2060},
    {89, 2060},
    {90, 2060},
    {91, 2060},
    {92, 2060},
    {93, 2060},
    {94, 2060},
    {95, 2060},
    {96, 2060},
    {97, 2060},
    {98, 2060},
    {99, 2060},
    {100, 2060},
    {101, 2060},
    {102, 2060},
    {103, 2060},
    {104, 2060},
    {105, 2060},
    {106, 2060},
    {107, 2060},
    {108, 2060},
    {109, 2060},
    {110, 2060},
    {111, 2060},
    {112, 2060},
    {113, 2060},
    {114, 2060},
    {115, 2060},
    {116, 2060},
    {117, 2060},
    {118, 2060},
    {119, 2060},
    {120, 2060},
    {121, 2060},

    {122, 2060},
    {123, 2060},
    {124, 2060},
    {125, 2060},
    {126, 2060},
    {127, 2060},
    {128, 2060},
    {129, 2060},
    {130, 2060},
    {131, 2060},
    {132, 2060},
    {133, 2060},
    {134, 2060},
    {135, 2060},
    {136, 2060},
    {137, 2060},
    {138, 2060},
    {139, 2060},
    {140, 2060},
    {141, 2060},
    {142, 2060},
    {143, 2060},
    {144, 2060},
    {145, 2060},
    {146, 2060},
    {147, 2060},
    {148, 2060},
    {149, 2060},
    {150, 2060},
    {151, 2060},
    {152, 2060},
    {153, 2060},
    {154, 2060},
    {155, 2060},
    {156, 2060},
    {157, 2060},
    {158, 2060},
    {159, 2060},
    {160, 2060},
    {161, 2060},
    {162, 2060},
    {163, 2060},
    {164, 2060},
    {165, 2060},
    {166, 2060},
    {167, 2060},
    {168, 2060},
    {169, 2060},
    {170, 2060},
    {171, 2060},

    {172, 2060},
    {173, 2060},
    {174, 2060},
    {175, 2060},
    {176, 2060},
    {177, 2060},
    {178, 2060},
    {179, 2060},
    {180, 2060},
    {181, 2060},
    {182, 2060},
    {183, 2060},
    {184, 2060},
    {185, 2060},
    {186, 2060},
    {187, 2060},
    {188, 2060},
    {189, 2060},
    {190, 2060},
    {191, 2060},
    {192, 2060},
    {193, 2060},
    {194, 2060},
    {195, 2060},
    {196, 2060},
    {197, 2060},
    {198, 2060},
    {199, 2060},
    {200, 2060},
    {201, 2060},
    {202, 2060},
    {203, 2060},
    {204, 2060},
    {205, 2060},
    {206, 2060},
    {207, 2060},
    {208, 2060},
    {209, 2060},
    {210, 2060},
    {211, 2060},
    {212, 2060},
    {213, 2060},
    {214, 2060},
    {215, 2060},
    {216, 2060},
    {217, 2060},
    {218, 2060},
    {219, 2060},
    {220, 2060},
    {221, 2060},

    {222, 2060},
    {223, 2060},
    {224, 2060},
    {225, 2060},
    {226, 2060},
    {227, 2060},
    {228, 2060},
    {229, 2060},
    {230, 2060},
    {231, 2060},
    {232, 2060},
    {233, 2060},
    {234, 2060},
    {235, 2060},
    {236, 2060},
    {237, 2060},
    {238, 2060},
    {239, 2060},
    {240, 2060},
    {241, 2060},
    {242, 2060},
    {243, 2060},
    {244, 2060},
    {245, 2060},
    {246, 2060},
    {247, 2060},
    {248, 2060},
    {249, 2060},
    {250, 2060},
    {251, 2060},
    {252, 2060},
    {253, 2060},
    {254, 2060},
    {255, 2060},
    {256, 2060},
    {0, 47},
    {0, 3144},
    {1, 0},
    {2, 0},
    {3, 0},
    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {0, 0},
    {0, 0},
    {11, 0},
    {0, 0},
    {0, 0},

    {14, 0},
    {15, 0},
    {16, 0},
    {17, 0},
    {18, 0},
    {19, 0},
    {20, 0},
    {21, 0},
    {22, 0},
    {23, 0},
    {24, 0},
    {25, 0},
    {26, 0},
    {27, 0},
    {28, 0},
    {29, 0},
    {30, 0},
    {31, 0},
    {0, 11},
    {0, 3111},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},
    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {0, 0},
    {59, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {65, 0},
    {66, 0},
    {67, 0},
    {68, 0},
    {69, 0},
    {70, 0},
    {71, 0},
    {72, 0},
    {73, 0},
    {74, 0},
    {75, 0},
    {76, 0},
    {77, 0},
    {78, 0},
    {79, 0},
    {80, 0},
    {81, 0},
    {82, 0},
    {83, 0},
    {84, 0},
    {85, 0},
    {86, 0},
    {87, 0},
    {88, 0},
    {89, 0},
    {90, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, 0},
    {95, 0},
    {96, 0},
    {97, 0},
    {98, 0},
    {99, 0},
    {100, 0},
    {101, 0},
    {102, 0},
    {103, 0},
    {104, 0},
    {105, 0},
    {106, 0},
    {107, 0},
    {108, 0},
    {109, 0},
    {110, 0},
    {111, 0},
    {112, 0},
    {113, 0},

    {114, 0},
    {115, 0},
    {116, 0},
    {117, 0},
    {118, 0},
    {119, 0},
    {120, 0},
    {121, 0},
    {122, 0},
    {0, 0},
    {0, 0},
    {92, 504},
    {126, 0},
    {127, 0},
    {128, 0},
    {129, 0},
    {130, 0},
    {131, 0},
    {132, 0},
    {133, 0},
    {134, 0},
    {135, 0},
    {136, 0},
    {137, 0},
    {138, 0},
    {139, 0},
    {140, 0},
    {141, 0},
    {142, 0},
    {143, 0},
    {144, 0},
    {145, 0},
    {146, 0},
    {147, 0},
    {148, 0},
    {149, 0},
    {150, 0},
    {151, 0},
    {152, 0},
    {153, 0},
    {154, 0},
    {155, 0},
    {156, 0},
    {157, 0},
    {158, 0},
    {159, 0},
    {160, 0},
    {161, 0},
    {162, 0},
    {163, 0},

    {164, 0},
    {165, 0},
    {166, 0},
    {167, 0},
    {168, 0},
    {169, 0},
    {170, 0},
    {171, 0},
    {172, 0},
    {173, 0},
    {174, 0},
    {175, 0},
    {176, 0},
    {177, 0},
    {178, 0},
    {179, 0},
    {180, 0},
    {181, 0},
    {182, 0},
    {183, 0},
    {184, 0},
    {185, 0},
    {186, 0},
    {187, 0},
    {188, 0},
    {189, 0},
    {190, 0},
    {191, 0},
    {192, 0},
    {193, 0},
    {194, 0},
    {195, 0},
    {196, 0},
    {197, 0},
    {198, 0},
    {199, 0},
    {200, 0},
    {201, 0},
    {202, 0},
    {203, 0},
    {204, 0},
    {205, 0},
    {206, 0},
    {207, 0},
    {208, 0},
    {209, 0},
    {210, 0},
    {211, 0},
    {212, 0},
    {213, 0},

    {214, 0},
    {215, 0},
    {216, 0},
    {217, 0},
    {218, 0},
    {219, 0},
    {220, 0},
    {221, 0},
    {222, 0},
    {223, 0},
    {224, 0},
    {225, 0},
    {226, 0},
    {227, 0},
    {228, 0},
    {229, 0},
    {230, 0},
    {231, 0},
    {232, 0},
    {233, 0},
    {234, 0},
    {235, 0},
    {236, 0},
    {237, 0},
    {238, 0},
    {239, 0},
    {240, 0},
    {241, 0},
    {242, 0},
    {243, 0},
    {244, 0},
    {245, 0},
    {246, 0},
    {247, 0},
    {248, 0},
    {249, 0},
    {250, 0},
    {251, 0},
    {252, 0},
    {253, 0},
    {254, 0},
    {255, 0},
    {256, 0},
    {0, 38},
    {0, 2886},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {9, 0},
    {10, 0},
    {0, 0},
    {12, 0},
    {13, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 35},
    {0, 2867},
    {1, 0},
    {2, 0},
    {3, 0},
    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {0, 0},
    {0, 0},
    {11, 0},
    {0, 0},
    {32, 0},
    {14, 0},
    {15, 0},
    {16, 0},
    {17, 0},
    {18, 0},
    {19, 0},
    {20, 0},
    {21, 0},
    {22, 0},
    {23, 0},
    {24, 0},
    {25, 0},
    {26, 0},
    {27, 0},
    {28, 0},
    {29, 0},
    {30, 0},
    {31, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {39, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},
    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {0, 0},
    {59, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 0},
    {66, 0},
    {67, 0},
    {68, 0},
    {69, 0},
    {70, 0},
    {71, 0},
    {72, 0},
    {73, 0},
    {74, 0},
    {75, 0},
    {76, 0},
    {77, 0},
    {78, 0},
    {79, 0},
    {80, 0},
    {81, 0},
    {82, 0},
    {83, 0},
    {84, 0},
    {85, 0},
    {86, 0},

    {87, 0},
    {88, 0},
    {89, 0},
    {90, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, 0},
    {95, 0},
    {96, 0},
    {97, 0},
    {98, 0},
    {99, 0},
    {100, 0},
    {101, 0},
    {102, 0},
    {103, 0},
    {104, 0},
    {105, 0},
    {106, 0},
    {107, 0},
    {108, 0},
    {109, 0},
    {110, 0},
    {111, 0},
    {112, 0},
    {113, 0},
    {114, 0},
    {115, 0},
    {116, 0},
    {117, 0},
    {118, 0},
    {119, 0},
    {120, 0},
    {121, 0},
    {122, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {126, 0},
    {127, 0},
    {128, 0},
    {129, 0},
    {130, 0},
    {131, 0},
    {132, 0},
    {133, 0},
    {134, 0},
    {135, 0},
    {136, 0},

    {137, 0},
    {138, 0},
    {139, 0},
    {140, 0},
    {141, 0},
    {142, 0},
    {143, 0},
    {144, 0},
    {145, 0},
    {146, 0},
    {147, 0},
    {148, 0},
    {149, 0},
    {150, 0},
    {151, 0},
    {152, 0},
    {153, 0},
    {154, 0},
    {155, 0},
    {156, 0},
    {157, 0},
    {158, 0},
    {159, 0},
    {160, 0},
    {161, 0},
    {162, 0},
    {163, 0},
    {164, 0},
    {165, 0},
    {166, 0},
    {167, 0},
    {168, 0},
    {169, 0},
    {170, 0},
    {171, 0},
    {172, 0},
    {173, 0},
    {174, 0},
    {175, 0},
    {176, 0},
    {177, 0},
    {178, 0},
    {179, 0},
    {180, 0},
    {181, 0},
    {182, 0},
    {183, 0},
    {184, 0},
    {185, 0},
    {186, 0},

    {187, 0},
    {188, 0},
    {189, 0},
    {190, 0},
    {191, 0},
    {192, 0},
    {193, 0},
    {194, 0},
    {195, 0},
    {196, 0},
    {197, 0},
    {198, 0},
    {199, 0},
    {200, 0},
    {201, 0},
    {202, 0},
    {203, 0},
    {204, 0},
    {205, 0},
    {206, 0},
    {207, 0},
    {208, 0},
    {209, 0},
    {210, 0},
    {211, 0},
    {212, 0},
    {213, 0},
    {214, 0},
    {215, 0},
    {216, 0},
    {217, 0},
    {218, 0},
    {219, 0},
    {220, 0},
    {221, 0},
    {222, 0},
    {223, 0},
    {224, 0},
    {225, 0},
    {226, 0},
    {227, 0},
    {228, 0},
    {229, 0},
    {230, 0},
    {231, 0},
    {232, 0},
    {233, 0},
    {234, 0},
    {235, 0},
    {236, 0},

    {237, 0},
    {238, 0},
    {239, 0},
    {240, 0},
    {241, 0},
    {242, 0},
    {243, 0},
    {244, 0},
    {245, 0},
    {246, 0},
    {247, 0},
    {248, 0},
    {249, 0},
    {250, 0},
    {251, 0},
    {252, 0},
    {253, 0},
    {254, 0},
    {255, 0},
    {256, 0},
    {0, 43},
    {0, 2609},
    {0, 15},
    {0, 2607},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 2584},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 1525},
    {49, 1525},
    {50, 1525},
    {51, 1525},
    {52, 1525},
    {53, 1525},
    {54, 1525},
    {55, 1525},
    {56, 1525},
    {57, 1525},
    {0, 44},
    {0, 2550},
    {1, -594},
    {2, -594},
    {3, -594},
    {4, -594},
    {5, -594},
    {6, -594},
    {7, -594},
    {8, -594},
    {0, 0},
    {0, 0},
    {11, -594},
    {0, 0},
    {0, 0},
    {14, -594},
    {15, -594},
    {16, -594},
    {17, -594},
    {18, -594},
    {19, -594},

    {20, -594},
    {21, -594},
    {22, -594},
    {23, -594},
    {24, -594},
    {25, -594},
    {26, -594},
    {27, -594},
    {28, -594},
    {29, -594},
    {30, -594},
    {31, -594},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, -594},
    {0, 0},
    {0, 0},
    {0, 0},
    {43, 1476},
    {0, 0},
    {45, 1476},
    {0, 0},
    {0, 0},
    {48, 1554},
    {49, 1554},
    {50, 1554},
    {51, 1554},
    {52, 1554},
    {53, 1554},
    {54, 1554},
    {55, 1554},
    {56, 1554},
    {57, 1554},
    {0, 0},
    {59, -594},
    {117, 2290},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, -594},
    {66, -594},
    {67, -594},
    {68, -594},
    {69, -594},

    {70, -594},
    {71, -594},
    {72, -594},
    {73, -594},
    {74, -594},
    {75, -594},
    {76, -594},
    {77, -594},
    {78, -594},
    {79, -594},
    {80, -594},
    {81, -594},
    {82, -594},
    {83, -594},
    {84, -594},
    {85, -594},
    {86, -594},
    {87, -594},
    {88, -594},
    {89, -594},
    {90, -594},
    {125, -527},
    {0, 0},
    {0, 0},
    {94, -594},
    {95, -594},
    {96, -594},
    {97, -594},
    {98, -594},
    {99, -594},
    {100, -594},
    {101, -594},
    {102, -594},
    {103, -594},
    {104, -594},
    {105, -594},
    {106, -594},
    {107, -594},
    {108, -594},
    {109, -594},
    {110, -594},
    {111, -594},
    {112, -594},
    {113, -594},
    {114, -594},
    {115, -594},
    {116, -594},
    {117, -594},
    {118, -594},
    {119, -594},

    {120, -594},
    {121, -594},
    {122, -594},
    {0, 0},
    {0, 0},
    {0, 0},
    {126, -594},
    {127, -594},
    {128, -594},
    {129, -594},
    {130, -594},
    {131, -594},
    {132, -594},
    {133, -594},
    {134, -594},
    {135, -594},
    {136, -594},
    {137, -594},
    {138, -594},
    {139, -594},
    {140, -594},
    {141, -594},
    {142, -594},
    {143, -594},
    {144, -594},
    {145, -594},
    {146, -594},
    {147, -594},
    {148, -594},
    {149, -594},
    {150, -594},
    {151, -594},
    {152, -594},
    {153, -594},
    {154, -594},
    {155, -594},
    {156, -594},
    {157, -594},
    {158, -594},
    {159, -594},
    {160, -594},
    {161, -594},
    {162, -594},
    {163, -594},
    {164, -594},
    {165, -594},
    {166, -594},
    {167, -594},
    {168, -594},
    {169, -594},

    {170, -594},
    {171, -594},
    {172, -594},
    {173, -594},
    {174, -594},
    {175, -594},
    {176, -594},
    {177, -594},
    {178, -594},
    {179, -594},
    {180, -594},
    {181, -594},
    {182, -594},
    {183, -594},
    {184, -594},
    {185, -594},
    {186, -594},
    {187, -594},
    {188, -594},
    {189, -594},
    {190, -594},
    {191, -594},
    {192, -594},
    {193, -594},
    {194, -594},
    {195, -594},
    {196, -594},
    {197, -594},
    {198, -594},
    {199, -594},
    {200, -594},
    {201, -594},
    {202, -594},
    {203, -594},
    {204, -594},
    {205, -594},
    {206, -594},
    {207, -594},
    {208, -594},
    {209, -594},
    {210, -594},
    {211, -594},
    {212, -594},
    {213, -594},
    {214, -594},
    {215, -594},
    {216, -594},
    {217, -594},
    {218, -594},
    {219, -594},

    {220, -594},
    {221, -594},
    {222, -594},
    {223, -594},
    {224, -594},
    {225, -594},
    {226, -594},
    {227, -594},
    {228, -594},
    {229, -594},
    {230, -594},
    {231, -594},
    {232, -594},
    {233, -594},
    {234, -594},
    {235, -594},
    {236, -594},
    {237, -594},
    {238, -594},
    {239, -594},
    {240, -594},
    {241, -594},
    {242, -594},
    {243, -594},
    {244, -594},
    {245, -594},
    {246, -594},
    {247, -594},
    {248, -594},
    {249, -594},
    {250, -594},
    {251, -594},
    {252, -594},
    {253, -594},
    {254, -594},
    {255, -594},
    {256, -594},
    {0, 42},
    {0, 2292},
    {1, -852},
    {2, -852},
    {3, -852},
    {4, -852},
    {5, -852},
    {6, -852},
    {7, -852},
    {8, -852},
    {0, 0},
    {0, 0},
    {11, -852},

    {0, 0},
    {0, 0},
    {14, -852},
    {15, -852},
    {16, -852},
    {17, -852},
    {18, -852},
    {19, -852},
    {20, -852},
    {21, -852},
    {22, -852},
    {23, -852},
    {24, -852},
    {25, -852},
    {26, -852},
    {27, -852},
    {28, -852},
    {29, -852},
    {30, -852},
    {31, -852},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, -852},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {46, -317},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},
    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {0, 0},
    {59, -852},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {65, -852},
    {66, -852},
    {67, -852},
    {68, -852},
    {69, -258},
    {70, -852},
    {71, -852},
    {72, -852},
    {73, -852},
    {74, -852},
    {75, -852},
    {76, -852},
    {77, -852},
    {78, -852},
    {79, -852},
    {80, -852},
    {81, -852},
    {82, -852},
    {83, -852},
    {84, -852},
    {85, -852},
    {86, -852},
    {87, -852},
    {88, -852},
    {89, -852},
    {90, -852},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, -852},
    {95, -852},
    {96, -852},
    {97, -852},
    {98, -852},
    {99, -852},
    {100, -852},
    {101, -258},
    {102, -852},
    {103, -852},
    {104, -852},
    {105, -852},
    {106, -852},
    {107, -852},
    {108, -852},
    {109, -852},
    {110, -852},
    {111, -852},

    {112, -852},
    {113, -852},
    {114, -852},
    {115, -852},
    {116, -852},
    {117, -852},
    {118, -852},
    {119, -852},
    {120, -852},
    {121, -852},
    {122, -852},
    {0, 0},
    {0, 0},
    {0, 0},
    {126, -852},
    {127, -852},
    {128, -852},
    {129, -852},
    {130, -852},
    {131, -852},
    {132, -852},
    {133, -852},
    {134, -852},
    {135, -852},
    {136, -852},
    {137, -852},
    {138, -852},
    {139, -852},
    {140, -852},
    {141, -852},
    {142, -852},
    {143, -852},
    {144, -852},
    {145, -852},
    {146, -852},
    {147, -852},
    {148, -852},
    {149, -852},
    {150, -852},
    {151, -852},
    {152, -852},
    {153, -852},
    {154, -852},
    {155, -852},
    {156, -852},
    {157, -852},
    {158, -852},
    {159, -852},
    {160, -852},
    {161, -852},

    {162, -852},
    {163, -852},
    {164, -852},
    {165, -852},
    {166, -852},
    {167, -852},
    {168, -852},
    {169, -852},
    {170, -852},
    {171, -852},
    {172, -852},
    {173, -852},
    {174, -852},
    {175, -852},
    {176, -852},
    {177, -852},
    {178, -852},
    {179, -852},
    {180, -852},
    {181, -852},
    {182, -852},
    {183, -852},
    {184, -852},
    {185, -852},
    {186, -852},
    {187, -852},
    {188, -852},
    {189, -852},
    {190, -852},
    {191, -852},
    {192, -852},
    {193, -852},
    {194, -852},
    {195, -852},
    {196, -852},
    {197, -852},
    {198, -852},
    {199, -852},
    {200, -852},
    {201, -852},
    {202, -852},
    {203, -852},
    {204, -852},
    {205, -852},
    {206, -852},
    {207, -852},
    {208, -852},
    {209, -852},
    {210, -852},
    {211, -852},

    {212, -852},
    {213, -852},
    {214, -852},
    {215, -852},
    {216, -852},
    {217, -852},
    {218, -852},
    {219, -852},
    {220, -852},
    {221, -852},
    {222, -852},
    {223, -852},
    {224, -852},
    {225, -852},
    {226, -852},
    {227, -852},
    {228, -852},
    {229, -852},
    {230, -852},
    {231, -852},
    {232, -852},
    {233, -852},
    {234, -852},
    {235, -852},
    {236, -852},
    {237, -852},
    {238, -852},
    {239, -852},
    {240, -852},
    {241, -852},
    {242, -852},
    {243, -852},
    {244, -852},
    {245, -852},
    {246, -852},
    {247, -852},
    {248, -852},
    {249, -852},
    {250, -852},
    {251, -852},
    {252, -852},
    {253, -852},
    {254, -852},
    {255, -852},
    {256, -852},
    {0, 20},
    {0, 2034},
    {1, 0},
    {2, 0},
    {3, 0},

    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {9, 0},
    {10, 0},
    {11, 0},
    {12, 0},
    {13, 0},
    {14, 0},
    {15, 0},
    {16, 0},
    {17, 0},
    {18, 0},
    {19, 0},
    {20, 0},
    {21, 0},
    {22, 0},
    {23, 0},
    {24, 0},
    {25, 0},
    {26, 0},
    {27, 0},
    {28, 0},
    {29, 0},
    {30, 0},
    {31, 0},
    {32, 0},
    {33, 0},
    {0, 0},
    {35, 0},
    {36, 0},
    {37, 0},
    {38, 0},
    {39, 0},
    {40, 0},
    {41, 0},
    {42, 0},
    {43, 0},
    {44, 0},
    {45, 0},
    {46, 0},
    {47, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},

    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {58, 0},
    {59, 0},
    {60, 0},
    {61, 0},
    {62, 0},
    {63, 0},
    {64, 0},
    {65, 0},
    {66, 0},
    {67, 0},
    {68, 0},
    {69, 0},
    {70, 0},
    {71, 0},
    {72, 0},
    {73, 0},
    {74, 0},
    {75, 0},
    {76, 0},
    {77, 0},
    {78, 0},
    {79, 0},
    {80, 0},
    {81, 0},
    {82, 0},
    {83, 0},
    {84, 0},
    {85, 0},
    {86, 0},
    {87, 0},
    {88, 0},
    {89, 0},
    {90, 0},
    {91, 0},
    {0, 0},
    {93, 0},
    {94, 0},
    {95, 0},
    {96, 0},
    {97, 0},
    {98, 0},
    {99, 0},
    {100, 0},
    {101, 0},
    {102, 0},
    {103, 0},

    {104, 0},
    {105, 0},
    {106, 0},
    {107, 0},
    {108, 0},
    {109, 0},
    {110, 0},
    {111, 0},
    {112, 0},
    {113, 0},
    {114, 0},
    {115, 0},
    {116, 0},
    {117, 0},
    {118, 0},
    {119, 0},
    {120, 0},
    {121, 0},
    {122, 0},
    {123, 0},
    {124, 0},
    {125, 0},
    {126, 0},
    {127, 0},
    {128, 0},
    {129, 0},
    {130, 0},
    {131, 0},
    {132, 0},
    {133, 0},
    {134, 0},
    {135, 0},
    {136, 0},
    {137, 0},
    {138, 0},
    {139, 0},
    {140, 0},
    {141, 0},
    {142, 0},
    {143, 0},
    {144, 0},
    {145, 0},
    {146, 0},
    {147, 0},
    {148, 0},
    {149, 0},
    {150, 0},
    {151, 0},
    {152, 0},
    {153, 0},

    {154, 0},
    {155, 0},
    {156, 0},
    {157, 0},
    {158, 0},
    {159, 0},
    {160, 0},
    {161, 0},
    {162, 0},
    {163, 0},
    {164, 0},
    {165, 0},
    {166, 0},
    {167, 0},
    {168, 0},
    {169, 0},
    {170, 0},
    {171, 0},
    {172, 0},
    {173, 0},
    {174, 0},
    {175, 0},
    {176, 0},
    {177, 0},
    {178, 0},
    {179, 0},
    {180, 0},
    {181, 0},
    {182, 0},
    {183, 0},
    {184, 0},
    {185, 0},
    {186, 0},
    {187, 0},
    {188, 0},
    {189, 0},
    {190, 0},
    {191, 0},
    {192, 0},
    {193, 0},
    {194, 0},
    {195, 0},
    {196, 0},
    {197, 0},
    {198, 0},
    {199, 0},
    {200, 0},
    {201, 0},
    {202, 0},
    {203, 0},

    {204, 0},
    {205, 0},
    {206, 0},
    {207, 0},
    {208, 0},
    {209, 0},
    {210, 0},
    {211, 0},
    {212, 0},
    {213, 0},
    {214, 0},
    {215, 0},
    {216, 0},
    {217, 0},
    {218, 0},
    {219, 0},
    {220, 0},
    {221, 0},
    {222, 0},
    {223, 0},
    {224, 0},
    {225, 0},
    {226, 0},
    {227, 0},
    {228, 0},
    {229, 0},
    {230, 0},
    {231, 0},
    {232, 0},
    {233, 0},
    {234, 0},
    {235, 0},
    {236, 0},
    {237, 0},
    {238, 0},
    {239, 0},
    {240, 0},
    {241, 0},
    {242, 0},
    {243, 0},
    {244, 0},
    {245, 0},
    {246, 0},
    {247, 0},
    {248, 0},
    {249, 0},
    {250, 0},
    {251, 0},
    {252, 0},
    {253, 0},

    {254, 0},
    {255, 0},
    {256, 0},
    {0, 13},
    {0, 1776},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 14},
    {0, 1738},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {48, 1038},
    {49, 1038},
    {50, 1038},
    {51, 1038},
    {52, 1038},
    {53, 1038},
    {54, 1038},
    {55, 1038},
    {56, 1038},
    {57, 1038},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 1038},
    {66, 1038},
    {67, 1038},
    {68, 1038},
    {69, 1038},
    {70, 1038},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 1061},
    {49, 1061},
    {50, 1061},
    {51, 1061},
    {52, 1061},
    {53, 1061},
    {54, 1061},
    {55, 1061},
    {56, 1061},
    {57, 1061},

    {0, 0},
    {97, 1038},
    {98, 1038},
    {99, 1038},
    {100, 1038},
    {101, 1038},
    {102, 1038},
    {65, 1061},
    {66, 1061},
    {67, 1061},
    {68, 1061},
    {69, 1061},
    {70, 1061},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {123, 1061},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, 1061},
    {98, 1061},
    {99, 1061},
    {100, 1061},
    {101, 1061},
    {102, 1061},
    {0, 1},
    {0, 1634},
    {1, 0},
    {2, 0},
    {3, 0},

    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {0, 0},
    {0, 0},
    {11, 0},
    {0, 0},
    {0, 0},
    {14, 0},
    {15, 0},
    {16, 0},
    {17, 0},
    {18, 0},
    {19, 0},
    {20, 0},
    {21, 0},
    {22, 0},
    {23, 0},
    {24, 0},
    {25, 0},
    {26, 0},
    {27, 0},
    {28, 0},
    {29, 0},
    {30, 0},
    {31, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},

    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {0, 0},
    {59, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 0},
    {66, 0},
    {67, 0},
    {68, 0},
    {69, 0},
    {70, 0},
    {71, 0},
    {72, 0},
    {73, 0},
    {74, 0},
    {75, 0},
    {76, 0},
    {77, 0},
    {78, 0},
    {79, 0},
    {80, 0},
    {81, 0},
    {82, 0},
    {83, 0},
    {84, 0},
    {85, 0},
    {86, 0},
    {87, 0},
    {88, 0},
    {89, 0},
    {90, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, 0},
    {95, 0},
    {96, 0},
    {97, 0},
    {98, 0},
    {99, 0},
    {100, 0},
    {101, 0},
    {102, 0},
    {103, 0},

    {104, 0},
    {105, 0},
    {106, 0},
    {107, 0},
    {108, 0},
    {109, 0},
    {110, 0},
    {111, 0},
    {112, 0},
    {113, 0},
    {114, 0},
    {115, 0},
    {116, 0},
    {117, 0},
    {118, 0},
    {119, 0},
    {120, 0},
    {121, 0},
    {122, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {126, 0},
    {127, 0},
    {128, 0},
    {129, 0},
    {130, 0},
    {131, 0},
    {132, 0},
    {133, 0},
    {134, 0},
    {135, 0},
    {136, 0},
    {137, 0},
    {138, 0},
    {139, 0},
    {140, 0},
    {141, 0},
    {142, 0},
    {143, 0},
    {144, 0},
    {145, 0},
    {146, 0},
    {147, 0},
    {148, 0},
    {149, 0},
    {150, 0},
    {151, 0},
    {152, 0},
    {153, 0},

    {154, 0},
    {155, 0},
    {156, 0},
    {157, 0},
    {158, 0},
    {159, 0},
    {160, 0},
    {161, 0},
    {162, 0},
    {163, 0},
    {164, 0},
    {165, 0},
    {166, 0},
    {167, 0},
    {168, 0},
    {169, 0},
    {170, 0},
    {171, 0},
    {172, 0},
    {173, 0},
    {174, 0},
    {175, 0},
    {176, 0},
    {177, 0},
    {178, 0},
    {179, 0},
    {180, 0},
    {181, 0},
    {182, 0},
    {183, 0},
    {184, 0},
    {185, 0},
    {186, 0},
    {187, 0},
    {188, 0},
    {189, 0},
    {190, 0},
    {191, 0},
    {192, 0},
    {193, 0},
    {194, 0},
    {195, 0},
    {196, 0},
    {197, 0},
    {198, 0},
    {199, 0},
    {200, 0},
    {201, 0},
    {202, 0},
    {203, 0},

    {204, 0},
    {205, 0},
    {206, 0},
    {207, 0},
    {208, 0},
    {209, 0},
    {210, 0},
    {211, 0},
    {212, 0},
    {213, 0},
    {214, 0},
    {215, 0},
    {216, 0},
    {217, 0},
    {218, 0},
    {219, 0},
    {220, 0},
    {221, 0},
    {222, 0},
    {223, 0},
    {224, 0},
    {225, 0},
    {226, 0},
    {227, 0},
    {228, 0},
    {229, 0},
    {230, 0},
    {231, 0},
    {232, 0},
    {233, 0},
    {234, 0},
    {235, 0},
    {236, 0},
    {237, 0},
    {238, 0},
    {239, 0},
    {240, 0},
    {241, 0},
    {242, 0},
    {243, 0},
    {244, 0},
    {245, 0},
    {246, 0},
    {247, 0},
    {248, 0},
    {249, 0},
    {250, 0},
    {251, 0},
    {252, 0},
    {253, 0},

    {254, 0},
    {255, 0},
    {256, 0},
    {0, 2},
    {0, 1376},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {9, 0},
    {10, 0},
    {0, 0},
    {12, 0},
    {13, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {32, 0},
    {0, 22},
    {0, 1342},
    {1, 0},
    {2, 0},
    {3, 0},
    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {9, 0},
    {10, 0},
    {11, 0},

    {12, 0},
    {13, 0},
    {14, 0},
    {15, 0},
    {16, 0},
    {17, 0},
    {18, 0},
    {19, 0},
    {20, 0},
    {21, 0},
    {22, 0},
    {23, 0},
    {24, 0},
    {25, 0},
    {26, 0},
    {27, 0},
    {28, 0},
    {29, 0},
    {30, 0},
    {31, 0},
    {32, 0},
    {33, 0},
    {34, 0},
    {35, 0},
    {36, 0},
    {37, 0},
    {38, 0},
    {39, 0},
    {40, 0},
    {41, 0},
    {0, 0},
    {43, 0},
    {44, 0},
    {45, 0},
    {46, 0},
    {47, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},
    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {58, 0},
    {59, 0},
    {60, 0},
    {61, 0},

    {62, 0},
    {63, 0},
    {64, 0},
    {65, 0},
    {66, 0},
    {67, 0},
    {68, 0},
    {69, 0},
    {70, 0},
    {71, 0},
    {72, 0},
    {73, 0},
    {74, 0},
    {75, 0},
    {76, 0},
    {77, 0},
    {78, 0},
    {79, 0},
    {80, 0},
    {81, 0},
    {82, 0},
    {83, 0},
    {84, 0},
    {85, 0},
    {86, 0},
    {87, 0},
    {88, 0},
    {89, 0},
    {90, 0},
    {91, 0},
    {92, 0},
    {93, 0},
    {94, 0},
    {95, 0},
    {96, 0},
    {97, 0},
    {98, 0},
    {99, 0},
    {100, 0},
    {101, 0},
    {102, 0},
    {103, 0},
    {104, 0},
    {105, 0},
    {106, 0},
    {107, 0},
    {108, 0},
    {109, 0},
    {110, 0},
    {111, 0},

    {112, 0},
    {113, 0},
    {114, 0},
    {115, 0},
    {116, 0},
    {117, 0},
    {118, 0},
    {119, 0},
    {120, 0},
    {121, 0},
    {122, 0},
    {123, 0},
    {124, 0},
    {125, 0},
    {126, 0},
    {127, 0},
    {128, 0},
    {129, 0},
    {130, 0},
    {131, 0},
    {132, 0},
    {133, 0},
    {134, 0},
    {135, 0},
    {136, 0},
    {137, 0},
    {138, 0},
    {139, 0},
    {140, 0},
    {141, 0},
    {142, 0},
    {143, 0},
    {144, 0},
    {145, 0},
    {146, 0},
    {147, 0},
    {148, 0},
    {149, 0},
    {150, 0},
    {151, 0},
    {152, 0},
    {153, 0},
    {154, 0},
    {155, 0},
    {156, 0},
    {157, 0},
    {158, 0},
    {159, 0},
    {160, 0},
    {161, 0},

    {162, 0},
    {163, 0},
    {164, 0},
    {165, 0},
    {166, 0},
    {167, 0},
    {168, 0},
    {169, 0},
    {170, 0},
    {171, 0},
    {172, 0},
    {173, 0},
    {174, 0},
    {175, 0},
    {176, 0},
    {177, 0},
    {178, 0},
    {179, 0},
    {180, 0},
    {181, 0},
    {182, 0},
    {183, 0},
    {184, 0},
    {185, 0},
    {186, 0},
    {187, 0},
    {188, 0},
    {189, 0},
    {190, 0},
    {191, 0},
    {192, 0},
    {193, 0},
    {194, 0},
    {195, 0},
    {196, 0},
    {197, 0},
    {198, 0},
    {199, 0},
    {200, 0},
    {201, 0},
    {202, 0},
    {203, 0},
    {204, 0},
    {205, 0},
    {206, 0},
    {207, 0},
    {208, 0},
    {209, 0},
    {210, 0},
    {211, 0},

    {212, 0},
    {213, 0},
    {214, 0},
    {215, 0},
    {216, 0},
    {217, 0},
    {218, 0},
    {219, 0},
    {220, 0},
    {221, 0},
    {222, 0},
    {223, 0},
    {224, 0},
    {225, 0},
    {226, 0},
    {227, 0},
    {228, 0},
    {229, 0},
    {230, 0},
    {231, 0},
    {232, 0},
    {233, 0},
    {234, 0},
    {235, 0},
    {236, 0},
    {237, 0},
    {238, 0},
    {239, 0},
    {240, 0},
    {241, 0},
    {242, 0},
    {243, 0},
    {244, 0},
    {245, 0},
    {246, 0},
    {247, 0},
    {248, 0},
    {249, 0},
    {250, 0},
    {251, 0},
    {252, 0},
    {253, 0},
    {254, 0},
    {255, 0},
    {256, 0},
    {0, 41},
    {0, 1084},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 44},
    {0, 1074},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},

    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {48, 469},
    {49, 469},
    {50, 469},
    {51, 469},
    {52, 469},
    {53, 469},
    {54, 469},
    {55, 469},
    {56, 469},
    {57, 469},
    {0, 0},
    {69, 441},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 40},
    {0, 996},
    {1, -2148},
    {2, -2148},
    {3, -2148},
    {4, -2148},
    {5, -2148},
    {6, -2148},
    {7, -2148},
    {8, -2148},
    {0, 0},
    {0, 0},
    {11, -2148},
    {0, 0},
    {101, 441},
    {14, -2148},
    {15, -2148},

    {16, -2148},
    {17, -2148},
    {18, -2148},
    {19, -2148},
    {20, -2148},
    {21, -2148},
    {22, -2148},
    {23, -2148},
    {24, -2148},
    {25, -2148},
    {26, -2148},
    {27, -2148},
    {28, -2148},
    {29, -2148},
    {30, -2148},
    {31, -2148},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {39, -2148},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},
    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {0, 0},
    {59, -2148},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, -2148},

    {66, -2148},
    {67, -2148},
    {68, -2148},
    {69, -2148},
    {70, -2148},
    {71, -2148},
    {72, -2148},
    {73, -2148},
    {74, -2148},
    {75, -2148},
    {76, -2148},
    {77, -2148},
    {78, -2148},
    {79, -2148},
    {80, -2148},
    {81, -2148},
    {82, -2148},
    {83, -2148},
    {84, -2148},
    {85, -2148},
    {86, -2148},
    {87, -2148},
    {88, -2148},
    {89, -2148},
    {90, -2148},
    {0, 0},
    {0, 0},
    {0, 0},
    {94, -2148},
    {95, -2148},
    {96, -2148},
    {97, -2148},
    {98, -2148},
    {99, -2148},
    {100, -2148},
    {101, -2148},
    {102, -2148},
    {103, -2148},
    {104, -2148},
    {105, -2148},
    {106, -2148},
    {107, -2148},
    {108, -2148},
    {109, -2148},
    {110, -2148},
    {111, -2148},
    {112, -2148},
    {113, -2148},
    {114, -2148},
    {115, -2148},

    {116, -2148},
    {117, -2148},
    {118, -2148},
    {119, -2148},
    {120, -2148},
    {121, -2148},
    {122, -2148},
    {0, 0},
    {0, 0},
    {0, 0},
    {126, -2148},
    {127, -2148},
    {128, -2148},
    {129, -2148},
    {130, -2148},
    {131, -2148},
    {132, -2148},
    {133, -2148},
    {134, -2148},
    {135, -2148},
    {136, -2148},
    {137, -2148},
    {138, -2148},
    {139, -2148},
    {140, -2148},
    {141, -2148},
    {142, -2148},
    {143, -2148},
    {144, -2148},
    {145, -2148},
    {146, -2148},
    {147, -2148},
    {148, -2148},
    {149, -2148},
    {150, -2148},
    {151, -2148},
    {152, -2148},
    {153, -2148},
    {154, -2148},
    {155, -2148},
    {156, -2148},
    {157, -2148},
    {158, -2148},
    {159, -2148},
    {160, -2148},
    {161, -2148},
    {162, -2148},
    {163, -2148},
    {164, -2148},
    {165, -2148},

    {166, -2148},
    {167, -2148},
    {168, -2148},
    {169, -2148},
    {170, -2148},
    {171, -2148},
    {172, -2148},
    {173, -2148},
    {174, -2148},
    {175, -2148},
    {176, -2148},
    {177, -2148},
    {178, -2148},
    {179, -2148},
    {180, -2148},
    {181, -2148},
    {182, -2148},
    {183, -2148},
    {184, -2148},
    {185, -2148},
    {186, -2148},
    {187, -2148},
    {188, -2148},
    {189, -2148},
    {190, -2148},
    {191, -2148},
    {192, -2148},
    {193, -2148},
    {194, -2148},
    {195, -2148},
    {196, -2148},
    {197, -2148},
    {198, -2148},
    {199, -2148},
    {200, -2148},
    {201, -2148},
    {202, -2148},
    {203, -2148},
    {204, -2148},
    {205, -2148},
    {206, -2148},
    {207, -2148},
    {208, -2148},
    {209, -2148},
    {210, -2148},
    {211, -2148},
    {212, -2148},
    {213, -2148},
    {214, -2148},
    {215, -2148},

    {216, -2148},
    {217, -2148},
    {218, -2148},
    {219, -2148},
    {220, -2148},
    {221, -2148},
    {222, -2148},
    {223, -2148},
    {224, -2148},
    {225, -2148},
    {226, -2148},
    {227, -2148},
    {228, -2148},
    {229, -2148},
    {230, -2148},
    {231, -2148},
    {232, -2148},
    {233, -2148},
    {234, -2148},
    {235, -2148},
    {236, -2148},
    {237, -2148},
    {238, -2148},
    {239, -2148},
    {240, -2148},
    {241, -2148},
    {242, -2148},
    {243, -2148},
    {244, -2148},
    {245, -2148},
    {246, -2148},
    {247, -2148},
    {248, -2148},
    {249, -2148},
    {250, -2148},
    {251, -2148},
    {252, -2148},
    {253, -2148},
    {254, -2148},
    {255, -2148},
    {256, -2148},
    {0, 13},
    {0, 738},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 715},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 154},
    {49, 154},
    {50, 154},
    {51, 154},
    {52, 154},
    {53, 154},
    {54, 154},
    {55, 154},
    {56, 154},
    {57, 154},

    {0, 0},
    {0, 0},
    {0, 14},
    {0, 677},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 154},
    {66, 154},
    {67, 154},
    {68, 154},
    {69, 154},
    {70, 154},
    {48, 154},
    {49, 154},
    {50, 154},
    {51, 154},
    {52, 154},
    {53, 154},
    {54, 154},
    {55, 154},
    {56, 154},
    {57, 154},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 154},
    {66, 154},
    {67, 154},
    {68, 154},
    {69, 154},
    {70, 154},
    {0, 44},
    {0, 643},
    {0, 0},
    {97, 154},
    {98, 154},
    {99, 154},
    {100, 154},
    {101, 154},
    {102, 154},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {48, -2736},
    {49, -2736},
    {50, -2736},
    {51, -2736},
    {52, -2736},
    {53, -2736},
    {54, -2736},
    {55, -2736},
    {56, -2736},
    {57, -2736},
    {0, 0},
    {97, 154},
    {98, 154},
    {99, 154},
    {100, 154},
    {101, 154},
    {102, 154},
    {65, -2736},
    {66, -2736},
    {67, -2736},
    {68, -2736},
    {69, -2736},
    {70, -2736},
    {0, 40},
    {0, 605},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {43, -431},
    {0, 0},
    {45, -431},
    {0, 0},
    {0, 0},
    {48, 38},
    {49, 38},
    {50, 38},
    {51, 38},
    {52, 38},
    {53, 38},
    {54, 38},
    {55, 38},
    {56, 38},
    {57, 38},
    {0, 13},
    {0, 584},
    {0, 0},
    {0, 0},
    {0, 0},

    {97, -2736},
    {98, -2736},
    {99, -2736},
    {100, -2736},
    {101, -2736},
    {102, -2736},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 561},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 0},
    {49, 0},
    {50, 0},
    {51, 0},
    {52, 0},
    {53, 0},
    {54, 0},
    {55, 0},
    {56, 0},
    {57, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 61},
    {49, 61},
    {50, 61},
    {51, 61},
    {52, 61},
    {53, 61},

    {54, 61},
    {55, 61},
    {56, 61},
    {57, 61},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 523},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 61},
    {66, 61},
    {67, 61},
    {68, 61},
    {69, 61},
    {70, 61},
    {48, 61},
    {49, 61},
    {50, 61},
    {51, 61},
    {52, 61},
    {53, 61},
    {54, 61},
    {55, 61},
    {56, 61},
    {57, 61},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 500},
    {0, 0},
    {0, 0},
    {0, 0},
    {65, 61},
    {66, 61},
    {67, 61},
    {68, 61},
    {69, 61},
    {70, 61},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, 61},
    {98, 61},
    {99, 61},
    {100, 61},
    {101, 61},
    {102, 61},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, -2588},
    {49, -2588},
    {50, -2588},
    {51, -2588},
    {52, -2588},
    {53, -2588},
    {54, -2588},
    {55, -2588},
    {56, -2588},
    {57, -2588},
    {0, 0},
    {97, 61},
    {98, 61},
    {99, 61},
    {100, 61},
    {101, 61},
    {102, 61},
    {65, -2588},
    {66, -2588},
    {67, -2588},
    {68, -2588},
    {69, -2588},
    {70, -2588},
    {48, 61},
    {49, 61},
    {50, 61},
    {51, 61},
    {52, 61},
    {53, 61},
    {54, 61},
    {55, 61},
    {56, 61},
    {57, 61},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 439},
    {0, 0},
    {0, 0},
    {125, -2550},
    {65, 61},
    {66, 61},
    {67, 61},
    {68, 61},
    {69, 61},

    {70, 61},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, -2588},
    {98, -2588},
    {99, -2588},
    {100, -2588},
    {101, -2588},
    {102, -2588},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, 61},
    {98, 61},
    {99, 61},
    {100, 61},
    {101, 61},
    {102, 61},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, 61},
    {49, 61},
    {50, 61},
    {51, 61},
    {52, 61},
    {53, 61},
    {54, 61},
    {55, 61},
    {56, 61},
    {57, 61},
    {0, 0},

    {0, 0},
    {0, 13},
    {0, 378},
    {0, 0},
    {0, 0},
    {125, -2611},
    {65, 61},
    {66, 61},
    {67, 61},
    {68, 61},
    {69, 61},
    {70, 61},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, 61},
    {98, 61},
    {99, 61},
    {100, 61},
    {101, 61},
    {102, 61},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {48, 120},
    {49, 120},
    {50, 120},
    {51, 120},
    {52, 120},
    {53, 120},
    {54, 120},
    {55, 120},
    {56, 120},
    {57, 120},
    {0, 0},
    {0, 0},
    {0, 13},
    {0, 317},
    {0, 0},
    {0, 0},
    {125, -2672},
    {65, 120},
    {66, 120},
    {67, 120},
    {68, 120},
    {69, 120},
    {70, 120},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, 120},

    {98, 120},
    {99, 120},
    {100, 120},
    {101, 120},
    {102, 120},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, -421},
    {49, -421},
    {50, -421},
    {51, -421},
    {52, -421},
    {53, -421},
    {54, -421},
    {55, -421},
    {56, -421},
    {57, -421},
    {0, 13},
    {0, 258},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {125, -2733},
    {65, -421},
    {66, -421},
    {67, -421},
    {68, -421},
    {69, -421},
    {70, -421},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, -421},
    {98, -421},
    {99, -421},
    {100, -421},
    {101, -421},
    {102, -421},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {48, -2326},
    {49, -2326},
    {50, -2326},
    {51, -2326},
    {52, -2326},
    {53, -2326},
    {54, -2326},
    {55, -2326},
    {56, -2326},
    {57, -2326},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {123, -398},
    {65, -2326},
    {66, -2326},
    {67, -2326},
    {68, -2326},
    {69, -2326},
    {70, -2326},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {97, -2326},
    {98, -2326},
    {99, -2326},
    {100, -2326},
    {101, -2326},
    {102, -2326},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {125, -2853},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},

    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {257, 49},
    {1, 0},
};

static const struct yy_trans_info *yy_start_state_list[11] = {
    &yy_transition[1],
    &yy_transition[3],
    &yy_transition[261],
    &yy_transition[519],
    &yy_transition[777],
    &yy_transition[1035],
    &yy_transition[1293],
    &yy_transition[1551],
    &yy_transition[1809],
    &yy_transition[2067],
    &yy_transition[2325],

};

extern int yy_flex_debug;
int yy_flex_debug = 0;

                                                         
                                         
   
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *yytext;
#line 1 "jsonpath_scan.l"
#line 2 "jsonpath_scan.l"
                                                                            
   
                   
                                        
   
                                                                             
                                            
   
                                                           
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "mb/pg_wchar.h"
#include "nodes/pg_list.h"

static JsonPathString scanstring;

                                                          
static YY_BUFFER_STATE scanbufhandle;
static char *scanbuf;
static int scanbuflen;

static void
addstring(bool init, char *s, int l);
static void
addchar(bool init, char s);
static enum yytokentype
checkKeyword(void);
static void
parseUnicode(char *s, int l);
static void
parseHexChar(char *s);

                                                                             
#undef fprintf
#define fprintf(file, fmt, msg) fprintf_to_ereport(fmt, msg)

static void
fprintf_to_ereport(const char *fmt, const char *msg)
{
  ereport(ERROR, (errmsg_internal("%s", msg)));
}

                     

#line 2449 "jsonpath_scan.c"
#define YY_NO_INPUT 1
   
                                                              
                                               
                     
                          
                               
                                  
                           
   

                                                                     
#line 2462 "jsonpath_scan.c"

#define INITIAL 0
#define xq 1
#define xnq 2
#define xvq 3
#define xc 4

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

YYSTYPE *
yyget_lval(void);

void
yyset_lval(YYSTYPE *yylval_param);

                                                                        
              
   

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
yylex(YYSTYPE *yylval_param);

#define YY_DECL int yylex(YYSTYPE *yylval_param)
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

  YYSTYPE *yylval;

  yylval = yylval_param;

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
#line 97 "jsonpath_scan.l"

#line 2694 "jsonpath_scan.c"

    while (              1) /* loops until end-of-file is reached */
    {
      yy_cp = (yy_c_buf_p);

                              
      *yy_cp = (yy_hold_char);

                                                                   
                          
         
      yy_bp = yy_cp;

      yy_current_state = yy_start_state_list[(yy_start)];
    yy_match:
    {
      const struct yy_trans_info *yy_trans_info;

      YY_CHAR yy_c;

      for (yy_c = YY_SC_TO_UI(*yy_cp); (yy_trans_info = &yy_current_state[yy_c])->yy_verify == yy_c; yy_c = YY_SC_TO_UI(*++yy_cp))
      {
        yy_current_state += yy_trans_info->yy_nxt;
      }
    }

    yy_find_action:
      yy_act = yy_current_state[-1].yy_nxt;

      YY_DO_BEFORE_ACTION;

    do_action:                                                     

      switch (yy_act)
      {                                 
      case 1:
        YY_RULE_SETUP
#line 99 "jsonpath_scan.l"
        {
          addstring(false, yytext, yyleng);
        }
        YY_BREAK
      case 2:
                                  
        YY_RULE_SETUP
#line 103 "jsonpath_scan.l"
        {
          yylval->str = scanstring;
          BEGIN INITIAL;
          return checkKeyword();
        }
        YY_BREAK
      case 3:
        YY_RULE_SETUP
#line 109 "jsonpath_scan.l"
        {
          yylval->str = scanstring;
          BEGIN xc;
        }
        YY_BREAK
      case 4:
        YY_RULE_SETUP
#line 114 "jsonpath_scan.l"
        {
          yylval->str = scanstring;
          yyless(0);
          BEGIN INITIAL;
          return checkKeyword();
        }
        YY_BREAK
      case YY_STATE_EOF(xnq):
#line 121 "jsonpath_scan.l"
      {
        yylval->str = scanstring;
        BEGIN INITIAL;
        return checkKeyword();
      }
        YY_BREAK
      case 5:
        YY_RULE_SETUP
#line 127 "jsonpath_scan.l"
        {
          addchar(false, '\b');
        }
        YY_BREAK
      case 6:
        YY_RULE_SETUP
#line 129 "jsonpath_scan.l"
        {
          addchar(false, '\f');
        }
        YY_BREAK
      case 7:
        YY_RULE_SETUP
#line 131 "jsonpath_scan.l"
        {
          addchar(false, '\n');
        }
        YY_BREAK
      case 8:
        YY_RULE_SETUP
#line 133 "jsonpath_scan.l"
        {
          addchar(false, '\r');
        }
        YY_BREAK
      case 9:
        YY_RULE_SETUP
#line 135 "jsonpath_scan.l"
        {
          addchar(false, '\t');
        }
        YY_BREAK
      case 10:
        YY_RULE_SETUP
#line 137 "jsonpath_scan.l"
        {
          addchar(false, '\v');
        }
        YY_BREAK
      case 11:
        YY_RULE_SETUP
#line 139 "jsonpath_scan.l"
        {
          parseUnicode(yytext, yyleng);
        }
        YY_BREAK
      case 12:
        YY_RULE_SETUP
#line 141 "jsonpath_scan.l"
        {
          parseHexChar(yytext);
        }
        YY_BREAK
      case 13:
        YY_RULE_SETUP
#line 143 "jsonpath_scan.l"
        {
          yyerror(NULL, "invalid unicode sequence");
        }
        YY_BREAK
      case 14:
        YY_RULE_SETUP
#line 145 "jsonpath_scan.l"
        {
          yyerror(NULL, "invalid hex character sequence");
        }
        YY_BREAK
      case 15:
        YY_RULE_SETUP
#line 147 "jsonpath_scan.l"
        {
                                                       
          yyless(yyleng - 1);
          parseUnicode(yytext, yyleng);
        }
        YY_BREAK
      case 16:
        YY_RULE_SETUP
#line 153 "jsonpath_scan.l"
        {
          addchar(false, yytext[1]);
        }
        YY_BREAK
      case 17:
        YY_RULE_SETUP
#line 155 "jsonpath_scan.l"
        {
          yyerror(NULL, "unexpected end after backslash");
        }
        YY_BREAK
      case YY_STATE_EOF(xq):
      case YY_STATE_EOF(xvq):
#line 157 "jsonpath_scan.l"
      {
        yyerror(NULL, "unexpected end of quoted string");
      }
        YY_BREAK
      case 18:
        YY_RULE_SETUP
#line 159 "jsonpath_scan.l"
        {
          yylval->str = scanstring;
          BEGIN INITIAL;
          return STRING_P;
        }
        YY_BREAK
      case 19:
        YY_RULE_SETUP
#line 165 "jsonpath_scan.l"
        {
          yylval->str = scanstring;
          BEGIN INITIAL;
          return VARIABLE_P;
        }
        YY_BREAK
      case 20:
                                   
        YY_RULE_SETUP
#line 171 "jsonpath_scan.l"
        {
          addstring(false, yytext, yyleng);
        }
        YY_BREAK
      case 21:
        YY_RULE_SETUP
#line 173 "jsonpath_scan.l"
        {
          BEGIN INITIAL;
        }
        YY_BREAK
      case 22:
                                   
        YY_RULE_SETUP
#line 175 "jsonpath_scan.l"
        {
        }
        YY_BREAK
      case 23:
        YY_RULE_SETUP
#line 177 "jsonpath_scan.l"
        {
        }
        YY_BREAK
      case YY_STATE_EOF(xc):
#line 179 "jsonpath_scan.l"
      {
        yyerror(NULL, "unexpected end of comment");
      }
        YY_BREAK
      case 24:
        YY_RULE_SETUP
#line 181 "jsonpath_scan.l"
        {
          return AND_P;
        }
        YY_BREAK
      case 25:
        YY_RULE_SETUP
#line 183 "jsonpath_scan.l"
        {
          return OR_P;
        }
        YY_BREAK
      case 26:
        YY_RULE_SETUP
#line 185 "jsonpath_scan.l"
        {
          return NOT_P;
        }
        YY_BREAK
      case 27:
        YY_RULE_SETUP
#line 187 "jsonpath_scan.l"
        {
          return ANY_P;
        }
        YY_BREAK
      case 28:
        YY_RULE_SETUP
#line 189 "jsonpath_scan.l"
        {
          return LESS_P;
        }
        YY_BREAK
      case 29:
        YY_RULE_SETUP
#line 191 "jsonpath_scan.l"
        {
          return LESSEQUAL_P;
        }
        YY_BREAK
      case 30:
        YY_RULE_SETUP
#line 193 "jsonpath_scan.l"
        {
          return EQUAL_P;
        }
        YY_BREAK
      case 31:
        YY_RULE_SETUP
#line 195 "jsonpath_scan.l"
        {
          return NOTEQUAL_P;
        }
        YY_BREAK
      case 32:
        YY_RULE_SETUP
#line 197 "jsonpath_scan.l"
        {
          return NOTEQUAL_P;
        }
        YY_BREAK
      case 33:
        YY_RULE_SETUP
#line 199 "jsonpath_scan.l"
        {
          return GREATEREQUAL_P;
        }
        YY_BREAK
      case 34:
        YY_RULE_SETUP
#line 201 "jsonpath_scan.l"
        {
          return GREATER_P;
        }
        YY_BREAK
      case 35:
        YY_RULE_SETUP
#line 203 "jsonpath_scan.l"
        {
          addstring(true, yytext + 1, yyleng - 1);
          addchar(false, '\0');
          yylval->str = scanstring;
          return VARIABLE_P;
        }
        YY_BREAK
      case 36:
        YY_RULE_SETUP
#line 210 "jsonpath_scan.l"
        {
          addchar(true, '\0');
          BEGIN xvq;
        }
        YY_BREAK
      case 37:
        YY_RULE_SETUP
#line 215 "jsonpath_scan.l"
        {
          return *yytext;
        }
        YY_BREAK
      case 38:
                                   
        YY_RULE_SETUP
#line 217 "jsonpath_scan.l"
        {             
        }
        YY_BREAK
      case 39:
        YY_RULE_SETUP
#line 219 "jsonpath_scan.l"
        {
          addchar(true, '\0');
          BEGIN xc;
        }
        YY_BREAK
      case 40:
        YY_RULE_SETUP
#line 224 "jsonpath_scan.l"
        {
          addstring(true, yytext, yyleng);
          addchar(false, '\0');
          yylval->str = scanstring;
          return NUMERIC_P;
        }
        YY_BREAK
      case 41:
        YY_RULE_SETUP
#line 231 "jsonpath_scan.l"
        {
          addstring(true, yytext, yyleng);
          addchar(false, '\0');
          yylval->str = scanstring;
          return NUMERIC_P;
        }
        YY_BREAK
      case 42:
        YY_RULE_SETUP
#line 238 "jsonpath_scan.l"
        {
          addstring(true, yytext, yyleng);
          addchar(false, '\0');
          yylval->str = scanstring;
          return INT_P;
        }
        YY_BREAK
      case 43:
        YY_RULE_SETUP
#line 245 "jsonpath_scan.l"
        {
                                                      
          yyless(yyleng - 1);
          addstring(true, yytext, yyleng);
          addchar(false, '\0');
          yylval->str = scanstring;
          return INT_P;
        }
        YY_BREAK
      case 44:
        YY_RULE_SETUP
#line 254 "jsonpath_scan.l"
        {
          yyerror(NULL, "invalid floating point number");
        }
        YY_BREAK
      case 45:
        YY_RULE_SETUP
#line 256 "jsonpath_scan.l"
        {
          addchar(true, '\0');
          BEGIN xq;
        }
        YY_BREAK
      case 46:
        YY_RULE_SETUP
#line 261 "jsonpath_scan.l"
        {
          yyless(0);
          addchar(true, '\0');
          BEGIN xnq;
        }
        YY_BREAK
      case 47:
        YY_RULE_SETUP
#line 267 "jsonpath_scan.l"
        {
          addstring(true, yytext, yyleng);
          BEGIN xnq;
        }
        YY_BREAK
      case YY_STATE_EOF(INITIAL):
#line 272 "jsonpath_scan.l"
      {
        yyterminate();
      }
        YY_BREAK
      case 48:
        YY_RULE_SETUP
#line 274 "jsonpath_scan.l"
        YY_FATAL_ERROR("flex scanner jammed");
        YY_BREAK
#line 3065 "jsonpath_scan.c"

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
            yy_cp = (yy_c_buf_p);
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

  yy_current_state = yy_start_state_list[(yy_start)];

  for (yy_cp = (yytext_ptr) + YY_MORE_ADJ; yy_cp < (yy_c_buf_p); ++yy_cp)
  {
    yy_current_state += yy_current_state[(*yy_cp ? YY_SC_TO_UI(*yy_cp) : 256)].yy_nxt;
  }

  return yy_current_state;
}

                                                                    
   
            
                                                   
   
static yy_state_type
yy_try_NUL_trans(yy_state_type yy_current_state)
{
  int yy_is_jam;

  int yy_c = 256;
  const struct yy_trans_info *yy_trans_info;

  yy_trans_info = &yy_current_state[(unsigned int)yy_c];
  yy_current_state += yy_trans_info->yy_nxt;
  yy_is_jam = (yy_trans_info->yy_verify != yy_c);

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

#define YYTABLES_NAME "yytables"

#line 274 "jsonpath_scan.l"

                    

void
jsonpath_yyerror(JsonPathParseResult **result, const char *message)
{
  if (*yytext == YY_END_OF_BUFFER_CHAR)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                       
                       errmsg("%s at end of jsonpath input", _(message))));
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                             
                       errmsg("%s at or near \"%s\" of jsonpath input", _(message), yytext)));
  }
}

typedef struct JsonPathKeyword
{
  int16 len;
  bool lowercase;
  int val;
  const char *keyword;
} JsonPathKeyword;

   
                                                          
                      
   
static const JsonPathKeyword keywords[] = {
    {2, false, IS_P, "is"},
    {2, false, TO_P, "to"},
    {3, false, ABS_P, "abs"},
    {3, false, LAX_P, "lax"},
    {4, false, FLAG_P, "flag"},
    {4, false, LAST_P, "last"},
    {4, true, NULL_P, "null"},
    {4, false, SIZE_P, "size"},
    {4, true, TRUE_P, "true"},
    {4, false, TYPE_P, "type"},
    {4, false, WITH_P, "with"},
    {5, true, FALSE_P, "false"},
    {5, false, FLOOR_P, "floor"},
    {6, false, DOUBLE_P, "double"},
    {6, false, EXISTS_P, "exists"},
    {6, false, STARTS_P, "starts"},
    {6, false, STRICT_P, "strict"},
    {7, false, CEILING_P, "ceiling"},
    {7, false, UNKNOWN_P, "unknown"},
    {8, false, KEYVALUE_P, "keyvalue"},
    {10, false, LIKE_REGEX_P, "like_regex"},
};

                                                    
static enum yytokentype
checkKeyword()
{
  int res = IDENT_P;
  int diff;
  const JsonPathKeyword *StopLow = keywords, *StopHigh = keywords + lengthof(keywords), *StopMiddle;

  if (scanstring.len > keywords[lengthof(keywords) - 1].len)
  {
    return res;
  }

  while (StopLow < StopHigh)
  {
    StopMiddle = StopLow + ((StopHigh - StopLow) >> 1);

    if (StopMiddle->len == scanstring.len)
    {
      diff = pg_strncasecmp(StopMiddle->keyword, scanstring.val, scanstring.len);
    }
    else
    {
      diff = StopMiddle->len - scanstring.len;
    }

    if (diff < 0)
    {
      StopLow = StopMiddle + 1;
    }
    else if (diff > 0)
    {
      StopHigh = StopMiddle;
    }
    else
    {
      if (StopMiddle->lowercase)
      {
        diff = strncmp(StopMiddle->keyword, scanstring.val, scanstring.len);
      }

      if (diff == 0)
      {
        res = StopMiddle->val;
      }

      break;
    }
  }

  return res;
}

   
                                            
   
static void
jsonpath_scanner_init(const char *str, int slen)
{
  if (slen <= 0)
  {
    slen = strlen(str);
  }

     
                                        
     
  yy_init_globals();

     
                                                                 
     

  scanbuflen = slen;
  scanbuf = palloc(slen + 2);
  memcpy(scanbuf, str, slen);
  scanbuf[slen] = scanbuf[slen + 1] = YY_END_OF_BUFFER_CHAR;
  scanbufhandle = yy_scan_buffer(scanbuf, slen + 2);

  BEGIN(INITIAL);
}

   
                                                                          
   
static void
jsonpath_scanner_finish(void)
{
  yy_delete_buffer(scanbufhandle);
  pfree(scanbuf);
}

   
                                                                   
                             
   
static void
resizeString(bool init, int appendLen)
{
  if (init)
  {
    scanstring.total = Max(32, appendLen);
    scanstring.val = (char *)palloc(scanstring.total);
    scanstring.len = 0;
  }
  else
  {
    if (scanstring.len + appendLen >= scanstring.total)
    {
      while (scanstring.len + appendLen >= scanstring.total)
      {
        scanstring.total *= 2;
      }
      scanstring.val = repalloc(scanstring.val, scanstring.total);
    }
  }
}

                                                         
static void
addstring(bool init, char *s, int l)
{
  resizeString(init, l + 1);
  memcpy(scanstring.val + scanstring.len, s, l);
  scanstring.len += l;
}

                                       
static void
addchar(bool init, char c)
{
  resizeString(init, 1);
  scanstring.val[scanstring.len] = c;
  if (c != '\0')
  {
    scanstring.len++;
  }
}

                                  
JsonPathParseResult *
parsejsonpath(const char *str, int len)
{
  JsonPathParseResult *parseresult;

  jsonpath_scanner_init(str, len);

  if (jsonpath_yyparse((void *)&parseresult) != 0)
  {
    jsonpath_yyerror(NULL, "bogus input");                       
  }

  jsonpath_scanner_finish();

  return parseresult;
}

                                     
static int
hexval(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 0xA;
  }
  if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 0xA;
  }
  jsonpath_yyerror(NULL, "invalid hexadecimal digit");
  return 0;                  
}

                                               
static void
addUnicodeChar(int ch)
{
     
                                                         
                                                           
                                                            
                               
     

  if (ch == 0)
  {
                                                          
    ereport(ERROR, (errcode(ERRCODE_UNTRANSLATABLE_CHARACTER), errmsg("unsupported Unicode escape sequence"), errdetail("\\u0000 cannot be converted to text.")));
  }
  else if (GetDatabaseEncoding() == PG_UTF8)
  {
    char utf8str[5];
    int utf8len;

    unicode_to_utf8(ch, (unsigned char *)utf8str);
    utf8len = pg_utf_mblen((unsigned char *)utf8str);
    addstring(false, utf8str, utf8len);
  }
  else if (ch <= 0x007f)
  {
       
                                                       
                                                          
                  
       
    addchar(false, (char)ch);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "jsonpath"),
                       errdetail("Unicode escape values cannot be used for code "
                                 "point values above 007F when the server encoding "
                                 "is not UTF8.")));
  }
}

                                                        
static void
addUnicode(int ch, int *hi_surrogate)
{
  if (ch >= 0xd800 && ch <= 0xdbff)
  {
    if (*hi_surrogate != -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "jsonpath"),
                         errdetail("Unicode high surrogate must not follow "
                                   "a high surrogate.")));
    }
    *hi_surrogate = (ch & 0x3ff) << 10;
    return;
  }
  else if (ch >= 0xdc00 && ch <= 0xdfff)
  {
    if (*hi_surrogate == -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "jsonpath"),
                         errdetail("Unicode low surrogate must follow a high "
                                   "surrogate.")));
    }
    ch = 0x10000 + *hi_surrogate + (ch & 0x3ff);
    *hi_surrogate = -1;
  }
  else if (*hi_surrogate != -1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "jsonpath"),
                       errdetail("Unicode low surrogate must follow a high "
                                 "surrogate.")));
  }

  addUnicodeChar(ch);
}

   
                                                      
                                
   
static void
parseUnicode(char *s, int l)
{
  int i = 2;
  int hi_surrogate = -1;

  for (i = 2; i < l; i += 2)                
  {
    int ch = 0;
    int j;

    if (s[i] == '{')                        
    {
      while (s[++i] != '}' && i < l)
      {
        ch = (ch << 4) | hexval(s[i]);
      }
      i++;               
    }
    else                     
    {
      for (j = 0; j < 4 && i < l; j++)
      {
        ch = (ch << 4) | hexval(s[i++]);
      }
    }

    addUnicode(ch, &hi_surrogate);
  }

  if (hi_surrogate != -1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "jsonpath"),
                       errdetail("Unicode low surrogate must follow a high "
                                 "surrogate.")));
  }
}

                                              
static void
parseHexChar(char *s)
{
  int ch = (hexval(s[2]) << 4) | hexval(s[3]);

  addUnicodeChar(ch);
}

   
                                                                      
                                                                    
   

void *
jsonpath_yyalloc(yy_size_t bytes)
{
  return palloc(bytes);
}

void *
jsonpath_yyrealloc(void *ptr, yy_size_t bytes)
{
  if (ptr)
  {
    return repalloc(ptr, bytes);
  }
  else
  {
    return palloc(bytes);
  }
}

void
jsonpath_yyfree(void *ptr)
{
  if (ptr)
  {
    pfree(ptr);
  }
}
