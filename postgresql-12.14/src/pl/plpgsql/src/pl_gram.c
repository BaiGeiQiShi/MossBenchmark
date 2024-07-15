                                               

                                                   
 
                                                                                 
        
 
                                                                        
                                                                        
                                                                     
                                       
 
                                                                   
                                                                  
                                                                 
                                                
 
                                                                     
                                                                            

                                                                      
                                                                     
                                                                   
                                                                     
                                                                       
                                                                    
                                                                      
                                                                  
                                           
 
                                                                       
                            

                                                             
                                                            

                                                                  
                                                                
                                                                     

                                                                  
                                                                      
                                                                  
                                                                 
                                                                  
                              

                                                
#define YYBISON 30705

                            
#define YYBISON_VERSION "3.7.5"

                     
#define YYSKELETON_NAME "yacc.c"

                    
#define YYPURE 0

                    
#define YYPUSH 0

                    
#define YYPULL 1

                                                  
#define yyparse plpgsql_yyparse
#define yylex plpgsql_yylex
#define yyerror plpgsql_yyerror
#define yydebug plpgsql_yydebug
#define yynerrs plpgsql_yynerrs
#define yylval plpgsql_yylval
#define yychar plpgsql_yychar
#define yylloc plpgsql_yylloc

                                   
#line 1 "pl_gram.y"

                                                                            
   
                                                             
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include "catalog/namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "parser/parser.h"
#include "parser/parse_type.h"
#include "parser/scanner.h"
#include "parser/scansup.h"
#include "utils/builtins.h"

#include "plpgsql.h"

                                                                
#define YYLLOC_DEFAULT(Current, Rhs, N)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (N)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      (Current) = (Rhs)[1];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      (Current) = (Rhs)[0];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  } while (0)

   
                                                                           
                                                                         
                                                                           
                                                                         
                                                                       
                             
   
#define YYMALLOC palloc
#define YYFREE pfree

typedef struct
{
  int location;
  int leaderlen;
} sql_error_callback_arg;

#define parser_errposition(pos) plpgsql_scanner_errposition(pos)

union YYSTYPE;                                                

static bool
tok_is_keyword(int token, union YYSTYPE *lval, int kw_token, const char *kw_str);
static void
word_is_not_variable(PLword *word, int location);
static void
cword_is_not_variable(PLcword *cword, int location);
static void
current_token_is_not_variable(int tok);
static PLpgSQL_expr *
read_sql_construct(int until, int until2, int until3, const char *expected, const char *sqlstart, bool isexpression, bool valid_sql, bool trim, int *startloc, int *endtoken);
static PLpgSQL_expr *
read_sql_expression(int until, const char *expected);
static PLpgSQL_expr *
read_sql_expression2(int until, int until2, const char *expected, int *endtoken);
static PLpgSQL_expr *
read_sql_stmt(const char *sqlstart);
static PLpgSQL_type *
read_datatype(int tok);
static PLpgSQL_stmt *
make_execsql_stmt(int firsttoken, int location);
static PLpgSQL_stmt_fetch *
read_fetch_direction(void);
static void
complete_direction(PLpgSQL_stmt_fetch *fetch, bool *check_FROM);
static PLpgSQL_stmt *
make_return_stmt(int location);
static PLpgSQL_stmt *
make_return_next_stmt(int location);
static PLpgSQL_stmt *
make_return_query_stmt(int location);
static PLpgSQL_stmt *
make_case(int location, PLpgSQL_expr *t_expr, List *case_when_list, List *else_stmts);
static char *
NameOfDatum(PLwdatum *wdatum);
static void
check_assignable(PLpgSQL_datum *datum, int location);
static void
read_into_target(PLpgSQL_variable **target, bool *strict);
static PLpgSQL_row *
read_into_scalar_list(char *initial_name, PLpgSQL_datum *initial_datum, int initial_location);
static PLpgSQL_row *
make_scalar_list1(char *initial_name, PLpgSQL_datum *initial_datum, int lineno, int location);
static void
check_sql_expr(const char *stmt, int location, int leaderlen);
static void
plpgsql_sql_error_callback(void *arg);
static PLpgSQL_type *
parse_datatype(const char *string, int location);
static void
check_labels(const char *start_label, const char *end_label, int end_location);
static PLpgSQL_expr *
read_cursor_args(PLpgSQL_var *cursor, int until, const char *expected);
static List *
read_raise_options(void);
static void
check_raise_parameters(PLpgSQL_stmt_raise *stmt);

#line 194 "pl_gram.c"

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else
#define YY_CAST(Type, Val) ((Type)(Val))
#define YY_REINTERPRET_CAST(Type, Val) ((Type)(Val))
#endif
#endif
#ifndef YY_NULLPTR
#if defined __cplusplus
#if 201103L <= __cplusplus
#define YY_NULLPTR nullptr
#else
#define YY_NULLPTR 0
#endif
#else
#define YY_NULLPTR ((void *)0)
#endif
#endif

#include "pl_gram.h"
                   
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                                         
  YYSYMBOL_YYerror = 1,                               
  YYSYMBOL_YYUNDEF = 2,                                         
  YYSYMBOL_IDENT = 3,                                 
  YYSYMBOL_FCONST = 4,                                 
  YYSYMBOL_SCONST = 5,                                 
  YYSYMBOL_BCONST = 6,                                 
  YYSYMBOL_XCONST = 7,                                 
  YYSYMBOL_Op = 8,                                 
  YYSYMBOL_ICONST = 9,                                 
  YYSYMBOL_PARAM = 10,                                
  YYSYMBOL_TYPECAST = 11,                                
  YYSYMBOL_DOT_DOT = 12,                                
  YYSYMBOL_COLON_EQUALS = 13,                                
  YYSYMBOL_EQUALS_GREATER = 14,                                
  YYSYMBOL_LESS_EQUALS = 15,                                
  YYSYMBOL_GREATER_EQUALS = 16,                                
  YYSYMBOL_NOT_EQUALS = 17,                                
  YYSYMBOL_T_WORD = 18,                                
  YYSYMBOL_T_CWORD = 19,                                
  YYSYMBOL_T_DATUM = 20,                                
  YYSYMBOL_LESS_LESS = 21,                                
  YYSYMBOL_GREATER_GREATER = 22,                                
  YYSYMBOL_K_ABSOLUTE = 23,                                
  YYSYMBOL_K_ALIAS = 24,                                
  YYSYMBOL_K_ALL = 25,                                
  YYSYMBOL_K_AND = 26,                                
  YYSYMBOL_K_ARRAY = 27,                                
  YYSYMBOL_K_ASSERT = 28,                                
  YYSYMBOL_K_BACKWARD = 29,                                
  YYSYMBOL_K_BEGIN = 30,                                
  YYSYMBOL_K_BY = 31,                                
  YYSYMBOL_K_CALL = 32,                                
  YYSYMBOL_K_CASE = 33,                                
  YYSYMBOL_K_CHAIN = 34,                                
  YYSYMBOL_K_CLOSE = 35,                                
  YYSYMBOL_K_COLLATE = 36,                                
  YYSYMBOL_K_COLUMN = 37,                                
  YYSYMBOL_K_COLUMN_NAME = 38,                                
  YYSYMBOL_K_COMMIT = 39,                                
  YYSYMBOL_K_CONSTANT = 40,                                
  YYSYMBOL_K_CONSTRAINT = 41,                                
  YYSYMBOL_K_CONSTRAINT_NAME = 42,                                
  YYSYMBOL_K_CONTINUE = 43,                                
  YYSYMBOL_K_CURRENT = 44,                                
  YYSYMBOL_K_CURSOR = 45,                                
  YYSYMBOL_K_DATATYPE = 46,                                
  YYSYMBOL_K_DEBUG = 47,                                
  YYSYMBOL_K_DECLARE = 48,                                
  YYSYMBOL_K_DEFAULT = 49,                                
  YYSYMBOL_K_DETAIL = 50,                                
  YYSYMBOL_K_DIAGNOSTICS = 51,                                
  YYSYMBOL_K_DO = 52,                                
  YYSYMBOL_K_DUMP = 53,                                
  YYSYMBOL_K_ELSE = 54,                                
  YYSYMBOL_K_ELSIF = 55,                                
  YYSYMBOL_K_END = 56,                                
  YYSYMBOL_K_ERRCODE = 57,                                
  YYSYMBOL_K_ERROR = 58,                                
  YYSYMBOL_K_EXCEPTION = 59,                                
  YYSYMBOL_K_EXECUTE = 60,                                
  YYSYMBOL_K_EXIT = 61,                                
  YYSYMBOL_K_FETCH = 62,                                
  YYSYMBOL_K_FIRST = 63,                                
  YYSYMBOL_K_FOR = 64,                                
  YYSYMBOL_K_FOREACH = 65,                                
  YYSYMBOL_K_FORWARD = 66,                                
  YYSYMBOL_K_FROM = 67,                                
  YYSYMBOL_K_GET = 68,                                
  YYSYMBOL_K_HINT = 69,                                
  YYSYMBOL_K_IF = 70,                                
  YYSYMBOL_K_IMPORT = 71,                                
  YYSYMBOL_K_IN = 72,                                
  YYSYMBOL_K_INFO = 73,                                
  YYSYMBOL_K_INSERT = 74,                                
  YYSYMBOL_K_INTO = 75,                                
  YYSYMBOL_K_IS = 76,                                
  YYSYMBOL_K_LAST = 77,                                
  YYSYMBOL_K_LOG = 78,                                
  YYSYMBOL_K_LOOP = 79,                                
  YYSYMBOL_K_MESSAGE = 80,                                
  YYSYMBOL_K_MESSAGE_TEXT = 81,                                
  YYSYMBOL_K_MOVE = 82,                                
  YYSYMBOL_K_NEXT = 83,                                
  YYSYMBOL_K_NO = 84,                                
  YYSYMBOL_K_NOT = 85,                                
  YYSYMBOL_K_NOTICE = 86,                                
  YYSYMBOL_K_NULL = 87,                                
  YYSYMBOL_K_OPEN = 88,                                
  YYSYMBOL_K_OPTION = 89,                                
  YYSYMBOL_K_OR = 90,                                
  YYSYMBOL_K_PERFORM = 91,                                
  YYSYMBOL_K_PG_CONTEXT = 92,                                
  YYSYMBOL_K_PG_DATATYPE_NAME = 93,                                
  YYSYMBOL_K_PG_EXCEPTION_CONTEXT = 94,                                
  YYSYMBOL_K_PG_EXCEPTION_DETAIL = 95,                                
  YYSYMBOL_K_PG_EXCEPTION_HINT = 96,                                
  YYSYMBOL_K_PRINT_STRICT_PARAMS = 97,                                
  YYSYMBOL_K_PRIOR = 98,                                
  YYSYMBOL_K_QUERY = 99,                                
  YYSYMBOL_K_RAISE = 100,                               
  YYSYMBOL_K_RELATIVE = 101,                               
  YYSYMBOL_K_RESET = 102,                               
  YYSYMBOL_K_RETURN = 103,                               
  YYSYMBOL_K_RETURNED_SQLSTATE = 104,                               
  YYSYMBOL_K_REVERSE = 105,                               
  YYSYMBOL_K_ROLLBACK = 106,                               
  YYSYMBOL_K_ROW_COUNT = 107,                               
  YYSYMBOL_K_ROWTYPE = 108,                               
  YYSYMBOL_K_SCHEMA = 109,                               
  YYSYMBOL_K_SCHEMA_NAME = 110,                               
  YYSYMBOL_K_SCROLL = 111,                               
  YYSYMBOL_K_SET = 112,                               
  YYSYMBOL_K_SLICE = 113,                               
  YYSYMBOL_K_SQLSTATE = 114,                               
  YYSYMBOL_K_STACKED = 115,                               
  YYSYMBOL_K_STRICT = 116,                               
  YYSYMBOL_K_TABLE = 117,                               
  YYSYMBOL_K_TABLE_NAME = 118,                               
  YYSYMBOL_K_THEN = 119,                               
  YYSYMBOL_K_TO = 120,                               
  YYSYMBOL_K_TYPE = 121,                               
  YYSYMBOL_K_USE_COLUMN = 122,                               
  YYSYMBOL_K_USE_VARIABLE = 123,                               
  YYSYMBOL_K_USING = 124,                               
  YYSYMBOL_K_VARIABLE_CONFLICT = 125,                               
  YYSYMBOL_K_WARNING = 126,                               
  YYSYMBOL_K_WHEN = 127,                               
  YYSYMBOL_K_WHILE = 128,                               
  YYSYMBOL_129_ = 129,                              
  YYSYMBOL_130_ = 130,                              
  YYSYMBOL_131_ = 131,                              
  YYSYMBOL_132_ = 132,                              
  YYSYMBOL_133_ = 133,                              
  YYSYMBOL_134_ = 134,                              
  YYSYMBOL_135_ = 135,                              
  YYSYMBOL_YYACCEPT = 136,                              
  YYSYMBOL_pl_function = 137,                               
  YYSYMBOL_comp_options = 138,                               
  YYSYMBOL_comp_option = 139,                               
  YYSYMBOL_option_value = 140,                               
  YYSYMBOL_opt_semi = 141,                               
  YYSYMBOL_pl_block = 142,                               
  YYSYMBOL_decl_sect = 143,                               
  YYSYMBOL_decl_start = 144,                               
  YYSYMBOL_decl_stmts = 145,                               
  YYSYMBOL_decl_stmt = 146,                               
  YYSYMBOL_decl_statement = 147,                               
  YYSYMBOL_148_1 = 148,                             
  YYSYMBOL_opt_scrollable = 149,                               
  YYSYMBOL_decl_cursor_query = 150,                               
  YYSYMBOL_decl_cursor_args = 151,                               
  YYSYMBOL_decl_cursor_arglist = 152,                               
  YYSYMBOL_decl_cursor_arg = 153,                               
  YYSYMBOL_decl_is_for = 154,                               
  YYSYMBOL_decl_aliasitem = 155,                               
  YYSYMBOL_decl_varname = 156,                               
  YYSYMBOL_decl_const = 157,                               
  YYSYMBOL_decl_datatype = 158,                               
  YYSYMBOL_decl_collate = 159,                               
  YYSYMBOL_decl_notnull = 160,                               
  YYSYMBOL_decl_defval = 161,                               
  YYSYMBOL_decl_defkey = 162,                               
  YYSYMBOL_assign_operator = 163,                               
  YYSYMBOL_proc_sect = 164,                               
  YYSYMBOL_proc_stmt = 165,                               
  YYSYMBOL_stmt_perform = 166,                               
  YYSYMBOL_stmt_call = 167,                               
  YYSYMBOL_stmt_assign = 168,                               
  YYSYMBOL_stmt_getdiag = 169,                               
  YYSYMBOL_getdiag_area_opt = 170,                               
  YYSYMBOL_getdiag_list = 171,                               
  YYSYMBOL_getdiag_list_item = 172,                               
  YYSYMBOL_getdiag_item = 173,                               
  YYSYMBOL_getdiag_target = 174,                               
  YYSYMBOL_assign_var = 175,                               
  YYSYMBOL_stmt_if = 176,                               
  YYSYMBOL_stmt_elsifs = 177,                               
  YYSYMBOL_stmt_else = 178,                               
  YYSYMBOL_stmt_case = 179,                               
  YYSYMBOL_opt_expr_until_when = 180,                               
  YYSYMBOL_case_when_list = 181,                               
  YYSYMBOL_case_when = 182,                               
  YYSYMBOL_opt_case_else = 183,                               
  YYSYMBOL_stmt_loop = 184,                               
  YYSYMBOL_stmt_while = 185,                               
  YYSYMBOL_stmt_for = 186,                               
  YYSYMBOL_for_control = 187,                               
  YYSYMBOL_for_variable = 188,                               
  YYSYMBOL_stmt_foreach_a = 189,                               
  YYSYMBOL_foreach_slice = 190,                               
  YYSYMBOL_stmt_exit = 191,                               
  YYSYMBOL_exit_type = 192,                               
  YYSYMBOL_stmt_return = 193,                               
  YYSYMBOL_stmt_raise = 194,                               
  YYSYMBOL_stmt_assert = 195,                               
  YYSYMBOL_loop_body = 196,                               
  YYSYMBOL_stmt_execsql = 197,                               
  YYSYMBOL_stmt_dynexecute = 198,                               
  YYSYMBOL_stmt_open = 199,                               
  YYSYMBOL_stmt_fetch = 200,                               
  YYSYMBOL_stmt_move = 201,                               
  YYSYMBOL_opt_fetch_direction = 202,                               
  YYSYMBOL_stmt_close = 203,                               
  YYSYMBOL_stmt_null = 204,                               
  YYSYMBOL_stmt_commit = 205,                               
  YYSYMBOL_stmt_rollback = 206,                               
  YYSYMBOL_opt_transaction_chain = 207,                               
  YYSYMBOL_stmt_set = 208,                               
  YYSYMBOL_cursor_variable = 209,                               
  YYSYMBOL_exception_sect = 210,                               
  YYSYMBOL_211_2 = 211,                            
  YYSYMBOL_proc_exceptions = 212,                               
  YYSYMBOL_proc_exception = 213,                               
  YYSYMBOL_proc_conditions = 214,                               
  YYSYMBOL_proc_condition = 215,                               
  YYSYMBOL_expr_until_semi = 216,                               
  YYSYMBOL_expr_until_rightbracket = 217,                               
  YYSYMBOL_expr_until_then = 218,                               
  YYSYMBOL_expr_until_loop = 219,                               
  YYSYMBOL_opt_block_label = 220,                               
  YYSYMBOL_opt_loop_label = 221,                               
  YYSYMBOL_opt_label = 222,                               
  YYSYMBOL_opt_exitcond = 223,                               
  YYSYMBOL_any_identifier = 224,                               
  YYSYMBOL_unreserved_keyword = 225                                
};
typedef enum yysymbol_kind_t yysymbol_kind_t;

#ifdef short
#undef short
#endif

                                                                   
                                                         
                                                                 

#ifndef __PTRDIFF_MAX__
#include <limits.h>                                   
#if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stdint.h>                                   
#define YY_STDINT_H
#endif
#endif

                                                                       
                                                                      
                                                                       
                                              

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

                                                              
                                                                       
                                                                   
                                                                   
                                                                     
#ifdef __hpux
#undef UINT_LEAST8_MAX
#undef UINT_LEAST16_MAX
#define UINT_LEAST8_MAX 255
#define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
#if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#define YYPTRDIFF_T __PTRDIFF_TYPE__
#define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
#elif defined PTRDIFF_MAX
#ifndef ptrdiff_t
#include <stddef.h>                                   
#endif
#define YYPTRDIFF_T ptrdiff_t
#define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
#else
#define YYPTRDIFF_T long
#define YYPTRDIFF_MAXIMUM LONG_MAX
#endif
#endif

#ifndef YYSIZE_T
#ifdef __SIZE_TYPE__
#define YYSIZE_T __SIZE_TYPE__
#elif defined size_t
#define YYSIZE_T size_t
#elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stddef.h>                                   
#define YYSIZE_T size_t
#else
#define YYSIZE_T unsigned
#endif
#endif

#define YYSIZE_MAXIMUM YY_CAST(YYPTRDIFF_T, (YYPTRDIFF_MAXIMUM < YY_CAST(YYSIZE_T, -1) ? YYPTRDIFF_MAXIMUM : YY_CAST(YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST(YYPTRDIFF_T, sizeof(X))

                                             
typedef yytype_int16 yy_state_t;

                                     
typedef int yy_state_fast_t;

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h>                                   
#define YY_(Msgid) dgettext("bison-runtime", Msgid)
#endif
#endif
#ifndef YY_
#define YY_(Msgid) Msgid
#endif
#endif

#ifndef YY_ATTRIBUTE_PURE
#if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define YY_ATTRIBUTE_PURE
#endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
#if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define YY_ATTRIBUTE_UNUSED
#endif
#endif

                                                      
#if !defined lint || defined __GNUC__
#define YY_USE(E) ((void)(E))
#else
#define YY_USE(E)            
#endif

#if defined __GNUC__ && !defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
                                                                         
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"") _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value)               
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif

#define YY_ASSERT(E) ((void)(0 && (E)))

#if !defined yyoverflow

                                                                         

#ifdef YYSTACK_USE_ALLOCA
#if YYSTACK_USE_ALLOCA
#ifdef __GNUC__
#define YYSTACK_ALLOC __builtin_alloca
#elif defined __BUILTIN_VA_ARG_INCR
#include <alloca.h>                                   
#elif defined _AIX
#define YYSTACK_ALLOC __alloca
#elif defined _MSC_VER
#include <malloc.h>                                   
#define alloca _alloca
#else
#define YYSTACK_ALLOC alloca
#if !defined _ALLOCA_H && !defined EXIT_SUCCESS
#include <stdlib.h>                                   
                                                  
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#endif
#endif
#endif

#ifdef YYSTACK_ALLOC
                                            
