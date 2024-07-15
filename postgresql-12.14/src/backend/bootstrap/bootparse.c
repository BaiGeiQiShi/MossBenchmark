                                               

                                                   
 
                                                                                 
        
 
                                                                        
                                                                        
                                                                     
                                       
 
                                                                   
                                                                  
                                                                 
                                                
 
                                                                     
                                                                            

                                                                      
                                                                     
                                                                   
                                                                     
                                                                       
                                                                    
                                                                      
                                                                  
                                           
 
                                                                       
                            

                                                             
                                                            

                                                                  
                                                                
                                                                     

                                                                  
                                                                      
                                                                  
                                                                 
                                                                  
                              

                                                
#define YYBISON 30705

                            
#define YYBISON_VERSION "3.7.5"

                     
#define YYSKELETON_NAME "yacc.c"

                    
#define YYPURE 0

                    
#define YYPUSH 0

                    
#define YYPULL 1

                                                  
#define yyparse boot_yyparse
#define yylex boot_yylex
#define yyerror boot_yyerror
#define yydebug boot_yydebug
#define yynerrs boot_yynerrs
#define yylval boot_yylval
#define yychar boot_yychar

                                   
#line 1 "bootparse.y"

                                                                            
   
               
                                                             
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>

#include "access/attnum.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/tupdesc.h"
#include "bootstrap/bootstrap.h"
#include "catalog/catalog.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_class.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/toasting.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"
#include "nodes/primnodes.h"
#include "rewrite/prs2lock.h"
#include "storage/block.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/itemptr.h"
#include "storage/off.h"
#include "storage/smgr.h"
#include "tcop/dest.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                           
                                                                         
                                                                           
                                                                         
                                                                       
                             
   
#define YYMALLOC palloc
#define YYFREE pfree

static MemoryContext per_line_ctx = NULL;

static void
do_start(void)
{
  Assert(CurrentMemoryContext == CurTransactionContext);
                                                               
  if (per_line_ctx == NULL)
  {
    per_line_ctx = AllocSetContextCreate(CurTransactionContext, "bootstrap per-line processing", ALLOCSET_DEFAULT_SIZES);
  }
  MemoryContextSwitchTo(per_line_ctx);
}

static void
do_end(void)
{
                                                           
  MemoryContextSwitchTo(CurTransactionContext);
  MemoryContextReset(per_line_ctx);
  CHECK_FOR_INTERRUPTS();                                         
  if (isatty(0))
  {
    printf("bootstrap> ");
    fflush(stdout);
  }
}

static int num_columns_read = 0;

#line 177 "bootparse.c"

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
extern int boot_yydebug;
#endif

                   
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype
{
  YYEMPTY = -2,
  YYEOF = 0,                                  
  YYerror = 256,                      
  YYUNDEF = 257,                                
  ID = 258,                        
  COMMA = 259,                        
  EQUALS = 260,                        
  LPAREN = 261,                        
  RPAREN = 262,                        
  NULLVAL = 263,                        
  OPEN = 264,                        
  XCLOSE = 265,                        
  XCREATE = 266,                        
  INSERT_TUPLE = 267,                        
  XDECLARE = 268,                        
  INDEX = 269,                        
  ON = 270,                        
  USING = 271,                        
  XBUILD = 272,                        
  INDICES = 273,                        
  UNIQUE = 274,                        
  XTOAST = 275,                        
  OBJ_ID = 276,                        
  XBOOTSTRAP = 277,                        
  XSHARED_RELATION = 278,                        
  XROWTYPE_OID = 279,                        
  XFORCE = 280,                        
  XNOT = 281,                        
  XNULL = 282                         
};
typedef enum yytokentype yytoken_kind_t;
#endif

                  
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 104 "bootparse.y"

  List *list;
  IndexElem *ielem;
  char *str;
  const char *kw;
  int ival;
  Oid oidval;

#line 260 "bootparse.c"
};
typedef union YYSTYPE YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE boot_yylval;

