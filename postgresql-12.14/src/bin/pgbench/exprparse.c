                                               

                                                   
 
                                                                                 
        
 
                                                                        
                                                                        
                                                                     
                                       
 
                                                                   
                                                                  
                                                                 
                                                
 
                                                                     
                                                                            

                                                                      
                                                                     
                                                                   
                                                                     
                                                                       
                                                                    
                                                                      
                                                                  
                                           
 
                                                                       
                            

                                                             
                                                            

                                                                  
                                                                
                                                                     

                                                                  
                                                                      
                                                                  
                                                                 
                                                                  
                              

                                                
#define YYBISON 30705

                            
#define YYBISON_VERSION "3.7.5"

                     
#define YYSKELETON_NAME "yacc.c"

                    
#define YYPURE 1

                    
#define YYPUSH 0

                    
#define YYPULL 1

                                                  
#define yyparse expr_yyparse
#define yylex expr_yylex
#define yyerror expr_yyerror
#define yydebug expr_yydebug
#define yynerrs expr_yynerrs

                                   
#line 1 "exprparse.y"

                                                                            
   
               
                                                  
   
                                                                         
                                                                        
   
                               
   
                                                                            
   

#include "postgres_fe.h"

#include "pgbench.h"

#define PGBENCH_NARGS_VARIABLE (-1)
#define PGBENCH_NARGS_CASE (-2)
#define PGBENCH_NARGS_HASH (-3)

PgBenchExpr *expr_parse_result;

static PgBenchExprList *
make_elist(PgBenchExpr *exp, PgBenchExprList *list);
static PgBenchExpr *
make_null_constant(void);
static PgBenchExpr *
make_boolean_constant(bool bval);
static PgBenchExpr *
make_integer_constant(int64 ival);
static PgBenchExpr *
make_double_constant(double dval);
static PgBenchExpr *
make_variable(char *varname);
static PgBenchExpr *
make_op(yyscan_t yyscanner, const char *operator, PgBenchExpr * lexpr, PgBenchExpr *rexpr);
static PgBenchExpr *
make_uop(yyscan_t yyscanner, const char *operator, PgBenchExpr * expr);
static int
find_func(yyscan_t yyscanner, const char *fname);
static PgBenchExpr *
make_func(yyscan_t yyscanner, int fnumber, PgBenchExprList *args);
static PgBenchExpr *
make_case(yyscan_t yyscanner, PgBenchExprList *when_then_list, PgBenchExpr *else_part);

#line 115 "exprparse.c"

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
extern int expr_yydebug;
#endif

                   
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype
{
  YYEMPTY = -2,
  YYEOF = 0,                                       
  YYerror = 256,                           
  YYUNDEF = 257,                                     
  NULL_CONST = 258,                             
  INTEGER_CONST = 259,                             
  MAXINT_PLUS_ONE_CONST = 260,                             
  DOUBLE_CONST = 261,                             
  BOOLEAN_CONST = 262,                             
  VARIABLE = 263,                             
  FUNCTION = 264,                             
  AND_OP = 265,                             
  OR_OP = 266,                             
  NOT_OP = 267,                             
  NE_OP = 268,                             
  LE_OP = 269,                             
  GE_OP = 270,                             
  LS_OP = 271,                             
  RS_OP = 272,                             
  IS_OP = 273,                             
  CASE_KW = 274,                             
  WHEN_KW = 275,                             
  THEN_KW = 276,                             
  ELSE_KW = 277,                             
  END_KW = 278,                             
  ISNULL_OP = 279,                             
  NOTNULL_OP = 280,                             
  UNARY = 281                              
};
typedef enum yytokentype yytoken_kind_t;
#endif

                  
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 48 "exprparse.y"

  int64 ival;
  double dval;
  bool bval;
  char *str;
  PgBenchExpr *expr;
  PgBenchExprList *elist;

#line 197 "exprparse.c"
};
typedef union YYSTYPE YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