#define YYSTACK_FREE(Ptr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    ;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)
#ifndef YYSTACK_ALLOC_MAXIMUM
                                                                          
                                                                       
                                                                       
                                                                   
#define YYSTACK_ALLOC_MAXIMUM 4032                            
#endif
#else
#define YYSTACK_ALLOC YYMALLOC
#define YYSTACK_FREE YYFREE
#ifndef YYSTACK_ALLOC_MAXIMUM
#define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#endif
#if (defined __cplusplus && !defined EXIT_SUCCESS && !((defined YYMALLOC || defined malloc) && (defined YYFREE || defined free)))
#include <stdlib.h>                                   
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#ifndef YYMALLOC
#define YYMALLOC malloc
#if !defined malloc && !defined EXIT_SUCCESS
void *malloc(YYSIZE_T);                                   
#endif
#endif
#ifndef YYFREE
#define YYFREE free
#if !defined free && !defined EXIT_SUCCESS
void
free(void *);                                   
#endif
#endif
#endif
#endif                          

#if (!defined yyoverflow && (!defined __cplusplus || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

                                                            
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

                                                                          
#define YYSTACK_GAP_MAXIMUM (YYSIZEOF(union yyalloc) - 1)

                                                                      
                  
#define YYSTACK_BYTES(N) ((N) * (YYSIZEOF(yy_state_t) + YYSIZEOF(YYSTYPE) + YYSIZEOF(YYLTYPE)) + 2 * YYSTACK_GAP_MAXIMUM)

#define YYCOPY_NEEDED 1

                                                             
                                                                         
                                                                  
                                                                     
             
#define YYSTACK_RELOCATE(Stack_alloc, Stack)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    YYPTRDIFF_T yynewbytes;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    YYCOPY(&yyptr->Stack_alloc, Stack, yysize);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Stack = &yyptr->Stack_alloc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    yynewbytes = yystacksize * YYSIZEOF(*Stack) + YYSTACK_GAP_MAXIMUM;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    yyptr += yynewbytes / YYSIZEOF(*yyptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  } while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
                                                                      
                   
#ifndef YYCOPY
#if defined __GNUC__ && 1 < __GNUC__
#define YYCOPY(Dst, Src, Count) __builtin_memcpy(Dst, Src, YY_CAST(YYSIZE_T, (Count)) * sizeof(*(Src)))
#else
#define YYCOPY(Dst, Src, Count)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    YYPTRDIFF_T yyi;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    for (yyi = 0; yyi < (Count); yyi++)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      (Dst)[yyi] = (Src)[yyi];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)
#endif
#endif
#endif                     

                                                        
#define YYFINAL 3
                                       
#define YYLAST 1465

                                        
#define YYNTOKENS 136
                                        
#define YYNNTS 90
                                   
#define YYNRULES 258
                                     
#define YYNSTATES 342

                                          
#define YYMAXUTOK 383

                                                                      
                                                         
#define YYTRANSLATE(YYX) (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

                                                                      
                            
static const yytype_uint8 yytranslate[] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 129, 2, 2, 2, 2, 131, 132, 2, 2, 133, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 130, 2, 134, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 135, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128};

#if YYDEBUG
                                                                     
static const yytype_int16 yyrline[] = {0, 363, 363, 369, 370, 373, 377, 386, 390, 394, 400, 404, 409, 410, 413, 436, 444, 451, 460, 472, 473, 476, 477, 481, 494, 532, 538, 537, 590, 593, 597, 604, 610, 613, 644, 648, 654, 662, 663, 665, 680, 695, 723, 751, 782, 783, 788, 799, 800, 805, 810, 817, 818, 822, 824, 830, 831, 839, 840, 844, 845, 855, 857, 859, 861, 863, 865, 867, 869, 871, 873, 875, 877, 879, 881, 883, 885, 887, 889, 891, 893, 895, 897, 899, 901, 903, 907, 921, 935, 952, 967, 1030, 1033, 1037, 1043, 1047, 1053, 1066, 1110, 1121, 1126, 1134, 1139, 1156, 1174, 1177, 1191, 1194, 1200, 1207, 1221, 1225, 1231, 1243, 1246, 1261, 1279, 1298, 1332, 1594, 1620, 1634, 1641, 1680, 1683, 1689, 1742, 1746, 1752, 1778, 1923, 1947, 1965, 1969, 1973, 1983, 1995, 2059, 2137, 2167, 2180, 2185, 2199, 2206, 2220, 2235, 2236, 2237, 2240, 2253, 2268, 2290, 2295, 2303, 2305, 2304, 2346, 2350, 2356, 2369, 2378, 2384, 2421, 2425, 2429, 2433, 2437, 2441, 2449, 2453, 2461, 2464, 2471, 2473, 2480,
    2484, 2488, 2497, 2498, 2499, 2500, 2501, 2502, 2503, 2504, 2505, 2506, 2507, 2508, 2509, 2510, 2511, 2512, 2513, 2514, 2515, 2516, 2517, 2518, 2519, 2520, 2521, 2522, 2523, 2524, 2525, 2526, 2527, 2528, 2529, 2530, 2531, 2532, 2533, 2534, 2535, 2536, 2537, 2538, 2539, 2540, 2541, 2542, 2543, 2544, 2545, 2546, 2547, 2548, 2549, 2550, 2551, 2552, 2553, 2554, 2555, 2556, 2557, 2558, 2559, 2560, 2561, 2562, 2563, 2564, 2565, 2566, 2567, 2568, 2569, 2570, 2571, 2572, 2573, 2574, 2575, 2576, 2577, 2578};
#endif

                                        
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
                                                                 
                                     
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

                                                                
                                                                       
static const char *const yytname[] = {"\"end of file\"", "error", "\"invalid token\"", "IDENT", "FCONST", "SCONST", "BCONST", "XCONST", "Op", "ICONST", "PARAM", "TYPECAST", "DOT_DOT", "COLON_EQUALS", "EQUALS_GREATER", "LESS_EQUALS", "GREATER_EQUALS", "NOT_EQUALS", "T_WORD", "T_CWORD", "T_DATUM", "LESS_LESS", "GREATER_GREATER", "K_ABSOLUTE", "K_ALIAS", "K_ALL", "K_AND", "K_ARRAY", "K_ASSERT", "K_BACKWARD", "K_BEGIN", "K_BY", "K_CALL", "K_CASE", "K_CHAIN", "K_CLOSE", "K_COLLATE", "K_COLUMN", "K_COLUMN_NAME", "K_COMMIT", "K_CONSTANT", "K_CONSTRAINT", "K_CONSTRAINT_NAME", "K_CONTINUE", "K_CURRENT", "K_CURSOR", "K_DATATYPE", "K_DEBUG", "K_DECLARE", "K_DEFAULT", "K_DETAIL", "K_DIAGNOSTICS", "K_DO", "K_DUMP", "K_ELSE", "K_ELSIF", "K_END", "K_ERRCODE", "K_ERROR", "K_EXCEPTION", "K_EXECUTE", "K_EXIT", "K_FETCH", "K_FIRST", "K_FOR", "K_FOREACH", "K_FORWARD", "K_FROM", "K_GET", "K_HINT", "K_IF", "K_IMPORT", "K_IN", "K_INFO", "K_INSERT", "K_INTO", "K_IS", "K_LAST", "K_LOG", "K_LOOP", "K_MESSAGE",
    "K_MESSAGE_TEXT", "K_MOVE", "K_NEXT", "K_NO", "K_NOT", "K_NOTICE", "K_NULL", "K_OPEN", "K_OPTION", "K_OR", "K_PERFORM", "K_PG_CONTEXT", "K_PG_DATATYPE_NAME", "K_PG_EXCEPTION_CONTEXT", "K_PG_EXCEPTION_DETAIL", "K_PG_EXCEPTION_HINT", "K_PRINT_STRICT_PARAMS", "K_PRIOR", "K_QUERY", "K_RAISE", "K_RELATIVE", "K_RESET", "K_RETURN", "K_RETURNED_SQLSTATE", "K_REVERSE", "K_ROLLBACK", "K_ROW_COUNT", "K_ROWTYPE", "K_SCHEMA", "K_SCHEMA_NAME", "K_SCROLL", "K_SET", "K_SLICE", "K_SQLSTATE", "K_STACKED", "K_STRICT", "K_TABLE", "K_TABLE_NAME", "K_THEN", "K_TO", "K_TYPE", "K_USE_COLUMN", "K_USE_VARIABLE", "K_USING", "K_VARIABLE_CONFLICT", "K_WARNING", "K_WHEN", "K_WHILE", "'#'", "';'", "'('", "')'", "','", "'='", "'['", "$accept", "pl_function", "comp_options", "comp_option", "option_value", "opt_semi", "pl_block", "decl_sect", "decl_start", "decl_stmts", "decl_stmt", "decl_statement", "$@1", "opt_scrollable", "decl_cursor_query", "decl_cursor_args", "decl_cursor_arglist", "decl_cursor_arg",
    "decl_is_for", "decl_aliasitem", "decl_varname", "decl_const", "decl_datatype", "decl_collate", "decl_notnull", "decl_defval", "decl_defkey", "assign_operator", "proc_sect", "proc_stmt", "stmt_perform", "stmt_call", "stmt_assign", "stmt_getdiag", "getdiag_area_opt", "getdiag_list", "getdiag_list_item", "getdiag_item", "getdiag_target", "assign_var", "stmt_if", "stmt_elsifs", "stmt_else", "stmt_case", "opt_expr_until_when", "case_when_list", "case_when", "opt_case_else", "stmt_loop", "stmt_while", "stmt_for", "for_control", "for_variable", "stmt_foreach_a", "foreach_slice", "stmt_exit", "exit_type", "stmt_return", "stmt_raise", "stmt_assert", "loop_body", "stmt_execsql", "stmt_dynexecute", "stmt_open", "stmt_fetch", "stmt_move", "opt_fetch_direction", "stmt_close", "stmt_null", "stmt_commit", "stmt_rollback", "opt_transaction_chain", "stmt_set", "cursor_variable", "exception_sect", "@2", "proc_exceptions", "proc_exception", "proc_conditions", "proc_condition", "expr_until_semi",
    "expr_until_rightbracket", "expr_until_then", "expr_until_loop", "opt_block_label", "opt_loop_label", "opt_label", "opt_exitcond", "any_identifier", "unreserved_keyword", YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
                                                                 
                                                                    
static const yytype_int16 yytoknum[] = {0, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313, 314, 315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383, 35, 59, 40, 41, 44, 61, 91};
#endif

#define YYPACT_NINF (-263)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-168)

#define yytable_value_is_error(Yyn) 0

                                                                   
                 
static const yytype_int16 yypact[] = {-263, 36, -13, -263, 369, -66, -263, -87, 21, 13, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, 44, -263, 18, 693, -39, -263, -263, -263, -263, 260, -263, -263, -263, -263, -263, -263, -263, -263, 1086, -263, 369, -263, 260, -263, -263, 4, -263, -263, -263, -263, 369, -263, -263, -263, 73, 63, -263, -263, -263, -263, -263, -263, -35, -263, -263, -263, -263, -32, 73, -263, -263, -263, -263, 63, -263, -31, -263, -263, -263, -263, -263, -10, -263, -263, -263, -263, -263, -263, -263, 369, -263, -263,
    -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, 45, -26, 92, -263, 53, -263, 7, -263, 74, -263, 106, 2, -263, -263, -263, 0, -17, 1, 5, 73, -263, -263, 82, -263, 73, -263, -263, -263, 6, -263, -263, -263, -263, -263, -70, -263, 369, 89, 89, -263, -263, -263, 478, -263, -263, 98, 10, -263, -52, -263, -263, -263, 101, -263, 369, 5, -263, 62, 93, 908, 8, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, 67, 27, 1175, -263, -263, -263, -263, 11, -263, 12, 587, 57, -263, -263, -263, 88, -263, -45, -263, -263, -263, -263, -263, -263, -60, -263, -7, 14, 24, -263, -263, -263, -263, 136, 75, 69, -263, -263, 799, -29, -263, -263, -263, 59, -8, -6, 1264, 117, 369, -263, -263, 93, -263, -263, -263, 95, -263, 125, 369, -46, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, -263, 23, -263, 150, -263, -263, 1353, -263, 84, -263, 25, -263, 799, -263, -263, -263, 997, 26, -263, -263, -263, -263, -263};

                                                                       
                                                                       
                                     
static const yytype_int16 yydefact[] = {3, 0, 166, 1, 0, 0, 4, 12, 0, 15, 174, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 0, 175, 0, 0, 0, 13, 2, 59, 18, 16, 167, 5, 10, 6, 11, 7, 9, 8, 168, 42, 0, 22, 17, 20, 21, 44, 43, 134, 135, 101, 0, 130, 87, 109, 0, 147, 127, 88, 154, 136, 126, 140, 91, 164, 132, 133, 140, 0, 0, 162, 129, 149, 128, 147, 148, 0, 60, 75, 76, 62, 77, 0, 63, 64, 65, 66, 67, 68, 69, 170, 70, 71, 72, 73, 74, 78, 79, 80, 81, 82, 83, 84, 85, 0, 0, 0, 19, 0, 45, 0, 30, 0, 46, 0, 0, 151, 152, 150, 0, 0, 0, 0, 0, 92, 93, 0, 59, 0, 142, 137, 86, 0, 61, 58, 57, 163, 162, 0, 171, 170, 0, 0, 59, 165, 23, 0, 29,
    26, 47, 169, 164, 113, 111, 141, 145, 0, 143, 0, 155, 157, 0, 0, 168, 0, 144, 102, 89, 162, 172, 125, 14, 120, 121, 119, 59, 0, 123, 168, 115, 59, 39, 41, 0, 40, 32, 0, 51, 59, 59, 110, 0, 146, 0, 160, 161, 156, 138, 99, 100, 0, 95, 0, 98, 106, 139, 173, 117, 118, 0, 0, 0, 116, 25, 0, 0, 48, 50, 49, 0, 0, 168, 168, 0, 0, 59, 90, 0, 97, 59, 164, 0, 124, 0, 170, 0, 34, 46, 38, 37, 31, 52, 56, 53, 24, 54, 55, 0, 159, 168, 94, 96, 168, 59, 0, 165, 0, 33, 0, 36, 27, 108, 168, 0, 59, 131, 35, 103, 122};

                          
static const yytype_int16 yypgoto[] = {-263, -263, -263, -263, -263, -263, 155, -263, -263, -263, 42, -263, -263, -263, -263, -263, -263, -172, -263, -263, -262, -263, -150, -263, -263, -263, -263, -241, -97, -263, -263, -263, -263, -263, -263, -263, -139, -263, -263, -205, -263, -263, -263, -263, -263, -263, -63, -263, -263, -263, -263, -263, -49, -263, -263, -263, -263, -263, -263, -263, -232, -263, -263, -263, -263, -263, 32, -263, -263, -263, -263, 20, -263, -124, -263, -263, -263, -59, -263, -123, -178, -263, -213, -153, -263, -263, -203, -263, -4, -96};

                            
static const yytype_int16 yydefgoto[] = {0, 1, 2, 6, 107, 100, 149, 8, 103, 116, 117, 118, 258, 185, 333, 288, 308, 309, 313, 256, 119, 186, 222, 260, 293, 317, 318, 210, 251, 150, 151, 152, 153, 154, 199, 273, 274, 324, 275, 155, 156, 277, 304, 157, 188, 225, 226, 264, 158, 159, 160, 248, 249, 161, 283, 162, 163, 164, 165, 166, 252, 167, 168, 169, 170, 171, 196, 172, 173, 174, 175, 194, 176, 192, 177, 195, 232, 233, 266, 267, 204, 239, 200, 253, 9, 178, 211, 243, 212, 95};

                                                                    
                                                                   
                                                              
static const yytype_int16 yytable[] = {94, 108, 262, 207, 112, 207, 207, 120, 4, 197, 244, 261, 121, 122, 123, 124, 280, 228, 203, 109, 120, 285, 125, 96, -166, 310, 126, 127, 181, 128, 276, 97, 240, 129, 301, 311, 3, 130, 214, 215, -167, 315, -166, 99, 182, 297, 131, 312, -112, -28, -112, 101, 319, 216, 133, 134, 135, 241, -167, 98, 242, 102, 136, 279, 137, 138, 104, 229, 139, 310, 299, 105, 234, 300, 298, 224, 140, 237, 302, 303, 198, 141, 142, 110, 111, 143, 330, 331, 183, 193, 326, 189, 190, 191, 144, 276, 145, 146, 202, 206, 147, 213, 217, 236, 329, 341, 148, 245, 246, 247, 179, 271, 272, 123, 218, 184, 5, 219, 220, 221, 187, -112, 316, 257, 208, 209, 208, 208, 223, 224, 227, 230, 231, 235, 259, 265, 238, 270, 278, 281, 282, 286, 292, 287, 296, 305, 314, 306, 307, 209, 320, 327, 328, 334, 336, 338, 340, 7, 180, 339, 332, 323, 263, 291, 294, 295, 250, 205, 121, 122, 123, 124, 201, 269, 321, 337, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 120, 0, 130, 0, 0, 0, 0,
    -166, 0, 0, 322, 131, 0, 0, 325, -158, 0, 0, 0, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 268, 0, 335, 0, 0, 140, 0, 0, 120, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 0, 0, 0, 0, 0, 148, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -158, 113, 0, 0, 114, 0, 12, 13, 0, 14, 15, 16, 17, 0, 0, 18, 268, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 115, 33, 34, 35, 36, 37, 0, 38, 0, 39, 40, 41, 0, 42, 43, 44, 0, 0, 45, 0, 46, 47, 0, 48, 0, 49, 50, 0, 51, 52, 53, 0, 54, 55, 56, 57, 58, 0, 59, 0, 60, 61, 0, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 87, 88, 0, 0, 89, 90, 91, 0, 92, 93, 10, 0, 11, 0, 0, 12, 13, 0, 14, 15, 16, 17, 0, 0, 18, 0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 0, 33, 34, 35, 36, 37, 0, 38, 0, 39, 40, 41, 0, 42, 43, 44, 0, 0, 45, 0, 46, 47, 0, 48, 0, 49, 50, 0, 51, 52, 53, 0, 54, 55, 56, 57, 58, 0, 59, 0, 60, 61, 0, 62, 63, 64, 65,
    66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 87, 88, 0, 0, 89, 90, 91, 0, 92, 93, 254, 255, 0, 0, 0, 12, 13, 0, 14, 15, 16, 17, 0, 0, 18, 0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 0, 33, 34, 35, 36, 37, 0, 38, 0, 39, 40, 41, 0, 42, 43, 44, 0, 0, 45, 0, 46, 47, 0, 48, 0, 49, 50, 0, 51, 52, 53, 0, 54, 55, 56, 57, 58, 0, 59, 0, 60, 61, 0, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 87, 88, 0, 0, 89, 90, 91, 0, 92, 93, 289, 290, 0, 0, 0, 12, 13, 0, 14, 15, 16, 17, 0, 0, 18, 0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 0, 33, 34, 35, 36, 37, 0, 38, 0, 39, 40, 41, 0, 42, 43, 44, 0, 0, 45, 0, 46, 47, 0, 48, 0, 49, 50, 0, 51, 52, 53, 0, 54, 55, 56, 57, 58, 0, 59, 0, 60, 61, 0, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 87, 88, 0, 0, 89, 90, 91, 106, 92, 93, 0, 0, 12, 13, 0, 14, 15, 16, 17, 0, 0, 18,
    0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 0, 33, 34, 35, 36, 37, 0, 38, 0, 39, 40, 41, 0, 42, 43, 44, 0, 0, 45, 0, 46, 47, 0, 48, 0, 49, 50, 0, 51, 52, 53, 0, 54, 55, 56, 57, 58, 0, 59, 0, 60, 61, 0, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 87, 88, 0, 0, 89, 90, 91, 113, 92, 93, 0, 0, 12, 13, 0, 14, 15, 16, 17, 0, 0, 18, 0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 0, 33, 34, 35, 36, 37, 0, 38, 0, 39, 40, 41, 0, 42, 43, 44, 0, 0, 45, 0, 46, 47, 0, 48, 0, 49, 50, 0, 51, 52, 53, 0, 54, 55, 56, 57, 58, 0, 59, 0, 60, 61, 0, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 87, 88, 0, 0, 89, 90, 91, 0, 92, 93, 121, 122, 123, 124, 0, 0, 0, 0, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 0, 0, 130, 0, 0, 0, 0, -166, 0, 0, 0, 131, 0, -104, -104, -104, 0, 0, 0, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 0, 0, 0,
    0, 0, 140, 0, 0, 0, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 121, 122, 123, 124, 0, 148, 0, 0, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 0, 0, 130, 0, 0, 0, 0, -166, 0, 0, 0, 131, 0, -105, -105, -105, 0, 0, 0, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 0, 0, 0, 0, 0, 140, 0, 0, 0, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 121, 122, 123, 124, 0, 148, 0, 0, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 0, 0, 130, 0, 0, 0, 0, -166, 0, 0, 0, 131, 0, 0, 0, -153, 0, 0, 132, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 0, 0, 0, 0, 0, 140, 0, 0, 0, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 121, 122, 123, 124, 0, 148, 0, 0, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 0, 0, 130, 0, 0, 0, 0, -166, 0, 0, 0, 131, 0, 0, 0, 284, 0, 0, 0, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 0, 0, 0,
    0, 0, 140, 0, 0, 0, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 121, 122, 123, 124, 0, 148, 0, 0, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 0, 0, 130, 0, 0, 0, 0, -166, 0, 0, 0, 131, 0, 0, 0, -114, 0, 0, 0, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 0, 0, 0, 0, 0, 140, 0, 0, 0, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 121, 122, 123, 124, 0, 148, 0, 0, 0, 0, 125, 0, -166, 0, 126, 127, 0, 128, 0, 0, 0, 129, 0, 0, 0, 130, 0, 0, 0, 0, -166, 0, 0, 0, 131, 0, 0, 0, -107, 0, 0, 0, 133, 134, 135, 0, 0, 0, 0, 0, 136, 0, 137, 138, 0, 0, 139, 0, 0, 0, 0, 0, 0, 0, 140, 0, 0, 0, 0, 141, 142, 0, 0, 143, 0, 0, 0, 0, 0, 0, 0, 0, 144, 0, 145, 146, 0, 0, 147, 0, 0, 0, 0, 0, 148};

static const yytype_int16 yycheck[] = {4, 97, 54, 13, 101, 13, 13, 103, 21, 44, 213, 224, 18, 19, 20, 21, 248, 34, 142, 58, 116, 253, 28, 89, 30, 287, 32, 33, 24, 35, 235, 97, 210, 39, 275, 64, 0, 43, 64, 65, 30, 49, 48, 130, 40, 90, 52, 76, 54, 45, 56, 30, 293, 79, 60, 61, 62, 127, 48, 125, 130, 48, 68, 241, 70, 71, 22, 84, 74, 331, 130, 53, 196, 133, 119, 127, 82, 201, 54, 55, 115, 87, 88, 122, 123, 91, 132, 133, 84, 26, 303, 18, 19, 20, 100, 300, 102, 103, 130, 130, 106, 56, 128, 200, 307, 337, 112, 18, 19, 20, 114, 18, 19, 20, 22, 111, 129, 64, 111, 45, 124, 127, 130, 219, 134, 135, 134, 134, 22, 127, 130, 130, 127, 51, 36, 34, 130, 75, 130, 72, 113, 130, 85, 131, 56, 9, 87, 72, 79, 135, 33, 56, 27, 130, 70, 130, 130, 2, 116, 331, 310, 300, 225, 259, 261, 262, 215, 147, 18, 19, 20, 21, 140, 232, 297, 328, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, 287, -1, 43, -1, -1, -1, -1, 48, -1, -1, 298, 52, -1, -1, 302, 56, -1, -1, -1, 60, 61, 62, -1, -1, -1, -1, -1, 68, -1,
    70, 71, -1, -1, 74, -1, -1, 231, -1, 326, -1, -1, 82, -1, -1, 331, -1, 87, 88, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, -1, -1, -1, -1, -1, 112, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 127, 18, -1, -1, 21, -1, 23, 24, -1, 26, 27, 28, 29, -1, -1, 32, 297, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, -1, 55, -1, 57, 58, 59, -1, 61, 62, 63, -1, -1, 66, -1, 68, 69, -1, 71, -1, 73, 74, -1, 76, 77, 78, -1, 80, 81, 82, 83, 84, -1, 86, -1, 88, 89, -1, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, -1, 117, 118, -1, -1, 121, 122, 123, -1, 125, 126, 18, -1, 20, -1, -1, 23, 24, -1, 26, 27, 28, 29, -1, -1, 32, -1, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1, 49, 50, 51, 52, 53, -1, 55, -1, 57, 58, 59, -1, 61, 62, 63, -1, -1, 66, -1, 68, 69, -1, 71, -1, 73, 74, -1, 76, 77, 78, -1, 80, 81, 82, 83, 84, -1, 86, -1, 88, 89, -1, 91,
    92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, -1, 117, 118, -1, -1, 121, 122, 123, -1, 125, 126, 18, 19, -1, -1, -1, 23, 24, -1, 26, 27, 28, 29, -1, -1, 32, -1, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1, 49, 50, 51, 52, 53, -1, 55, -1, 57, 58, 59, -1, 61, 62, 63, -1, -1, 66, -1, 68, 69, -1, 71, -1, 73, 74, -1, 76, 77, 78, -1, 80, 81, 82, 83, 84, -1, 86, -1, 88, 89, -1, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, -1, 117, 118, -1, -1, 121, 122, 123, -1, 125, 126, 18, 19, -1, -1, -1, 23, 24, -1, 26, 27, 28, 29, -1, -1, 32, -1, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1, 49, 50, 51, 52, 53, -1, 55, -1, 57, 58, 59, -1, 61, 62, 63, -1, -1, 66, -1, 68, 69, -1, 71, -1, 73, 74, -1, 76, 77, 78, -1, 80, 81, 82, 83, 84, -1, 86, -1, 88, 89, -1, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
    109, 110, 111, 112, 113, 114, 115, -1, 117, 118, -1, -1, 121, 122, 123, 18, 125, 126, -1, -1, 23, 24, -1, 26, 27, 28, 29, -1, -1, 32, -1, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1, 49, 50, 51, 52, 53, -1, 55, -1, 57, 58, 59, -1, 61, 62, 63, -1, -1, 66, -1, 68, 69, -1, 71, -1, 73, 74, -1, 76, 77, 78, -1, 80, 81, 82, 83, 84, -1, 86, -1, 88, 89, -1, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, -1, 117, 118, -1, -1, 121, 122, 123, 18, 125, 126, -1, -1, 23, 24, -1, 26, 27, 28, 29, -1, -1, 32, -1, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1, 49, 50, 51, 52, 53, -1, 55, -1, 57, 58, 59, -1, 61, 62, 63, -1, -1, 66, -1, 68, 69, -1, 71, -1, 73, 74, -1, 76, 77, 78, -1, 80, 81, 82, 83, 84, -1, 86, -1, 88, 89, -1, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, -1, 117, 118, -1, -1, 121, 122, 123, -1, 125, 126, 18, 19, 20, 21,
    -1, -1, -1, -1, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, -1, -1, 43, -1, -1, -1, -1, 48, -1, -1, -1, 52, -1, 54, 55, 56, -1, -1, -1, 60, 61, 62, -1, -1, -1, -1, -1, 68, -1, 70, 71, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, 87, 88, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, 18, 19, 20, 21, -1, 112, -1, -1, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, -1, -1, 43, -1, -1, -1, -1, 48, -1, -1, -1, 52, -1, 54, 55, 56, -1, -1, -1, 60, 61, 62, -1, -1, -1, -1, -1, 68, -1, 70, 71, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, 87, 88, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, 18, 19, 20, 21, -1, 112, -1, -1, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, -1, -1, 43, -1, -1, -1, -1, 48, -1, -1, -1, 52, -1, -1, -1, 56, -1, -1, 59, 60, 61, 62, -1, -1, -1, -1, -1, 68, -1, 70, 71, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, 87, 88, -1,
    -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, 18, 19, 20, 21, -1, 112, -1, -1, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, -1, -1, 43, -1, -1, -1, -1, 48, -1, -1, -1, 52, -1, -1, -1, 56, -1, -1, -1, 60, 61, 62, -1, -1, -1, -1, -1, 68, -1, 70, 71, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, 87, 88, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, 18, 19, 20, 21, -1, 112, -1, -1, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, -1, -1, 43, -1, -1, -1, -1, 48, -1, -1, -1, 52, -1, -1, -1, 56, -1, -1, -1, 60, 61, 62, -1, -1, -1, -1, -1, 68, -1, 70, 71, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, 87, 88, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, 18, 19, 20, 21, -1, 112, -1, -1, -1, -1, 28, -1, 30, -1, 32, 33, -1, 35, -1, -1, -1, 39, -1, -1, -1, 43, -1, -1, -1, -1, 48, -1, -1, -1, 52, -1, -1, -1, 56, -1, -1, -1, 60, 61, 62, -1, -1, -1, -1, -1,
    68, -1, 70, 71, -1, -1, 74, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, -1, 87, 88, -1, -1, 91, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 102, 103, -1, -1, 106, -1, -1, -1, -1, -1, 112};

                                                               
                                 
static const yytype_uint8 yystos[] = {0, 137, 138, 0, 21, 129, 139, 142, 143, 220, 18, 20, 23, 24, 26, 27, 28, 29, 32, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 50, 51, 52, 53, 55, 57, 58, 59, 61, 62, 63, 66, 68, 69, 71, 73, 74, 76, 77, 78, 80, 81, 82, 83, 84, 86, 88, 89, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 117, 118, 121, 122, 123, 125, 126, 224, 225, 89, 97, 125, 130, 141, 30, 48, 144, 22, 53, 18, 140, 225, 58, 122, 123, 164, 18, 21, 48, 145, 146, 147, 156, 225, 18, 19, 20, 21, 28, 32, 33, 35, 39, 43, 52, 59, 60, 61, 62, 68, 70, 71, 74, 82, 87, 88, 91, 100, 102, 103, 106, 112, 142, 165, 166, 167, 168, 169, 175, 176, 179, 184, 185, 186, 189, 191, 192, 193, 194, 195, 197, 198, 199, 200, 201, 203, 204, 205, 206, 208, 210, 221, 224, 146, 24, 40, 84, 111, 149, 157, 224, 180, 18, 19, 20, 209, 26, 207, 211, 202, 44, 115, 170, 218, 202, 130, 209, 216, 207, 130, 13, 134, 135, 163, 222, 224, 56,
    64, 65, 79, 128, 22, 64, 111, 45, 158, 22, 127, 181, 182, 130, 34, 84, 130, 127, 212, 213, 209, 51, 164, 209, 130, 217, 216, 127, 130, 223, 222, 18, 19, 20, 187, 188, 188, 164, 196, 219, 18, 19, 155, 225, 148, 36, 159, 218, 54, 182, 183, 34, 214, 215, 224, 213, 75, 18, 19, 171, 172, 174, 175, 177, 130, 216, 196, 72, 113, 190, 56, 196, 130, 131, 151, 18, 19, 225, 85, 160, 164, 164, 56, 90, 119, 130, 133, 163, 54, 55, 178, 9, 72, 79, 152, 153, 156, 64, 76, 154, 87, 49, 130, 161, 162, 163, 33, 215, 164, 172, 173, 164, 218, 56, 27, 222, 132, 133, 158, 150, 130, 164, 70, 219, 130, 153, 130, 196};

                                                                  
static const yytype_uint8 yyr1[] = {0, 136, 137, 138, 138, 139, 139, 139, 139, 139, 140, 140, 141, 141, 142, 143, 143, 143, 144, 145, 145, 146, 146, 146, 147, 147, 148, 147, 149, 149, 149, 150, 151, 151, 152, 152, 153, 154, 154, 155, 155, 155, 156, 156, 157, 157, 158, 159, 159, 159, 159, 160, 160, 161, 161, 162, 162, 163, 163, 164, 164, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 166, 167, 167, 168, 169, 170, 170, 170, 171, 171, 172, 173, 174, 174, 174, 175, 175, 176, 177, 177, 178, 178, 179, 180, 181, 181, 182, 183, 183, 184, 185, 186, 187, 188, 188, 188, 189, 190, 190, 191, 192, 192, 193, 194, 195, 196, 197, 197, 197, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 207, 207, 208, 208, 209, 209, 209, 210, 211, 210, 212, 212, 213, 214, 214, 215, 216, 217, 218, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224, 224, 224, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
    225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225};

                                                                         
static const yytype_int8 yyr2[] = {0, 2, 3, 0, 2, 3, 3, 3, 3, 3, 1, 1, 0, 1, 6, 1, 2, 3, 1, 2, 1, 1, 1, 3, 6, 5, 0, 7, 0, 2, 1, 0, 0, 3, 1, 3, 2, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 2, 2, 2, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 3, 5, 0, 1, 1, 3, 1, 3, 0, 1, 1, 1, 1, 3, 8, 0, 4, 0, 2, 7, 0, 2, 1, 3, 0, 2, 3, 4, 4, 2, 1, 1, 1, 8, 0, 2, 3, 1, 1, 1, 1, 1, 5, 1, 1, 1, 1, 1, 2, 4, 4, 0, 3, 2, 3, 3, 2, 3, 0, 1, 1, 1, 1, 1, 0, 0, 3, 2, 1, 4, 3, 1, 1, 0, 0, 0, 0, 0, 3, 0, 3, 0, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

enum
{
  YYENOMEM = -2
};

#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = YYEMPTY)

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab

#define YYRECOVERING() (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (yychar == YYEMPTY)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      yychar = (Token);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      yylval = (Value);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      YYPOPSTACK(yylen);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      yystate = *yyssp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      goto yybackup;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      yyerror(YY_("syntax error: cannot back up"));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      YYERROR;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  while (0)

                                                      
                             
#define YYERRCODE YYUNDEF

                                                                
                                                                
                                                    

#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(Current, Rhs, N)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (N)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      (Current).first_line = YYRHSLOC(Rhs, 1).first_line;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      (Current).first_column = YYRHSLOC(Rhs, 1).first_column;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      (Current).last_line = YYRHSLOC(Rhs, N).last_line;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      (Current).last_column = YYRHSLOC(Rhs, N).last_column;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      (Current).first_line = (Current).last_line = YYRHSLOC(Rhs, 0).last_line;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      (Current).first_column = (Current).last_column = YYRHSLOC(Rhs, 0).last_column;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])

                                     
#if YYDEBUG

#ifndef YYFPRINTF
#include <stdio.h>                                   
#define YYFPRINTF fprintf
#endif

#define YYDPRINTF(Args)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      YYFPRINTF Args;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                                          
                                                                  
                                                                      

#ifndef YY_LOCATION_PRINT
#if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

                                                                   

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_(FILE *yyo, YYLTYPE const *const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
  {
    res += YYFPRINTF(yyo, "%d", yylocp->first_line);
    if (0 <= yylocp->first_column)
    {
      res += YYFPRINTF(yyo, ".%d", yylocp->first_column);
    }
  }
  if (0 <= yylocp->last_line)
  {
    if (yylocp->first_line < yylocp->last_line)
    {
      res += YYFPRINTF(yyo, "-%d", yylocp->last_line);
      if (0 <= end_col)
      {
        res += YYFPRINTF(yyo, ".%d", end_col);
      }
    }
    else if (0 <= end_col && yylocp->first_column < end_col)
    {
      res += YYFPRINTF(yyo, "-%d", end_col);
    }
  }
  return res;
}

#define YY_LOCATION_PRINT(File, Loc) yy_location_print_(File, &(Loc))

#else
#define YY_LOCATION_PRINT(File, Loc) ((void)0)
#endif
#endif                                 

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      YYFPRINTF(stderr, "%s ", Title);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      yy_symbol_print(stderr, Kind, Value, Location);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      YYFPRINTF(stderr, "\n");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                       
                                       
                                      

static void
yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep, YYLTYPE const *const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE(yyoutput);
  YY_USE(yylocationp);
  if (!yyvaluep)
  {
    return;
  }
#ifdef YYPRINT
  if (yykind < YYNTOKENS)
  {
    YYPRINT(yyo, yytoknum[yykind], *yyvaluep);
  }
#endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

                               
                               
                              

static void
yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep, YYLTYPE const *const yylocationp)
{
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name(yykind));

  YY_LOCATION_PRINT(yyo, *yylocationp);
  YYFPRINTF(yyo, ": ");
  yy_symbol_value_print(yyo, yykind, yyvaluep, yylocationp);
  YYFPRINTF(yyo, ")");
}

                                                                      
                                                                      
                                                                      
                                                                     

