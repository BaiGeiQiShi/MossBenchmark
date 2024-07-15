                                               

                                                   
 
                                                                                 
        
 
                                                                        
                                                                        
                                                                     
                                       
 
                                                                   
                                                                  
                                                                 
                                                
 
                                                                     
                                                                            

                                                                      
                                                                     
                                                                   
                                                                     
                                                                       
                                                                    
                                                                      
                                                                  
                                           
 
                                                                       
                            

                                                             
                                                            

                                                                  
                                                                
                                                                     

                                                                  
                                                                      
                                                                  
                                                                 
                                                                  
                              

                                                
#define YYBISON 30705

                            
#define YYBISON_VERSION "3.7.5"

                     
#define YYSKELETON_NAME "yacc.c"

                    
#define YYPURE 1

                    
#define YYPUSH 0

                    
#define YYPULL 1

                                                  
#define yyparse jsonpath_yyparse
#define yylex jsonpath_yylex
#define yyerror jsonpath_yyerror
#define yydebug jsonpath_yydebug
#define yynerrs jsonpath_yynerrs

                                   
#line 1 "jsonpath_gram.y"

                                                                            
   
                   
                                              
   
                                                                         
   
                                                           
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "regex/regex.h"
#include "utils/builtins.h"
#include "utils/jsonpath.h"

                                                           
typedef struct JsonPathString
{
  char *val;
  int len;
  int total;
} JsonPathString;

union YYSTYPE;

                                                    
int
jsonpath_yylex(union YYSTYPE *yylval_param);
int
jsonpath_yyparse(JsonPathParseResult **result);
void
jsonpath_yyerror(JsonPathParseResult **result, const char *message);

static JsonPathParseItem *
makeItemType(JsonPathItemType type);
static JsonPathParseItem *
makeItemString(JsonPathString *s);
static JsonPathParseItem *
makeItemVariable(JsonPathString *s);
static JsonPathParseItem *
makeItemKey(JsonPathString *s);
static JsonPathParseItem *
makeItemNumeric(JsonPathString *s);
static JsonPathParseItem *
makeItemBool(bool val);
static JsonPathParseItem *
makeItemBinary(JsonPathItemType type, JsonPathParseItem *la, JsonPathParseItem *ra);
static JsonPathParseItem *
makeItemUnary(JsonPathItemType type, JsonPathParseItem *a);
static JsonPathParseItem *
makeItemList(List *list);
static JsonPathParseItem *
makeIndexArray(List *list);
static JsonPathParseItem *
makeAny(int first, int last);
static JsonPathParseItem *
makeItemLikeRegex(JsonPathParseItem *expr, JsonPathString *pattern, JsonPathString *flags);

   
                                                                           
                                                                         
                                                                           
                                                                         
                                                                       
                             
   
#define YYMALLOC palloc
#define YYFREE pfree

#line 148 "jsonpath_gram.c"

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

                    
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
extern int jsonpath_yydebug;
#endif

                   
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype
{
  YYEMPTY = -2,
  YYEOF = 0,                                
  YYerror = 256,                    
  YYUNDEF = 257,                              
  TO_P = 258,                      
  NULL_P = 259,                      
  TRUE_P = 260,                      
  FALSE_P = 261,                      
  IS_P = 262,                      
  UNKNOWN_P = 263,                      
  EXISTS_P = 264,                      
  IDENT_P = 265,                      
  STRING_P = 266,                      
  NUMERIC_P = 267,                      
  INT_P = 268,                      
  VARIABLE_P = 269,                      
  OR_P = 270,                      
  AND_P = 271,                      
  NOT_P = 272,                      
  LESS_P = 273,                      
  LESSEQUAL_P = 274,                      
  EQUAL_P = 275,                      
  NOTEQUAL_P = 276,                      
  GREATEREQUAL_P = 277,                      
  GREATER_P = 278,                      
  ANY_P = 279,                      
  STRICT_P = 280,                      
  LAX_P = 281,                      
  LAST_P = 282,                      
  STARTS_P = 283,                      
  WITH_P = 284,                      
  LIKE_REGEX_P = 285,                      
  FLAG_P = 286,                      
  ABS_P = 287,                      
  SIZE_P = 288,                      
  TYPE_P = 289,                      
  FLOOR_P = 290,                      
  DOUBLE_P = 291,                      
  CEILING_P = 292,                      
  KEYVALUE_P = 293,                      
  UMINUS = 294                       
};
typedef enum yytokentype yytoken_kind_t;
#endif

                  
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 79 "jsonpath_gram.y"

  JsonPathString str;
  List *elems;                                 
  List *indexs;                       
  JsonPathParseItem *value;
  JsonPathParseResult *result;
  JsonPathItemType optype;
  bool boolean;
  int integer;

#line 245 "jsonpath_gram.c"
};
typedef union YYSTYPE YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

