                                               

                                                   
 
                                                                                 
        
 
                                                                        
                                                                        
                                                                     
                                       
 
                                                                   
                                                                  
                                                                 
                                                
 
                                                                     
                                                                            

                                                                      
                                                                     
                                                                   
                                                                     
                                                                       
                                                                    
                                                                      
                                                                  
                                           
 
                                                                       
                            

                                                             
                                                            

                                                                  
                                                                
                                                                     

                                                                  
                                                                      
                                                                  
                                                                 
                                                                  
                              

                                                
#define YYBISON 30705

                            
#define YYBISON_VERSION "3.7.5"

                     
#define YYSKELETON_NAME "yacc.c"

                    
#define YYPURE 0

                    
#define YYPUSH 0

                    
#define YYPULL 1

                                                  
#define yyparse replication_yyparse
#define yylex replication_yylex
#define yyerror replication_yyerror
#define yydebug replication_yydebug
#define yynerrs replication_yynerrs
#define yylval replication_yylval
#define yychar replication_yychar

                                   
#line 1 "repl_gram.y"

                                                                            
   
                                                        
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "access/xlogdefs.h"
#include "nodes/makefuncs.h"
#include "nodes/replnodes.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"

                                            
Node *replication_parse_result;

   
                                                                           
                                                                         
                                                                           
                                                                         
                                                                       
                             
   
#define YYMALLOC palloc
#define YYFREE pfree

#line 119 "repl_gram.c"

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
extern int replication_yydebug;
#endif

                   
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype
{
  YYEMPTY = -2,
  YYEOF = 0,                                           
  YYerror = 256,                               
  YYUNDEF = 257,                                         
  SCONST = 258,                                 
  IDENT = 259,                                 
  UCONST = 260,                                 
  RECPTR = 261,                                 
  K_BASE_BACKUP = 262,                                 
  K_IDENTIFY_SYSTEM = 263,                                 
  K_SHOW = 264,                                 
  K_START_REPLICATION = 265,                                 
  K_CREATE_REPLICATION_SLOT = 266,                                 
  K_DROP_REPLICATION_SLOT = 267,                                 
  K_TIMELINE_HISTORY = 268,                                 
  K_LABEL = 269,                                 
  K_PROGRESS = 270,                                 
  K_FAST = 271,                                 
  K_WAIT = 272,                                 
  K_NOWAIT = 273,                                 
  K_MAX_RATE = 274,                                 
  K_WAL = 275,                                 
  K_TABLESPACE_MAP = 276,                                 
  K_NOVERIFY_CHECKSUMS = 277,                                 
  K_TIMELINE = 278,                                 
  K_PHYSICAL = 279,                                 
  K_LOGICAL = 280,                                 
  K_SLOT = 281,                                 
  K_RESERVE_WAL = 282,                                 
  K_TEMPORARY = 283,                                 
  K_EXPORT_SNAPSHOT = 284,                                 
  K_NOEXPORT_SNAPSHOT = 285,                                 
  K_USE_SNAPSHOT = 286                                  
};
typedef enum yytokentype yytoken_kind_t;
#endif

                  
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 45 "repl_gram.y"

  char *str;
  bool boolval;
  uint32 uintval;

  XLogRecPtr recptr;
  Node *node;
  List *list;
  DefElem *defelt;

#line 208 "repl_gram.c"
};
typedef union YYSTYPE YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE replication_yylval;