int
expr_yyparse(yyscan_t yyscanner);

                   
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                                     
  YYSYMBOL_YYerror = 1,                           
  YYSYMBOL_YYUNDEF = 2,                                     
  YYSYMBOL_NULL_CONST = 3,                             
  YYSYMBOL_INTEGER_CONST = 4,                             
  YYSYMBOL_MAXINT_PLUS_ONE_CONST = 5,                             
  YYSYMBOL_DOUBLE_CONST = 6,                             
  YYSYMBOL_BOOLEAN_CONST = 7,                             
  YYSYMBOL_VARIABLE = 8,                             
  YYSYMBOL_FUNCTION = 9,                             
  YYSYMBOL_AND_OP = 10,                            
  YYSYMBOL_OR_OP = 11,                            
  YYSYMBOL_NOT_OP = 12,                            
  YYSYMBOL_NE_OP = 13,                            
  YYSYMBOL_LE_OP = 14,                            
  YYSYMBOL_GE_OP = 15,                            
  YYSYMBOL_LS_OP = 16,                            
  YYSYMBOL_RS_OP = 17,                            
  YYSYMBOL_IS_OP = 18,                            
  YYSYMBOL_CASE_KW = 19,                            
  YYSYMBOL_WHEN_KW = 20,                            
  YYSYMBOL_THEN_KW = 21,                            
  YYSYMBOL_ELSE_KW = 22,                            
  YYSYMBOL_END_KW = 23,                            
  YYSYMBOL_ISNULL_OP = 24,                            
  YYSYMBOL_NOTNULL_OP = 25,                            
  YYSYMBOL_26_ = 26,                            
  YYSYMBOL_27_ = 27,                            
  YYSYMBOL_28_ = 28,                            
  YYSYMBOL_29_ = 29,                            
  YYSYMBOL_30_ = 30,                            
  YYSYMBOL_31_ = 31,                            
  YYSYMBOL_32_ = 32,                            
  YYSYMBOL_33_ = 33,                            
  YYSYMBOL_34_ = 34,                            
  YYSYMBOL_35_ = 35,                            
  YYSYMBOL_36_ = 36,                            
  YYSYMBOL_37_ = 37,                            
  YYSYMBOL_UNARY = 38,                            
  YYSYMBOL_39_ = 39,                            
  YYSYMBOL_40_ = 40,                            
  YYSYMBOL_41_ = 41,                            
  YYSYMBOL_YYACCEPT = 42,                           
  YYSYMBOL_result = 43,                            
  YYSYMBOL_elist = 44,                            
  YYSYMBOL_expr = 45,                            
  YYSYMBOL_when_then_list = 46,                            
  YYSYMBOL_case_control = 47,                            
  YYSYMBOL_function = 48                             
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

                                             
typedef yytype_int8 yy_state_t;

                                     
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

                                                        
#define YYFINAL 25
                                       
#define YYLAST 320

                                        
#define YYNTOKENS 42
                                        
#define YYNNTS 7
                                   
#define YYNRULES 47
                                     
#define YYNSTATES 88

                                          
#define YYMAXUTOK 281

                                                                      
                                                         
#define YYTRANSLATE(YYX) (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

                                                                      
                            
static const yytype_int8 yytranslate[] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 30, 2, 37, 31, 2, 40, 41, 35, 33, 39, 34, 2, 36, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 26, 28, 27, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 29, 2, 32, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 38};

#if YYDEBUG
                                                                     
static const yytype_uint8 yyrline[] = {0, 82, 82, 87, 88, 89, 92, 93, 95, 98, 101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 123, 124, 128, 129, 134, 138, 144, 145, 146, 147, 149, 150, 151, 155, 156, 159, 160, 162};
#endif

                                        
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
                                                                 
                                     
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

                                                                
                                                                       