int
jsonpath_yyparse(JsonPathParseResult **result);

                   
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                                    
  YYSYMBOL_YYerror = 1,                          
  YYSYMBOL_YYUNDEF = 2,                                    
  YYSYMBOL_TO_P = 3,                            
  YYSYMBOL_NULL_P = 4,                            
  YYSYMBOL_TRUE_P = 5,                            
  YYSYMBOL_FALSE_P = 6,                            
  YYSYMBOL_IS_P = 7,                            
  YYSYMBOL_UNKNOWN_P = 8,                            
  YYSYMBOL_EXISTS_P = 9,                            
  YYSYMBOL_IDENT_P = 10,                           
  YYSYMBOL_STRING_P = 11,                           
  YYSYMBOL_NUMERIC_P = 12,                           
  YYSYMBOL_INT_P = 13,                           
  YYSYMBOL_VARIABLE_P = 14,                           
  YYSYMBOL_OR_P = 15,                           
  YYSYMBOL_AND_P = 16,                           
  YYSYMBOL_NOT_P = 17,                           
  YYSYMBOL_LESS_P = 18,                           
  YYSYMBOL_LESSEQUAL_P = 19,                           
  YYSYMBOL_EQUAL_P = 20,                           
  YYSYMBOL_NOTEQUAL_P = 21,                           
  YYSYMBOL_GREATEREQUAL_P = 22,                           
  YYSYMBOL_GREATER_P = 23,                           
  YYSYMBOL_ANY_P = 24,                           
  YYSYMBOL_STRICT_P = 25,                           
  YYSYMBOL_LAX_P = 26,                           
  YYSYMBOL_LAST_P = 27,                           
  YYSYMBOL_STARTS_P = 28,                           
  YYSYMBOL_WITH_P = 29,                           
  YYSYMBOL_LIKE_REGEX_P = 30,                           
  YYSYMBOL_FLAG_P = 31,                           
  YYSYMBOL_ABS_P = 32,                           
  YYSYMBOL_SIZE_P = 33,                           
  YYSYMBOL_TYPE_P = 34,                           
  YYSYMBOL_FLOOR_P = 35,                           
  YYSYMBOL_DOUBLE_P = 36,                           
  YYSYMBOL_CEILING_P = 37,                           
  YYSYMBOL_KEYVALUE_P = 38,                           
  YYSYMBOL_39_ = 39,                           
  YYSYMBOL_40_ = 40,                           
  YYSYMBOL_41_ = 41,                           
  YYSYMBOL_42_ = 42,                           
  YYSYMBOL_43_ = 43,                           
  YYSYMBOL_UMINUS = 44,                           
  YYSYMBOL_45_ = 45,                           
  YYSYMBOL_46_ = 46,                           
  YYSYMBOL_47_ = 47,                           
  YYSYMBOL_48_ = 48,                           
  YYSYMBOL_49_ = 49,                           
  YYSYMBOL_50_ = 50,                           
  YYSYMBOL_51_ = 51,                           
  YYSYMBOL_52_ = 52,                           
  YYSYMBOL_53_ = 53,                           
  YYSYMBOL_54_ = 54,                           
  YYSYMBOL_55_ = 55,                           
  YYSYMBOL_YYACCEPT = 56,                          
  YYSYMBOL_result = 57,                           
  YYSYMBOL_expr_or_predicate = 58,                           
  YYSYMBOL_mode = 59,                           
  YYSYMBOL_scalar_value = 60,                           
  YYSYMBOL_comp_op = 61,                           
  YYSYMBOL_delimited_predicate = 62,                           
  YYSYMBOL_predicate = 63,                           
  YYSYMBOL_starts_with_initial = 64,                           
  YYSYMBOL_path_primary = 65,                           
  YYSYMBOL_accessor_expr = 66,                           
  YYSYMBOL_expr = 67,                           
  YYSYMBOL_index_elem = 68,                           
  YYSYMBOL_index_list = 69,                           
  YYSYMBOL_array_accessor = 70,                           
  YYSYMBOL_any_level = 71,                           
  YYSYMBOL_any_path = 72,                           
  YYSYMBOL_accessor_op = 73,                           
  YYSYMBOL_key = 74,                           
  YYSYMBOL_key_name = 75,                           
  YYSYMBOL_method = 76                            
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

                                             
typedef yytype_uint8 yy_state_t;

                                     
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

#if (!defined yyoverflow && (!defined __cplusplus || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

                                                            
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

                                                                          
#define YYSTACK_GAP_MAXIMUM (YYSIZEOF(union yyalloc) - 1)

                                                                      
                  
#define YYSTACK_BYTES(N) ((N) * (YYSIZEOF(yy_state_t) + YYSIZEOF(YYSTYPE)) + YYSTACK_GAP_MAXIMUM)

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

                                                        
#define YYFINAL 5
                                       
#define YYLAST 234

                                        
#define YYNTOKENS 56
                                        
#define YYNNTS 21
                                   
#define YYNRULES 99
                                     
#define YYNSTATES 137

                                          
#define YYMAXUTOK 294

                                                                      
                                                         
#define YYTRANSLATE(YYX) (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

                                                                      
                            
static const yytype_int8 yytranslate[] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 47, 43, 2, 2, 45, 46, 41, 39, 49, 40, 54, 42, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 55, 48, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 50, 2, 51, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 52, 2, 53, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 44};

#if YYDEBUG
                                                                     
static const yytype_int16 yyrline[] = {0, 127, 127, 133, 137, 138, 142, 143, 144, 148, 149, 150, 151, 152, 153, 154, 158, 159, 160, 161, 162, 163, 167, 168, 172, 173, 174, 175, 176, 177, 179, 181, 182, 187, 188, 192, 193, 194, 195, 199, 200, 201, 202, 206, 207, 208, 209, 210, 211, 212, 213, 214, 218, 219, 223, 224, 228, 229, 233, 234, 238, 239, 240, 245, 246, 247, 248, 249, 250, 254, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 284, 285, 286, 287, 288, 289, 290};
#endif

                                        
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
                                                                 
                                     
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

                                                                
                                                                       