int
replication_yyparse(void);

                   
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                                          
  YYSYMBOL_YYerror = 1,                                
  YYSYMBOL_YYUNDEF = 2,                                          
  YYSYMBOL_SCONST = 3,                                  
  YYSYMBOL_IDENT = 4,                                  
  YYSYMBOL_UCONST = 5,                                  
  YYSYMBOL_RECPTR = 6,                                  
  YYSYMBOL_K_BASE_BACKUP = 7,                                  
  YYSYMBOL_K_IDENTIFY_SYSTEM = 8,                                  
  YYSYMBOL_K_SHOW = 9,                                  
  YYSYMBOL_K_START_REPLICATION = 10,                                 
  YYSYMBOL_K_CREATE_REPLICATION_SLOT = 11,                                 
  YYSYMBOL_K_DROP_REPLICATION_SLOT = 12,                                 
  YYSYMBOL_K_TIMELINE_HISTORY = 13,                                 
  YYSYMBOL_K_LABEL = 14,                                 
  YYSYMBOL_K_PROGRESS = 15,                                 
  YYSYMBOL_K_FAST = 16,                                 
  YYSYMBOL_K_WAIT = 17,                                 
  YYSYMBOL_K_NOWAIT = 18,                                 
  YYSYMBOL_K_MAX_RATE = 19,                                 
  YYSYMBOL_K_WAL = 20,                                 
  YYSYMBOL_K_TABLESPACE_MAP = 21,                                 
  YYSYMBOL_K_NOVERIFY_CHECKSUMS = 22,                                 
  YYSYMBOL_K_TIMELINE = 23,                                 
  YYSYMBOL_K_PHYSICAL = 24,                                 
  YYSYMBOL_K_LOGICAL = 25,                                 
  YYSYMBOL_K_SLOT = 26,                                 
  YYSYMBOL_K_RESERVE_WAL = 27,                                 
  YYSYMBOL_K_TEMPORARY = 28,                                 
  YYSYMBOL_K_EXPORT_SNAPSHOT = 29,                                 
  YYSYMBOL_K_NOEXPORT_SNAPSHOT = 30,                                 
  YYSYMBOL_K_USE_SNAPSHOT = 31,                                 
  YYSYMBOL_32_ = 32,                                 
  YYSYMBOL_33_ = 33,                                 
  YYSYMBOL_34_ = 34,                                 
  YYSYMBOL_35_ = 35,                                 
  YYSYMBOL_36_ = 36,                                 
  YYSYMBOL_YYACCEPT = 37,                                
  YYSYMBOL_firstcmd = 38,                                 
  YYSYMBOL_opt_semicolon = 39,                                 
  YYSYMBOL_command = 40,                                 
  YYSYMBOL_identify_system = 41,                                 
  YYSYMBOL_show = 42,                                 
  YYSYMBOL_var_name = 43,                                 
  YYSYMBOL_base_backup = 44,                                 
  YYSYMBOL_base_backup_opt_list = 45,                                 
  YYSYMBOL_base_backup_opt = 46,                                 
  YYSYMBOL_create_replication_slot = 47,                                 
  YYSYMBOL_create_slot_opt_list = 48,                                 
  YYSYMBOL_create_slot_opt = 49,                                 
  YYSYMBOL_drop_replication_slot = 50,                                 
  YYSYMBOL_start_replication = 51,                                 
  YYSYMBOL_start_logical_replication = 52,                                 
  YYSYMBOL_timeline_history = 53,                                 
  YYSYMBOL_opt_physical = 54,                                 
  YYSYMBOL_opt_temporary = 55,                                 
  YYSYMBOL_opt_slot = 56,                                 
  YYSYMBOL_opt_timeline = 57,                                 
  YYSYMBOL_plugin_options = 58,                                 
  YYSYMBOL_plugin_opt_list = 59,                                 
  YYSYMBOL_plugin_opt_elem = 60,                                 
  YYSYMBOL_plugin_opt_arg = 61                                  
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

                                                        
#define YYFINAL 26
                                       
#define YYLAST 48

                                        
#define YYNTOKENS 37
                                        
#define YYNNTS 25
                                   
#define YYNRULES 55
                                     
#define YYNSTATES 74

                                          
#define YYMAXUTOK 286

                                                                      
                                                         
#define YYTRANSLATE(YYX) (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

                                                                      
                            
static const yytype_int8 yytranslate[] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 34, 35, 2, 2, 36, 2, 33, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 32, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