static const char *const yytname[] = {"\"end of file\"", "error", "\"invalid token\"", "NULL_CONST", "INTEGER_CONST", "MAXINT_PLUS_ONE_CONST", "DOUBLE_CONST", "BOOLEAN_CONST", "VARIABLE", "FUNCTION", "AND_OP", "OR_OP", "NOT_OP", "NE_OP", "LE_OP", "GE_OP", "LS_OP", "RS_OP", "IS_OP", "CASE_KW", "WHEN_KW", "THEN_KW", "ELSE_KW", "END_KW", "ISNULL_OP", "NOTNULL_OP", "'<'", "'>'", "'='", "'|'", "'#'", "'&'", "'~'", "'+'", "'-'", "'*'", "'/'", "'%'", "UNARY", "','", "'('", "')'", "$accept", "result", "elist", "expr", "when_then_list", "case_control", "function", YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
                                                                 
                                                                    
static const yytype_int16 yytoknum[] = {0, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 60, 62, 61, 124, 35, 38, 126, 43, 45, 42, 47, 37, 281, 44, 40, 41};
#endif

#define YYPACT_NINF (-33)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) ((Yyn) == YYTABLE_NINF)

                                                                   
                 
static const yytype_int16 yypact[] = {64, -33, -33, -33, -33, -33, -33, 64, -19, 64, 64, 46, 64, 13, 205, -33, -22, 258, 64, -6, 11, -33, -33, -33, 92, -33, 64, 64, 64, 64, 64, 64, 64, 3, -33, -33, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 121, 64, 64, -33, -33, 258, 233, 283, 283, 283, 11, 11, -33, -33, 5, 283, 283, 283, 11, 11, 11, -9, -9, -33, -33, -33, -32, 205, 64, 149, 177, -33, -33, 64, -33, 205, 64, -33, 205, 205};

                                                                       
                                                                       
                                     
static const yytype_int8 yydefact[] = {0, 36, 38, 39, 37, 40, 47, 0, 0, 0, 0, 0, 0, 0, 2, 42, 0, 11, 0, 0, 10, 7, 9, 8, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 30, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 45, 6, 28, 29, 22, 18, 20, 26, 27, 32, 34, 0, 17, 19, 21, 24, 25, 23, 12, 13, 14, 15, 16, 0, 4, 0, 0, 0, 33, 35, 0, 41, 44, 0, 46, 5, 43};

                          
static const yytype_int8 yypgoto[] = {-33, -33, -33, -7, -33, -33, -33};

                            
static const yytype_int8 yydefgoto[] = {0, 13, 74, 14, 19, 15, 16};

                                                                    
                                                                   
                                                              
static const yytype_int8 yytable[] = {17, 18, 20, 21, 23, 24, 60, 81, 79, 82, 61, 48, 80, 25, 49, 62, 50, 51, 47, 53, 54, 55, 56, 57, 58, 59, 44, 45, 46, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 75, 0, 77, 78, 42, 43, 44, 45, 46, 1, 2, 22, 3, 4, 5, 6, 0, 0, 7, 0, 0, 0, 0, 0, 0, 8, 0, 1, 2, 83, 3, 4, 5, 6, 86, 0, 7, 87, 9, 10, 11, 0, 0, 8, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 10, 11, 0, 0, 0, 26, 27, 12, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 0, 34, 35, 36, 37, 38, 39, 40, 41, 0, 42, 43, 44, 45, 46, 0, 26, 27, 52, 28, 29, 30, 31, 32, 33, 0, 0, 76, 0, 0, 34, 35, 36, 37, 38, 39, 40, 41, 0, 42, 43, 44, 45, 46, 26, 27, 0, 28, 29, 30, 31, 32, 33, 0, 0, 84, 0, 0, 34, 35, 36, 37, 38, 39, 40, 41, 0, 42, 43, 44, 45, 46, 26, 27, 0, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 85, 34, 35, 36, 37, 38, 39, 40, 41, 0, 42, 43, 44, 45, 46, 26, 27, 0, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 0, 34, 35, 36, 37, 38, 39, 40, 41, 0, 42, 43, 44, 45, 46, 26, 0, 0, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 0, 34, 35, 36, 37,
    38, 39, 40, 41, 0, 42, 43, 44, 45, 46, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 0, 34, 35, 36, 37, 38, 39, 40, 41, 0, 42, 43, 44, 45, 46, -1, -1, -1, 31, 32, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, 39, 40, 41, 0, 42, 43, 44, 45, 46};

static const yytype_int8 yycheck[] = {7, 20, 9, 10, 11, 12, 3, 39, 3, 41, 7, 18, 7, 0, 20, 12, 22, 23, 40, 26, 27, 28, 29, 30, 31, 32, 35, 36, 37, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1, 49, 50, 33, 34, 35, 36, 37, 3, 4, 5, 6, 7, 8, 9, -1, -1, 12, -1, -1, -1, -1, -1, -1, 19, -1, 3, 4, 76, 6, 7, 8, 9, 81, -1, 12, 84, 32, 33, 34, -1, -1, 19, -1, -1, 40, -1, -1, -1, -1, -1, -1, -1, -1, -1, 32, 33, 34, -1, -1, -1, 10, 11, 40, 13, 14, 15, 16, 17, 18, -1, -1, -1, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, -1, 10, 11, 41, 13, 14, 15, 16, 17, 18, -1, -1, 21, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, 10, 11, -1, 13, 14, 15, 16, 17, 18, -1, -1, 21, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, 10, 11, -1, 13, 14, 15, 16, 17, 18, -1, -1, -1, -1, 23, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, 10, 11, -1, 13, 14, 15, 16, 17, 18, -1, -1, -1, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, 10, -1,
    -1, 13, 14, 15, 16, 17, 18, -1, -1, -1, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, 13, 14, 15, 16, 17, 18, -1, -1, -1, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, -1, 33, 34, 35, 36, 37};

                                                               
                                 
static const yytype_int8 yystos[] = {0, 3, 4, 6, 7, 8, 9, 12, 19, 32, 33, 34, 40, 43, 45, 47, 48, 45, 20, 46, 45, 45, 5, 45, 45, 0, 10, 11, 13, 14, 15, 16, 17, 18, 24, 25, 26, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 40, 45, 20, 22, 23, 41, 45, 45, 45, 45, 45, 45, 45, 3, 7, 12, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 45, 21, 45, 45, 3, 7, 39, 41, 45, 21, 23, 45, 45};

                                                                  
static const yytype_int8 yyr1[] = {0, 42, 43, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 47, 47, 48};

                                                                         
static const yytype_int8 yyr2[] = {0, 2, 1, 0, 1, 3, 3, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 4, 3, 4, 1, 1, 1, 1, 1, 4, 1, 5, 4, 3, 5, 1};

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
      yyerror(yyscanner, YY_("syntax error: cannot back up"));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
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
      yy_symbol_print(stderr, Kind, Value, yyscanner);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      YYFPRINTF(stderr, "\n");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                       
                                       
                                      

static void
yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep, yyscan_t yyscanner)
{
  FILE *yyoutput = yyo;
  YY_USE(yyoutput);
  YY_USE(yyscanner);
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
yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep, yyscan_t yyscanner)
{
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name(yykind));

  yy_symbol_value_print(yyo, yykind, yyvaluep, yyscanner);
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
yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule, yyscan_t yyscanner)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1, yylno);
                                   
  for (yyi = 0; yyi < yynrhs; yyi++)
  {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]), &yyvsp[(yyi + 1) - (yynrhs)], yyscanner);
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      yy_reduce_print(yyssp, yyvsp, Rule, yyscanner);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
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
yydestruct(const char *yymsg, yysymbol_kind_t yykind, YYSTYPE *yyvaluep, yyscan_t yyscanner)
{
  YY_USE(yyvaluep);
  YY_USE(yyscanner);
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
yyparse(yyscan_t yyscanner)
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
    yychar = yylex(&yylval, yyscanner);
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
#line 82 "exprparse.y"
  {
    expr_parse_result = (yyvsp[0].expr);
    (void)yynerrs;                                
  }
#line 1338 "exprparse.c"
  break;

  case 3:                     
#line 87 "exprparse.y"
  {
    (yyval.elist) = NULL;
  }
#line 1344 "exprparse.c"
  break;

  case 4:                   
#line 88 "exprparse.y"
  {
    (yyval.elist) = make_elist((yyvsp[0].expr), NULL);
  }
#line 1350 "exprparse.c"
  break;

  case 5:                             
#line 89 "exprparse.y"
  {
    (yyval.elist) = make_elist((yyvsp[0].expr), (yyvsp[-2].elist));
  }
#line 1356 "exprparse.c"
  break;

  case 6:                          
#line 92 "exprparse.y"
  {
    (yyval.expr) = (yyvsp[-1].expr);
  }
#line 1362 "exprparse.c"
  break;

  case 7:                      
#line 93 "exprparse.y"
  {
    (yyval.expr) = (yyvsp[0].expr);
  }
#line 1368 "exprparse.c"
  break;

  case 8:                      
#line 95 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "-", make_integer_constant(0), (yyvsp[0].expr));
  }