int
boot_yyparse(void);

                   
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                                            
  YYSYMBOL_YYerror = 1,                                  
  YYSYMBOL_YYUNDEF = 2,                                            
  YYSYMBOL_ID = 3,                                    
  YYSYMBOL_COMMA = 4,                                    
  YYSYMBOL_EQUALS = 5,                                    
  YYSYMBOL_LPAREN = 6,                                    
  YYSYMBOL_RPAREN = 7,                                    
  YYSYMBOL_NULLVAL = 8,                                    
  YYSYMBOL_OPEN = 9,                                    
  YYSYMBOL_XCLOSE = 10,                                   
  YYSYMBOL_XCREATE = 11,                                   
  YYSYMBOL_INSERT_TUPLE = 12,                                   
  YYSYMBOL_XDECLARE = 13,                                   
  YYSYMBOL_INDEX = 14,                                   
  YYSYMBOL_ON = 15,                                   
  YYSYMBOL_USING = 16,                                   
  YYSYMBOL_XBUILD = 17,                                   
  YYSYMBOL_INDICES = 18,                                   
  YYSYMBOL_UNIQUE = 19,                                   
  YYSYMBOL_XTOAST = 20,                                   
  YYSYMBOL_OBJ_ID = 21,                                   
  YYSYMBOL_XBOOTSTRAP = 22,                                   
  YYSYMBOL_XSHARED_RELATION = 23,                                   
  YYSYMBOL_XROWTYPE_OID = 24,                                   
  YYSYMBOL_XFORCE = 25,                                   
  YYSYMBOL_XNOT = 26,                                   
  YYSYMBOL_XNULL = 27,                                   
  YYSYMBOL_YYACCEPT = 28,                                  
  YYSYMBOL_TopLevel = 29,                                   
  YYSYMBOL_Boot_Queries = 30,                                   
  YYSYMBOL_Boot_Query = 31,                                   
  YYSYMBOL_Boot_OpenStmt = 32,                                   
  YYSYMBOL_Boot_CloseStmt = 33,                                   
  YYSYMBOL_Boot_CreateStmt = 34,                                   
  YYSYMBOL_35_1 = 35,                                  
  YYSYMBOL_36_2 = 36,                                  
  YYSYMBOL_Boot_InsertStmt = 37,                                   
  YYSYMBOL_38_3 = 38,                                  
  YYSYMBOL_Boot_DeclareIndexStmt = 39,                                   
  YYSYMBOL_Boot_DeclareUniqueIndexStmt = 40,                                   
  YYSYMBOL_Boot_DeclareToastStmt = 41,                                   
  YYSYMBOL_Boot_BuildIndsStmt = 42,                                   
  YYSYMBOL_boot_index_params = 43,                                   
  YYSYMBOL_boot_index_param = 44,                                   
  YYSYMBOL_optbootstrap = 45,                                   
  YYSYMBOL_optsharedrelation = 46,                                   
  YYSYMBOL_optrowtypeoid = 47,                                   
  YYSYMBOL_boot_column_list = 48,                                   
  YYSYMBOL_boot_column_def = 49,                                   
  YYSYMBOL_boot_column_nullness = 50,                                   
  YYSYMBOL_oidspec = 51,                                   
  YYSYMBOL_boot_column_val_list = 52,                                   
  YYSYMBOL_boot_column_val = 53,                                   
  YYSYMBOL_boot_ident = 54                                    
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

                                                        
#define YYFINAL 46
                                       
#define YYLAST 169

                                        
#define YYNTOKENS 28
                                        
#define YYNNTS 27
                                   
#define YYNRULES 65
                                     
#define YYNSTATES 110

                                          
#define YYMAXUTOK 282

                                                                      
                                                         
#define YYTRANSLATE(YYX) (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

                                                                      
                            