#if YYDEBUG
                                                                     
static const yytype_int16 yyrline[] = {0, 105, 105, 111, 112, 116, 117, 118, 119, 120, 121, 122, 123, 130, 140, 147, 148, 157, 166, 169, 173, 178, 183, 188, 193, 198, 203, 208, 217, 228, 242, 245, 249, 254, 259, 264, 273, 281, 295, 310, 325, 342, 343, 347, 348, 352, 355, 359, 367, 372, 373, 377, 381, 388, 395, 396};
#endif

                                        
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
                                                                 
                                     
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

                                                                
                                                                       
static const char *const yytname[] = {"\"end of file\"", "error", "\"invalid token\"", "SCONST", "IDENT", "UCONST", "RECPTR", "K_BASE_BACKUP", "K_IDENTIFY_SYSTEM", "K_SHOW", "K_START_REPLICATION", "K_CREATE_REPLICATION_SLOT", "K_DROP_REPLICATION_SLOT", "K_TIMELINE_HISTORY", "K_LABEL", "K_PROGRESS", "K_FAST", "K_WAIT", "K_NOWAIT", "K_MAX_RATE", "K_WAL", "K_TABLESPACE_MAP", "K_NOVERIFY_CHECKSUMS", "K_TIMELINE", "K_PHYSICAL", "K_LOGICAL", "K_SLOT", "K_RESERVE_WAL", "K_TEMPORARY", "K_EXPORT_SNAPSHOT", "K_NOEXPORT_SNAPSHOT", "K_USE_SNAPSHOT", "';'", "'.'", "'('", "')'", "','", "$accept", "firstcmd", "opt_semicolon", "command", "identify_system", "show", "var_name", "base_backup", "base_backup_opt_list", "base_backup_opt", "create_replication_slot", "create_slot_opt_list", "create_slot_opt", "drop_replication_slot", "start_replication", "start_logical_replication", "timeline_history", "opt_physical", "opt_temporary", "opt_slot", "opt_timeline", "plugin_options", "plugin_opt_list",
    "plugin_opt_elem", "plugin_opt_arg", YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
                                                                 
                                                                    
static const yytype_int16 yytoknum[] = {0, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 59, 46, 40, 41, 44};
#endif

#define YYPACT_NINF (-26)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

                                                                   
                 
static const yytype_int8 yypact[] = {2, -26, -26, -1, -9, 21, 22, 23, 27, -2, -26, -26, -26, -26, -26, -26, -26, -26, -14, -26, -4, 28, 7, 5, 17, -26, -26, -26, -26, 32, -26, -26, -26, 31, -26, -26, -26, -26, 33, 13, -26, 34, -26, -3, -26, -26, -26, -26, 35, 16, -26, 38, 9, 39, -26, -11, -26, 41, -26, -26, -26, -26, -26, -26, -26, -11, 43, -12, -26, -26, -26, -26, 41, -26};

                                                                       
                                                                       
                                     
static const yytype_int8 yydefact[] = {0, 19, 13, 0, 46, 0, 0, 0, 0, 4, 5, 12, 6, 9, 10, 7, 8, 11, 17, 15, 14, 0, 42, 44, 36, 40, 1, 3, 2, 0, 21, 22, 24, 0, 23, 26, 27, 18, 0, 45, 41, 0, 43, 0, 37, 20, 25, 16, 0, 48, 31, 0, 50, 0, 38, 28, 31, 0, 39, 47, 35, 32, 33, 34, 30, 29, 55, 0, 51, 54, 53, 49, 0, 52};

                          
static const yytype_int8 yypgoto[] = {-26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -8, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -25, -26};

                            
static const yytype_int8 yydefgoto[] = {0, 8, 28, 9, 10, 11, 20, 12, 18, 37, 13, 55, 64, 14, 15, 16, 17, 41, 43, 22, 54, 58, 67, 68, 70};

                                                                    
                                                                   
                                                              
static const yytype_int8 yytable[] = {29, 30, 31, 19, 32, 33, 34, 35, 36, 1, 2, 3, 4, 5, 6, 7, 60, 21, 61, 62, 63, 50, 51, 71, 72, 23, 24, 26, 25, 38, 27, 40, 39, 42, 44, 45, 46, 47, 48, 53, 49, 52, 56, 57, 59, 66, 69, 73, 65};

static const yytype_int8 yycheck[] = {14, 15, 16, 4, 18, 19, 20, 21, 22, 7, 8, 9, 10, 11, 12, 13, 27, 26, 29, 30, 31, 24, 25, 35, 36, 4, 4, 0, 5, 33, 32, 24, 4, 28, 17, 3, 5, 4, 25, 23, 6, 6, 4, 34, 5, 4, 3, 72, 56};

                                                               
                                 
static const yytype_int8 yystos[] = {0, 7, 8, 9, 10, 11, 12, 13, 38, 40, 41, 42, 44, 47, 50, 51, 52, 53, 45, 4, 43, 26, 56, 4, 4, 5, 0, 32, 39, 14, 15, 16, 18, 19, 20, 21, 22, 46, 33, 4, 24, 54, 28, 55, 17, 3, 5, 4, 25, 6, 24, 25, 6, 23, 57, 48, 4, 34, 58, 5, 27, 29, 30, 31, 49, 48, 4, 59, 60, 3, 61, 35, 36, 60};

                                                                  
static const yytype_int8 yyr1[] = {0, 37, 38, 39, 39, 40, 40, 40, 40, 40, 40, 40, 40, 41, 42, 43, 43, 44, 45, 45, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 48, 48, 49, 49, 49, 49, 50, 50, 51, 52, 53, 54, 54, 55, 55, 56, 56, 57, 57, 58, 58, 59, 59, 60, 61, 61};

                                                                         
static const yytype_int8 yyr2[] = {0, 2, 2, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 3, 2, 2, 0, 2, 1, 1, 1, 1, 2, 1, 1, 5, 6, 2, 0, 1, 1, 1, 1, 2, 3, 5, 6, 2, 1, 0, 1, 0, 2, 0, 2, 0, 3, 0, 1, 3, 2, 1, 0};

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
#line 106 "repl_gram.y"
  {
    replication_parse_result = (yyvsp[-1].node);
  }
#line 1308 "repl_gram.c"
  break;

  case 13:                                          
#line 131 "repl_gram.y"
  {
    (yyval.node) = (Node *)makeNode(IdentifySystemCmd);
  }
#line 1316 "repl_gram.c"
  break;

  case 14:                             
#line 141 "repl_gram.y"
  {
    VariableShowStmt *n = makeNode(VariableShowStmt);
    n->name = (yyvsp[0].str);
    (yyval.node) = (Node *)n;
  }
#line 1326 "repl_gram.c"
  break;

  case 15:                       
#line 147 "repl_gram.y"
  {
    (yyval.str) = (yyvsp[0].str);
  }
#line 1332 "repl_gram.c"
  break;

  case 16:                                    
#line 149 "repl_gram.y"
  {
    (yyval.str) = psprintf("%s.%s", (yyvsp[-2].str), (yyvsp[0].str));
  }
#line 1338 "repl_gram.c"
  break;

  case 17:                                                       
#line 158 "repl_gram.y"
  {
    BaseBackupCmd *cmd = makeNode(BaseBackupCmd);
    cmd->options = (yyvsp[0].list);
    (yyval.node) = (Node *)cmd;
  }
#line 1348 "repl_gram.c"
  break;

  case 18:                                                                  
#line 167 "repl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].defelt));
  }