static const char *const yytname[] = {"\"end of file\"", "error", "\"invalid token\"", "TO_P", "NULL_P", "TRUE_P", "FALSE_P", "IS_P", "UNKNOWN_P", "EXISTS_P", "IDENT_P", "STRING_P", "NUMERIC_P", "INT_P", "VARIABLE_P", "OR_P", "AND_P", "NOT_P", "LESS_P", "LESSEQUAL_P", "EQUAL_P", "NOTEQUAL_P", "GREATEREQUAL_P", "GREATER_P", "ANY_P", "STRICT_P", "LAX_P", "LAST_P", "STARTS_P", "WITH_P", "LIKE_REGEX_P", "FLAG_P", "ABS_P", "SIZE_P", "TYPE_P", "FLOOR_P", "DOUBLE_P", "CEILING_P", "KEYVALUE_P", "'+'", "'-'", "'*'", "'/'", "'%'", "UMINUS", "'('", "')'", "'$'", "'@'", "','", "'['", "']'", "'{'", "'}'", "'.'", "'?'", "$accept", "result", "expr_or_predicate", "mode", "scalar_value", "comp_op", "delimited_predicate", "predicate", "starts_with_initial", "path_primary", "accessor_expr", "expr", "index_elem", "index_list", "array_accessor", "any_level", "any_path", "accessor_op", "key", "key_name", "method", YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
                                                                 
                                                                    
static const yytype_int16 yytoknum[] = {0, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 43, 45, 42, 47, 37, 294, 40, 41, 36, 64, 44, 91, 93, 123, 125, 46, 63};
#endif

#define YYPACT_NINF (-46)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-100)

#define yytable_value_is_error(Yyn) 0

                                                                   
                 
static const yytype_int16 yypact[] = {67, -46, -46, 8, 51, -46, -46, -46, -46, -26, -46, -46, -46, -46, -3, -46, 113, 113, 51, -46, -46, -46, -46, -46, 17, -46, -45, 191, 113, 51, -46, 51, -46, -46, 15, 162, 51, 51, 68, 138, -25, -46, -46, -46, -46, -46, -46, -46, -46, 42, 14, 113, 113, 113, 113, 113, 113, 89, 20, 191, 29, 4, -45, 13, -46, -5, -2, -46, -23, -46, -46, -46, -46, -46, -46, -46, -46, -46, 24, -46, -46, -46, -46, -46, -46, -46, 32, 38, 49, 52, 59, 61, 69, -46, -46, -46, -46, 75, 51, 7, 74, 60, 60, -46, -46, -46, 46, -46, -46, -45, 104, -46, -46, -46, 113, 113, -46, -11, 76, 54, -46, -46, -46, 110, -46, 46, -46, -46, -46, 0, -46, -46, -46, -11, -46, 70, -46};

                                                                       
                                                                       
                                     
static const yytype_int8 yydefact[] = {8, 6, 7, 0, 0, 1, 10, 11, 12, 0, 9, 13, 14, 15, 0, 38, 0, 0, 0, 36, 37, 2, 35, 24, 5, 39, 43, 4, 0, 0, 28, 0, 45, 46, 0, 0, 0, 0, 0, 0, 0, 65, 42, 18, 20, 16, 17, 21, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 22, 44, 27, 26, 0, 52, 54, 0, 72, 73, 74, 75, 76, 77, 78, 70, 71, 60, 79, 80, 88, 89, 90, 91, 92, 81, 82, 83, 84, 85, 86, 87, 64, 66, 63, 69, 0, 0, 0, 31, 47, 48, 49, 50, 51, 25, 23, 22, 0, 0, 41, 40, 56, 0, 0, 57, 0, 0, 0, 33, 34, 30, 0, 29, 53, 55, 58, 59, 0, 67, 68, 32, 0, 61, 0, 62};

                          
static const yytype_int8 yypgoto[] = {-46, -46, -46, -46, -46, -46, 119, -14, -46, -46, -46, -4, 19, -46, -46, 3, -46, -19, -46, -46, -46};

                            
static const yytype_uint8 yydefgoto[] = {0, 3, 21, 4, 22, 56, 23, 24, 122, 25, 26, 59, 67, 68, 41, 129, 94, 111, 95, 96, 97};

                                                                    
                                                                   
                                                              
static const yytype_int16 yytable[] = {27, 114, 127, 133, 34, 38, 9, 42, 5, 39, 40, 110, 32, 33, 35, 58, 128, 60, 120, 28, 98, 121, 63, 64, 57, 100, 115, 35, 116, 37, 36, 37, 36, 37, 66, 36, 37, 51, 52, 53, 54, 55, 29, 112, 36, 37, 113, 101, 102, 103, 104, 105, 106, 134, 38, 6, 7, 8, 39, 40, 9, 61, 10, 11, 12, 13, 108, -3, 14, 36, 37, 99, 6, 7, 8, 109, 117, -93, 15, 10, 11, 12, 13, -94, 119, 51, 52, 53, 54, 55, 16, 17, 1, 2, -95, 15, 18, -96, 19, 20, 131, 53, 54, 55, -97, 123, -98, 16, 17, 65, 125, 66, 124, 31, -99, 19, 20, 6, 7, 8, 118, 132, 130, 136, 10, 11, 12, 13, 51, 52, 53, 54, 55, 30, 126, 107, 135, 0, 0, 0, 15, 69, 70, 71, 72, 73, 74, 75, 76, 77, 0, 0, 16, 17, 0, 0, 0, 0, 31, 0, 19, 20, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 0, 0, 93, 43, 44, 45, 46, 47, 48, 0, 0, 0, 0, 49, 0, 50, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 53, 54, 55, 0, 0, 62, 43, 44, 45, 46, 47, 48, 0, 0, 0, 0, 49, 0, 50, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 53, 54, 55};

static const yytype_int16 yycheck[] = {4, 3, 13, 3, 18, 50, 9, 26, 0, 54, 55, 7, 16, 17, 18, 29, 27, 31, 11, 45, 45, 14, 36, 37, 28, 11, 49, 31, 51, 16, 15, 16, 15, 16, 38, 15, 16, 39, 40, 41, 42, 43, 45, 62, 15, 16, 51, 51, 52, 53, 54, 55, 56, 53, 50, 4, 5, 6, 54, 55, 9, 46, 11, 12, 13, 14, 46, 0, 17, 15, 16, 29, 4, 5, 6, 46, 52, 45, 27, 11, 12, 13, 14, 45, 98, 39, 40, 41, 42, 43, 39, 40, 25, 26, 45, 27, 45, 45, 47, 48, 46, 41, 42, 43, 45, 31, 45, 39, 40, 41, 114, 115, 8, 45, 45, 47, 48, 4, 5, 6, 45, 11, 46, 53, 11, 12, 13, 14, 39, 40, 41, 42, 43, 14, 115, 46, 133, -1, -1, -1, 27, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1, -1, 39, 40, -1, -1, -1, -1, 45, -1, 47, 48, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, -1, -1, 41, 18, 19, 20, 21, 22, 23, -1, -1, -1, -1, 28, -1, 30, -1, -1, -1, -1, -1, -1, -1, -1, 39, 40, 41, 42, 43, -1, -1, 46, 18, 19, 20, 21, 22, 23, -1, -1, -1, -1, 28, -1, 30, -1, -1, -1, -1, -1, -1, -1, -1, 39, 40, 41, 42, 43};

                                                               
                                 
static const yytype_int8 yystos[] = {0, 25, 26, 57, 59, 0, 4, 5, 6, 9, 11, 12, 13, 14, 17, 27, 39, 40, 45, 47, 48, 58, 60, 62, 63, 65, 66, 67, 45, 45, 62, 45, 67, 67, 63, 67, 15, 16, 50, 54, 55, 70, 73, 18, 19, 20, 21, 22, 23, 28, 30, 39, 40, 41, 42, 43, 61, 67, 63, 67, 63, 46, 46, 63, 63, 41, 67, 68, 69, 3, 4, 5, 6, 7, 8, 9, 10, 11, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 41, 72, 74, 75, 76, 45, 29, 11, 67, 67, 67, 67, 67, 67, 46, 46, 46, 7, 73, 73, 51, 3, 49, 51, 52, 45, 63, 11, 14, 64, 31, 8, 67, 68, 13, 27, 71, 46, 46, 11, 3, 53, 71, 53};

                                                                  
static const yytype_int8 yyr1[] = {0, 56, 57, 57, 58, 58, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 61, 61, 61, 61, 61, 61, 62, 62, 63, 63, 63, 63, 63, 63, 63, 63, 63, 64, 64, 65, 65, 65, 65, 66, 66, 66, 66, 67, 67, 67, 67, 67, 67, 67, 67, 67, 68, 68, 69, 69, 70, 70, 71, 71, 72, 72, 72, 73, 73, 73, 73, 73, 73, 74, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 76, 76, 76, 76, 76, 76, 76};

                                                                         
static const yytype_int8 yyr2[] = {0, 2, 2, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 4, 1, 3, 3, 3, 2, 5, 4, 3, 5, 1, 1, 1, 1, 1, 1, 1, 4, 4, 2, 1, 3, 2, 2, 3, 3, 3, 3, 3, 1, 3, 1, 3, 3, 3, 1, 1, 1, 4, 6, 2, 2, 1, 2, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

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
      yyerror(result, YY_("syntax error: cannot back up"));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      YYERROR;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  while (0)

                                                      
                             
#define YYERRCODE YYUNDEF

                                     
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
#define YY_LOCATION_PRINT(File, Loc) ((void)0)
#endif

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      YYFPRINTF(stderr, "%s ", Title);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      yy_symbol_print(stderr, Kind, Value, result);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      YYFPRINTF(stderr, "\n");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                       
                                       
                                      

static void
yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep, JsonPathParseResult **result)
{
  FILE *yyoutput = yyo;
  YY_USE(yyoutput);
  YY_USE(result);
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
yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep, JsonPathParseResult **result)
{
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name(yykind));

  yy_symbol_value_print(yyo, yykind, yyvaluep, result);
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
yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule, JsonPathParseResult **result)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1, yylno);
                                   
  for (yyi = 0; yyi < yynrhs; yyi++)
  {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]), &yyvsp[(yyi + 1) - (yynrhs)], result);
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      yy_reduce_print(yyssp, yyvsp, Rule, result);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
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
yydestruct(const char *yymsg, yysymbol_kind_t yykind, YYSTYPE *yyvaluep, JsonPathParseResult **result)
{
  YY_USE(yyvaluep);
  YY_USE(result);
  if (!yymsg)
  {
    yymsg = "Deleting";
  }
  YY_SYMBOL_PRINT(yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

              
              
             

int
yyparse(JsonPathParseResult **result)
{
                              
  int yychar;

                                                    
                                                                     
                              
  YY_INITIAL_VALUE(static YYSTYPE yyval_default;)
  YYSTYPE yylval YY_INITIAL_VALUE(= yyval_default);

                                        
  int yynerrs = 0;

  yy_state_fast_t yystate = 0;
                                                                 
  int yyerrstatus = 0;

                                                                        
                                      

                    
  YYPTRDIFF_T yystacksize = YYINITDEPTH;

                                             
  yy_state_t yyssa[YYINITDEPTH];
  yy_state_t *yyss = yyssa;
  yy_state_t *yyssp = yyss;

                                                      
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp = yyvs;

  int yyn;
                                     
  int yyresult;
                               
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
                                                                       
                         
  YYSTYPE yyval;

#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N))

                                                           
                                                      
  int yylen = 0;

  YYDPRINTF((stderr, "Starting parse\n"));

  yychar = YYEMPTY;                                 
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

                                                                   
                                                                 
                                                                    
                                                   
      yyoverflow(YY_("memory exhausted"), &yyss1, yysize * YYSIZEOF(*yyssp), &yyvs1, yysize * YYSIZEOF(*yyvsp), &yystacksize);
      yyss = yyss1;
      yyvs = yyvs1;
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
#undef YYSTACK_RELOCATE
      if (yyss1 != yyssa)
      {
        YYSTACK_FREE(yyss1);
      }
    }
#endif

    yyssp = yyss + yysize - 1;
    yyvsp = yyvs + yysize - 1;

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
    yychar = yylex(&yylval);
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

  YY_REDUCE_PRINT(yyn);
  switch (yyn)
  {
  case 2:                                      
#line 127 "jsonpath_gram.y"
  {
    *result = palloc(sizeof(JsonPathParseResult));
    (*result)->expr = (yyvsp[0].value);
    (*result)->lax = (yyvsp[-1].boolean);
    (void)yynerrs;
  }
#line 1439 "jsonpath_gram.c"
  break;

  case 3:                      
#line 133 "jsonpath_gram.y"
  {
    *result = NULL;
  }
#line 1445 "jsonpath_gram.c"
  break;

  case 4:                               
#line 137 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1451 "jsonpath_gram.c"
  break;

  case 5:                                    
#line 138 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1457 "jsonpath_gram.c"
  break;

  case 6:                      
#line 142 "jsonpath_gram.y"
  {
    (yyval.boolean) = false;
  }
#line 1463 "jsonpath_gram.c"
  break;

  case 7:                   
#line 143 "jsonpath_gram.y"
  {
    (yyval.boolean) = true;
  }
#line 1469 "jsonpath_gram.c"
  break;

  case 8:                    
#line 144 "jsonpath_gram.y"
  {
    (yyval.boolean) = true;
  }
#line 1475 "jsonpath_gram.c"
  break;

  case 9:                              
#line 148 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemString(&(yyvsp[0].str));
  }
#line 1481 "jsonpath_gram.c"
  break;

  case 10:                            
#line 149 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemString(NULL);
  }
#line 1487 "jsonpath_gram.c"
  break;

  case 11:                            
#line 150 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBool(true);
  }