static void
yy_stack_print(yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF(stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
  {
    int yybot = *yybottom;
    YYFPRINTF(stderr, " %d", yybot);
  }
  YYFPRINTF(stderr, "\n");
}

#define YY_STACK_PRINT(Bottom, Top)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      yy_stack_print((Bottom), (Top));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

                                                    
                                                    
                                                   

static void
yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1, yylno);
                                   
  for (yyi = 0; yyi < yynrhs; yyi++)
  {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]), &yyvsp[(yyi + 1) - (yynrhs)], &(yylsp[(yyi + 1) - (yynrhs)]));
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      yy_reduce_print(yyssp, yyvsp, yylsp, Rule);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

                                                                      
                                    
int yydebug;
#else               
#define YYDPRINTF(Args) ((void)0)
#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
#define YY_STACK_PRINT(Bottom, Top)
#define YY_REDUCE_PRINT(Rule)
#endif               

                                                          
#ifndef YYINITDEPTH
#define YYINITDEPTH 200
#endif

                                                                     
                                                    
 
                                                                  
                                                      
                                                            

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

                                                   
                                                   
                                                  

static void
yydestruct(const char *yymsg, yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE(yyvaluep);
  YY_USE(yylocationp);
  if (!yymsg)
  {
    yymsg = "Deleting";
  }
  YY_SYMBOL_PRINT(yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

                            
int yychar;

                                                  
YYSTYPE yylval;
                                              
YYLTYPE yylloc
#if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
    = {1, 1, 1, 1}
#endif
;
                                      
int yynerrs;

              
              
             

int
yyparse(void)
{
  yy_state_fast_t yystate = 0;
                                                                 
  int yyerrstatus = 0;

                                                                        
                                      

                    
  YYPTRDIFF_T yystacksize = YYINITDEPTH;

                                             
  yy_state_t yyssa[YYINITDEPTH];
  yy_state_t *yyss = yyssa;
  yy_state_t *yyssp = yyss;

                                                      
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp = yyvs;

                                                
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp = yyls;

  int yyn;
                                     
  int yyresult;
                               
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
                                                                       
                         
  YYSTYPE yyval;
  YYLTYPE yyloc;

                                                         
  YYLTYPE yyerror_range[3];

#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

                                                           
                                                      
  int yylen = 0;

  YYDPRINTF((stderr, "Starting parse\n"));

  yychar = YYEMPTY;                                 
  yylsp[0] = yylloc;
  goto yysetstate;

                                                                
                                                                
                                                               
yynewstate:
                                                                    
                                                                          
  yyssp++;

                                                                        
                                                                        
                                                                       
yysetstate:
  YYDPRINTF((stderr, "Entering state %d\n", yystate));
  YY_ASSERT(0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST(yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT(yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
  {
                                                                      
    YYPTRDIFF_T yysize = yyssp - yyss + 1;

#if defined yyoverflow
    {
                                                                    
                                                              
                    
      yy_state_t *yyss1 = yyss;
      YYSTYPE *yyvs1 = yyvs;
      YYLTYPE *yyls1 = yyls;

                                                                   
                                                                 
                                                                    
                                                   
      yyoverflow(YY_("memory exhausted"), &yyss1, yysize * YYSIZEOF(*yyssp), &yyvs1, yysize * YYSIZEOF(*yyvsp), &yyls1, yysize * YYSIZEOF(*yylsp), &yystacksize);
      yyss = yyss1;
      yyvs = yyvs1;
      yyls = yyls1;
    }
#else                               
                                        
    if (YYMAXDEPTH <= yystacksize)
    {
      goto yyexhaustedlab;
    }
    yystacksize *= 2;
    if (YYMAXDEPTH < yystacksize)
    {
      yystacksize = YYMAXDEPTH;
    }

    {
      yy_state_t *yyss1 = yyss;
      union yyalloc *yyptr = YY_CAST(union yyalloc *, YYSTACK_ALLOC(YY_CAST(YYSIZE_T, YYSTACK_BYTES(yystacksize))));
      if (!yyptr)
      {
        goto yyexhaustedlab;
      }
      YYSTACK_RELOCATE(yyss_alloc, yyss);
      YYSTACK_RELOCATE(yyvs_alloc, yyvs);
      YYSTACK_RELOCATE(yyls_alloc, yyls);
#undef YYSTACK_RELOCATE
      if (yyss1 != yyssa)
      {
        YYSTACK_FREE(yyss1);
      }
    }
#endif

    yyssp = yyss + yysize - 1;
    yyvsp = yyvs + yysize - 1;
    yylsp = yyls + yysize - 1;

    YY_IGNORE_USELESS_CAST_BEGIN
    YYDPRINTF((stderr, "Stack size increased to %ld\n", YY_CAST(long, yystacksize)));
    YY_IGNORE_USELESS_CAST_END

    if (yyss + yystacksize - 1 <= yyssp)
    {
      YYABORT;
    }
  }
#endif                                                       

  if (yystate == YYFINAL)
  {
    YYACCEPT;
  }

  goto yybackup;

               
               
              
yybackup:
                                                                
                                                                   

                                                                             
  yyn = yypact[yystate];
  if (yypact_value_is_default(yyn))
  {
    goto yydefault;
  }

                                                                      

                                                                       
  if (yychar == YYEMPTY)
  {
    YYDPRINTF((stderr, "Reading a token\n"));
    yychar = yylex();
  }

  if (yychar <= YYEOF)
  {
    yychar = YYEOF;
    yytoken = YYSYMBOL_YYEOF;
    YYDPRINTF((stderr, "Now at end of input.\n"));
  }
  else if (yychar == YYerror)
  {
                                                                     
                                                              
                                                                  
                                 
    yychar = YYUNDEF;
    yytoken = YYSYMBOL_YYerror;
    yyerror_range[1] = yylloc;
    goto yyerrlab1;
  }
  else
  {
    yytoken = YYTRANSLATE(yychar);
    YY_SYMBOL_PRINT("Next token is", yytoken, &yylval, &yylloc);
  }

                                                                     
                                           
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
  {
    goto yydefault;
  }
  yyn = yytable[yyn];
  if (yyn <= 0)
  {
    if (yytable_value_is_error(yyn))
    {
      goto yyerrlab;
    }
    yyn = -yyn;
    goto yyreduce;
  }

                                                                   
                
  if (yyerrstatus)
  {
    yyerrstatus--;
  }

                                   
  YY_SYMBOL_PRINT("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

                                   
  yychar = YYEMPTY;
  goto yynewstate;

                                                               
                                                               
                                                              
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
  {
    goto yyerrlab;
  }
  goto yyreduce;

                                 
                                 
                                
yyreduce:
                                                    
  yylen = yyr2[yyn];

                                                                     
                
 
                                                          
                                             
                                                        
                                                                     
                                                          
  yyval = yyvsp[1 - yylen];

                         
  YYLLOC_DEFAULT(yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT(yyn);
  switch (yyn)
  {
  case 2:                                                   
#line 364 "pl_gram.y"
  {
    plpgsql_parse_result = (PLpgSQL_stmt_block *)(yyvsp[-1].stmt);
  }
#line 2055 "pl_gram.c"
  break;

  case 5:                                        
#line 374 "pl_gram.y"
  {
    plpgsql_DumpExecTree = true;
  }
#line 2063 "pl_gram.c"
  break;

  case 6:                                                           
#line 378 "pl_gram.y"
  {
    if (strcmp((yyvsp[0].str), "on") == 0)
    {
      plpgsql_curr_compile->print_strict_params = true;
    }
    else if (strcmp((yyvsp[0].str), "off") == 0)
    {
      plpgsql_curr_compile->print_strict_params = false;
    }
    else
    {
      elog(ERROR, "unrecognized print_strict_params option %s", (yyvsp[0].str));
    }
  }
#line 2076 "pl_gram.c"
  break;

  case 7:                                                    
#line 387 "pl_gram.y"
  {
    plpgsql_curr_compile->resolve_option = PLPGSQL_RESOLVE_ERROR;
  }
#line 2084 "pl_gram.c"
  break;

  case 8:                                                           
#line 391 "pl_gram.y"
  {
    plpgsql_curr_compile->resolve_option = PLPGSQL_RESOLVE_VARIABLE;
  }
#line 2092 "pl_gram.c"
  break;

  case 9:                                                         
#line 395 "pl_gram.y"
  {
    plpgsql_curr_compile->resolve_option = PLPGSQL_RESOLVE_COLUMN;
  }
#line 2100 "pl_gram.c"
  break;

  case 10:                            
#line 401 "pl_gram.y"
  {
    (yyval.str) = (yyvsp[0].word).ident;
  }
#line 2108 "pl_gram.c"
  break;

  case 11:                                        
#line 405 "pl_gram.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].keyword));
  }
#line 2116 "pl_gram.c"
  break;

  case 14:                                                                            
#line 414 "pl_gram.y"
  {
    PLpgSQL_stmt_block *new;

    new = palloc0(sizeof(PLpgSQL_stmt_block));

    new->cmd_type = PLPGSQL_STMT_BLOCK;
    new->lineno = plpgsql_location_to_lineno((yylsp[-4]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->label = (yyvsp[-5].declhdr).label;
    new->n_initvars = (yyvsp[-5].declhdr).n_initvars;
    new->initvarnos = (yyvsp[-5].declhdr).initvarnos;
    new->body = (yyvsp[-3].list);
    new->exceptions = (yyvsp[-2].exception_block);

    check_labels((yyvsp[-5].declhdr).label, (yyvsp[0].str), (yylsp[0]));
    plpgsql_ns_pop();

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 2140 "pl_gram.c"
  break;

  case 15:                                  
#line 437 "pl_gram.y"
  {
                                                      
    plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
    (yyval.declhdr).label = (yyvsp[0].str);
    (yyval.declhdr).n_initvars = 0;
    (yyval.declhdr).initvarnos = NULL;
  }
#line 2152 "pl_gram.c"
  break;

  case 16:                                             
#line 445 "pl_gram.y"
  {
    plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
    (yyval.declhdr).label = (yyvsp[-1].str);
    (yyval.declhdr).n_initvars = 0;
    (yyval.declhdr).initvarnos = NULL;
  }
#line 2163 "pl_gram.c"
  break;

  case 17:                                                        
#line 452 "pl_gram.y"
  {
    plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
    (yyval.declhdr).label = (yyvsp[-2].str);
                                                   
    (yyval.declhdr).n_initvars = plpgsql_add_initdatums(&((yyval.declhdr).initvarnos));
  }
#line 2174 "pl_gram.c"
  break;

  case 18:                             
#line 461 "pl_gram.y"
  {
                                                   
    plpgsql_add_initdatums(NULL);
       
                                                   
                                 
       
    plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_DECLARE;
  }
#line 2188 "pl_gram.c"
  break;

  case 22:                            
#line 478 "pl_gram.y"
  {
                                         
  }
#line 2196 "pl_gram.c"
  break;

  case 23:                                                           
#line 482 "pl_gram.y"
  {
       
                                                        
                                                           
       
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("block label must be placed before DECLARE, not after"), parser_errposition((yylsp[-2]))));
  }
#line 2211 "pl_gram.c"
  break;

  case 24:                                                                                                   
#line 495 "pl_gram.y"
  {
    PLpgSQL_variable *var;

       
                                                      
                                                         
                                                    
                  
       
    if (OidIsValid((yyvsp[-2].oid)))
    {
      if (!OidIsValid((yyvsp[-3].dtype)->collation))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be((yyvsp[-3].dtype)->typoid)), parser_errposition((yylsp[-2]))));
      }
      (yyvsp[-3].dtype)->collation = (yyvsp[-2].oid);
    }

    var = plpgsql_build_variable((yyvsp[-5].varname).name, (yyvsp[-5].varname).lineno, (yyvsp[-3].dtype), true);
    var->isconst = (yyvsp[-4].boolean);
    var->notnull = (yyvsp[-1].boolean);
    var->default_val = (yyvsp[0].expr);

       
                                                          
                                                       
       
    if (var->notnull && var->default_val == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("variable \"%s\" must have a default value, since it's declared NOT NULL", var->refname), parser_errposition((yylsp[-1]))));
    }
  }
#line 2253 "pl_gram.c"
  break;

  case 25:                                                                     
#line 533 "pl_gram.y"
  {
    plpgsql_ns_additem((yyvsp[-1].nsitem)->itemtype, (yyvsp[-1].nsitem)->itemno, (yyvsp[-4].varname).name);
  }
#line 2262 "pl_gram.c"
  break;

  case 26:                   
#line 538 "pl_gram.y"
  {
    plpgsql_ns_push((yyvsp[-2].varname).name, PLPGSQL_LABEL_OTHER);
  }
#line 2268 "pl_gram.c"
  break;

  case 27:                                                                                                               
#line 540 "pl_gram.y"
  {
    PLpgSQL_var *new;
    PLpgSQL_expr *curname_def;
    char buf[1024];
    char *cp1;
    char *cp2;

                                             
    plpgsql_ns_pop();

    new = (PLpgSQL_var *)plpgsql_build_variable((yyvsp[-6].varname).name, (yyvsp[-6].varname).lineno, plpgsql_build_datatype(REFCURSOROID, -1, InvalidOid, NULL), true);

    curname_def = palloc0(sizeof(PLpgSQL_expr));

    strcpy(buf, "SELECT ");
    cp1 = new->refname;
    cp2 = buf + strlen(buf);
       
                                                     
                                                 
       
    if (strchr(cp1, '\\') != NULL)
    {
      *cp2++ = ESCAPE_STRING_SYNTAX;
    }
    *cp2++ = '\'';
    while (*cp1)
    {
      if (SQL_STR_DOUBLE(*cp1, true))
      {
        *cp2++ = *cp1;
      }
      *cp2++ = *cp1++;
    }
    strcpy(cp2, "'::pg_catalog.refcursor");
    curname_def->query = pstrdup(buf);
    new->default_val = curname_def;

    new->cursor_explicit_expr = (yyvsp[0].expr);
    if ((yyvsp[-2].datum) == NULL)
    {
      new->cursor_explicit_argrow = -1;
    }
    else
    {
      new->cursor_explicit_argrow = (yyvsp[-2].datum)->dno;
    }
    new->cursor_options = CURSOR_OPT_FAST_PLAN | (yyvsp[-5].ival);
  }
#line 2320 "pl_gram.c"
  break;

  case 28:                              
#line 590 "pl_gram.y"
  {
    (yyval.ival) = 0;
  }
#line 2328 "pl_gram.c"
  break;

  case 29:                                     
#line 594 "pl_gram.y"
  {
    (yyval.ival) = CURSOR_OPT_NO_SCROLL;
  }
#line 2336 "pl_gram.c"
  break;

  case 30:                                
#line 598 "pl_gram.y"
  {
    (yyval.ival) = CURSOR_OPT_SCROLL;
  }
#line 2344 "pl_gram.c"
  break;

  case 31:                                 
#line 604 "pl_gram.y"
  {
    (yyval.expr) = read_sql_stmt("");
  }
#line 2352 "pl_gram.c"
  break;

  case 32:                                
#line 610 "pl_gram.y"
  {
    (yyval.datum) = NULL;
  }
#line 2360 "pl_gram.c"
  break;

  case 33:                                                     
#line 614 "pl_gram.y"
  {
    PLpgSQL_row *new;
    int i;
    ListCell *l;

    new = palloc0(sizeof(PLpgSQL_row));
    new->dtype = PLPGSQL_DTYPE_ROW;
    new->refname = "(unnamed row)";
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->rowtupdesc = NULL;
    new->nfields = list_length((yyvsp[-1].list));
    new->fieldnames = palloc(new->nfields * sizeof(char *));
    new->varnos = palloc(new->nfields * sizeof(int));

    i = 0;
    foreach (l, (yyvsp[-1].list))
    {
      PLpgSQL_variable *arg = (PLpgSQL_variable *)lfirst(l);
      Assert(!arg->isconst);
      new->fieldnames[i] = arg->refname;
      new->varnos[i] = arg->dno;
      i++;
    }
    list_free((yyvsp[-1].list));

    plpgsql_adddatum((PLpgSQL_datum *)new);
    (yyval.datum) = (PLpgSQL_datum *)new;
  }
#line 2393 "pl_gram.c"
  break;

  case 34:                                            
#line 645 "pl_gram.y"
  {
    (yyval.list) = list_make1((yyvsp[0].datum));
  }
#line 2401 "pl_gram.c"
  break;

  case 35:                                                                    
#line 649 "pl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].datum));
  }
#line 2409 "pl_gram.c"
  break;

  case 36:                                                   
#line 655 "pl_gram.y"
  {
    (yyval.datum) = (PLpgSQL_datum *)plpgsql_build_variable((yyvsp[-1].varname).name, (yyvsp[-1].varname).lineno, (yyvsp[0].dtype), true);
  }
#line 2419 "pl_gram.c"
  break;

  case 39:                              
#line 666 "pl_gram.y"
  {
    PLpgSQL_nsitem *nsi;

    nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false, (yyvsp[0].word).ident, NULL, NULL, NULL);
    if (nsi == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("variable \"%s\" does not exist", (yyvsp[0].word).ident), parser_errposition((yylsp[0]))));
    }
    (yyval.nsitem) = nsi;
  }