#line 1354 "repl_gram.c"
  break;

  case 19:                                    
#line 169 "repl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 1360 "repl_gram.c"
  break;

  case 20:                                       
#line 174 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("label", (Node *)makeString((yyvsp[0].str)), -1);
  }
#line 1369 "repl_gram.c"
  break;

  case 21:                                   
#line 179 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("progress", (Node *)makeInteger(true), -1);
  }
#line 1378 "repl_gram.c"
  break;

  case 22:                               
#line 184 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("fast", (Node *)makeInteger(true), -1);
  }
#line 1387 "repl_gram.c"
  break;

  case 23:                              
#line 189 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("wal", (Node *)makeInteger(true), -1);
  }
#line 1396 "repl_gram.c"
  break;

  case 24:                                 
#line 194 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("nowait", (Node *)makeInteger(true), -1);
  }
#line 1405 "repl_gram.c"
  break;

  case 25:                                          
#line 199 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("max_rate", (Node *)makeInteger((yyvsp[0].uintval)), -1);
  }
#line 1414 "repl_gram.c"
  break;

  case 26:                                         
#line 204 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("tablespace_map", (Node *)makeInteger(true), -1);
  }
#line 1423 "repl_gram.c"
  break;

  case 27:                                             
#line 209 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("noverify_checksums", (Node *)makeInteger(true), -1);
  }