#line 1493 "jsonpath_gram.c"
  break;

  case 12:                             
#line 151 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBool(false);
  }
#line 1499 "jsonpath_gram.c"
  break;

  case 13:                               
#line 152 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemNumeric(&(yyvsp[0].str));
  }
#line 1505 "jsonpath_gram.c"
  break;

  case 14:                           
#line 153 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemNumeric(&(yyvsp[0].str));
  }
#line 1511 "jsonpath_gram.c"
  break;

  case 15:                                
#line 154 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemVariable(&(yyvsp[0].str));
  }
#line 1517 "jsonpath_gram.c"
  break;

  case 16:                        
#line 158 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiEqual;
  }
#line 1523 "jsonpath_gram.c"
  break;

  case 17:                           
#line 159 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiNotEqual;
  }
#line 1529 "jsonpath_gram.c"
  break;

  case 18:                       
#line 160 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiLess;
  }
#line 1535 "jsonpath_gram.c"
  break;

  case 19:                          
#line 161 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiGreater;
  }
#line 1541 "jsonpath_gram.c"
  break;

  case 20:                            
#line 162 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiLessOrEqual;
  }
#line 1547 "jsonpath_gram.c"
  break;

  case 21:                               
#line 163 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiGreaterOrEqual;
  }
#line 1553 "jsonpath_gram.c"
  break;

  case 22:                                              