#line 1375 "exprparse.c"
  break;

  case 9:                                       
#line 99 "exprparse.y"
  {
    (yyval.expr) = make_integer_constant(PG_INT64_MIN);
  }
#line 1381 "exprparse.c"
  break;

  case 10:                      
#line 101 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "#", make_integer_constant(~INT64CONST(0)), (yyvsp[0].expr));
  }
#line 1388 "exprparse.c"
  break;

  case 11:                         
#line 103 "exprparse.y"
  {
    (yyval.expr) = make_uop(yyscanner, "!not", (yyvsp[0].expr));
  }
#line 1394 "exprparse.c"
  break;

  case 12:                           
#line 104 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "+", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1400 "exprparse.c"
  break;

  case 13:                           
#line 105 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "-", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1406 "exprparse.c"
  break;

  case 14:                           
#line 106 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "*", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1412 "exprparse.c"
  break;

  case 15:                           
#line 107 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "/", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1418 "exprparse.c"
  break;

  case 16:                           
#line 108 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "mod", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1424 "exprparse.c"
  break;

  case 17:                           
#line 109 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "<", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1430 "exprparse.c"
  break;

  case 18:                             
#line 110 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "<=", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1436 "exprparse.c"
  break;

  case 19:                           
#line 111 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "<", (yyvsp[0].expr), (yyvsp[-2].expr));
  }