#line 1432 "repl_gram.c"
  break;

  case 28:                                                                                                              
#line 218 "repl_gram.y"
  {
    CreateReplicationSlotCmd *cmd;
    cmd = makeNode(CreateReplicationSlotCmd);
    cmd->kind = REPLICATION_KIND_PHYSICAL;
    cmd->slotname = (yyvsp[-3].str);
    cmd->temporary = (yyvsp[-2].boolval);
    cmd->options = (yyvsp[0].list);
    (yyval.node) = (Node *)cmd;
  }
#line 1446 "repl_gram.c"
  break;

  case 29:                                                                                                                   
#line 229 "repl_gram.y"
  {
    CreateReplicationSlotCmd *cmd;
    cmd = makeNode(CreateReplicationSlotCmd);
    cmd->kind = REPLICATION_KIND_LOGICAL;
    cmd->slotname = (yyvsp[-4].str);
    cmd->temporary = (yyvsp[-3].boolval);
    cmd->plugin = (yyvsp[-1].str);
    cmd->options = (yyvsp[0].list);
    (yyval.node) = (Node *)cmd;
  }
#line 1461 "repl_gram.c"
  break;

  case 30:                                                                  
#line 243 "repl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].defelt));
  }
#line 1467 "repl_gram.c"
  break;

  case 31:                                    
#line 245 "repl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 1473 "repl_gram.c"
  break;

  case 32:                                          
#line 250 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("export_snapshot", (Node *)makeInteger(true), -1);
  }
#line 1482 "repl_gram.c"
  break;

  case 33:                                            
#line 255 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("export_snapshot", (Node *)makeInteger(false), -1);
  }
#line 1491 "repl_gram.c"
  break;

  case 34:                                       
#line 260 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("use_snapshot", (Node *)makeInteger(true), -1);
  }
#line 1500 "repl_gram.c"
  break;

  case 35:                                      
#line 265 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem("reserve_wal", (Node *)makeInteger(true), -1);
  }
#line 1509 "repl_gram.c"
  break;

  case 36:                                                            
#line 274 "repl_gram.y"
  {
    DropReplicationSlotCmd *cmd;
    cmd = makeNode(DropReplicationSlotCmd);
    cmd->slotname = (yyvsp[0].str);
    cmd->wait = false;
    (yyval.node) = (Node *)cmd;
  }
#line 1521 "repl_gram.c"
  break;

  case 37:                                                                   
#line 282 "repl_gram.y"
  {
    DropReplicationSlotCmd *cmd;
    cmd = makeNode(DropReplicationSlotCmd);
    cmd->slotname = (yyvsp[-1].str);
    cmd->wait = true;
    (yyval.node) = (Node *)cmd;
  }
#line 1533 "repl_gram.c"
  break;

  case 38:                                                                                        