#line 167 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[-1].value);
  }
#line 1559 "jsonpath_gram.c"
  break;

  case 23:                                                  
#line 168 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemUnary(jpiExists, (yyvsp[-1].value));
  }
#line 1565 "jsonpath_gram.c"
  break;

  case 24:                                      
#line 172 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1571 "jsonpath_gram.c"
  break;

  case 25:                                    
#line 173 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary((yyvsp[-1].optype), (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1577 "jsonpath_gram.c"
  break;

  case 26:                                            
#line 174 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiAnd, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1583 "jsonpath_gram.c"
  break;

  case 27:                                           
#line 175 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiOr, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1589 "jsonpath_gram.c"
  break;

  case 28:                                            
#line 176 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemUnary(jpiNot, (yyvsp[0].value));
  }
#line 1595 "jsonpath_gram.c"
  break;

  case 29:                                                   
#line 178 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemUnary(jpiIsUnknown, (yyvsp[-3].value));
  }
#line 1601 "jsonpath_gram.c"
  break;

  case 30:                                                           
#line 180 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiStartsWith, (yyvsp[-3].value), (yyvsp[0].value));
  }
#line 1607 "jsonpath_gram.c"
  break;

  case 31:                                             
#line 181 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemLikeRegex((yyvsp[-2].value), &(yyvsp[0].str), NULL);
  }