#line 1442 "exprparse.c"
  break;

  case 20:                             
#line 112 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "<=", (yyvsp[0].expr), (yyvsp[-2].expr));
  }
#line 1448 "exprparse.c"
  break;

  case 21:                           
#line 113 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "=", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1454 "exprparse.c"
  break;

  case 22:                             
#line 114 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "<>", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1460 "exprparse.c"
  break;

  case 23:                           
#line 115 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "&", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1466 "exprparse.c"
  break;

  case 24:                           
#line 116 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "|", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1472 "exprparse.c"
  break;

  case 25:                           
#line 117 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "#", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1478 "exprparse.c"
  break;

  case 26:                             
#line 118 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "<<", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1484 "exprparse.c"
  break;

  case 27:                             
#line 119 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, ">>", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1490 "exprparse.c"
  break;

  case 28:                              
#line 120 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "!and", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1496 "exprparse.c"
  break;

  case 29:                             
#line 121 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "!or", (yyvsp[-2].expr), (yyvsp[0].expr));
  }
#line 1502 "exprparse.c"
  break;

  case 30:                            
#line 123 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "!is", (yyvsp[-1].expr), make_null_constant());
  }
#line 1508 "exprparse.c"
  break;

  case 31:                             
#line 124 "exprparse.y"
  {
    (yyval.expr) = make_uop(yyscanner, "!not", make_op(yyscanner, "!is", (yyvsp[-1].expr), make_null_constant()));
  }
#line 1517 "exprparse.c"
  break;

  case 32:                                   
#line 128 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "!is", (yyvsp[-2].expr), make_null_constant());
  }
#line 1523 "exprparse.c"
  break;

  case 33:                                          
#line 130 "exprparse.y"
  {
    (yyval.expr) = make_uop(yyscanner, "!not", make_op(yyscanner, "!is", (yyvsp[-3].expr), make_null_constant()));
  }