#line 296 "repl_gram.y"
  {
    StartReplicationCmd *cmd;

    cmd = makeNode(StartReplicationCmd);
    cmd->kind = REPLICATION_KIND_PHYSICAL;
    cmd->slotname = (yyvsp[-3].str);
    cmd->startpoint = (yyvsp[-1].recptr);
    cmd->timeline = (yyvsp[0].uintval);
    (yyval.node) = (Node *)cmd;
  }
#line 1548 "repl_gram.c"
  break;

  case 39:                                                                                                   
#line 311 "repl_gram.y"
  {
    StartReplicationCmd *cmd;
    cmd = makeNode(StartReplicationCmd);
    cmd->kind = REPLICATION_KIND_LOGICAL;
    cmd->slotname = (yyvsp[-3].str);
    cmd->startpoint = (yyvsp[-1].recptr);
    cmd->options = (yyvsp[0].list);
    (yyval.node) = (Node *)cmd;
  }
#line 1562 "repl_gram.c"
  break;

  case 40:                                                   
#line 326 "repl_gram.y"
  {
    TimeLineHistoryCmd *cmd;

    if ((yyvsp[0].uintval) <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), (errmsg("invalid timeline %u", (yyvsp[0].uintval)))));
    }

    cmd = makeNode(TimeLineHistoryCmd);
    cmd->timeline = (yyvsp[0].uintval);

    (yyval.node) = (Node *)cmd;
  }
#line 1580 "repl_gram.c"
  break;

  case 43:                                  
#line 347 "repl_gram.y"
  {
    (yyval.boolval) = true;
  }
#line 1586 "repl_gram.c"
  break;

  case 44:                             
#line 348 "repl_gram.y"
  {
    (yyval.boolval) = false;
  }
#line 1592 "repl_gram.c"
  break;

  case 45:                              
#line 353 "repl_gram.y"
  {
    (yyval.str) = (yyvsp[0].str);
  }
#line 1598 "repl_gram.c"
  break;

  case 46:                        
#line 355 "repl_gram.y"
  {
    (yyval.str) = NULL;
  }
#line 1604 "repl_gram.c"
  break;

  case 47:                                       
#line 360 "repl_gram.y"
  {
    if ((yyvsp[0].uintval) <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), (errmsg("invalid timeline %u", (yyvsp[0].uintval)))));
    }
    (yyval.uintval) = (yyvsp[0].uintval);
  }
#line 1616 "repl_gram.c"
  break;

  case 48:                            
#line 367 "repl_gram.y"
  {
    (yyval.uintval) = 0;
  }
#line 1622 "repl_gram.c"
  break;

  case 49:                                               
#line 372 "repl_gram.y"
  {
    (yyval.list) = (yyvsp[-1].list);
  }
#line 1628 "repl_gram.c"
  break;

  case 50:                              
#line 373 "repl_gram.y"
  {
    (yyval.list) = NIL;
  }
#line 1634 "repl_gram.c"
  break;

  case 51:                                        
#line 378 "repl_gram.y"
  {
    (yyval.list) = list_make1((yyvsp[0].defelt));
  }
#line 1642 "repl_gram.c"
  break;

  case 52:                                                            
#line 382 "repl_gram.y"
  {
    (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].defelt));
  }
#line 1650 "repl_gram.c"
  break;

  case 53:                                             
#line 389 "repl_gram.y"
  {
    (yyval.defelt) = makeDefElem((yyvsp[-1].str), (yyvsp[0].node), -1);
  }
#line 1658 "repl_gram.c"
  break;

  case 54:                              
#line 395 "repl_gram.y"
  {
    (yyval.node) = (Node *)makeString((yyvsp[0].str));
  }
#line 1664 "repl_gram.c"
  break;

  case 55:                              
#line 396 "repl_gram.y"
  {
    (yyval.node) = NULL;
  }
#line 1670 "repl_gram.c"
  break;

#line 1674 "repl_gram.c"

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

#line 399 "repl_gram.y"

#include "repl_scanner.c"