#line 1613 "jsonpath_gram.c"
  break;

  case 32:                                                             
#line 183 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemLikeRegex((yyvsp[-4].value), &(yyvsp[-2].str), &(yyvsp[0].str));
  }
#line 1619 "jsonpath_gram.c"
  break;

  case 33:                                     
#line 187 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemString(&(yyvsp[0].str));
  }
#line 1625 "jsonpath_gram.c"
  break;

  case 34:                                       
#line 188 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemVariable(&(yyvsp[0].str));
  }
#line 1631 "jsonpath_gram.c"
  break;

  case 35:                                  
#line 192 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1637 "jsonpath_gram.c"
  break;

  case 36:                         
#line 193 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemType(jpiRoot);
  }
#line 1643 "jsonpath_gram.c"
  break;

  case 37:                         
#line 194 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemType(jpiCurrent);
  }
#line 1649 "jsonpath_gram.c"
  break;

  case 38:                            
#line 195 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemType(jpiLast);
  }
#line 1655 "jsonpath_gram.c"
  break;

  case 39:                                   
#line 199 "jsonpath_gram.y"
  {
    (yyval.elems) = list_make1((yyvsp[0].value));
  }
#line 1661 "jsonpath_gram.c"
  break;

  case 40:                                               
#line 200 "jsonpath_gram.y"
  {
    (yyval.elems) = list_make2((yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1667 "jsonpath_gram.c"
  break;

  case 41:                                                    
#line 201 "jsonpath_gram.y"
  {
    (yyval.elems) = list_make2((yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1673 "jsonpath_gram.c"
  break;

  case 42:                                                
#line 202 "jsonpath_gram.y"
  {
    (yyval.elems) = lappend((yyvsp[-1].elems), (yyvsp[0].value));
  }
#line 1679 "jsonpath_gram.c"
  break;

  case 43:                           
#line 206 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemList((yyvsp[0].elems));
  }
#line 1685 "jsonpath_gram.c"
  break;

  case 44:                          
#line 207 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[-1].value);
  }
#line 1691 "jsonpath_gram.c"
  break;

  case 45:                      
#line 208 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemUnary(jpiPlus, (yyvsp[0].value));
  }
#line 1697 "jsonpath_gram.c"
  break;

  case 46:                      
#line 209 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemUnary(jpiMinus, (yyvsp[0].value));
  }
#line 1703 "jsonpath_gram.c"
  break;

  case 47:                           
#line 210 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiAdd, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1709 "jsonpath_gram.c"
  break;

  case 48:                           
#line 211 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiSub, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1715 "jsonpath_gram.c"
  break;

  case 49:                           
#line 212 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiMul, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1721 "jsonpath_gram.c"
  break;

  case 50:                           
#line 213 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiDiv, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1727 "jsonpath_gram.c"
  break;

  case 51:                           
#line 214 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiMod, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1733 "jsonpath_gram.c"
  break;

  case 52:                        
#line 218 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiSubscript, (yyvsp[0].value), NULL);
  }
#line 1739 "jsonpath_gram.c"
  break;

  case 53:                                  
#line 219 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemBinary(jpiSubscript, (yyvsp[-2].value), (yyvsp[0].value));
  }
#line 1745 "jsonpath_gram.c"
  break;

  case 54:                              
#line 223 "jsonpath_gram.y"
  {
    (yyval.indexs) = list_make1((yyvsp[0].value));
  }
#line 1751 "jsonpath_gram.c"
  break;

  case 55:                                             
#line 224 "jsonpath_gram.y"
  {
    (yyval.indexs) = lappend((yyvsp[-2].indexs), (yyvsp[0].value));
  }
#line 1757 "jsonpath_gram.c"
  break;

  case 56:                                   
#line 228 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemType(jpiAnyArray);
  }
#line 1763 "jsonpath_gram.c"
  break;

  case 57:                                          
#line 229 "jsonpath_gram.y"
  {
    (yyval.value) = makeIndexArray((yyvsp[-1].indexs));
  }
#line 1769 "jsonpath_gram.c"
  break;

  case 58:                        
#line 233 "jsonpath_gram.y"
  {
    (yyval.integer) = pg_atoi((yyvsp[0].str).val, 4, 0);
  }