#line 1532 "exprparse.c"
  break;

  case 34:                                      
#line 135 "exprparse.y"
  {
    (yyval.expr) = make_op(yyscanner, "!is", (yyvsp[-2].expr), make_boolean_constant((yyvsp[0].bval)));
  }
#line 1540 "exprparse.c"
  break;

  case 35:                                             
#line 139 "exprparse.y"
  {
    (yyval.expr) = make_uop(yyscanner, "!not", make_op(yyscanner, "!is", (yyvsp[-3].expr), make_boolean_constant((yyvsp[0].bval))));
  }
#line 1549 "exprparse.c"
  break;

  case 36:                        
#line 144 "exprparse.y"
  {
    (yyval.expr) = make_null_constant();
  }
#line 1555 "exprparse.c"
  break;

  case 37:                           
#line 145 "exprparse.y"
  {
    (yyval.expr) = make_boolean_constant((yyvsp[0].bval));
  }
#line 1561 "exprparse.c"
  break;

  case 38:                           
#line 146 "exprparse.y"
  {
    (yyval.expr) = make_integer_constant((yyvsp[0].ival));
  }
#line 1567 "exprparse.c"
  break;

  case 39:                          
#line 147 "exprparse.y"
  {
    (yyval.expr) = make_double_constant((yyvsp[0].dval));
  }
#line 1573 "exprparse.c"
  break;

  case 40:                      
#line 149 "exprparse.y"
  {
    (yyval.expr) = make_variable((yyvsp[0].str));
  }
#line 1579 "exprparse.c"
  break;

  case 41:                                    
#line 150 "exprparse.y"
  {
    (yyval.expr) = make_func(yyscanner, (yyvsp[-3].ival), (yyvsp[-1].elist));
  }
#line 1585 "exprparse.c"
  break;

  case 42:                          
#line 151 "exprparse.y"
  {
    (yyval.expr) = (yyvsp[0].expr);
  }
#line 1591 "exprparse.c"
  break;

  case 43:                                                                
#line 155 "exprparse.y"
  {
    (yyval.elist) = make_elist((yyvsp[0].expr), make_elist((yyvsp[-2].expr), (yyvsp[-4].elist)));
  }
#line 1597 "exprparse.c"
  break;

  case 44:                                                 
#line 156 "exprparse.y"
  {
    (yyval.elist) = make_elist((yyvsp[0].expr), make_elist((yyvsp[-2].expr), NULL));
  }
#line 1603 "exprparse.c"
  break;

  case 45:                                                   
#line 159 "exprparse.y"
  {
    (yyval.expr) = make_case(yyscanner, (yyvsp[-1].elist), make_null_constant());
  }
#line 1609 "exprparse.c"
  break;

  case 46:                                                                
#line 160 "exprparse.y"
  {
    (yyval.expr) = make_case(yyscanner, (yyvsp[-3].elist), (yyvsp[-1].expr));
  }
#line 1615 "exprparse.c"
  break;

  case 47:                          
#line 162 "exprparse.y"
  {
    (yyval.ival) = find_func(yyscanner, (yyvsp[0].str));
    pg_free((yyvsp[0].str));
  }
#line 1621 "exprparse.c"
  break;