#line 2438 "pl_gram.c"
  break;

  case 40:                                          
#line 681 "pl_gram.y"
  {
    PLpgSQL_nsitem *nsi;

    nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false, (yyvsp[0].keyword), NULL, NULL, NULL);
    if (nsi == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("variable \"%s\" does not exist", (yyvsp[0].keyword)), parser_errposition((yylsp[0]))));
    }
    (yyval.nsitem) = nsi;
  }
#line 2457 "pl_gram.c"
  break;

  case 41:                               
#line 696 "pl_gram.y"
  {
    PLpgSQL_nsitem *nsi;

    if (list_length((yyvsp[0].cword).idents) == 2)
    {
      nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false, strVal(linitial((yyvsp[0].cword).idents)), strVal(lsecond((yyvsp[0].cword).idents)), NULL, NULL);
    }
    else if (list_length((yyvsp[0].cword).idents) == 3)
    {
      nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false, strVal(linitial((yyvsp[0].cword).idents)), strVal(lsecond((yyvsp[0].cword).idents)), strVal(lthird((yyvsp[0].cword).idents)), NULL);
    }
    else
    {
      nsi = NULL;
    }
    if (nsi == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("variable \"%s\" does not exist", NameListToString((yyvsp[0].cword).idents)), parser_errposition((yylsp[0]))));
    }
    (yyval.nsitem) = nsi;
  }
#line 2487 "pl_gram.c"
  break;

  case 42:                            
#line 724 "pl_gram.y"
  {
    (yyval.varname).name = (yyvsp[0].word).ident;
    (yyval.varname).lineno = plpgsql_location_to_lineno((yylsp[0]));
       
                                                      
                             
       
    if (plpgsql_ns_lookup(plpgsql_ns_top(), true, (yyvsp[0].word).ident, NULL, NULL, NULL) != NULL)
    {
      yyerror("duplicate declaration");
    }

    if (plpgsql_curr_compile->extra_warnings & PLPGSQL_XCHECK_SHADOWVAR || plpgsql_curr_compile->extra_errors & PLPGSQL_XCHECK_SHADOWVAR)
    {
      PLpgSQL_nsitem *nsi;
      nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false, (yyvsp[0].word).ident, NULL, NULL, NULL);
      if (nsi != NULL)
      {
        ereport(plpgsql_curr_compile->extra_errors & PLPGSQL_XCHECK_SHADOWVAR ? ERROR : WARNING, (errcode(ERRCODE_DUPLICATE_ALIAS), errmsg("variable \"%s\" shadows a previously defined variable", (yyvsp[0].word).ident), parser_errposition((yylsp[0]))));
      }
    }
  }
#line 2519 "pl_gram.c"
  break;

  case 43:                                        
#line 752 "pl_gram.y"
  {
    (yyval.varname).name = pstrdup((yyvsp[0].keyword));
    (yyval.varname).lineno = plpgsql_location_to_lineno((yylsp[0]));
       
                                                      
                             
       
    if (plpgsql_ns_lookup(plpgsql_ns_top(), true, (yyvsp[0].keyword), NULL, NULL, NULL) != NULL)
    {
      yyerror("duplicate declaration");
    }

    if (plpgsql_curr_compile->extra_warnings & PLPGSQL_XCHECK_SHADOWVAR || plpgsql_curr_compile->extra_errors & PLPGSQL_XCHECK_SHADOWVAR)
    {
      PLpgSQL_nsitem *nsi;
      nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false, (yyvsp[0].keyword), NULL, NULL, NULL);
      if (nsi != NULL)
      {
        ereport(plpgsql_curr_compile->extra_errors & PLPGSQL_XCHECK_SHADOWVAR ? ERROR : WARNING, (errcode(ERRCODE_DUPLICATE_ALIAS), errmsg("variable \"%s\" shadows a previously defined variable", (yyvsp[0].keyword)), parser_errposition((yylsp[0]))));
      }
    }
  }
#line 2551 "pl_gram.c"
  break;

  case 44:                          
#line 782 "pl_gram.y"
  {
    (yyval.boolean) = false;
  }
#line 2557 "pl_gram.c"
  break;

  case 45:                              
#line 784 "pl_gram.y"
  {
    (yyval.boolean) = true;
  }
#line 2563 "pl_gram.c"
  break;

  case 46:                             
#line 788 "pl_gram.y"
  {
       
                                                   
                          
       
    (yyval.dtype) = read_datatype(yychar);
    yyclearin;
  }
#line 2576 "pl_gram.c"
  break;

  case 47:                            
#line 799 "pl_gram.y"
  {
    (yyval.oid) = InvalidOid;
  }
#line 2582 "pl_gram.c"
  break;

  case 48:                                      
#line 801 "pl_gram.y"
  {
    (yyval.oid) = get_collation_oid(list_make1(makeString((yyvsp[0].word).ident)), false);
  }
#line 2591 "pl_gram.c"
  break;

  case 49:                                                  
#line 806 "pl_gram.y"
  {
    (yyval.oid) = get_collation_oid(list_make1(makeString(pstrdup((yyvsp[0].keyword)))), false);
  }
#line 2600 "pl_gram.c"
  break;

  case 50:                                       
#line 811 "pl_gram.y"
  {
    (yyval.oid) = get_collation_oid((yyvsp[0].cword).idents, false);
  }
#line 2608 "pl_gram.c"
  break;

  case 51:                            
#line 817 "pl_gram.y"
  {
    (yyval.boolean) = false;
  }
#line 2614 "pl_gram.c"
  break;

  case 52:                                  
#line 819 "pl_gram.y"
  {
    (yyval.boolean) = true;
  }
#line 2620 "pl_gram.c"
  break;

  case 53:                        
#line 823 "pl_gram.y"
  {
    (yyval.expr) = NULL;
  }
#line 2626 "pl_gram.c"
  break;

  case 54:                                
#line 825 "pl_gram.y"
  {
    (yyval.expr) = read_sql_expression(';', ";");
  }
#line 2634 "pl_gram.c"
  break;

  case 59:                         
#line 844 "pl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 2640 "pl_gram.c"
  break;

  case 60:                                      
#line 846 "pl_gram.y"
  {
                                                        
    if ((yyvsp[0].stmt) == NULL)
    {
      (yyval.list) = (yyvsp[-1].list);
    }
    else
    {
      (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].stmt));
    }
  }
#line 2652 "pl_gram.c"
  break;

  case 61:                               
#line 856 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[-1].stmt);
  }
#line 2658 "pl_gram.c"
  break;

  case 62:                              
#line 858 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2664 "pl_gram.c"
  break;

  case 63:                          
#line 860 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2670 "pl_gram.c"
  break;

  case 64:                            
#line 862 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2676 "pl_gram.c"
  break;

  case 65:                            
#line 864 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2682 "pl_gram.c"
  break;

  case 66:                             
#line 866 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2688 "pl_gram.c"
  break;

  case 67:                           
#line 868 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2694 "pl_gram.c"
  break;

  case 68:                                 
#line 870 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2700 "pl_gram.c"
  break;

  case 69:                            
#line 872 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2706 "pl_gram.c"
  break;

  case 70:                              
#line 874 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2712 "pl_gram.c"
  break;

  case 71:                             
#line 876 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2718 "pl_gram.c"
  break;

  case 72:                              
#line 878 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2724 "pl_gram.c"
  break;

  case 73:                               
#line 880 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2730 "pl_gram.c"
  break;

  case 74:                                  
#line 882 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2736 "pl_gram.c"
  break;

  case 75:                               
#line 884 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2742 "pl_gram.c"
  break;

  case 76:                            
#line 886 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2748 "pl_gram.c"
  break;

  case 77:                               
#line 888 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2754 "pl_gram.c"
  break;

  case 78:                            
#line 890 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2760 "pl_gram.c"
  break;

  case 79:                             
#line 892 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2766 "pl_gram.c"
  break;

  case 80:                            
#line 894 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2772 "pl_gram.c"
  break;

  case 81:                             
#line 896 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2778 "pl_gram.c"
  break;

  case 82:                            
#line 898 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2784 "pl_gram.c"
  break;

  case 83:                              
#line 900 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2790 "pl_gram.c"
  break;

  case 84:                                
#line 902 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2796 "pl_gram.c"
  break;

  case 85:                           
#line 904 "pl_gram.y"
  {
    (yyval.stmt) = (yyvsp[0].stmt);
  }
#line 2802 "pl_gram.c"
  break;

  case 86:                                               