#line 1775 "jsonpath_gram.c"
  break;

  case 59:                         
#line 234 "jsonpath_gram.y"
  {
    (yyval.integer) = -1;
  }
#line 1781 "jsonpath_gram.c"
  break;

  case 60:                       
#line 238 "jsonpath_gram.y"
  {
    (yyval.value) = makeAny(0, -1);
  }
#line 1787 "jsonpath_gram.c"
  break;

  case 61:                                         
#line 239 "jsonpath_gram.y"
  {
    (yyval.value) = makeAny((yyvsp[-1].integer), (yyvsp[-1].integer));
  }
#line 1793 "jsonpath_gram.c"
  break;

  case 62:                                                        
#line 241 "jsonpath_gram.y"
  {
    (yyval.value) = makeAny((yyvsp[-3].integer), (yyvsp[-1].integer));
  }
#line 1799 "jsonpath_gram.c"
  break;

  case 63:                            
#line 245 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1805 "jsonpath_gram.c"
  break;

  case 64:                            
#line 246 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemType(jpiAnyKey);
  }
#line 1811 "jsonpath_gram.c"
  break;

  case 65:                                   
#line 247 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1817 "jsonpath_gram.c"
  break;

  case 66:                                 
#line 248 "jsonpath_gram.y"
  {
    (yyval.value) = (yyvsp[0].value);
  }
#line 1823 "jsonpath_gram.c"
  break;

  case 67:                                       
#line 249 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemType((yyvsp[-2].optype));
  }
#line 1829 "jsonpath_gram.c"
  break;

  case 68:                                          
#line 250 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemUnary(jpiFilter, (yyvsp[-1].value));
  }
#line 1835 "jsonpath_gram.c"
  break;

  case 69:                     
#line 254 "jsonpath_gram.y"
  {
    (yyval.value) = makeItemKey(&(yyvsp[0].str));
  }
#line 1841 "jsonpath_gram.c"
  break;

  case 93:                     
#line 284 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiAbs;
  }
#line 1847 "jsonpath_gram.c"
  break;

  case 94:                      
#line 285 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiSize;
  }
#line 1853 "jsonpath_gram.c"
  break;

  case 95:                      
#line 286 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiType;
  }
#line 1859 "jsonpath_gram.c"
  break;

  case 96:                       
#line 287 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiFloor;
  }
#line 1865 "jsonpath_gram.c"
  break;

  case 97:                        
#line 288 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiDouble;
  }
#line 1871 "jsonpath_gram.c"
  break;

  case 98:                         
#line 289 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiCeiling;
  }
#line 1877 "jsonpath_gram.c"
  break;

  case 99:                          
#line 290 "jsonpath_gram.y"
  {
    (yyval.optype) = jpiKeyValue;
  }
#line 1883 "jsonpath_gram.c"
  break;

#line 1887 "jsonpath_gram.c"

  default:
    break;
  }
                                                                     
                                                                    
                                                                      
                                                                      
                                                                         
                                                                        
                                                                     
                                                                     
                                                                       
                                                                     
                                            
  YY_SYMBOL_PRINT("-> $$ =", YY_CAST(yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK(yylen);
  yylen = 0;

  *++yyvsp = yyval;

                                                                    
                                                                     
                           
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
    yyerror(result, YY_("syntax error"));
  }

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
      yydestruct("Error: discarding", yytoken, &yylval, result);
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

    yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp, result);
    YYPOPSTACK(1);
    yystate = *yyssp;
    YY_STACK_PRINT(yyss, yyssp);
  }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

                               
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
  yyerror(result, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif

                                                           
                                                           
                                                          
yyreturn:
  if (yychar != YYEMPTY)
  {
                                                                        
                                                           
    yytoken = YYTRANSLATE(yychar);
    yydestruct("Cleanup: discarding lookahead", yytoken, &yylval, result);
  }
                                                                   
                                  
  YYPOPSTACK(yylen);
  YY_STACK_PRINT(yyss, yyssp);
  while (yyssp != yyss)
  {
    yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp, result);
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

#line 292 "jsonpath_gram.y"

   
                                                                               
          
   

static JsonPathParseItem *
makeItemType(JsonPathItemType type)
{
  JsonPathParseItem *v = palloc(sizeof(*v));

  CHECK_FOR_INTERRUPTS();

  v->type = type;
  v->next = NULL;

  return v;
}

static JsonPathParseItem *
makeItemString(JsonPathString *s)
{
  JsonPathParseItem *v;

  if (s == NULL)
  {
    v = makeItemType(jpiNull);
  }
  else
  {
    v = makeItemType(jpiString);
    v->value.string.val = s->val;
    v->value.string.len = s->len;
  }

  return v;
}

static JsonPathParseItem *
makeItemVariable(JsonPathString *s)
{
  JsonPathParseItem *v;

  v = makeItemType(jpiVariable);
  v->value.string.val = s->val;
  v->value.string.len = s->len;

  return v;
}

static JsonPathParseItem *
makeItemKey(JsonPathString *s)
{
  JsonPathParseItem *v;

  v = makeItemString(s);
  v->type = jpiKey;

  return v;
}

static JsonPathParseItem *
makeItemNumeric(JsonPathString *s)
{
  JsonPathParseItem *v;

  v = makeItemType(jpiNumeric);
  v->value.numeric = DatumGetNumeric(DirectFunctionCall3(numeric_in, CStringGetDatum(s->val), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1)));

  return v;
}