static const yytype_int8 yytranslate[] = {0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

#if YYDEBUG
                                                                     
static const yytype_int16 yyrline[] = {0, 134, 134, 135, 139, 140, 144, 145, 146, 147, 148, 149, 150, 151, 155, 164, 174, 184, 173, 270, 269, 288, 338, 388, 400, 410, 411, 415, 430, 431, 435, 436, 440, 441, 445, 446, 450, 459, 460, 461, 465, 469, 470, 471, 475, 477, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 495, 496, 497, 498, 499, 500, 501};
#endif

                                        
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
                                                                 
                                     
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

                                                                
                                                                       
static const char *const yytname[] = {"\"end of file\"", "error", "\"invalid token\"", "ID", "COMMA", "EQUALS", "LPAREN", "RPAREN", "NULLVAL", "OPEN", "XCLOSE", "XCREATE", "INSERT_TUPLE", "XDECLARE", "INDEX", "ON", "USING", "XBUILD", "INDICES", "UNIQUE", "XTOAST", "OBJ_ID", "XBOOTSTRAP", "XSHARED_RELATION", "XROWTYPE_OID", "XFORCE", "XNOT", "XNULL", "$accept", "TopLevel", "Boot_Queries", "Boot_Query", "Boot_OpenStmt", "Boot_CloseStmt", "Boot_CreateStmt", "$@1", "$@2", "Boot_InsertStmt", "$@3", "Boot_DeclareIndexStmt", "Boot_DeclareUniqueIndexStmt", "Boot_DeclareToastStmt", "Boot_BuildIndsStmt", "boot_index_params", "boot_index_param", "optbootstrap", "optsharedrelation", "optrowtypeoid", "boot_column_list", "boot_column_def", "boot_column_nullness", "oidspec", "boot_column_val_list", "boot_column_val", "boot_ident", YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
                                                                 
                                                                    
static const yytype_int16 yytoknum[] = {0, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282};
#endif

#define YYPACT_NINF (-53)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

                                                                   
                 
static const yytype_int16 yypact[] = {-4, 142, 142, 142, -53, 2, -14, 25, -4, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, 142, 20, 142, 13, 142, -53, -53, -53, 6, -53, 117, 142, 142, 142, -53, 8, -53, 92, -53, -53, 14, 142, 17, -53, 9, 117, -53, -53, 142, 19, 142, 142, 29, -53, 21, 142, -53, -53, -53, 142, 22, 142, 30, 142, 35, -53, 37, 142, 34, 142, 36, 142, 10, -53, 142, 142, -53, -53, 23, 142, -53, -53, 11, -3, -53, -53, -53, 18, -53, -53};

                                                                       
                                                                       
                                     
static const yytype_int8 yydefact[] = {3, 0, 0, 0, 19, 0, 0, 0, 2, 4, 6, 7, 8, 9, 10, 11, 12, 13, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 14, 15, 0, 0, 0, 0, 0, 24, 1, 5, 29, 40, 0, 0, 0, 0, 28, 31, 45, 0, 41, 44, 0, 0, 0, 30, 33, 0, 20, 42, 0, 0, 0, 0, 0, 43, 0, 0, 23, 32, 16, 0, 0, 0, 0, 0, 17, 34, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 35, 18, 39, 0, 21, 27, 0, 0, 36, 25, 22, 0, 38, 37};

                          
static const yytype_int8 yypgoto[] = {-53, -53, -53, 38, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -51, -52, -53, -53, -53, -53, -39, -53, -41, -53, -46, -1};

                            
static const yytype_int8 yydefgoto[] = {0, 7, 8, 9, 10, 11, 12, 81, 90, 13, 41, 14, 15, 16, 17, 92, 93, 55, 64, 72, 84, 85, 104, 48, 57, 58, 49};

                                                                    
                                                                   
                                                              
static const yytype_int8 yytable[] = {38, 39, 40, 53, 45, 1, 2, 3, 4, 5, 60, 67, 62, 6, 99, 99, 42, 100, 106, 73, 69, 43, 44, 107, 108, 46, 50, 52, 54, 68, 77, 63, 70, 71, 75, 78, 87, 79, 83, 89, 95, 51, 91, 97, 102, 109, 47, 105, 103, 59, 96, 61, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 59, 0, 0, 74, 0, 76, 0, 0, 0, 0, 80, 0, 0, 0, 82, 0, 86, 0, 88, 0, 0, 0, 94, 0, 86, 0, 98, 0, 0, 101, 94, 18, 65, 0, 94, 66, 56, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 18, 0, 0, 0, 0, 56, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 18, 0, 0, 0, 0, 0, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37};

static const yytype_int8 yycheck[] = {1, 2, 3, 44, 18, 9, 10, 11, 12, 13, 51, 57, 53, 17, 4, 4, 14, 7, 7, 65, 61, 19, 20, 26, 27, 0, 6, 14, 22, 15, 71, 23, 15, 24, 15, 6, 6, 16, 16, 4, 6, 42, 5, 7, 95, 27, 8, 99, 25, 50, 89, 52, -1, -1, -1, -1, 57, -1, -1, -1, -1, -1, -1, -1, 65, -1, -1, 68, -1, 70, -1, -1, -1, -1, 75, -1, -1, -1, 79, -1, 81, -1, 83, -1, -1, -1, 87, -1, 89, -1, 91, -1, -1, 94, 95, 3, 4, -1, 99, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 3, -1, -1, -1, -1, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 3, -1, -1, -1, -1, -1, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

                                                               
                                 
static const yytype_int8 yystos[] = {0, 9, 10, 11, 12, 13, 17, 29, 30, 31, 32, 33, 34, 37, 39, 40, 41, 42, 3, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 54, 54, 54, 38, 14, 19, 20, 18, 0, 31, 51, 54, 6, 54, 14, 51, 22, 45, 8, 52, 53, 54, 51, 54, 51, 23, 46, 4, 7, 53, 15, 51, 15, 24, 47, 53, 54, 15, 54, 51, 6, 16, 54, 35, 54, 16, 48, 49, 54, 6, 54, 4, 36, 5, 43, 44, 54, 6, 49, 7, 54, 4, 7, 54, 43, 25, 50, 44, 7, 26, 27, 27};

                                                                  
static const yytype_int8 yyr1[] = {0, 28, 29, 29, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31, 32, 33, 35, 36, 34, 38, 37, 39, 40, 41, 42, 43, 43, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 50, 50, 50, 51, 52, 52, 52, 53, 53, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54};

                                                                         
static const yytype_int8 yyr2[] = {0, 2, 1, 0, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 11, 0, 5, 11, 12, 6, 2, 3, 1, 2, 1, 0, 1, 0, 2, 0, 1, 3, 4, 3, 2, 0, 1, 1, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

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
  case 14:                                      
#line 156 "bootparse.y"
  {
    do_start();
    boot_openrel((yyvsp[0].str));
    do_end();
  }
#line 1387 "bootparse.c"
  break;

  case 15:                                         
#line 165 "bootparse.y"
  {
    do_start();
    closerel((yyvsp[0].str));
    do_end();
  }
#line 1397 "bootparse.c"
  break;

  case 16:                   
#line 174 "bootparse.y"
  {
    do_start();
    numattr = 0;
    elog(DEBUG4, "creating%s%s relation %s %u", (yyvsp[-3].ival) ? " bootstrap" : "", (yyvsp[-2].ival) ? " shared" : "", (yyvsp[-5].str), (yyvsp[-4].oidval));
  }
#line 1411 "bootparse.c"
  break;

  case 17:                   
#line 184 "bootparse.y"
  {
    do_end();
  }
#line 1419 "bootparse.c"
  break;

  case 18:                                                                                                                                       
#line 188 "bootparse.y"
  {
    TupleDesc tupdesc;
    bool shared_relation;
    bool mapped_relation;

    do_start();

    tupdesc = CreateTupleDesc(numattr, attrtypes);

    shared_relation = (yyvsp[-6].ival);

       
                                                         
                                                             
                                                          
                                                          
                                                             
                                                             
                                      
       
    mapped_relation = ((yyvsp[-7].ival) || shared_relation);

    if ((yyvsp[-7].ival))
    {
      TransactionId relfrozenxid;
      MultiXactId relminmxid;

      if (boot_reldesc)
      {
        elog(DEBUG4, "create bootstrap: warning, open relation exists, closing first");
        closerel(NULL);
      }

      boot_reldesc = heap_create((yyvsp[-9].str), PG_CATALOG_NAMESPACE, shared_relation ? GLOBALTABLESPACE_OID : 0, (yyvsp[-8].oidval), InvalidOid, HEAP_TABLE_AM_OID, tupdesc, RELKIND_RELATION, RELPERSISTENCE_PERMANENT, shared_relation, mapped_relation, true, &relfrozenxid, &relminmxid);
      elog(DEBUG4, "bootstrap relation created");
    }
    else
    {
      Oid id;

      id = heap_create_with_catalog((yyvsp[-9].str), PG_CATALOG_NAMESPACE, shared_relation ? GLOBALTABLESPACE_OID : 0, (yyvsp[-8].oidval), (yyvsp[-5].oidval), InvalidOid, BOOTSTRAP_SUPERUSERID, HEAP_TABLE_AM_OID, tupdesc, NIL, RELKIND_RELATION, RELPERSISTENCE_PERMANENT, shared_relation, mapped_relation, ONCOMMIT_NOOP, (Datum)0, false, true, false, InvalidOid, NULL);
      elog(DEBUG4, "relation created with OID %u", id);
    }
    do_end();
  }
#line 1502 "bootparse.c"
  break;

  case 19:                   
#line 270 "bootparse.y"
  {
    do_start();
    elog(DEBUG4, "inserting row");
    num_columns_read = 0;
  }
#line 1512 "bootparse.c"
  break;

  case 20:                                                                            
#line 276 "bootparse.y"
  {
    if (num_columns_read != numattr)
    {
      elog(ERROR, "incorrect number of columns in row (expected %d, got %d)", numattr, num_columns_read);
    }
    if (boot_reldesc == NULL)
    {
      elog(FATAL, "relation not open");
    }
    InsertOneTuple();
    do_end();
  }
#line 1526 "bootparse.c"
  break;

  case 21:                                                                                                                               
#line 289 "bootparse.y"
  {
    IndexStmt *stmt = makeNode(IndexStmt);
    Oid relationId;

    elog(DEBUG4, "creating index \"%s\"", (yyvsp[-8].str));

    do_start();

    stmt->idxname = (yyvsp[-8].str);
    stmt->relation = makeRangeVar(NULL, (yyvsp[-5].str), -1);
    stmt->accessMethod = (yyvsp[-3].str);
    stmt->tableSpace = NULL;
    stmt->indexParams = (yyvsp[-1].list);
    stmt->indexIncludingParams = NIL;
    stmt->options = NIL;
    stmt->whereClause = NULL;
    stmt->excludeOpNames = NIL;
    stmt->idxcomment = NULL;
    stmt->indexOid = InvalidOid;
    stmt->oldNode = InvalidOid;
    stmt->unique = false;
    stmt->primary = false;
    stmt->isconstraint = false;
    stmt->deferrable = false;
    stmt->initdeferred = false;
    stmt->transformed = false;
    stmt->concurrent = false;
    stmt->if_not_exists = false;
    stmt->reset_default_tblspc = false;

                                                               
    relationId = RangeVarGetRelid(stmt->relation, NoLock, false);

    DefineIndex(relationId, stmt, (yyvsp[-7].oidval), InvalidOid, InvalidOid, false, false, false, true,                 
        false);
    do_end();
  }
#line 1577 "bootparse.c"
  break;

  case 22:                                                                                                                                            
#line 339 "bootparse.y"
  {
    IndexStmt *stmt = makeNode(IndexStmt);
    Oid relationId;

    elog(DEBUG4, "creating unique index \"%s\"", (yyvsp[-8].str));

    do_start();

    stmt->idxname = (yyvsp[-8].str);
    stmt->relation = makeRangeVar(NULL, (yyvsp[-5].str), -1);
    stmt->accessMethod = (yyvsp[-3].str);
    stmt->tableSpace = NULL;
    stmt->indexParams = (yyvsp[-1].list);
    stmt->indexIncludingParams = NIL;
    stmt->options = NIL;
    stmt->whereClause = NULL;
    stmt->excludeOpNames = NIL;
    stmt->idxcomment = NULL;
    stmt->indexOid = InvalidOid;
    stmt->oldNode = InvalidOid;
    stmt->unique = true;
    stmt->primary = false;
    stmt->isconstraint = false;
    stmt->deferrable = false;
    stmt->initdeferred = false;
    stmt->transformed = false;
    stmt->concurrent = false;
    stmt->if_not_exists = false;
    stmt->reset_default_tblspc = false;

                                                               
    relationId = RangeVarGetRelid(stmt->relation, NoLock, false);

    DefineIndex(relationId, stmt, (yyvsp[-7].oidval), InvalidOid, InvalidOid, false, false, false, true,                 
        false);
    do_end();
  }
#line 1628 "bootparse.c"
  break;

  case 23:                                                                            
#line 389 "bootparse.y"
  {
    elog(DEBUG4, "creating toast table for table \"%s\"", (yyvsp[0].str));

    do_start();

    BootstrapToastTable((yyvsp[0].str), (yyvsp[-3].oidval), (yyvsp[-2].oidval));
    do_end();
  }
#line 1641 "bootparse.c"
  break;

  case 24:                                          
#line 401 "bootparse.y"
  {
    do_start();
    build_indices();
    do_end();
  }
#line 1651 "bootparse.c"
  break;

  case 25:                                                                   
#line 410 "bootparse.y"
  {
    (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].ielem));
  }
#line 1657 "bootparse.c"
  break;

  case 26:                                           
#line 411 "bootparse.y"
  {
    (yyval.list) = list_make1((yyvsp[0].ielem));
  }
#line 1663 "bootparse.c"
  break;

  case 27:                                               
#line 416 "bootparse.y"
  {
    IndexElem *n = makeNode(IndexElem);
    n->name = (yyvsp[-1].str);
    n->expr = NULL;
    n->indexcolname = NULL;
    n->collation = NIL;
    n->opclass = list_make1(makeString((yyvsp[0].str)));
    n->ordering = SORTBY_DEFAULT;
    n->nulls_ordering = SORTBY_NULLS_DEFAULT;
    (yyval.ielem) = n;
  }
#line 1679 "bootparse.c"
  break;

  case 28:                                
#line 430 "bootparse.y"
  {
    (yyval.ival) = 1;
  }
#line 1685 "bootparse.c"
  break;

  case 29:                            
#line 431 "bootparse.y"
  {
    (yyval.ival) = 0;
  }
#line 1691 "bootparse.c"
  break;

  case 30:                                           
#line 435 "bootparse.y"
  {
    (yyval.ival) = 1;
  }
#line 1697 "bootparse.c"
  break;

  case 31:                                 
#line 436 "bootparse.y"
  {
    (yyval.ival) = 0;
  }
#line 1703 "bootparse.c"
  break;

  case 32:                                           
#line 440 "bootparse.y"
  {
    (yyval.oidval) = (yyvsp[0].oidval);
  }
#line 1709 "bootparse.c"
  break;

  case 33:                             
#line 441 "bootparse.y"
  {
    (yyval.oidval) = InvalidOid;
  }
#line 1715 "bootparse.c"
  break;

  case 36:                                                                          
#line 451 "bootparse.y"
  {
    if (++numattr > MAXATTR)
    {
      elog(FATAL, "too many columns");
    }
    DefineAttr((yyvsp[-3].str), (yyvsp[-1].str), numattr - 1, (yyvsp[0].ival));
  }
#line 1725 "bootparse.c"
  break;

  case 37:                                               
#line 459 "bootparse.y"
  {
    (yyval.ival) = BOOTCOL_NULL_FORCE_NOT_NULL;
  }
#line 1731 "bootparse.c"
  break;

  case 38:                                          
#line 460 "bootparse.y"
  {
    (yyval.ival) = BOOTCOL_NULL_FORCE_NULL;
  }
#line 1737 "bootparse.c"
  break;

  case 39:                                    
#line 461 "bootparse.y"
  {
    (yyval.ival) = BOOTCOL_NULL_AUTO;
  }
#line 1743 "bootparse.c"
  break;

  case 40:                           
#line 465 "bootparse.y"
  {
    (yyval.oidval) = atooid((yyvsp[0].str));
  }
#line 1749 "bootparse.c"
  break;

  case 44:                                   
#line 476 "bootparse.y"
  {
    InsertOneValue((yyvsp[0].str), num_columns_read++);
  }
#line 1755 "bootparse.c"
  break;

  case 45:                                
#line 478 "bootparse.y"
  {
    InsertOneNull(num_columns_read++);
  }
#line 1761 "bootparse.c"
  break;

  case 46:                      
#line 482 "bootparse.y"
  {
    (yyval.str) = (yyvsp[0].str);
  }
#line 1767 "bootparse.c"
  break;

  case 47:                        
#line 483 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1773 "bootparse.c"
  break;

  case 48:                          
#line 484 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1779 "bootparse.c"
  break;

  case 49:                           
#line 485 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1785 "bootparse.c"
  break;

  case 50:                                
#line 486 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1791 "bootparse.c"
  break;

  case 51:                            
#line 487 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1797 "bootparse.c"
  break;

  case 52:                         
#line 488 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1803 "bootparse.c"
  break;

  case 53:                      
#line 489 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1809 "bootparse.c"
  break;

  case 54:                         
#line 490 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1815 "bootparse.c"
  break;

  case 55:                          
#line 491 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1821 "bootparse.c"
  break;

  case 56:                           
#line 492 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1827 "bootparse.c"
  break;

  case 57:                          
#line 493 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1833 "bootparse.c"
  break;

  case 58:                          
#line 494 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1839 "bootparse.c"
  break;

  case 59:                          
#line 495 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1845 "bootparse.c"
  break;

  case 60:                              
#line 496 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1851 "bootparse.c"
  break;

  case 61:                                    
#line 497 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1857 "bootparse.c"
  break;

  case 62:                                
#line 498 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1863 "bootparse.c"
  break;

  case 63:                          
#line 499 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1869 "bootparse.c"
  break;

  case 64:                        
#line 500 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1875 "bootparse.c"
  break;

  case 65:                         
#line 501 "bootparse.y"
  {
    (yyval.str) = pstrdup((yyvsp[0].kw));
  }
#line 1881 "bootparse.c"
  break;

#line 1885 "bootparse.c"

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

#line 503 "bootparse.y"

#include "bootscanner.c"