#line 908 "pl_gram.y"
  {
    PLpgSQL_stmt_perform *new;

    new = palloc0(sizeof(PLpgSQL_stmt_perform));
    new->cmd_type = PLPGSQL_STMT_PERFORM;
    new->lineno = plpgsql_location_to_lineno((yylsp[-1]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->expr = (yyvsp[0].expr);

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 2818 "pl_gram.c"
  break;

  case 87:                         
#line 922 "pl_gram.y"
  {
    PLpgSQL_stmt_call *new;

    new = palloc0(sizeof(PLpgSQL_stmt_call));
    new->cmd_type = PLPGSQL_STMT_CALL;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->expr = read_sql_stmt("CALL ");
    new->is_call = true;

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 2836 "pl_gram.c"
  break;

  case 88:                       
#line 936 "pl_gram.y"
  {
                                                             
    PLpgSQL_stmt_call *new;

    new = palloc0(sizeof(PLpgSQL_stmt_call));
    new->cmd_type = PLPGSQL_STMT_CALL;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->expr = read_sql_stmt("DO ");
    new->is_call = false;

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 2855 "pl_gram.c"
  break;

  case 89:                                                               
#line 953 "pl_gram.y"
  {
    PLpgSQL_stmt_assign *new;

    new = palloc0(sizeof(PLpgSQL_stmt_assign));
    new->cmd_type = PLPGSQL_STMT_ASSIGN;
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->varno = (yyvsp[-2].datum)->dno;
    new->expr = (yyvsp[0].expr);

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 2872 "pl_gram.c"
  break;

  case 90:                                                                           
#line 968 "pl_gram.y"
  {
    PLpgSQL_stmt_getdiag *new;
    ListCell *lc;

    new = palloc0(sizeof(PLpgSQL_stmt_getdiag));
    new->cmd_type = PLPGSQL_STMT_GETDIAG;
    new->lineno = plpgsql_location_to_lineno((yylsp[-4]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->is_stacked = (yyvsp[-3].boolean);
    new->diag_items = (yyvsp[-1].list);

       
                                                          
       
    foreach (lc, new->diag_items)
    {
      PLpgSQL_diag_item *ditem = (PLpgSQL_diag_item *)lfirst(lc);

      switch (ditem->kind)
      {
                                                       
      case PLPGSQL_GETDIAG_ROW_COUNT:
        if (new->is_stacked)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("diagnostics item %s is not allowed in GET STACKED DIAGNOSTICS", plpgsql_getdiag_kindname(ditem->kind)), parser_errposition((yylsp[-4]))));
        }
        break;
                                                       
      case PLPGSQL_GETDIAG_ERROR_CONTEXT:
      case PLPGSQL_GETDIAG_ERROR_DETAIL:
      case PLPGSQL_GETDIAG_ERROR_HINT:
      case PLPGSQL_GETDIAG_RETURNED_SQLSTATE:
      case PLPGSQL_GETDIAG_COLUMN_NAME:
      case PLPGSQL_GETDIAG_CONSTRAINT_NAME:
      case PLPGSQL_GETDIAG_DATATYPE_NAME:
      case PLPGSQL_GETDIAG_MESSAGE_TEXT:
      case PLPGSQL_GETDIAG_TABLE_NAME:
      case PLPGSQL_GETDIAG_SCHEMA_NAME:
        if (!new->is_stacked)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("diagnostics item %s is not allowed in GET CURRENT DIAGNOSTICS", plpgsql_getdiag_kindname(ditem->kind)), parser_errposition((yylsp[-4]))));
        }
        break;
                                                   
      case PLPGSQL_GETDIAG_CONTEXT:
        break;
      default:
        elog(ERROR, "unrecognized diagnostic item kind: %d", ditem->kind);
        break;
      }
    }

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 2936 "pl_gram.c"
  break;

  case 91:                                
#line 1030 "pl_gram.y"
  {
    (yyval.boolean) = false;
  }
#line 2944 "pl_gram.c"
  break;

  case 92:                                   
#line 1034 "pl_gram.y"
  {
    (yyval.boolean) = false;
  }
#line 2952 "pl_gram.c"
  break;

  case 93:                                   
#line 1038 "pl_gram.y"
  {
    (yyval.boolean) = true;
  }
#line 2960 "pl_gram.c"
  break;

  case 94:                                                        
#line 1044 "pl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].diagitem));
  }
#line 2968 "pl_gram.c"
  break;

  case 95:                                       
#line 1048 "pl_gram.y"
  {
    (yyval.list) = list_make1((yyvsp[0].diagitem));
  }
#line 2976 "pl_gram.c"
  break;

  case 96:                                                                      
#line 1054 "pl_gram.y"
  {
    PLpgSQL_diag_item *new;

    new = palloc(sizeof(PLpgSQL_diag_item));
    new->target = (yyvsp[-2].datum)->dno;
    new->kind = (yyvsp[0].ival);

    (yyval.diagitem) = new;
  }
#line 2990 "pl_gram.c"
  break;

  case 97:                            
#line 1066 "pl_gram.y"
  {
    int tok = yylex();

    if (tok_is_keyword(tok, &yylval, K_ROW_COUNT, "row_count"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_ROW_COUNT;
    }
    else if (tok_is_keyword(tok, &yylval, K_PG_CONTEXT, "pg_context"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_CONTEXT;
    }
    else if (tok_is_keyword(tok, &yylval, K_PG_EXCEPTION_DETAIL, "pg_exception_detail"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_ERROR_DETAIL;
    }
    else if (tok_is_keyword(tok, &yylval, K_PG_EXCEPTION_HINT, "pg_exception_hint"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_ERROR_HINT;
    }
    else if (tok_is_keyword(tok, &yylval, K_PG_EXCEPTION_CONTEXT, "pg_exception_context"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_ERROR_CONTEXT;
    }
    else if (tok_is_keyword(tok, &yylval, K_COLUMN_NAME, "column_name"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_COLUMN_NAME;
    }
    else if (tok_is_keyword(tok, &yylval, K_CONSTRAINT_NAME, "constraint_name"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_CONSTRAINT_NAME;
    }
    else if (tok_is_keyword(tok, &yylval, K_PG_DATATYPE_NAME, "pg_datatype_name"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_DATATYPE_NAME;
    }
    else if (tok_is_keyword(tok, &yylval, K_MESSAGE_TEXT, "message_text"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_MESSAGE_TEXT;
    }
    else if (tok_is_keyword(tok, &yylval, K_TABLE_NAME, "table_name"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_TABLE_NAME;
    }
    else if (tok_is_keyword(tok, &yylval, K_SCHEMA_NAME, "schema_name"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_SCHEMA_NAME;
    }
    else if (tok_is_keyword(tok, &yylval, K_RETURNED_SQLSTATE, "returned_sqlstate"))
    {
      (yyval.ival) = PLPGSQL_GETDIAG_RETURNED_SQLSTATE;
    }
    else
    {
      yyerror("unrecognized GET DIAGNOSTICS item");
    }
  }
#line 3037 "pl_gram.c"
  break;

  case 98:                                  
#line 1111 "pl_gram.y"
  {
    if ((yyvsp[0].datum)->dtype == PLPGSQL_DTYPE_ROW || (yyvsp[0].datum)->dtype == PLPGSQL_DTYPE_REC)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"%s\" is not a scalar variable", ((PLpgSQL_variable *)(yyvsp[0].datum))->refname), parser_errposition((yylsp[0]))));
    }
    (yyval.datum) = (yyvsp[0].datum);
  }
#line 3052 "pl_gram.c"
  break;

  case 99:                              
#line 1122 "pl_gram.y"
  {
                                                           
    word_is_not_variable(&((yyvsp[0].word)), (yylsp[0]));
  }
#line 3061 "pl_gram.c"
  break;

  case 100:                               
#line 1127 "pl_gram.y"
  {
                                                           
    cword_is_not_variable(&((yyvsp[0].cword)), (yylsp[0]));
  }
#line 3070 "pl_gram.c"
  break;

  case 101:                           
#line 1135 "pl_gram.y"
  {
    check_assignable((yyvsp[0].wdatum).datum, (yylsp[0]));
    (yyval.datum) = (yyvsp[0].wdatum).datum;
  }
#line 3079 "pl_gram.c"
  break;

  case 102:                                                          
#line 1140 "pl_gram.y"
  {
    PLpgSQL_arrayelem *new;

    new = palloc0(sizeof(PLpgSQL_arrayelem));
    new->dtype = PLPGSQL_DTYPE_ARRAYELEM;
    new->subscript = (yyvsp[0].expr);
    new->arrayparentno = (yyvsp[-2].datum)->dno;
                                                    
    new->parenttypoid = InvalidOid;

    plpgsql_adddatum((PLpgSQL_datum *)new);

    (yyval.datum) = (PLpgSQL_datum *)new;
  }
#line 3098 "pl_gram.c"
  break;

  case 103:                                                                                    
#line 1157 "pl_gram.y"
  {
    PLpgSQL_stmt_if *new;

    new = palloc0(sizeof(PLpgSQL_stmt_if));
    new->cmd_type = PLPGSQL_STMT_IF;
    new->lineno = plpgsql_location_to_lineno((yylsp[-7]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->cond = (yyvsp[-6].expr);
    new->then_body = (yyvsp[-5].list);
    new->elsif_list = (yyvsp[-4].list);
    new->else_body = (yyvsp[-3].list);

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3117 "pl_gram.c"
  break;

  case 104:                           
#line 1174 "pl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 3125 "pl_gram.c"
  break;

  case 105:                                                                  
#line 1178 "pl_gram.y"
  {
    PLpgSQL_if_elsif *new;

    new = palloc0(sizeof(PLpgSQL_if_elsif));
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->cond = (yyvsp[-1].expr);
    new->stmts = (yyvsp[0].list);

    (yyval.list) = lappend((yyvsp[-3].list), new);
  }
#line 3140 "pl_gram.c"
  break;

  case 106:                         
#line 1191 "pl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 3148 "pl_gram.c"
  break;

  case 107:                                   
#line 1195 "pl_gram.y"
  {
    (yyval.list) = (yyvsp[0].list);
  }
#line 3156 "pl_gram.c"
  break;

  case 108:                                                                                           
#line 1201 "pl_gram.y"
  {
    (yyval.stmt) = make_case((yylsp[-6]), (yyvsp[-5].expr), (yyvsp[-4].list), (yyvsp[-3].list));
  }
#line 3164 "pl_gram.c"
  break;

  case 109:                                   
#line 1207 "pl_gram.y"
  {
    PLpgSQL_expr *expr = NULL;
    int tok = yylex();

    if (tok != K_WHEN)
    {
      plpgsql_push_back_token(tok);
      expr = read_sql_expression(K_WHEN, "WHEN");
    }
    plpgsql_push_back_token(K_WHEN);
    (yyval.expr) = expr;
  }
#line 3181 "pl_gram.c"
  break;

  case 110:                                                
#line 1222 "pl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].casewhen));
  }
#line 3189 "pl_gram.c"
  break;

  case 111:                                 
#line 1226 "pl_gram.y"
  {
    (yyval.list) = list_make1((yyvsp[0].casewhen));
  }
#line 3197 "pl_gram.c"
  break;

  case 112:                                                   
#line 1232 "pl_gram.y"
  {
    PLpgSQL_case_when *new = palloc(sizeof(PLpgSQL_case_when));

    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->expr = (yyvsp[-1].expr);
    new->stmts = (yyvsp[0].list);
    (yyval.casewhen) = new;
  }
#line 3210 "pl_gram.c"
  break;

  case 113:                             
#line 1243 "pl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 3218 "pl_gram.c"
  break;

  case 114:                                       
#line 1247 "pl_gram.y"
  {
       
                                                    
                                                          
                                                      
                                                     
       
    if ((yyvsp[0].list) != NIL)
    {
      (yyval.list) = (yyvsp[0].list);
    }
    else
    {
      (yyval.list) = list_make1(NULL);
    }
  }
#line 3235 "pl_gram.c"
  break;

  case 115:                                                  
#line 1262 "pl_gram.y"
  {
    PLpgSQL_stmt_loop *new;

    new = palloc0(sizeof(PLpgSQL_stmt_loop));
    new->cmd_type = PLPGSQL_STMT_LOOP;
    new->lineno = plpgsql_location_to_lineno((yylsp[-1]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->label = (yyvsp[-2].str);
    new->body = (yyvsp[0].loop_body).stmts;

    check_labels((yyvsp[-2].str), (yyvsp[0].loop_body).end_label, (yyvsp[0].loop_body).end_label_location);
    plpgsql_ns_pop();

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3255 "pl_gram.c"
  break;

  case 116:                                                                    
#line 1280 "pl_gram.y"
  {
    PLpgSQL_stmt_while *new;

    new = palloc0(sizeof(PLpgSQL_stmt_while));
    new->cmd_type = PLPGSQL_STMT_WHILE;
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->label = (yyvsp[-3].str);
    new->cond = (yyvsp[-1].expr);
    new->body = (yyvsp[0].loop_body).stmts;

    check_labels((yyvsp[-3].str), (yyvsp[0].loop_body).end_label, (yyvsp[0].loop_body).end_label_location);
    plpgsql_ns_pop();

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3276 "pl_gram.c"
  break;

  case 117:                                                            
#line 1299 "pl_gram.y"
  {
                                                     
    if ((yyvsp[-1].stmt)->cmd_type == PLPGSQL_STMT_FORI)
    {
      PLpgSQL_stmt_fori *new;

      new = (PLpgSQL_stmt_fori *)(yyvsp[-1].stmt);
      new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
      new->label = (yyvsp[-3].str);
      new->body = (yyvsp[0].loop_body).stmts;
      (yyval.stmt) = (PLpgSQL_stmt *)new;
    }
    else
    {
      PLpgSQL_stmt_forq *new;

      Assert((yyvsp[-1].stmt)->cmd_type == PLPGSQL_STMT_FORS || (yyvsp[-1].stmt)->cmd_type == PLPGSQL_STMT_FORC || (yyvsp[-1].stmt)->cmd_type == PLPGSQL_STMT_DYNFORS);
                                                     
      new = (PLpgSQL_stmt_forq *)(yyvsp[-1].stmt);
      new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
      new->label = (yyvsp[-3].str);
      new->body = (yyvsp[0].loop_body).stmts;
      (yyval.stmt) = (PLpgSQL_stmt *)new;
    }

    check_labels((yyvsp[-3].str), (yyvsp[0].loop_body).end_label, (yyvsp[0].loop_body).end_label_location);
                                                   
    plpgsql_ns_pop();
  }
#line 3312 "pl_gram.c"
  break;

  case 118:                                      
#line 1333 "pl_gram.y"
  {
    int tok = yylex();
    int tokloc = yylloc;

    if (tok == K_EXECUTE)
    {
                                                 
      PLpgSQL_stmt_dynfors *new;
      PLpgSQL_expr *expr;
      int term;

      expr = read_sql_expression2(K_LOOP, K_USING, "LOOP or USING", &term);

      new = palloc0(sizeof(PLpgSQL_stmt_dynfors));
      new->cmd_type = PLPGSQL_STMT_DYNFORS;
      new->stmtid = ++plpgsql_curr_compile->nstatements;
      if ((yyvsp[-1].forvariable).row)
      {
        new->var = (PLpgSQL_variable *)(yyvsp[-1].forvariable).row;
        check_assignable((yyvsp[-1].forvariable).row, (yylsp[-1]));
      }
      else if ((yyvsp[-1].forvariable).scalar)
      {
                                           
        new->var = (PLpgSQL_variable *)make_scalar_list1((yyvsp[-1].forvariable).name, (yyvsp[-1].forvariable).scalar, (yyvsp[-1].forvariable).lineno, (yylsp[-1]));
                                                    
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("loop variable of loop over rows must be a record variable or list of scalar variables"), parser_errposition((yylsp[-1]))));
      }
      new->query = expr;

      if (term == K_USING)
      {
        do
        {
          expr = read_sql_expression2(',', K_LOOP, ", or LOOP", &term);
          new->params = lappend(new->params, expr);
        } while (term == ',');
      }

      (yyval.stmt) = (PLpgSQL_stmt *)new;
    }
    else if (tok == T_DATUM && yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_VAR && ((PLpgSQL_var *)yylval.wdatum.datum)->datatype->typoid == REFCURSOROID)
    {
                                  
      PLpgSQL_stmt_forc *new;
      PLpgSQL_var *cursor = (PLpgSQL_var *)yylval.wdatum.datum;

      new = (PLpgSQL_stmt_forc *)palloc0(sizeof(PLpgSQL_stmt_forc));
      new->cmd_type = PLPGSQL_STMT_FORC;
      new->stmtid = ++plpgsql_curr_compile->nstatements;
      new->curvar = cursor->dno;

                                                  
      if ((yyvsp[-1].forvariable).scalar && (yyvsp[-1].forvariable).row)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cursor FOR loop must have only one target variable"), parser_errposition((yylsp[-1]))));
      }

                                                
      if (cursor->cursor_explicit_expr == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cursor FOR loop must use a bound cursor variable"), parser_errposition(tokloc)));
      }

                                              
      new->argquery = read_cursor_args(cursor, K_LOOP, "LOOP");

                                                 
      new->var = (PLpgSQL_variable *)plpgsql_build_record((yyvsp[-1].forvariable).name, (yyvsp[-1].forvariable).lineno, NULL, RECORDOID, true);

      (yyval.stmt) = (PLpgSQL_stmt *)new;
    }
    else
    {
      PLpgSQL_expr *expr1;
      int expr1loc;
      bool reverse = false;

         
                                            
                                                 
                                             
                                               
                                          
                                           
                                                 
                                              
                                            
                       
         
      if (tok_is_keyword(tok, &yylval, K_REVERSE, "reverse"))
      {
        reverse = true;
      }
      else
      {
        plpgsql_push_back_token(tok);
      }

         
                                                
                                             
                                          
                                         
                                      
         
      expr1 = read_sql_construct(DOT_DOT, K_LOOP, 0, "LOOP", "SELECT ", true, false, true, &expr1loc, &tok);

      if (tok == DOT_DOT)
      {
                                                     
        PLpgSQL_expr *expr2;
        PLpgSQL_expr *expr_by;
        PLpgSQL_var *fvar;
        PLpgSQL_stmt_fori *new;

                                                   
        check_sql_expr(expr1->query, expr1loc, 7);

                                           
        expr2 = read_sql_expression2(K_LOOP, K_BY, "LOOP", &tok);

                                      
        if (tok == K_BY)
        {
          expr_by = read_sql_expression(K_LOOP, "LOOP");
        }
        else
        {
          expr_by = NULL;
        }

                                                    
        if ((yyvsp[-1].forvariable).scalar && (yyvsp[-1].forvariable).row)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("integer FOR loop must have only one target variable"), parser_errposition((yylsp[-1]))));
        }

                                            
        fvar = (PLpgSQL_var *)plpgsql_build_variable((yyvsp[-1].forvariable).name, (yyvsp[-1].forvariable).lineno, plpgsql_build_datatype(INT4OID, -1, InvalidOid, NULL), true);

        new = palloc0(sizeof(PLpgSQL_stmt_fori));
        new->cmd_type = PLPGSQL_STMT_FORI;
        new->stmtid = ++plpgsql_curr_compile->nstatements;
        new->var = fvar;
        new->reverse = reverse;
        new->lower = expr1;
        new->upper = expr2;
        new->step = expr_by;

        (yyval.stmt) = (PLpgSQL_stmt *)new;
      }
      else
      {
           
                                                      
                                                       
                                                       
                            
           
        char *tmp_query;
        PLpgSQL_stmt_fors *new;

        if (reverse)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot specify REVERSE in query FOR loop"), parser_errposition(tokloc)));
        }

        Assert(strncmp(expr1->query, "SELECT ", 7) == 0);
        tmp_query = pstrdup(expr1->query + 7);
        pfree(expr1->query);
        expr1->query = tmp_query;

        check_sql_expr(expr1->query, expr1loc, 0);

        new = palloc0(sizeof(PLpgSQL_stmt_fors));
        new->cmd_type = PLPGSQL_STMT_FORS;
        new->stmtid = ++plpgsql_curr_compile->nstatements;
        if ((yyvsp[-1].forvariable).row)
        {
          new->var = (PLpgSQL_variable *)(yyvsp[-1].forvariable).row;
          check_assignable((yyvsp[-1].forvariable).row, (yylsp[-1]));
        }
        else if ((yyvsp[-1].forvariable).scalar)
        {
                                             
          new->var = (PLpgSQL_variable *)make_scalar_list1((yyvsp[-1].forvariable).name, (yyvsp[-1].forvariable).scalar, (yyvsp[-1].forvariable).lineno, (yylsp[-1]));
                                                      
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("loop variable of loop over rows must be a record variable or list of scalar variables"), parser_errposition((yylsp[-1]))));
        }

        new->query = expr1;
        (yyval.stmt) = (PLpgSQL_stmt *)new;
      }
    }
  }
#line 3558 "pl_gram.c"
  break;

  case 119:                             
#line 1595 "pl_gram.y"
  {
    (yyval.forvariable).name = NameOfDatum(&((yyvsp[0].wdatum)));
    (yyval.forvariable).lineno = plpgsql_location_to_lineno((yylsp[0]));
    if ((yyvsp[0].wdatum).datum->dtype == PLPGSQL_DTYPE_ROW || (yyvsp[0].wdatum).datum->dtype == PLPGSQL_DTYPE_REC)
    {
      (yyval.forvariable).scalar = NULL;
      (yyval.forvariable).row = (yyvsp[0].wdatum).datum;
    }
    else
    {
      int tok;

      (yyval.forvariable).scalar = (yyvsp[0].wdatum).datum;
      (yyval.forvariable).row = NULL;
                                          
      tok = yylex();
      plpgsql_push_back_token(tok);
      if (tok == ',')
      {
        (yyval.forvariable).row = (PLpgSQL_datum *)read_into_scalar_list((yyval.forvariable).name, (yyval.forvariable).scalar, (yylsp[0]));
      }
    }
  }
#line 3588 "pl_gram.c"
  break;

  case 120:                            
#line 1621 "pl_gram.y"
  {
    int tok;

    (yyval.forvariable).name = (yyvsp[0].word).ident;
    (yyval.forvariable).lineno = plpgsql_location_to_lineno((yylsp[0]));
    (yyval.forvariable).scalar = NULL;
    (yyval.forvariable).row = NULL;
                                        
    tok = yylex();
    plpgsql_push_back_token(tok);
    if (tok == ',')
    {
      word_is_not_variable(&((yyvsp[0].word)), (yylsp[0]));
    }
  }
#line 3606 "pl_gram.c"
  break;

  case 121:                             
#line 1635 "pl_gram.y"
  {
                                                           
    cword_is_not_variable(&((yyvsp[0].cword)), (yylsp[0]));
  }
#line 3615 "pl_gram.c"
  break;

  case 122:                                                                                                                  
#line 1642 "pl_gram.y"
  {
    PLpgSQL_stmt_foreach_a *new;

    new = palloc0(sizeof(PLpgSQL_stmt_foreach_a));
    new->cmd_type = PLPGSQL_STMT_FOREACH_A;
    new->lineno = plpgsql_location_to_lineno((yylsp[-6]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->label = (yyvsp[-7].str);
    new->slice = (yyvsp[-4].ival);
    new->expr = (yyvsp[-1].expr);
    new->body = (yyvsp[0].loop_body).stmts;

    if ((yyvsp[-5].forvariable).row)
    {
      new->varno = (yyvsp[-5].forvariable).row->dno;
      check_assignable((yyvsp[-5].forvariable).row, (yylsp[-5]));
    }
    else if ((yyvsp[-5].forvariable).scalar)
    {
      new->varno = (yyvsp[-5].forvariable).scalar->dno;
      check_assignable((yyvsp[-5].forvariable).scalar, (yylsp[-5]));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("loop variable of FOREACH must be a known variable or list of variables"), parser_errposition((yylsp[-5]))));
    }

    check_labels((yyvsp[-7].str), (yyvsp[0].loop_body).end_label, (yyvsp[0].loop_body).end_label_location);
    plpgsql_ns_pop();

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3655 "pl_gram.c"
  break;

  case 123:                             
#line 1680 "pl_gram.y"
  {
    (yyval.ival) = 0;
  }
#line 3663 "pl_gram.c"
  break;

  case 124:                                     
#line 1684 "pl_gram.y"
  {
    (yyval.ival) = (yyvsp[0].ival);
  }
#line 3671 "pl_gram.c"
  break;

  case 125:                                                   
#line 1690 "pl_gram.y"
  {
    PLpgSQL_stmt_exit *new;

    new = palloc0(sizeof(PLpgSQL_stmt_exit));
    new->cmd_type = PLPGSQL_STMT_EXIT;
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->is_exit = (yyvsp[-2].boolean);
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->label = (yyvsp[-1].str);
    new->cond = (yyvsp[0].expr);

    if ((yyvsp[-1].str))
    {
                                                
      PLpgSQL_nsitem *label;

      label = plpgsql_ns_lookup_label(plpgsql_ns_top(), (yyvsp[-1].str));
      if (label == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                           errmsg("there is no label \"%s\" "
                                  "attached to any block or loop enclosing this statement",
                               (yyvsp[-1].str)),
                           parser_errposition((yylsp[-1]))));
      }
                                            
      if (label->itemno != PLPGSQL_LABEL_LOOP && !new->is_exit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("block label \"%s\" cannot be used in CONTINUE", (yyvsp[-1].str)), parser_errposition((yylsp[-1]))));
      }
    }
    else
    {
         
                                                       
                                                         
                                                      
         
      if (plpgsql_ns_find_nearest_loop(plpgsql_ns_top()) == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), new->is_exit ? errmsg("EXIT cannot be used outside a loop, unless it has a label") : errmsg("CONTINUE cannot be used outside a loop"), parser_errposition((yylsp[-2]))));
      }
    }

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3726 "pl_gram.c"
  break;

  case 126:                         
#line 1743 "pl_gram.y"
  {
    (yyval.boolean) = true;
  }
#line 3734 "pl_gram.c"
  break;

  case 127:                             
#line 1747 "pl_gram.y"
  {
    (yyval.boolean) = false;
  }
#line 3742 "pl_gram.c"
  break;

  case 128:                             
#line 1753 "pl_gram.y"
  {
    int tok;

    tok = yylex();
    if (tok == 0)
    {
      yyerror("unexpected end of function definition");
    }

    if (tok_is_keyword(tok, &yylval, K_NEXT, "next"))
    {
      (yyval.stmt) = make_return_next_stmt((yylsp[0]));
    }
    else if (tok_is_keyword(tok, &yylval, K_QUERY, "query"))
    {
      (yyval.stmt) = make_return_query_stmt((yylsp[0]));
    }
    else
    {
      plpgsql_push_back_token(tok);
      (yyval.stmt) = make_return_stmt((yylsp[0]));
    }
  }
#line 3770 "pl_gram.c"
  break;

  case 129:                           
#line 1779 "pl_gram.y"
  {
    PLpgSQL_stmt_raise *new;
    int tok;

    new = palloc(sizeof(PLpgSQL_stmt_raise));

    new->cmd_type = PLPGSQL_STMT_RAISE;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->elog_level = ERROR;              
    new->condname = NULL;
    new->message = NULL;
    new->params = NIL;
    new->options = NIL;

    tok = yylex();
    if (tok == 0)
    {
      yyerror("unexpected end of function definition");
    }

       
                                                     
                          
       
    if (tok != ';')
    {
         
                                                   
         
      if (tok_is_keyword(tok, &yylval, K_EXCEPTION, "exception"))
      {
        new->elog_level = ERROR;
        tok = yylex();
      }
      else if (tok_is_keyword(tok, &yylval, K_WARNING, "warning"))
      {
        new->elog_level = WARNING;
        tok = yylex();
      }
      else if (tok_is_keyword(tok, &yylval, K_NOTICE, "notice"))
      {
        new->elog_level = NOTICE;
        tok = yylex();
      }
      else if (tok_is_keyword(tok, &yylval, K_INFO, "info"))
      {
        new->elog_level = INFO;
        tok = yylex();
      }
      else if (tok_is_keyword(tok, &yylval, K_LOG, "log"))
      {
        new->elog_level = LOG;
        tok = yylex();
      }
      else if (tok_is_keyword(tok, &yylval, K_DEBUG, "debug"))
      {
        new->elog_level = DEBUG1;
        tok = yylex();
      }
      if (tok == 0)
      {
        yyerror("unexpected end of function definition");
      }

         
                                               
                                                    
                                                       
                                                        
         
      if (tok == SCONST)
      {
                                              
        new->message = yylval.str;
           
                                                
                                                    
                                                     
                                               
           
        tok = yylex();
        if (tok != ',' && tok != ';' && tok != K_USING)
        {
          yyerror("syntax error");
        }

        while (tok == ',')
        {
          PLpgSQL_expr *expr;

          expr = read_sql_construct(',', ';', K_USING, ", or ; or USING", "SELECT ", true, true, true, NULL, &tok);
          new->params = lappend(new->params, expr);
        }
      }
      else if (tok != K_USING)
      {
                                                
        if (tok_is_keyword(tok, &yylval, K_SQLSTATE, "sqlstate"))
        {
                                                     
          char *sqlstatestr;

          if (yylex() != SCONST)
          {
            yyerror("syntax error");
          }
          sqlstatestr = yylval.str;

          if (strlen(sqlstatestr) != 5)
          {
            yyerror("invalid SQLSTATE code");
          }
          if (strspn(sqlstatestr, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") != 5)
          {
            yyerror("invalid SQLSTATE code");
          }
          new->condname = sqlstatestr;
        }
        else
        {
          if (tok == T_WORD)
          {
            new->condname = yylval.word.ident;
          }
          else if (plpgsql_token_is_unreserved_keyword(tok))
          {
            new->condname = pstrdup(yylval.keyword);
          }
          else
          {
            yyerror("syntax error");
          }
          plpgsql_recognize_err_condition(new->condname, false);
        }
        tok = yylex();
        if (tok != ';' && tok != K_USING)
        {
          yyerror("syntax error");
        }
      }

      if (tok == K_USING)
      {
        new->options = read_raise_options();
      }
    }

    check_raise_parameters(new);

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3917 "pl_gram.c"
  break;

  case 130:                             
#line 1924 "pl_gram.y"
  {
    PLpgSQL_stmt_assert *new;
    int tok;

    new = palloc(sizeof(PLpgSQL_stmt_assert));

    new->cmd_type = PLPGSQL_STMT_ASSERT;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;

    new->cond = read_sql_expression2(',', ';', ", or ;", &tok);

    if (tok == ',')
    {
      new->message = read_sql_expression(';', ";");
    }
    else
    {
      new->message = NULL;
    }

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 3943 "pl_gram.c"
  break;

  case 131:                                                       
#line 1948 "pl_gram.y"
  {
    (yyval.loop_body).stmts = (yyvsp[-4].list);
    (yyval.loop_body).end_label = (yyvsp[-1].str);
    (yyval.loop_body).end_label_location = (yylsp[-1]);
  }
#line 3953 "pl_gram.c"
  break;

  case 132:                              
#line 1966 "pl_gram.y"
  {
    (yyval.stmt) = make_execsql_stmt(K_IMPORT, (yylsp[0]));
  }
#line 3961 "pl_gram.c"
  break;

  case 133:                              
#line 1970 "pl_gram.y"
  {
    (yyval.stmt) = make_execsql_stmt(K_INSERT, (yylsp[0]));
  }
#line 3969 "pl_gram.c"
  break;

  case 134:                            
#line 1974 "pl_gram.y"
  {
    int tok;

    tok = yylex();
    plpgsql_push_back_token(tok);
    if (tok == '=' || tok == COLON_EQUALS || tok == '[')
    {
      word_is_not_variable(&((yyvsp[0].word)), (yylsp[0]));
    }
    (yyval.stmt) = make_execsql_stmt(T_WORD, (yylsp[0]));
  }
#line 3983 "pl_gram.c"
  break;

  case 135:                             
#line 1984 "pl_gram.y"
  {
    int tok;

    tok = yylex();
    plpgsql_push_back_token(tok);
    if (tok == '=' || tok == COLON_EQUALS || tok == '[')
    {
      cword_is_not_variable(&((yyvsp[0].cword)), (yylsp[0]));
    }
    (yyval.stmt) = make_execsql_stmt(T_CWORD, (yylsp[0]));
  }
#line 3997 "pl_gram.c"
  break;

  case 136:                                  
#line 1996 "pl_gram.y"
  {
    PLpgSQL_stmt_dynexecute *new;
    PLpgSQL_expr *expr;
    int endtoken;

    expr = read_sql_construct(K_INTO, K_USING, ';', "INTO or USING or ;", "SELECT ", true, true, true, NULL, &endtoken);

    new = palloc(sizeof(PLpgSQL_stmt_dynexecute));
    new->cmd_type = PLPGSQL_STMT_DYNEXECUTE;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->query = expr;
    new->into = false;
    new->strict = false;
    new->target = NULL;
    new->params = NIL;

       
                                                      
                                                       
                                                         
                                                      
                                    
       
    for (;;)
    {
      if (endtoken == K_INTO)
      {
        if (new->into)                    
        {
          yyerror("syntax error");
        }
        new->into = true;
        read_into_target(&new->target, &new->strict);
        endtoken = yylex();
      }
      else if (endtoken == K_USING)
      {
        if (new->params)                     
        {
          yyerror("syntax error");
        }
        do
        {
          expr = read_sql_construct(',', ';', K_INTO, ", or ; or INTO", "SELECT ", true, true, true, NULL, &endtoken);
          new->params = lappend(new->params, expr);
        } while (endtoken == ',');
      }
      else if (endtoken == ';')
      {
        break;
      }
      else
      {
        yyerror("syntax error");
      }
    }

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4062 "pl_gram.c"
  break;

  case 137:                                         
#line 2060 "pl_gram.y"
  {
    PLpgSQL_stmt_open *new;
    int tok;

    new = palloc0(sizeof(PLpgSQL_stmt_open));
    new->cmd_type = PLPGSQL_STMT_OPEN;
    new->lineno = plpgsql_location_to_lineno((yylsp[-1]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->curvar = (yyvsp[0].var)->dno;
    new->cursor_options = CURSOR_OPT_FAST_PLAN;

    if ((yyvsp[0].var)->cursor_explicit_expr == NULL)
    {
                                                       
      tok = yylex();
      if (tok_is_keyword(tok, &yylval, K_NO, "no"))
      {
        tok = yylex();
        if (tok_is_keyword(tok, &yylval, K_SCROLL, "scroll"))
        {
          new->cursor_options |= CURSOR_OPT_NO_SCROLL;
          tok = yylex();
        }
      }
      else if (tok_is_keyword(tok, &yylval, K_SCROLL, "scroll"))
      {
        new->cursor_options |= CURSOR_OPT_SCROLL;
        tok = yylex();
      }

      if (tok != K_FOR)
      {
        yyerror("syntax error, expected \"FOR\"");
      }

      tok = yylex();
      if (tok == K_EXECUTE)
      {
        int endtoken;

        new->dynquery = read_sql_expression2(K_USING, ';', "USING or ;", &endtoken);

                                                      
        if (endtoken == K_USING)
        {
          PLpgSQL_expr *expr;

          do
          {
            expr = read_sql_expression2(',', ';', ", or ;", &endtoken);
            new->params = lappend(new->params, expr);
          } while (endtoken == ',');
        }
      }
      else
      {
        plpgsql_push_back_token(tok);
        new->query = read_sql_stmt("");
      }
    }
    else
    {
                                                 
      new->argquery = read_cursor_args((yyvsp[0].var), ';', ";");
    }

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4142 "pl_gram.c"
  break;

  case 138:                                                                      
#line 2138 "pl_gram.y"
  {
    PLpgSQL_stmt_fetch *fetch = (yyvsp[-2].fetch);
    PLpgSQL_variable *target;

                                                                    
    read_into_target(&target, NULL);

    if (yylex() != ';')
    {
      yyerror("syntax error");
    }

       
                                                        
                                
       
    if (fetch->returns_multiple_rows)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("FETCH statement cannot return multiple rows"), parser_errposition((yylsp[-3]))));
    }

    fetch->lineno = plpgsql_location_to_lineno((yylsp[-3]));
    fetch->target = target;
    fetch->curvar = (yyvsp[-1].var)->dno;
    fetch->is_move = false;

    (yyval.stmt) = (PLpgSQL_stmt *)fetch;
  }
#line 4174 "pl_gram.c"
  break;

  case 139:                                                                 
#line 2168 "pl_gram.y"
  {
    PLpgSQL_stmt_fetch *fetch = (yyvsp[-2].fetch);

    fetch->lineno = plpgsql_location_to_lineno((yylsp[-3]));
    fetch->curvar = (yyvsp[-1].var)->dno;
    fetch->is_move = true;

    (yyval.stmt) = (PLpgSQL_stmt *)fetch;
  }
#line 4188 "pl_gram.c"
  break;

  case 140:                                   
#line 2180 "pl_gram.y"
  {
    (yyval.fetch) = read_fetch_direction();
  }
#line 4196 "pl_gram.c"
  break;

  case 141:                                               
#line 2186 "pl_gram.y"
  {
    PLpgSQL_stmt_close *new;

    new = palloc(sizeof(PLpgSQL_stmt_close));
    new->cmd_type = PLPGSQL_STMT_CLOSE;
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->curvar = (yyvsp[-1].var)->dno;

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4212 "pl_gram.c"
  break;

  case 142:                             
#line 2200 "pl_gram.y"
  {
                                                   
    (yyval.stmt) = NULL;
  }
#line 4221 "pl_gram.c"
  break;

  case 143:                                                       
#line 2207 "pl_gram.y"
  {
    PLpgSQL_stmt_commit *new;

    new = palloc(sizeof(PLpgSQL_stmt_commit));
    new->cmd_type = PLPGSQL_STMT_COMMIT;
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->chain = (yyvsp[-1].ival);

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4237 "pl_gram.c"
  break;

  case 144:                                                           
#line 2221 "pl_gram.y"
  {
    PLpgSQL_stmt_rollback *new;

    new = palloc(sizeof(PLpgSQL_stmt_rollback));
    new->cmd_type = PLPGSQL_STMT_ROLLBACK;
    new->lineno = plpgsql_location_to_lineno((yylsp[-2]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->chain = (yyvsp[-1].ival);

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4253 "pl_gram.c"
  break;

  case 145:                                            
#line 2235 "pl_gram.y"
  {
    (yyval.ival) = true;
  }
#line 4259 "pl_gram.c"
  break;

  case 146:                                                 
#line 2236 "pl_gram.y"
  {
    (yyval.ival) = false;
  }
#line 4265 "pl_gram.c"
  break;

  case 147:                                     
#line 2237 "pl_gram.y"
  {
    (yyval.ival) = false;
  }
#line 4271 "pl_gram.c"
  break;

  case 148:                       
#line 2241 "pl_gram.y"
  {
    PLpgSQL_stmt_set *new;

    new = palloc0(sizeof(PLpgSQL_stmt_set));
    new->cmd_type = PLPGSQL_STMT_SET;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;

    new->expr = read_sql_stmt("SET ");

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4288 "pl_gram.c"
  break;

  case 149:                         
#line 2254 "pl_gram.y"
  {
    PLpgSQL_stmt_set *new;

    new = palloc0(sizeof(PLpgSQL_stmt_set));
    new->cmd_type = PLPGSQL_STMT_SET;
    new->lineno = plpgsql_location_to_lineno((yylsp[0]));
    new->stmtid = ++plpgsql_curr_compile->nstatements;
    new->expr = read_sql_stmt("RESET ");

    (yyval.stmt) = (PLpgSQL_stmt *)new;
  }
#line 4304 "pl_gram.c"
  break;

  case 150:                                
#line 2269 "pl_gram.y"
  {
       
                                                        
                                                          
                                                 
       
    if ((yyvsp[0].wdatum).datum->dtype != PLPGSQL_DTYPE_VAR || plpgsql_peek() == '[')
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cursor variable must be a simple variable"), parser_errposition((yylsp[0]))));
    }

    if (((PLpgSQL_var *)(yyvsp[0].wdatum).datum)->datatype->typoid != REFCURSOROID)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("variable \"%s\" must be of type cursor or refcursor", ((PLpgSQL_var *)(yyvsp[0].wdatum).datum)->refname), parser_errposition((yylsp[0]))));
    }
    (yyval.var) = (PLpgSQL_var *)(yyvsp[0].wdatum).datum;
  }
#line 4330 "pl_gram.c"
  break;

  case 151:                               
#line 2291 "pl_gram.y"
  {
                                                           
    word_is_not_variable(&((yyvsp[0].word)), (yylsp[0]));
  }
#line 4339 "pl_gram.c"
  break;

  case 152:                                
#line 2296 "pl_gram.y"
  {
                                                           
    cword_is_not_variable(&((yyvsp[0].cword)), (yylsp[0]));
  }
#line 4348 "pl_gram.c"
  break;

  case 153:                              
#line 2303 "pl_gram.y"
  {
    (yyval.exception_block) = NULL;
  }
#line 4354 "pl_gram.c"
  break;

  case 154:                  
#line 2305 "pl_gram.y"
  {
       
                                             
                                                 
                                                 
                                                    
                      
       
    int lineno = plpgsql_location_to_lineno((yylsp[0]));
    PLpgSQL_exception_block *new = palloc(sizeof(PLpgSQL_exception_block));
    PLpgSQL_variable *var;

    var = plpgsql_build_variable("sqlstate", lineno, plpgsql_build_datatype(TEXTOID, -1, plpgsql_curr_compile->fn_input_collation, NULL), true);
    var->isconst = true;
    new->sqlstate_varno = var->dno;

    var = plpgsql_build_variable("sqlerrm", lineno, plpgsql_build_datatype(TEXTOID, -1, plpgsql_curr_compile->fn_input_collation, NULL), true);
    var->isconst = true;
    new->sqlerrm_varno = var->dno;

    (yyval.exception_block) = new;
  }
#line 4391 "pl_gram.c"
  break;

  case 155:                                                      
#line 2338 "pl_gram.y"
  {
    PLpgSQL_exception_block *new = (yyvsp[-1].exception_block);
    new->exc_list = (yyvsp[0].list);

    (yyval.exception_block) = new;
  }
#line 4402 "pl_gram.c"
  break;

  case 156:                                                       
#line 2347 "pl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].exception));
  }
#line 4410 "pl_gram.c"
  break;

  case 157:                                       
#line 2351 "pl_gram.y"
  {
    (yyval.list) = list_make1((yyvsp[0].exception));
  }
#line 4418 "pl_gram.c"
  break;

  case 158:                                                               
#line 2357 "pl_gram.y"
  {
    PLpgSQL_exception *new;

    new = palloc0(sizeof(PLpgSQL_exception));
    new->lineno = plpgsql_location_to_lineno((yylsp[-3]));
    new->conditions = (yyvsp[-2].condition);
    new->action = (yyvsp[0].list);

    (yyval.exception) = new;
  }
#line 4433 "pl_gram.c"
  break;

  case 159:                                                            
#line 2370 "pl_gram.y"
  {
    PLpgSQL_condition *old;

    for (old = (yyvsp[-2].condition); old->next != NULL; old = old->next)
                ;
    old->next = (yyvsp[0].condition);
    (yyval.condition) = (yyvsp[-2].condition);
  }
#line 4446 "pl_gram.c"
  break;

  case 160:                                       
#line 2379 "pl_gram.y"
  {
    (yyval.condition) = (yyvsp[0].condition);
  }
#line 4454 "pl_gram.c"
  break;

  case 161:                                      
#line 2385 "pl_gram.y"
  {
    if (strcmp((yyvsp[0].str), "sqlstate") != 0)
    {
      (yyval.condition) = plpgsql_parse_err_condition((yyvsp[0].str));
    }
    else
    {
      PLpgSQL_condition *new;
      char *sqlstatestr;

                                                 
      if (yylex() != SCONST)
      {
        yyerror("syntax error");
      }
      sqlstatestr = yylval.str;

      if (strlen(sqlstatestr) != 5)
      {
        yyerror("invalid SQLSTATE code");
      }
      if (strspn(sqlstatestr, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") != 5)
      {
        yyerror("invalid SQLSTATE code");
      }

      new = palloc(sizeof(PLpgSQL_condition));
      new->sqlerrstate = MAKE_SQLSTATE(sqlstatestr[0], sqlstatestr[1], sqlstatestr[2], sqlstatestr[3], sqlstatestr[4]);
      new->condname = sqlstatestr;
      new->next = NULL;

      (yyval.condition) = new;
    }
  }
#line 4492 "pl_gram.c"
  break;

  case 162:                               
#line 2421 "pl_gram.y"
  {
    (yyval.expr) = read_sql_expression(';', ";");
  }
#line 4498 "pl_gram.c"
  break;

  case 163:                                       
#line 2425 "pl_gram.y"
  {
    (yyval.expr) = read_sql_expression(']', "]");
  }
#line 4504 "pl_gram.c"
  break;

  case 164:                               
#line 2429 "pl_gram.y"
  {
    (yyval.expr) = read_sql_expression(K_THEN, "THEN");
  }
#line 4510 "pl_gram.c"
  break;

  case 165:                               
#line 2433 "pl_gram.y"
  {
    (yyval.expr) = read_sql_expression(K_LOOP, "LOOP");
  }
#line 4516 "pl_gram.c"
  break;

  case 166:                               
#line 2437 "pl_gram.y"
  {
    plpgsql_ns_push(NULL, PLPGSQL_LABEL_BLOCK);
    (yyval.str) = NULL;
  }
#line 4525 "pl_gram.c"
  break;

  case 167:                                                                 
#line 2442 "pl_gram.y"
  {
    plpgsql_ns_push((yyvsp[-1].str), PLPGSQL_LABEL_BLOCK);
    (yyval.str) = (yyvsp[-1].str);
  }
#line 4534 "pl_gram.c"
  break;

  case 168:                              
#line 2449 "pl_gram.y"
  {
    plpgsql_ns_push(NULL, PLPGSQL_LABEL_LOOP);
    (yyval.str) = NULL;
  }
#line 4543 "pl_gram.c"
  break;

  case 169:                                                                
#line 2454 "pl_gram.y"
  {
    plpgsql_ns_push((yyvsp[-1].str), PLPGSQL_LABEL_LOOP);
    (yyval.str) = (yyvsp[-1].str);
  }
#line 4552 "pl_gram.c"
  break;

  case 170:                         
#line 2461 "pl_gram.y"
  {
    (yyval.str) = NULL;
  }
#line 4560 "pl_gram.c"
  break;

  case 171:                                 
#line 2465 "pl_gram.y"
  {
                                                            
    (yyval.str) = (yyvsp[0].str);
  }
#line 4569 "pl_gram.c"
  break;

  case 172:                         
#line 2472 "pl_gram.y"
  {
    (yyval.expr) = NULL;
  }
#line 4575 "pl_gram.c"
  break;

  case 173:                                            
#line 2474 "pl_gram.y"
  {
    (yyval.expr) = (yyvsp[0].expr);
  }
#line 4581 "pl_gram.c"
  break;

  case 174:                              
#line 2481 "pl_gram.y"
  {
    (yyval.str) = (yyvsp[0].word).ident;
  }
#line 4589 "pl_gram.c"
  break;

  case 175:                                          
#line 2485 "pl_gram.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].keyword));
  }
#line 4597 "pl_gram.c"
  break;

  case 176:                               
#line 2489 "pl_gram.y"
  {
    if ((yyvsp[0].wdatum).ident == NULL)                            
    {
      yyerror("syntax error");
    }
    (yyval.str) = (yyvsp[0].wdatum).ident;
  }
#line 4607 "pl_gram.c"
  break;

#line 4611 "pl_gram.c"

  default:
    break;
  }
                                                                     
                                                                    
                                                                      
                                                                      
                                                                         
                                                                        
                                                                     
                                                                     
                                                                       
                                                                     
                                            
  YY_SYMBOL_PRINT("-> $$ =", YY_CAST(yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK(yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

                                                                    
                                                                     
                           
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp ? yytable[yyi] : yydefgoto[yylhs]);
  }

  goto yynewstate;

                                          
                                          
                                         
yyerrlab:
                                                                      
                                                         
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE(yychar);
                                                                    
  if (!yyerrstatus)
  {
    ++yynerrs;
    yyerror(YY_("syntax error"));
  }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
  {
                                                                  
                             

    if (yychar <= YYEOF)
    {
                                               
      if (yychar == YYEOF)
      {
        YYABORT;
      }
    }
    else
    {
      yydestruct("Error: discarding", yytoken, &yylval, &yylloc);
      yychar = YYEMPTY;
    }
  }

                                                                     
               
  goto yyerrlab1;

                                                       
                                                       
                                                      
yyerrorlab:
                                                                       
                                                               
  if (0)
  {
    YYERROR;
  }

                                                                   
                      
  YYPOPSTACK(yylen);
  yylen = 0;
  YY_STACK_PRINT(yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;

                                                                 
                                                                 
                                                                
yyerrlab1:
  yyerrstatus = 3;                                                

                                                                     
  for (;;)
  {
    yyn = yypact[yystate];
    if (!yypact_value_is_default(yyn))
    {
      yyn += YYSYMBOL_YYerror;
      if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
      {
        yyn = yytable[yyn];
        if (0 < yyn)
        {
          break;
        }
      }
    }

                                                                          
    if (yyssp == yyss)
    {
      YYABORT;
    }

    yyerror_range[1] = *yylsp;
    yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp, yylsp);
    YYPOPSTACK(1);
    yystate = *yyssp;
    YY_STACK_PRINT(yyss, yyssp);
  }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT(*yylsp, yyerror_range, 2);

                               
  YY_SYMBOL_PRINT("Shifting", YY_ACCESSING_SYMBOL(yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;

                                         
                                         
                                        
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

                                       
                                       
                                      
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow
                                                     
                                                     
                                                    
yyexhaustedlab:
  yyerror(YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif

                                                           
                                                           
                                                          
yyreturn:
  if (yychar != YYEMPTY)
  {
                                                                        
                                                           
    yytoken = YYTRANSLATE(yychar);
    yydestruct("Cleanup: discarding lookahead", yytoken, &yylval, &yylloc);
  }
                                                                   
                                  
  YYPOPSTACK(yylen);
  YY_STACK_PRINT(yyss, yyssp);
  while (yyssp != yyss)
  {
    yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp, yylsp);
    YYPOPSTACK(1);
  }
#ifndef yyoverflow
  if (yyss != yyssa)
  {
    YYSTACK_FREE(yyss);
  }
#endif

  return yyresult;
}

#line 2581 "pl_gram.y"

   
                                                             
                                                                             
                                                                       
                      
   
static bool
tok_is_keyword(int token, union YYSTYPE *lval, int kw_token, const char *kw_str)
{
  if (token == kw_token)
  {
                                                                          
    return true;
  }
  else if (token == T_DATUM)
  {
       
                                                                      
                                                                       
                                
       
    if (!lval->wdatum.quoted && lval->wdatum.ident != NULL && strcmp(lval->wdatum.ident, kw_str) == 0)
    {
      return true;
    }
  }
  return false;                      
}

   
                                                                            
                              
   
static void
word_is_not_variable(PLword *word, int location)
{
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"%s\" is not a known variable", word->ident), parser_errposition(location)));
}

                       
static void
cword_is_not_variable(PLcword *cword, int location)
{
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"%s\" is not a known variable", NameListToString(cword->idents)), parser_errposition(location)));
}

   
                                                                    
                                                                   
                              
   
static void
current_token_is_not_variable(int tok)
{
  if (tok == T_WORD)
  {
    word_is_not_variable(&(yylval.word), yylloc);
  }
  else if (tok == T_CWORD)
  {
    cword_is_not_variable(&(yylval.cword), yylloc);
  }
  else
  {
    yyerror("syntax error");
  }
}

                                                                            
static PLpgSQL_expr *
read_sql_expression(int until, const char *expected)
{
  return read_sql_construct(until, 0, 0, expected, "SELECT ", true, true, true, NULL, NULL);
}

                                                                             
static PLpgSQL_expr *
read_sql_expression2(int until, int until2, const char *expected, int *endtoken)
{
  return read_sql_construct(until, until2, 0, expected, "SELECT ", true, true, true, NULL, endtoken);
}

                                                                        
static PLpgSQL_expr *
read_sql_stmt(const char *sqlstart)
{
  return read_sql_construct(';', 0, 0, ";", sqlstart, false, true, true, NULL, NULL);
}

   
                                                         
   
                                              
                                                                 
                                                                         
                                                                      
                                                        
                                                                               
                                                                                 
                                   
                                                                         
                                                              
                                                                
   
static PLpgSQL_expr *
read_sql_construct(int until, int until2, int until3, const char *expected, const char *sqlstart, bool isexpression, bool valid_sql, bool trim, int *startloc, int *endtoken)
{
  int tok;
  StringInfoData ds;
  IdentifierLookup save_IdentifierLookup;
  int startlocation = -1;
  int parenlevel = 0;
  PLpgSQL_expr *expr;

  initStringInfo(&ds);
  appendStringInfoString(&ds, sqlstart);

                                                               
  save_IdentifierLookup = plpgsql_IdentifierLookup;
  plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_EXPR;

  for (;;)
  {
    tok = yylex();
    if (startlocation < 0)                                  
    {
      startlocation = yylloc;
    }
    if (tok == until && parenlevel == 0)
    {
      break;
    }
    if (tok == until2 && parenlevel == 0)
    {
      break;
    }
    if (tok == until3 && parenlevel == 0)
    {
      break;
    }
    if (tok == '(' || tok == '[')
    {
      parenlevel++;
    }
    else if (tok == ')' || tok == ']')
    {
      parenlevel--;
      if (parenlevel < 0)
      {
        yyerror("mismatched parentheses");
      }
    }
       
                                                                      
                                                                      
                                              
       
    if (tok == 0 || tok == ';')
    {
      if (parenlevel != 0)
      {
        yyerror("mismatched parentheses");
      }
      if (isexpression)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("missing \"%s\" at end of SQL expression", expected), parser_errposition(yylloc)));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("missing \"%s\" at end of SQL statement", expected), parser_errposition(yylloc)));
      }
    }
  }

  plpgsql_IdentifierLookup = save_IdentifierLookup;

  if (startloc)
  {
    *startloc = startlocation;
  }
  if (endtoken)
  {
    *endtoken = tok;
  }

                                                
  if (startlocation >= yylloc)
  {
    if (isexpression)
    {
      yyerror("missing expression");
    }
    else
    {
      yyerror("missing SQL statement");
    }
  }

  plpgsql_append_source_text(&ds, startlocation, yylloc);

                                                  
  if (trim)
  {
    while (ds.len > 0 && scanner_isspace(ds.data[ds.len - 1]))
    {
      ds.data[--ds.len] = '\0';
    }
  }

  expr = palloc0(sizeof(PLpgSQL_expr));
  expr->query = pstrdup(ds.data);
  expr->plan = NULL;
  expr->paramnos = NULL;
  expr->rwparam = -1;
  expr->ns = plpgsql_ns_top();
  pfree(ds.data);

  if (valid_sql)
  {
    check_sql_expr(expr->query, startlocation, strlen(sqlstart));
  }

  return expr;
}