#line 1625 "exprparse.c"

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
    yyerror(yyscanner, YY_("syntax error"));
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
      yydestruct("Error: discarding", yytoken, &yylval, yyscanner);
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

    yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp, yyscanner);
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
  yyerror(yyscanner, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif

                                                           
                                                           
                                                          
yyreturn:
  if (yychar != YYEMPTY)
  {
                                                                        
                                                           
    yytoken = YYTRANSLATE(yychar);
    yydestruct("Cleanup: discarding lookahead", yytoken, &yylval, yyscanner);
  }
                                                                   
                                  
  YYPOPSTACK(yylen);
  YY_STACK_PRINT(yyss, yyssp);
  while (yyssp != yyss)
  {
    yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp, yyscanner);
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

#line 165 "exprparse.y"

static PgBenchExpr *
make_null_constant(void)
{
  PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

  expr->etype = ENODE_CONSTANT;
  expr->u.constant.type = PGBT_NULL;
  expr->u.constant.u.ival = 0;
  return expr;
}

static PgBenchExpr *
make_integer_constant(int64 ival)
{
  PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

  expr->etype = ENODE_CONSTANT;
  expr->u.constant.type = PGBT_INT;
  expr->u.constant.u.ival = ival;
  return expr;
}

static PgBenchExpr *
make_double_constant(double dval)
{
  PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

  expr->etype = ENODE_CONSTANT;
  expr->u.constant.type = PGBT_DOUBLE;
  expr->u.constant.u.dval = dval;
  return expr;
}

static PgBenchExpr *
make_boolean_constant(bool bval)
{
  PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

  expr->etype = ENODE_CONSTANT;
  expr->u.constant.type = PGBT_BOOLEAN;
  expr->u.constant.u.bval = bval;
  return expr;
}

static PgBenchExpr *
make_variable(char *varname)
{
  PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

  expr->etype = ENODE_VARIABLE;
  expr->u.variable.varname = varname;
  return expr;
}

                      
static PgBenchExpr *
make_op(yyscan_t yyscanner, const char *operator, PgBenchExpr * lexpr, PgBenchExpr *rexpr)
{
  return make_func(yyscanner, find_func(yyscanner, operator), make_elist(rexpr, make_elist(lexpr, NULL)));
}

                    
static PgBenchExpr *
make_uop(yyscan_t yyscanner, const char *operator, PgBenchExpr * expr)
{
  return make_func(yyscanner, find_func(yyscanner, operator), make_elist(expr, NULL));
}

   
                                
                                                                 
                                                
                                                                      
                           
                                                                     
                               
                                                                          
                                  
                                                        
   
static const struct
{
  const char *fname;
  int nargs;
  PgBenchFunction tag;
} PGBENCH_FUNCTIONS[] = {
                                                    
    {"+", 2, PGBENCH_ADD}, {"-", 2, PGBENCH_SUB}, {"*", 2, PGBENCH_MUL}, {"/", 2, PGBENCH_DIV}, {"mod", 2, PGBENCH_MOD},
                          
    {"abs", 1, PGBENCH_ABS}, {"least", PGBENCH_NARGS_VARIABLE, PGBENCH_LEAST}, {"greatest", PGBENCH_NARGS_VARIABLE, PGBENCH_GREATEST}, {"debug", 1, PGBENCH_DEBUG}, {"pi", 0, PGBENCH_PI}, {"sqrt", 1, PGBENCH_SQRT}, {"ln", 1, PGBENCH_LN}, {"exp", 1, PGBENCH_EXP}, {"int", 1, PGBENCH_INT}, {"double", 1, PGBENCH_DOUBLE}, {"random", 2, PGBENCH_RANDOM}, {"random_gaussian", 3, PGBENCH_RANDOM_GAUSSIAN}, {"random_exponential", 3, PGBENCH_RANDOM_EXPONENTIAL}, {"random_zipfian", 3, PGBENCH_RANDOM_ZIPFIAN}, {"pow", 2, PGBENCH_POW}, {"power", 2, PGBENCH_POW},
                           
    {"!and", 2, PGBENCH_AND}, {"!or", 2, PGBENCH_OR}, {"!not", 1, PGBENCH_NOT},
                                   
    {"&", 2, PGBENCH_BITAND}, {"|", 2, PGBENCH_BITOR}, {"#", 2, PGBENCH_BITXOR}, {"<<", 2, PGBENCH_LSHIFT}, {">>", 2, PGBENCH_RSHIFT},
                              
    {"=", 2, PGBENCH_EQ}, {"<>", 2, PGBENCH_NE}, {"<=", 2, PGBENCH_LE}, {"<", 2, PGBENCH_LT}, {"!is", 2, PGBENCH_IS},
                                                            
    {"!case_end", PGBENCH_NARGS_CASE, PGBENCH_CASE}, {"hash", PGBENCH_NARGS_HASH, PGBENCH_HASH_MURMUR2}, {"hash_murmur2", PGBENCH_NARGS_HASH, PGBENCH_HASH_MURMUR2}, {"hash_fnv1a", PGBENCH_NARGS_HASH, PGBENCH_HASH_FNV1A},
                                    
    {NULL, 0, 0}};

   
                                 
   
                                                                     
                                       
   
static int
find_func(yyscan_t yyscanner, const char *fname)
{
  int i = 0;

  while (PGBENCH_FUNCTIONS[i].fname)
  {
    if (pg_strcasecmp(fname, PGBENCH_FUNCTIONS[i].fname) == 0)
    {
      return i;
    }
    i++;
  }

  expr_yyerror_more(yyscanner, "unexpected function name", fname);

                   
  return -1;
}

                                    
static PgBenchExprList *
make_elist(PgBenchExpr *expr, PgBenchExprList *list)
{
  PgBenchExprLink *cons;

  if (list == NULL)
  {
    list = pg_malloc(sizeof(PgBenchExprList));
    list->head = NULL;
    list->tail = NULL;
  }

  cons = pg_malloc(sizeof(PgBenchExprLink));
  cons->expr = expr;
  cons->next = NULL;

  if (list->head == NULL)
  {
    list->head = cons;
  }
  else
  {
    list->tail->next = cons;
  }

  list->tail = cons;

  return list;
}

                                             
static int
elist_length(PgBenchExprList *list)
{
  PgBenchExprLink *link = list != NULL ? list->head : NULL;
  int len = 0;

  for (; link != NULL; link = link->next)
  {
    len++;
  }

  return len;
}

                                    
static PgBenchExpr *
make_func(yyscan_t yyscanner, int fnumber, PgBenchExprList *args)
{
  int len = elist_length(args);

  PgBenchExpr *expr = pg_malloc(sizeof(PgBenchExpr));

  Assert(fnumber >= 0);

                                                             
  switch (PGBENCH_FUNCTIONS[fnumber].nargs)
  {
                                                   
  case PGBENCH_NARGS_VARIABLE:
    if (len == 0)
    {
      expr_yyerror_more(yyscanner, "at least one argument expected", PGBENCH_FUNCTIONS[fnumber].fname);
    }
    break;

                                                 
  case PGBENCH_NARGS_CASE:
                                                                       
    if (len < 3 || len % 2 != 1)
    {
      expr_yyerror_more(yyscanner, "odd and >= 3 number of arguments expected", "case control structure");
    }
    break;

                                                  
  case PGBENCH_NARGS_HASH:
    if (len < 1 || len > 2)
    {
      expr_yyerror_more(yyscanner, "unexpected number of arguments", PGBENCH_FUNCTIONS[fnumber].fname);
    }

    if (len == 1)
    {
      PgBenchExpr *var = make_variable("default_seed");
      args = make_elist(var, args);
    }
    break;

                                              
  default:
    Assert(PGBENCH_FUNCTIONS[fnumber].nargs >= 0);

    if (PGBENCH_FUNCTIONS[fnumber].nargs != len)
    {
      expr_yyerror_more(yyscanner, "unexpected number of arguments", PGBENCH_FUNCTIONS[fnumber].fname);
    }
  }

  expr->etype = ENODE_FUNCTION;
  expr->u.function.function = PGBENCH_FUNCTIONS[fnumber].tag;

                                                                  
  expr->u.function.args = args != NULL ? args->head : NULL;
  if (args)
  {
    pg_free(args);
  }

  return expr;
}

static PgBenchExpr *
make_case(yyscan_t yyscanner, PgBenchExprList *when_then_list, PgBenchExpr *else_part)
{
  return make_func(yyscanner, find_func(yyscanner, "!case_end"), make_elist(else_part, when_then_list));
}

   
                                                                      
                                                                     
                                                                   
                                                                   
                                    
   

                                                         
#undef yyscan_t
                                                                           
#undef yylval

#include "exprscan.c"
