                                               

                                                   
 
                                                                                 
        
 
                                                                        
                                                                        
                                                                     
                                       
 
                                                                   
                                                                  
                                                                 
                                                
 
                                                                     
                                                                            

                                                                      
                                                                     
                                                                   
                                                                     
                                                                       
                                                                    
                                                                      
                                                                  
                                           
 
                                                                       
                            

                                                             
                                                            

                                                                  
                                                                
                                                                     

                                                                  
                                                                      
                                                                  
                                                                 
                                                                  
                              

                                                
#define YYBISON 30705

                            
#define YYBISON_VERSION "3.7.5"

                     
#define YYSKELETON_NAME "yacc.c"

                    
#define YYPURE 0

                    
#define YYPUSH 0

                    
#define YYPULL 1

                                                  
#define yyparse spec_yyparse
#define yylex spec_yylex
#define yyerror spec_yyerror
#define yydebug spec_yydebug
#define yynerrs spec_yynerrs
#define yylval spec_yylval
#define yychar spec_yychar

                                   
#line 1 "specparse.y"

                                                                            
   
               
                                                      
   
                                                                         
                                                                        
   
                                                                            
   

#include "postgres_fe.h"

#include "isolationtester.h"

TestSpec parseresult;                                     

#line 99 "specparse.c"

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
extern int spec_yydebug;
#endif

                   
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype
{
  YYEMPTY = -2,
  YYEOF = 0,                             
  YYerror = 256,                 
  YYUNDEF = 257,                           
  sqlblock = 258,                   
  identifier = 259,                   
  INTEGER = 260,                   
  NOTICES = 261,                   
  PERMUTATION = 262,                   
  SESSION = 263,                   
  SETUP = 264,                   
  STEP = 265,                   
  TEARDOWN = 266,                   
  TEST = 267                    
};
typedef enum yytokentype yytoken_kind_t;
#endif

                  
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 26 "specparse.y"

  char *str;
  int integer;
  Session *session;
  Step *step;
  Permutation *permutation;
  PermutationStep *permutationstep;
  PermutationStepBlocker *blocker;
  struct
  {
    void **elements;
    int nelements;
  } ptr_list;

#line 173 "specparse.c"
};
typedef union YYSTYPE YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE spec_yylval;

int
spec_yyparse(void);

                   
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                                      
  YYSYMBOL_YYerror = 1,                            
  YYSYMBOL_YYUNDEF = 2,                                      
  YYSYMBOL_sqlblock = 3,                              
  YYSYMBOL_identifier = 4,                              
  YYSYMBOL_INTEGER = 5,                              
  YYSYMBOL_NOTICES = 6,                              
  YYSYMBOL_PERMUTATION = 7,                              
  YYSYMBOL_SESSION = 8,                              
  YYSYMBOL_SETUP = 9,                              
  YYSYMBOL_STEP = 10,                             
  YYSYMBOL_TEARDOWN = 11,                             
  YYSYMBOL_TEST = 12,                             
  YYSYMBOL_13_ = 13,                             
  YYSYMBOL_14_ = 14,                             
  YYSYMBOL_15_ = 15,                             
  YYSYMBOL_16_ = 16,                             
  YYSYMBOL_YYACCEPT = 17,                            
  YYSYMBOL_TestSpec = 18,                             
  YYSYMBOL_setup_list = 19,                             
  YYSYMBOL_opt_setup = 20,                             
  YYSYMBOL_setup = 21,                             
  YYSYMBOL_opt_teardown = 22,                             
  YYSYMBOL_session_list = 23,                             
  YYSYMBOL_session = 24,                             
  YYSYMBOL_step_list = 25,                             
  YYSYMBOL_step = 26,                             
  YYSYMBOL_opt_permutation_list = 27,                             
  YYSYMBOL_permutation_list = 28,                             
  YYSYMBOL_permutation = 29,                             
  YYSYMBOL_permutation_step_list = 30,                             
  YYSYMBOL_permutation_step = 31,                             
  YYSYMBOL_blocker_list = 32,                             
  YYSYMBOL_blocker = 33                              
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

                                                        
#define YYFINAL 3
                                       