static PLpgSQL_type *
read_datatype(int tok)
{
  StringInfoData ds;
  char *type_name;
  int startlocation;
  PLpgSQL_type *result;
  int parenlevel = 0;

                                                            
  Assert(plpgsql_IdentifierLookup == IDENTIFIER_LOOKUP_DECLARE);

                                                                  
  if (tok == YYEMPTY)
  {
    tok = yylex();
  }

  startlocation = yylloc;

     
                                                                  
                              
     
  if (tok == T_WORD)
  {
    char *dtname = yylval.word.ident;

    tok = yylex();
    if (tok == '%')
    {
      tok = yylex();
      if (tok_is_keyword(tok, &yylval, K_TYPE, "type"))
      {
        result = plpgsql_parse_wordtype(dtname);
        if (result)
        {
          return result;
        }
      }
      else if (tok_is_keyword(tok, &yylval, K_ROWTYPE, "rowtype"))
      {
        result = plpgsql_parse_wordrowtype(dtname);
        if (result)
        {
          return result;
        }
      }
    }
  }
  else if (plpgsql_token_is_unreserved_keyword(tok))
  {
    char *dtname = pstrdup(yylval.keyword);

    tok = yylex();
    if (tok == '%')
    {
      tok = yylex();
      if (tok_is_keyword(tok, &yylval, K_TYPE, "type"))
      {
        result = plpgsql_parse_wordtype(dtname);
        if (result)
        {
          return result;
        }
      }
      else if (tok_is_keyword(tok, &yylval, K_ROWTYPE, "rowtype"))
      {
        result = plpgsql_parse_wordrowtype(dtname);
        if (result)
        {
          return result;
        }
      }
    }
  }
  else if (tok == T_CWORD)
  {
    List *dtnames = yylval.cword.idents;

    tok = yylex();
    if (tok == '%')
    {
      tok = yylex();
      if (tok_is_keyword(tok, &yylval, K_TYPE, "type"))
      {
        result = plpgsql_parse_cwordtype(dtnames);
        if (result)
        {
          return result;
        }
      }
      else if (tok_is_keyword(tok, &yylval, K_ROWTYPE, "rowtype"))
      {
        result = plpgsql_parse_cwordrowtype(dtnames);
        if (result)
        {
          return result;
        }
      }
    }
  }

  while (tok != ';')
  {
    if (tok == 0)
    {
      if (parenlevel != 0)
      {
        yyerror("mismatched parentheses");
      }
      else
      {
        yyerror("incomplete data type declaration");
      }
    }
                                                          
    if (tok == K_COLLATE || tok == K_NOT || tok == '=' || tok == COLON_EQUALS || tok == K_DEFAULT)
    {
      break;
    }
                                                              
    if ((tok == ',' || tok == ')') && parenlevel == 0)
    {
      break;
    }
    if (tok == '(')
    {
      parenlevel++;
    }
    else if (tok == ')')
    {
      parenlevel--;
    }

    tok = yylex();
  }

                                                   
  initStringInfo(&ds);
  plpgsql_append_source_text(&ds, startlocation, yylloc);
  type_name = ds.data;

  if (type_name[0] == '\0')
  {
    yyerror("missing data type declaration");
  }

  result = parse_datatype(type_name, startlocation);

  pfree(ds.data);

  plpgsql_push_back_token(tok);

  return result;
}