static JsonPathParseItem *
makeItemBool(bool val)
{
  JsonPathParseItem *v = makeItemType(jpiBool);

  v->value.boolean = val;

  return v;
}

static JsonPathParseItem *
makeItemBinary(JsonPathItemType type, JsonPathParseItem *la, JsonPathParseItem *ra)
{
  JsonPathParseItem *v = makeItemType(type);

  v->value.args.left = la;
  v->value.args.right = ra;

  return v;
}

static JsonPathParseItem *
makeItemUnary(JsonPathItemType type, JsonPathParseItem *a)
{
  JsonPathParseItem *v;

  if (type == jpiPlus && a->type == jpiNumeric && !a->next)
  {
    return a;
  }

  if (type == jpiMinus && a->type == jpiNumeric && !a->next)
  {
    v = makeItemType(jpiNumeric);
    v->value.numeric = DatumGetNumeric(DirectFunctionCall1(numeric_uminus, NumericGetDatum(a->value.numeric)));
    return v;
  }

  v = makeItemType(type);

  v->value.arg = a;

  return v;
}

static JsonPathParseItem *
makeItemList(List *list)
{
  JsonPathParseItem *head, *end;
  ListCell *cell = list_head(list);

  head = end = (JsonPathParseItem *)lfirst(cell);

  if (!lnext(cell))
  {
    return head;
  }

                                                        
  while (end->next)
  {
    end = end->next;
  }

  for_each_cell(cell, lnext(cell))
  {
    JsonPathParseItem *c = (JsonPathParseItem *)lfirst(cell);

    end->next = c;
    end = c;
  }

  return head;
}

static JsonPathParseItem *
makeIndexArray(List *list)
{
  JsonPathParseItem *v = makeItemType(jpiIndexArray);
  ListCell *cell;
  int i = 0;

  Assert(list_length(list) > 0);
  v->value.array.nelems = list_length(list);

  v->value.array.elems = palloc(sizeof(v->value.array.elems[0]) * v->value.array.nelems);

  foreach (cell, list)
  {
    JsonPathParseItem *jpi = lfirst(cell);

    Assert(jpi->type == jpiSubscript);

    v->value.array.elems[i].from = jpi->value.args.left;
    v->value.array.elems[i++].to = jpi->value.args.right;
  }

  return v;
}

static JsonPathParseItem *
makeAny(int first, int last)
{
  JsonPathParseItem *v = makeItemType(jpiAny);

  v->value.anybounds.first = (first >= 0) ? first : PG_UINT32_MAX;
  v->value.anybounds.last = (last >= 0) ? last : PG_UINT32_MAX;

  return v;
}

static JsonPathParseItem *
makeItemLikeRegex(JsonPathParseItem *expr, JsonPathString *pattern, JsonPathString *flags)
{
  JsonPathParseItem *v = makeItemType(jpiLikeRegex);
  int i;
  int cflags;

  v->value.like_regex.expr = expr;
  v->value.like_regex.pattern = pattern->val;
  v->value.like_regex.patternlen = pattern->len;

                                                                            
  v->value.like_regex.flags = 0;
  for (i = 0; flags && i < flags->len; i++)
  {
    switch (flags->val[i])
    {
    case 'i':
      v->value.like_regex.flags |= JSP_REGEX_ICASE;
      break;
    case 's':
      v->value.like_regex.flags |= JSP_REGEX_DOTALL;
      break;
    case 'm':
      v->value.like_regex.flags |= JSP_REGEX_MLINE;
      break;
    case 'x':
      v->value.like_regex.flags |= JSP_REGEX_WSPACE;
      break;
    case 'q':
      v->value.like_regex.flags |= JSP_REGEX_QUOTE;
      break;
    default:
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid input syntax for type %s", "jsonpath"), errdetail("unrecognized flag character \"%c\" in LIKE_REGEX predicate", flags->val[i])));
      break;
    }
  }

                                                        
  cflags = jspConvertRegexFlags(v->value.like_regex.flags);

                            
  (void)RE_compile_and_cache(cstring_to_text_with_len(pattern->val, pattern->len), cflags, DEFAULT_COLLATION_OID);

  return v;
}

   
                                                                             
   
int
jspConvertRegexFlags(uint32 xflags)
{
                                                                    
  int cflags = REG_ADVANCED;

                                                                   
  if (xflags & JSP_REGEX_ICASE)
  {
    cflags |= REG_ICASE;
  }

                                                                           
  if (xflags & JSP_REGEX_QUOTE)
  {
    cflags &= ~REG_ADVANCED;
    cflags |= REG_QUOTE;
  }
  else
  {
                                                       
    if (!(xflags & JSP_REGEX_DOTALL))
    {
      cflags |= REG_NLSTOP;
    }
    if (xflags & JSP_REGEX_MLINE)
    {
      cflags |= REG_NLANCH;
    }

       
                                                                         
                                                                       
                                                                        
                                                                   
                                            
       
    if (xflags & JSP_REGEX_WSPACE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("XQuery \"x\" flag (expanded regular expressions) is not implemented")));
    }
  }

  return cflags;
}

   
                                                                               
                                                                             
                                                                               
                                                                      
                 
   

#include "jsonpath_scan.c"