#define YYLAST 41

                                        
#define YYNTOKENS 17
                                        
#define YYNNTS 17
                                   
#define YYNRULES 29
                                     
#define YYNSTATES 43

                                          
#define YYMAXUTOK 267

                                                                      
                                                         
#define YYTRANSLATE(YYX) (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

                                                                      
                            
static const yytype_int8 yytranslate[] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 13, 14, 16, 2, 15, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

#if YYDEBUG
                                                                     
static const yytype_int16 yyrline[] = {0, 59, 59, 76, 80, 90, 91, 95, 99, 100, 104, 111, 120, 132, 139, 149, 161, 166, 172, 179, 189, 198, 205, 214, 222, 233, 240, 249, 258, 267};
#endif

                                        
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
                                                                 
                                     
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

                                                                
                                                                       
static const char *const yytname[] = {"\"end of file\"", "error", "\"invalid token\"", "sqlblock", "identifier", "INTEGER", "NOTICES", "PERMUTATION", "SESSION", "SETUP", "STEP", "TEARDOWN", "TEST", "'('", "')'", "','", "'*'", "$accept", "TestSpec", "setup_list", "opt_setup", "setup", "opt_teardown", "session_list", "session", "step_list", "step", "opt_permutation_list", "permutation_list", "permutation", "permutation_step_list", "permutation_step", "blocker_list", "blocker", YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
                                                                 
                                                                    
static const yytype_int16 yytoknum[] = {0, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 40, 41, 44, 42};
#endif

#define YYPACT_NINF (-14)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

                                                                   
                 
static const yytype_int8 yypact[] = {-14, 2, -8, -14, 3, 4, -14, 7, -14, -14, 6, -3, -14, 8, 12, -14, -14, 11, -14, 1, -14, 9, 12, -14, -14, 15, -2, -14, -4, -14, 17, -14, -14, 18, -14, -1, -14, -14, 16, -14, -4, -14, -14};

                                                                       
                                                                       
                                     
static const yytype_int8 yydefact[] = {3, 0, 8, 1, 0, 0, 4, 0, 7, 9, 0, 17, 11, 5, 0, 10, 2, 16, 19, 0, 6, 23, 20, 22, 18, 0, 8, 14, 0, 21, 0, 12, 13, 27, 29, 0, 26, 15, 0, 24, 0, 28, 25};

                          
static const yytype_int8 yypgoto[] = {-14, -14, -14, -14, 10, 0, -14, 14, -14, 5, -14, -14, 13, -14, 19, -14, -13};

                            
static const yytype_int8 yydefgoto[] = {0, 1, 2, 19, 6, 7, 11, 12, 26, 27, 16, 17, 18, 22, 23, 35, 36};

                                                                    
                                                                   
                                                              
static const yytype_int8 yytable[] = {33, 4, 3, 5, 14, 10, 8, 9, 25, 5, 13, 25, 34, 39, 40, 10, 21, 4, 14, 30, 37, 41, 28, 20, 38, 15, 31, 42, 0, 0, 24, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 29};

static const yytype_int8 yycheck[] = {4, 9, 0, 11, 7, 8, 3, 3, 10, 11, 4, 10, 16, 14, 15, 8, 4, 9, 7, 4, 3, 5, 13, 13, 6, 11, 26, 40, -1, -1, 17, 26, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22};

                                                               
                                 
static const yytype_int8 yystos[] = {0, 18, 19, 0, 9, 11, 21, 22, 3, 3, 8, 23, 24, 4, 7, 24, 27, 28, 29, 20, 21, 4, 30, 31, 29, 10, 25, 26, 13, 31, 4, 22, 26, 4, 16, 32, 33, 3, 6, 14, 15, 5, 33};

                                                                  
static const yytype_int8 yyr1[] = {0, 17, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 27, 28, 28, 29, 30, 30, 31, 31, 32, 32, 33, 33, 33};

                                                                         
static const yytype_int8 yyr2[] = {0, 2, 4, 0, 2, 0, 1, 2, 0, 2, 2, 1, 5, 2, 1, 3, 1, 0, 2, 1, 2, 2, 1, 1, 4, 3, 1, 1, 3, 1};

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
      yy_symbol_print(stderr, Kind, Value);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      YYFPRINTF(stderr, "\n");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                       
                                       
                                      

static void
yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE(yyoutput);
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
yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep)
{
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name(yykind));

  yy_symbol_value_print(yyo, yykind, yyvaluep);
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
yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1, yylno);
                                   
  for (yyi = 0; yyi < yynrhs; yyi++)
  {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]), &yyvsp[(yyi + 1) - (yynrhs)]);
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (yydebug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      yy_reduce_print(yyssp, yyvsp, Rule);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
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
yydestruct(const char *yymsg, yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE(yyvaluep);
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
#line 63 "specparse.y"
  {
    parseresult.setupsqls = (char **)(yyvsp[-3].ptr_list).elements;
    parseresult.nsetupsqls = (yyvsp[-3].ptr_list).nelements;
    parseresult.teardownsql = (yyvsp[-2].str);
    parseresult.sessions = (Session **)(yyvsp[-1].ptr_list).elements;
    parseresult.nsessions = (yyvsp[-1].ptr_list).nelements;
    parseresult.permutations = (Permutation **)(yyvsp[0].ptr_list).elements;
    parseresult.npermutations = (yyvsp[0].ptr_list).nelements;
  }
#line 1219 "specparse.c"
  break;

  case 3:                          
#line 76 "specparse.y"
  {
    (yyval.ptr_list).elements = NULL;
    (yyval.ptr_list).nelements = 0;
  }
#line 1228 "specparse.c"
  break;

  case 4:                                    
#line 81 "specparse.y"
  {
    (yyval.ptr_list).elements = pg_realloc((yyvsp[-1].ptr_list).elements, ((yyvsp[-1].ptr_list).nelements + 1) * sizeof(void *));
    (yyval.ptr_list).elements[(yyvsp[-1].ptr_list).nelements] = (yyvsp[0].str);
    (yyval.ptr_list).nelements = (yyvsp[-1].ptr_list).nelements + 1;
  }
#line 1239 "specparse.c"
  break;

  case 5:                         
#line 90 "specparse.y"
  {
    (yyval.str) = NULL;
  }
#line 1245 "specparse.c"
  break;

  case 6:                        
#line 91 "specparse.y"
  {
    (yyval.str) = (yyvsp[0].str);
  }
#line 1251 "specparse.c"
  break;

  case 7:                             
#line 95 "specparse.y"
  {
    (yyval.str) = (yyvsp[0].str);
  }
#line 1257 "specparse.c"
  break;

  case 8:                            
#line 99 "specparse.y"
  {
    (yyval.str) = NULL;
  }
#line 1263 "specparse.c"
  break;

  case 9:                                       
#line 100 "specparse.y"
  {
    (yyval.str) = (yyvsp[0].str);
  }
#line 1269 "specparse.c"
  break;

  case 10:                                          
#line 105 "specparse.y"
  {
    (yyval.ptr_list).elements = pg_realloc((yyvsp[-1].ptr_list).elements, ((yyvsp[-1].ptr_list).nelements + 1) * sizeof(void *));
    (yyval.ptr_list).elements[(yyvsp[-1].ptr_list).nelements] = (yyvsp[0].session);
    (yyval.ptr_list).nelements = (yyvsp[-1].ptr_list).nelements + 1;
  }
#line 1280 "specparse.c"
  break;

  case 11:                             
#line 112 "specparse.y"
  {
    (yyval.ptr_list).nelements = 1;
    (yyval.ptr_list).elements = pg_malloc(sizeof(void *));
    (yyval.ptr_list).elements[0] = (yyvsp[0].session);
  }
#line 1290 "specparse.c"
  break;

  case 12:                                                                    
#line 121 "specparse.y"
  {
    (yyval.session) = pg_malloc(sizeof(Session));
    (yyval.session)->name = (yyvsp[-3].str);
    (yyval.session)->setupsql = (yyvsp[-2].str);
    (yyval.session)->steps = (Step **)(yyvsp[-1].ptr_list).elements;
    (yyval.session)->nsteps = (yyvsp[-1].ptr_list).nelements;
    (yyval.session)->teardownsql = (yyvsp[0].str);
  }
#line 1303 "specparse.c"
  break;

  case 13:                                 
#line 133 "specparse.y"
  {
    (yyval.ptr_list).elements = pg_realloc((yyvsp[-1].ptr_list).elements, ((yyvsp[-1].ptr_list).nelements + 1) * sizeof(void *));
    (yyval.ptr_list).elements[(yyvsp[-1].ptr_list).nelements] = (yyvsp[0].step);
    (yyval.ptr_list).nelements = (yyvsp[-1].ptr_list).nelements + 1;
  }
#line 1314 "specparse.c"
  break;

  case 14:                       
#line 140 "specparse.y"
  {
    (yyval.ptr_list).nelements = 1;
    (yyval.ptr_list).elements = pg_malloc(sizeof(void *));
    (yyval.ptr_list).elements[0] = (yyvsp[0].step);
  }
#line 1324 "specparse.c"
  break;

  case 15:                                      
#line 150 "specparse.y"
  {
    (yyval.step) = pg_malloc(sizeof(Step));
    (yyval.step)->name = (yyvsp[-1].str);
    (yyval.step)->sql = (yyvsp[0].str);
    (yyval.step)->session = -1;                   
    (yyval.step)->used = false;
  }
#line 1336 "specparse.c"
  break;

  case 16:                                              
#line 162 "specparse.y"
  {
    (yyval.ptr_list) = (yyvsp[0].ptr_list);
  }
#line 1344 "specparse.c"
  break;

  case 17:                                    
#line 166 "specparse.y"
  {
    (yyval.ptr_list).elements = NULL;
    (yyval.ptr_list).nelements = 0;
  }
#line 1353 "specparse.c"
  break;

  case 18:                                                      
#line 173 "specparse.y"
  {
    (yyval.ptr_list).elements = pg_realloc((yyvsp[-1].ptr_list).elements, ((yyvsp[-1].ptr_list).nelements + 1) * sizeof(void *));
    (yyval.ptr_list).elements[(yyvsp[-1].ptr_list).nelements] = (yyvsp[0].permutation);
    (yyval.ptr_list).nelements = (yyvsp[-1].ptr_list).nelements + 1;
  }
#line 1364 "specparse.c"
  break;

  case 19:                                     
#line 180 "specparse.y"
  {
    (yyval.ptr_list).nelements = 1;
    (yyval.ptr_list).elements = pg_malloc(sizeof(void *));
    (yyval.ptr_list).elements[0] = (yyvsp[0].permutation);
  }
#line 1374 "specparse.c"
  break;

  case 20:                                                      
#line 190 "specparse.y"
  {
    (yyval.permutation) = pg_malloc(sizeof(Permutation));
    (yyval.permutation)->nsteps = (yyvsp[0].ptr_list).nelements;
    (yyval.permutation)->steps = (PermutationStep **)(yyvsp[0].ptr_list).elements;
  }
#line 1384 "specparse.c"
  break;

  case 21:                                                                     
#line 199 "specparse.y"
  {
    (yyval.ptr_list).elements = pg_realloc((yyvsp[-1].ptr_list).elements, ((yyvsp[-1].ptr_list).nelements + 1) * sizeof(void *));
    (yyval.ptr_list).elements[(yyvsp[-1].ptr_list).nelements] = (yyvsp[0].permutationstep);
    (yyval.ptr_list).nelements = (yyvsp[-1].ptr_list).nelements + 1;
  }
#line 1395 "specparse.c"
  break;

  case 22:                                               
#line 206 "specparse.y"
  {
    (yyval.ptr_list).nelements = 1;
    (yyval.ptr_list).elements = pg_malloc(sizeof(void *));
    (yyval.ptr_list).elements[0] = (yyvsp[0].permutationstep);
  }
#line 1405 "specparse.c"
  break;

  case 23:                                    
#line 215 "specparse.y"
  {
    (yyval.permutationstep) = pg_malloc(sizeof(PermutationStep));
    (yyval.permutationstep)->name = (yyvsp[0].str);
    (yyval.permutationstep)->blockers = NULL;
    (yyval.permutationstep)->nblockers = 0;
    (yyval.permutationstep)->step = NULL;
  }
#line 1417 "specparse.c"
  break;

  case 24:                                                         
#line 223 "specparse.y"
  {
    (yyval.permutationstep) = pg_malloc(sizeof(PermutationStep));
    (yyval.permutationstep)->name = (yyvsp[-3].str);
    (yyval.permutationstep)->blockers = (PermutationStepBlocker **)(yyvsp[-1].ptr_list).elements;
    (yyval.permutationstep)->nblockers = (yyvsp[-1].ptr_list).nelements;
    (yyval.permutationstep)->step = NULL;
  }
#line 1429 "specparse.c"
  break;

  case 25:                                              
#line 234 "specparse.y"
  {
    (yyval.ptr_list).elements = pg_realloc((yyvsp[-2].ptr_list).elements, ((yyvsp[-2].ptr_list).nelements + 1) * sizeof(void *));
    (yyval.ptr_list).elements[(yyvsp[-2].ptr_list).nelements] = (yyvsp[0].blocker);
    (yyval.ptr_list).nelements = (yyvsp[-2].ptr_list).nelements + 1;
  }
#line 1440 "specparse.c"
  break;

  case 26:                             
#line 241 "specparse.y"
  {
    (yyval.ptr_list).nelements = 1;
    (yyval.ptr_list).elements = pg_malloc(sizeof(void *));
    (yyval.ptr_list).elements[0] = (yyvsp[0].blocker);
  }
#line 1450 "specparse.c"
  break;

  case 27:                           
#line 250 "specparse.y"
  {
    (yyval.blocker) = pg_malloc(sizeof(PermutationStepBlocker));
    (yyval.blocker)->stepname = (yyvsp[0].str);
    (yyval.blocker)->blocktype = PSB_OTHER_STEP;
    (yyval.blocker)->num_notices = -1;
    (yyval.blocker)->step = NULL;
    (yyval.blocker)->target_notices = -1;
  }
#line 1463 "specparse.c"
  break;

  case 28:                                           
#line 259 "specparse.y"
  {
    (yyval.blocker) = pg_malloc(sizeof(PermutationStepBlocker));
    (yyval.blocker)->stepname = (yyvsp[-2].str);
    (yyval.blocker)->blocktype = PSB_NUM_NOTICES;
    (yyval.blocker)->num_notices = (yyvsp[0].integer);
    (yyval.blocker)->step = NULL;
    (yyval.blocker)->target_notices = -1;
  }
#line 1476 "specparse.c"
  break;

  case 29:                    
#line 268 "specparse.y"
  {
    (yyval.blocker) = pg_malloc(sizeof(PermutationStepBlocker));
    (yyval.blocker)->stepname = NULL;
    (yyval.blocker)->blocktype = PSB_ONCE;
    (yyval.blocker)->num_notices = -1;
    (yyval.blocker)->step = NULL;
    (yyval.blocker)->target_notices = -1;
  }
#line 1489 "specparse.c"
  break;

#line 1493 "specparse.c"

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
    yyerror(YY_("syntax error"));
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
      yydestruct("Error: discarding", yytoken, &yylval);
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

    yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp);
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
  yyerror(YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif

                                                           
                                                           
                                                          
yyreturn:
  if (yychar != YYEMPTY)
  {
                                                                        
                                                           
    yytoken = YYTRANSLATE(yychar);
    yydestruct("Cleanup: discarding lookahead", yytoken, &yylval);
  }
                                                                   
                                  
  YYPOPSTACK(yylen);
  YY_STACK_PRINT(yyss, yyssp);
  while (yyssp != yyss)
  {
    yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp);
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

#line 278 "specparse.y"

#include "specscanner.c"