static PLpgSQL_stmt *
make_execsql_stmt(int firsttoken, int location)
{
  StringInfoData ds;
  IdentifierLookup save_IdentifierLookup;
  PLpgSQL_stmt_execsql *execsql;
  PLpgSQL_expr *expr;
  PLpgSQL_variable *target = NULL;
  int tok;
  int prev_tok;
  bool have_into = false;
  bool have_strict = false;
  int into_start_loc = -1;
  int into_end_loc = -1;

  initStringInfo(&ds);

                                                               
  save_IdentifierLookup = plpgsql_IdentifierLookup;
  plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_EXPR;

     
                                                                      
                                                                      
     
                                                                           
                                                                           
                                           
     
                                                                        
                          
     
                                                                           
                                                                            
                                                                             
                                                                             
                               
     
                                                                            
                                                                        
                                                                           
                                                                         
     
                                                                        
                                                                     
     
                                                                           
                                        
     
  tok = firsttoken;
  for (;;)
  {
    prev_tok = tok;
    tok = yylex();
    if (have_into && into_end_loc < 0)
    {
      into_end_loc = yylloc;                                
    }
    if (tok == ';')
    {
      break;
    }
    if (tok == 0)
    {
      yyerror("unexpected end of function definition");
    }
    if (tok == K_INTO)
    {
      if (prev_tok == K_INSERT)
      {
        continue;                                        
      }
      if (firsttoken == K_IMPORT)
      {
        continue;                                            
      }
      if (have_into)
      {
        yyerror("INTO specified more than once");
      }
      have_into = true;
      into_start_loc = yylloc;
      plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
      read_into_target(&target, &have_strict);
      plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_EXPR;
    }
  }

  plpgsql_IdentifierLookup = save_IdentifierLookup;

  if (have_into)
  {
       
                                                                   
                                                                      
                                                             
       
    plpgsql_append_source_text(&ds, location, into_start_loc);
    appendStringInfoSpaces(&ds, into_end_loc - into_start_loc);
    plpgsql_append_source_text(&ds, into_end_loc, yylloc);
  }
  else
  {
    plpgsql_append_source_text(&ds, location, yylloc);
  }

                                                  
  while (ds.len > 0 && scanner_isspace(ds.data[ds.len - 1]))
  {
    ds.data[--ds.len] = '\0';
  }

  expr = palloc0(sizeof(PLpgSQL_expr));
  expr->query = pstrdup(ds.data);
  expr->plan = NULL;
  expr->paramnos = NULL;
  expr->rwparam = -1;
  expr->ns = plpgsql_ns_top();
  pfree(ds.data);

  check_sql_expr(expr->query, location, 0);

  execsql = palloc0(sizeof(PLpgSQL_stmt_execsql));
  execsql->cmd_type = PLPGSQL_STMT_EXECSQL;
  execsql->lineno = plpgsql_location_to_lineno(location);
  execsql->stmtid = ++plpgsql_curr_compile->nstatements;
  execsql->sqlstmt = expr;
  execsql->into = have_into;
  execsql->strict = have_strict;
  execsql->target = target;

  return (PLpgSQL_stmt *)execsql;
}

   
                                                                     
   
static PLpgSQL_stmt_fetch *
read_fetch_direction(void)
{
  PLpgSQL_stmt_fetch *fetch;
  int tok;
  bool check_FROM = true;

     
                                                                    
                                                           
     
  fetch = (PLpgSQL_stmt_fetch *)palloc0(sizeof(PLpgSQL_stmt_fetch));
  fetch->cmd_type = PLPGSQL_STMT_FETCH;
  fetch->stmtid = ++plpgsql_curr_compile->nstatements;
                               
  fetch->direction = FETCH_FORWARD;
  fetch->how_many = 1;
  fetch->expr = NULL;
  fetch->returns_multiple_rows = false;

  tok = yylex();
  if (tok == 0)
  {
    yyerror("unexpected end of function definition");
  }

  if (tok_is_keyword(tok, &yylval, K_NEXT, "next"))
  {
                      
  }
  else if (tok_is_keyword(tok, &yylval, K_PRIOR, "prior"))
  {
    fetch->direction = FETCH_BACKWARD;
  }
  else if (tok_is_keyword(tok, &yylval, K_FIRST, "first"))
  {
    fetch->direction = FETCH_ABSOLUTE;
  }
  else if (tok_is_keyword(tok, &yylval, K_LAST, "last"))
  {
    fetch->direction = FETCH_ABSOLUTE;
    fetch->how_many = -1;
  }
  else if (tok_is_keyword(tok, &yylval, K_ABSOLUTE, "absolute"))
  {
    fetch->direction = FETCH_ABSOLUTE;
    fetch->expr = read_sql_expression2(K_FROM, K_IN, "FROM or IN", NULL);
    check_FROM = false;
  }
  else if (tok_is_keyword(tok, &yylval, K_RELATIVE, "relative"))
  {
    fetch->direction = FETCH_RELATIVE;
    fetch->expr = read_sql_expression2(K_FROM, K_IN, "FROM or IN", NULL);
    check_FROM = false;
  }
  else if (tok_is_keyword(tok, &yylval, K_ALL, "all"))
  {
    fetch->how_many = FETCH_ALL;
    fetch->returns_multiple_rows = true;
  }
  else if (tok_is_keyword(tok, &yylval, K_FORWARD, "forward"))
  {
    complete_direction(fetch, &check_FROM);
  }
  else if (tok_is_keyword(tok, &yylval, K_BACKWARD, "backward"))
  {
    fetch->direction = FETCH_BACKWARD;
    complete_direction(fetch, &check_FROM);
  }
  else if (tok == K_FROM || tok == K_IN)
  {
                         
    check_FROM = false;
  }
  else if (tok == T_DATUM)
  {
                                                                     
    plpgsql_push_back_token(tok);
    check_FROM = false;
  }
  else
  {
       
                                                                 
                                                                      
                                                                       
                                                                        
                                                                     
                    
       
    plpgsql_push_back_token(tok);
    fetch->expr = read_sql_expression2(K_FROM, K_IN, "FROM or IN", NULL);
    fetch->returns_multiple_rows = true;
    check_FROM = false;
  }

                                                                
  if (check_FROM)
  {
    tok = yylex();
    if (tok != K_FROM && tok != K_IN)
    {
      yyerror("expected FROM or IN");
    }
  }

  return fetch;
}

   
                                                                        
                       
                                          
                                           
   
static void
complete_direction(PLpgSQL_stmt_fetch *fetch, bool *check_FROM)
{
  int tok;

  tok = yylex();
  if (tok == 0)
  {
    yyerror("unexpected end of function definition");
  }

  if (tok == K_FROM || tok == K_IN)
  {
    *check_FROM = false;
    return;
  }

  if (tok == K_ALL)
  {
    fetch->how_many = FETCH_ALL;
    fetch->returns_multiple_rows = true;
    *check_FROM = true;
    return;
  }

  plpgsql_push_back_token(tok);
  fetch->expr = read_sql_expression2(K_FROM, K_IN, "FROM or IN", NULL);
  fetch->returns_multiple_rows = true;
  *check_FROM = false;
}

static PLpgSQL_stmt *
make_return_stmt(int location)
{
  PLpgSQL_stmt_return *new;

  new = palloc0(sizeof(PLpgSQL_stmt_return));
  new->cmd_type = PLPGSQL_STMT_RETURN;
  new->lineno = plpgsql_location_to_lineno(location);
  new->stmtid = ++plpgsql_curr_compile->nstatements;
  new->expr = NULL;
  new->retvarno = -1;

  if (plpgsql_curr_compile->fn_retset)
  {
    if (yylex() != ';')
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("RETURN cannot have a parameter in function returning set"), errhint("Use RETURN NEXT or RETURN QUERY."), parser_errposition(yylloc)));
    }
  }
  else if (plpgsql_curr_compile->fn_rettype == VOIDOID)
  {
    if (yylex() != ';')
    {
      if (plpgsql_curr_compile->fn_prokind == PROKIND_PROCEDURE)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("RETURN cannot have a parameter in a procedure"), parser_errposition(yylloc)));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("RETURN cannot have a parameter in function returning void"), parser_errposition(yylloc)));
      }
    }
  }
  else if (plpgsql_curr_compile->out_param_varno >= 0)
  {
    if (yylex() != ';')
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("RETURN cannot have a parameter in function with OUT parameters"), parser_errposition(yylloc)));
    }
    new->retvarno = plpgsql_curr_compile->out_param_varno;
  }
  else
  {
       
                                                                          
                                                    
       
    int tok = yylex();

    if (tok == T_DATUM && plpgsql_peek() == ';' && (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_VAR || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_PROMISE || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC))
    {
      new->retvarno = yylval.wdatum.datum->dno;
                                                                
      tok = yylex();
      Assert(tok == ';');
    }
    else
    {
         
                                                             
         
                                                                
                                                
         
      plpgsql_push_back_token(tok);
      new->expr = read_sql_expression(';', ";");
    }
  }

  return (PLpgSQL_stmt *)new;
}

static PLpgSQL_stmt *
make_return_next_stmt(int location)
{
  PLpgSQL_stmt_return_next *new;

  if (!plpgsql_curr_compile->fn_retset)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot use RETURN NEXT in a non-SETOF function"), parser_errposition(location)));
  }

  new = palloc0(sizeof(PLpgSQL_stmt_return_next));
  new->cmd_type = PLPGSQL_STMT_RETURN_NEXT;
  new->lineno = plpgsql_location_to_lineno(location);
  new->stmtid = ++plpgsql_curr_compile->nstatements;
  new->expr = NULL;
  new->retvarno = -1;

  if (plpgsql_curr_compile->out_param_varno >= 0)
  {
    if (yylex() != ';')
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("RETURN NEXT cannot have a parameter in function with OUT parameters"), parser_errposition(yylloc)));
    }
    new->retvarno = plpgsql_curr_compile->out_param_varno;
  }
  else
  {
       
                                                                          
                                                    
       
    int tok = yylex();

    if (tok == T_DATUM && plpgsql_peek() == ';' && (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_VAR || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_PROMISE || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC))
    {
      new->retvarno = yylval.wdatum.datum->dno;
                                                                
      tok = yylex();
      Assert(tok == ';');
    }
    else
    {
         
                                                             
         
                                                                
                                                
         
      plpgsql_push_back_token(tok);
      new->expr = read_sql_expression(';', ";");
    }
  }

  return (PLpgSQL_stmt *)new;
}

static PLpgSQL_stmt *
make_return_query_stmt(int location)
{
  PLpgSQL_stmt_return_query *new;
  int tok;

  if (!plpgsql_curr_compile->fn_retset)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot use RETURN QUERY in a non-SETOF function"), parser_errposition(location)));
  }

  new = palloc0(sizeof(PLpgSQL_stmt_return_query));
  new->cmd_type = PLPGSQL_STMT_RETURN_QUERY;
  new->lineno = plpgsql_location_to_lineno(location);
  new->stmtid = ++plpgsql_curr_compile->nstatements;

                                      
  if ((tok = yylex()) != K_EXECUTE)
  {
                               
    plpgsql_push_back_token(tok);
    new->query = read_sql_stmt("");
  }
  else
  {
                     
    int term;

    new->dynquery = read_sql_expression2(';', K_USING, "; or USING", &term);
    if (term == K_USING)
    {
      do
      {
        PLpgSQL_expr *expr;

        expr = read_sql_expression2(',', ';', ", or ;", &term);
        new->params = lappend(new->params, expr);
      } while (term == ',');
    }
  }

  return (PLpgSQL_stmt *)new;
}

                                                        
static char *
NameOfDatum(PLwdatum *wdatum)
{
  if (wdatum->ident)
  {
    return wdatum->ident;
  }
  Assert(wdatum->idents != NIL);
  return NameListToString(wdatum->idents);
}

static void
check_assignable(PLpgSQL_datum *datum, int location)
{
  switch (datum->dtype)
  {
  case PLPGSQL_DTYPE_VAR:
  case PLPGSQL_DTYPE_PROMISE:
  case PLPGSQL_DTYPE_REC:
    if (((PLpgSQL_variable *)datum)->isconst)
    {
      ereport(ERROR, (errcode(ERRCODE_ERROR_IN_ASSIGNMENT), errmsg("variable \"%s\" is declared CONSTANT", ((PLpgSQL_variable *)datum)->refname), parser_errposition(location)));
    }
    break;
  case PLPGSQL_DTYPE_ROW:
                                                                     
    break;
  case PLPGSQL_DTYPE_RECFIELD:
                                        
    check_assignable(plpgsql_Datums[((PLpgSQL_recfield *)datum)->recparentno], location);
    break;
  case PLPGSQL_DTYPE_ARRAYELEM:
                                       
    check_assignable(plpgsql_Datums[((PLpgSQL_arrayelem *)datum)->arrayparentno], location);
    break;
  default:
    elog(ERROR, "unrecognized dtype: %d", datum->dtype);
    break;
  }
}

   
                                                                         
                 
   
static void
read_into_target(PLpgSQL_variable **target, bool *strict)
{
  int tok;

                           
  *target = NULL;
  if (strict)
  {
    *strict = false;
  }

  tok = yylex();
  if (strict && tok == K_STRICT)
  {
    *strict = true;
    tok = yylex();
  }

     
                                                                        
                                                                          
                                                                        
                                                                          
                                                                      
     
  switch (tok)
  {
  case T_DATUM:
    if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC)
    {
      check_assignable(yylval.wdatum.datum, yylloc);
      *target = (PLpgSQL_variable *)yylval.wdatum.datum;

      if ((tok = yylex()) == ',')
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("record variable cannot be part of multiple-item INTO list"), parser_errposition(yylloc)));
      }
      plpgsql_push_back_token(tok);
    }
    else
    {
      *target = (PLpgSQL_variable *)read_into_scalar_list(NameOfDatum(&(yylval.wdatum)), yylval.wdatum.datum, yylloc);
    }
    break;

  default:
                                                           
    current_token_is_not_variable(tok);
  }
}

   
                                                                     
                                                                     
                                                                
            
   
static PLpgSQL_row *
read_into_scalar_list(char *initial_name, PLpgSQL_datum *initial_datum, int initial_location)
{
  int nfields;
  char *fieldnames[1024];
  int varnos[1024];
  PLpgSQL_row *row;
  int tok;

  check_assignable(initial_datum, initial_location);
  fieldnames[0] = initial_name;
  varnos[0] = initial_datum->dno;
  nfields = 1;

  while ((tok = yylex()) == ',')
  {
                                  
    if (nfields >= 1024)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many INTO variables specified"), parser_errposition(yylloc)));
    }

    tok = yylex();
    switch (tok)
    {
    case T_DATUM:
      check_assignable(yylval.wdatum.datum, yylloc);
      if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW || yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"%s\" is not a scalar variable", NameOfDatum(&(yylval.wdatum))), parser_errposition(yylloc)));
      }
      fieldnames[nfields] = NameOfDatum(&(yylval.wdatum));
      varnos[nfields++] = yylval.wdatum.datum->dno;
      break;

    default:
                                                             
      current_token_is_not_variable(tok);
    }
  }

     
                                                                
                                
     
  plpgsql_push_back_token(tok);

  row = palloc0(sizeof(PLpgSQL_row));
  row->dtype = PLPGSQL_DTYPE_ROW;
  row->refname = "(unnamed row)";
  row->lineno = plpgsql_location_to_lineno(initial_location);
  row->rowtupdesc = NULL;
  row->nfields = nfields;
  row->fieldnames = palloc(sizeof(char *) * nfields);
  row->varnos = palloc(sizeof(int) * nfields);
  while (--nfields >= 0)
  {
    row->fieldnames[nfields] = fieldnames[nfields];
    row->varnos[nfields] = varnos[nfields];
  }

  plpgsql_adddatum((PLpgSQL_datum *)row);

  return row;
}

   
                                                               
                                                                 
   
                                                                   
                                                       
   
static PLpgSQL_row *
make_scalar_list1(char *initial_name, PLpgSQL_datum *initial_datum, int lineno, int location)
{
  PLpgSQL_row *row;

  check_assignable(initial_datum, location);

  row = palloc0(sizeof(PLpgSQL_row));
  row->dtype = PLPGSQL_DTYPE_ROW;
  row->refname = "(unnamed row)";
  row->lineno = lineno;
  row->rowtupdesc = NULL;
  row->nfields = 1;
  row->fieldnames = palloc(sizeof(char *));
  row->varnos = palloc(sizeof(int));
  row->fieldnames[0] = initial_name;
  row->varnos[0] = initial_datum->dno;

  plpgsql_adddatum((PLpgSQL_datum *)row);

  return row;
}

   
                                                                       
                                                               
                                                                  
                                                                
                                                                 
                                                                     
                                                                      
               
   
                                                                     
                                                                      
                                                                   
                                                                      
                                                                    
                                                          
   
                                                                           
                                                                          
                                                                              
                                                                            
                                                                   
   
static void
check_sql_expr(const char *stmt, int location, int leaderlen)
{
  sql_error_callback_arg cbarg;
  ErrorContextCallback syntax_errcontext;
  MemoryContext oldCxt;

  if (!plpgsql_check_syntax)
  {
    return;
  }

  cbarg.location = location;
  cbarg.leaderlen = leaderlen;

  syntax_errcontext.callback = plpgsql_sql_error_callback;
  syntax_errcontext.arg = &cbarg;
  syntax_errcontext.previous = error_context_stack;
  error_context_stack = &syntax_errcontext;

  oldCxt = MemoryContextSwitchTo(plpgsql_compile_tmp_cxt);
  (void)raw_parser(stmt);
  MemoryContextSwitchTo(oldCxt);

                                       
  error_context_stack = syntax_errcontext.previous;
}

static void
plpgsql_sql_error_callback(void *arg)
{
  sql_error_callback_arg *cbarg = (sql_error_callback_arg *)arg;
  int errpos;

     
                                                                    
                                                                  
                                                     
     
  parser_errposition(cbarg->location);

     
                                                                  
                                                                       
     
  errpos = geterrposition();
  if (errpos > cbarg->leaderlen)
  {
    int myerrpos = getinternalerrposition();

    if (myerrpos > 0)                   
    {
      internalerrposition(myerrpos + errpos - cbarg->leaderlen - 1);
    }
  }

                                                                      
  errposition(0);
}

   
                                                                   
   
                                                                    
                                                                     
                                                                   
                                                                    
                                                                
   
static PLpgSQL_type *
parse_datatype(const char *string, int location)
{
  TypeName *typeName;
  Oid type_id;
  int32 typmod;
  sql_error_callback_arg cbarg;
  ErrorContextCallback syntax_errcontext;

  cbarg.location = location;
  cbarg.leaderlen = 0;

  syntax_errcontext.callback = plpgsql_sql_error_callback;
  syntax_errcontext.arg = &cbarg;
  syntax_errcontext.previous = error_context_stack;
  error_context_stack = &syntax_errcontext;

                                                                    
  typeName = typeStringToTypeName(string);
  typenameTypeIdAndMod(NULL, typeName, &type_id, &typmod);

                                       
  error_context_stack = syntax_errcontext.previous;

                                                        
  return plpgsql_build_datatype(type_id, typmod, plpgsql_curr_compile->fn_input_collation, typeName);
}

   
                                                 
   
static void
check_labels(const char *start_label, const char *end_label, int end_location)
{
  if (end_label)
  {
    if (!start_label)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("end label \"%s\" specified for unlabelled block", end_label), parser_errposition(end_location)));
    }

    if (strcmp(start_label, end_label) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("end label \"%s\" differs from block's label \"%s\"", end_label, start_label), parser_errposition(end_location)));
    }
  }
}

   
                                                                         
   
                                                                        
                                                                         
                                                                       
                                                                          
                                                                             
            
   
static PLpgSQL_expr *
read_cursor_args(PLpgSQL_var *cursor, int until, const char *expected)
{
  PLpgSQL_expr *expr;
  PLpgSQL_row *row;
  int tok;
  int argc;
  char **argv;
  StringInfoData ds;
  char *sqlstart = "SELECT ";
  bool any_named = false;

  tok = yylex();
  if (cursor->cursor_explicit_argrow < 0)
  {
                               
    if (tok == '(')
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cursor \"%s\" has no arguments", cursor->refname), parser_errposition(yylloc)));
    }

    if (tok != until)
    {
      yyerror("syntax error");
    }

    return NULL;
  }

                                     
  if (tok != '(')
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cursor \"%s\" has arguments", cursor->refname), parser_errposition(yylloc)));
  }

     
                                     
     
  row = (PLpgSQL_row *)plpgsql_Datums[cursor->cursor_explicit_argrow];
  argv = (char **)palloc0(row->nfields * sizeof(char *));

  for (argc = 0; argc < row->nfields; argc++)
  {
    PLpgSQL_expr *item;
    int endtoken;
    int argpos;
    int tok1, tok2;
    int arglocation;

                                                           
    plpgsql_peek2(&tok1, &tok2, &arglocation, NULL);
    if (tok1 == IDENT && tok2 == COLON_EQUALS)
    {
      char *argname;
      IdentifierLookup save_IdentifierLookup;

                                                                  
      save_IdentifierLookup = plpgsql_IdentifierLookup;
      plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_DECLARE;
      yylex();
      argname = yylval.str;
      plpgsql_IdentifierLookup = save_IdentifierLookup;

                                                   
      for (argpos = 0; argpos < row->nfields; argpos++)
      {
        if (strcmp(row->fieldnames[argpos], argname) == 0)
        {
          break;
        }
      }
      if (argpos == row->nfields)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cursor \"%s\" has no argument named \"%s\"", cursor->refname, argname), parser_errposition(yylloc)));
      }

         
                                                                    
                 
         
      tok2 = yylex();
      if (tok2 != COLON_EQUALS)
      {
        yyerror("syntax error");
      }

      any_named = true;
    }
    else
    {
      argpos = argc;
    }

    if (argv[argpos] != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("value for parameter \"%s\" of cursor \"%s\" specified more than once", row->fieldnames[argpos], cursor->refname), parser_errposition(arglocation)));
    }

       
                                                                      
                                                                          
                                                                 
                                                                   
                                                                        
                                                                      
            
       
    item = read_sql_construct(',', ')', 0, ",\" or \")", sqlstart, true, true, false,                  
        NULL, &endtoken);

    argv[argpos] = item->query + strlen(sqlstart);

    if (endtoken == ')' && !(argc == row->nfields - 1))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("not enough arguments for cursor \"%s\"", cursor->refname), parser_errposition(yylloc)));
    }

    if (endtoken == ',' && (argc == row->nfields - 1))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("too many arguments for cursor \"%s\"", cursor->refname), parser_errposition(yylloc)));
    }
  }

                                     
  initStringInfo(&ds);
  appendStringInfoString(&ds, sqlstart);
  for (argc = 0; argc < row->nfields; argc++)
  {
    Assert(argv[argc] != NULL);

       
                                                                        
                                                         
       
    appendStringInfoString(&ds, argv[argc]);
    if (any_named)
    {
      appendStringInfo(&ds, " AS %s", quote_identifier(row->fieldnames[argc]));
    }
    if (argc < row->nfields - 1)
    {
      appendStringInfoString(&ds, ", ");
    }
  }
  appendStringInfoChar(&ds, ';');

  expr = palloc0(sizeof(PLpgSQL_expr));
  expr->query = pstrdup(ds.data);
  expr->plan = NULL;
  expr->paramnos = NULL;
  expr->rwparam = -1;
  expr->ns = plpgsql_ns_top();
  pfree(ds.data);

                                             
  tok = yylex();
  if (tok != until)
  {
    yyerror("syntax error");
  }

  return expr;
}

   
                                 
   
static List *
read_raise_options(void)
{
  List *result = NIL;

  for (;;)
  {
    PLpgSQL_raise_option *opt;
    int tok;

    if ((tok = yylex()) == 0)
    {
      yyerror("unexpected end of function definition");
    }

    opt = (PLpgSQL_raise_option *)palloc(sizeof(PLpgSQL_raise_option));

    if (tok_is_keyword(tok, &yylval, K_ERRCODE, "errcode"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_ERRCODE;
    }
    else if (tok_is_keyword(tok, &yylval, K_MESSAGE, "message"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_MESSAGE;
    }
    else if (tok_is_keyword(tok, &yylval, K_DETAIL, "detail"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_DETAIL;
    }
    else if (tok_is_keyword(tok, &yylval, K_HINT, "hint"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_HINT;
    }
    else if (tok_is_keyword(tok, &yylval, K_COLUMN, "column"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_COLUMN;
    }
    else if (tok_is_keyword(tok, &yylval, K_CONSTRAINT, "constraint"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_CONSTRAINT;
    }
    else if (tok_is_keyword(tok, &yylval, K_DATATYPE, "datatype"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_DATATYPE;
    }
    else if (tok_is_keyword(tok, &yylval, K_TABLE, "table"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_TABLE;
    }
    else if (tok_is_keyword(tok, &yylval, K_SCHEMA, "schema"))
    {
      opt->opt_type = PLPGSQL_RAISEOPTION_SCHEMA;
    }
    else
    {
      yyerror("unrecognized RAISE statement option");
    }

    tok = yylex();
    if (tok != '=' && tok != COLON_EQUALS)
    {
      yyerror("syntax error, expected \"=\"");
    }

    opt->expr = read_sql_expression2(',', ';', ", or ;", &tok);

    result = lappend(result, opt);

    if (tok == ';')
    {
      break;
    }
  }

  return result;
}

   
                                                                              
                                                              
   
static void
check_raise_parameters(PLpgSQL_stmt_raise *stmt)
{
  char *cp;
  int expected_nparams = 0;

  if (stmt->message == NULL)
  {
    return;
  }

  for (cp = stmt->message; *cp; cp++)
  {
    if (cp[0] == '%')
    {
                                       
      if (cp[1] == '%')
      {
        cp++;
      }
      else
      {
        expected_nparams++;
      }
    }
  }

  if (expected_nparams < list_length(stmt->params))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("too many parameters specified for RAISE")));
  }
  if (expected_nparams > list_length(stmt->params))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("too few parameters specified for RAISE")));
  }
}

   
                         
   
static PLpgSQL_stmt *
make_case(int location, PLpgSQL_expr *t_expr, List *case_when_list, List *else_stmts)
{
  PLpgSQL_stmt_case *new;

  new = palloc(sizeof(PLpgSQL_stmt_case));
  new->cmd_type = PLPGSQL_STMT_CASE;
  new->lineno = plpgsql_location_to_lineno(location);
  new->stmtid = ++plpgsql_curr_compile->nstatements;
  new->t_expr = t_expr;
  new->t_varno = 0;
  new->case_when_list = case_when_list;
  new->have_else = (else_stmts != NIL);
                                      
  if (list_length(else_stmts) == 1 && linitial(else_stmts) == NULL)
  {
    new->else_stmts = NIL;
  }
  else
  {
    new->else_stmts = else_stmts;
  }

     
                                                                      
                                                                         
                                                                       
                                                                      
                                                                        
                              
     
  if (t_expr)
  {
    char varname[32];
    PLpgSQL_var *t_var;
    ListCell *l;

                                                            
    snprintf(varname, sizeof(varname), "__Case__Variable_%d__", plpgsql_nDatums);

       
                                                                   
                                                                         
       
    t_var = (PLpgSQL_var *)plpgsql_build_variable(varname, new->lineno, plpgsql_build_datatype(INT4OID, -1, InvalidOid, NULL), true);
    new->t_varno = t_var->dno;

    foreach (l, case_when_list)
    {
      PLpgSQL_case_when *cwt = (PLpgSQL_case_when *)lfirst(l);
      PLpgSQL_expr *expr = cwt->expr;
      StringInfoData ds;

                                                                          
      Assert(strncmp(expr->query, "SELECT ", 7) == 0);

                                     
      initStringInfo(&ds);

      appendStringInfo(&ds, "SELECT \"%s\" IN (%s)", varname, expr->query + 7);

      pfree(expr->query);
      expr->query = pstrdup(ds.data);
                                                                
      expr->ns = plpgsql_ns_top();

      pfree(ds.data);
    }
  }

  return (PLpgSQL_stmt *)new;
}
