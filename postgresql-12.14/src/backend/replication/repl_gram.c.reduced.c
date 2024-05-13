/* A Bison parser, made by GNU Bison 3.7.5.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30705

/* Bison version string.  */
#define YYBISON_VERSION "3.7.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the variable and function names.  */
#define yyparse replication_yyparse


#define yydebug replication_yydebug




/* First part of user prologue.  */
#line 1 "repl_gram.y"

/*-------------------------------------------------------------------------
 *
 * repl_gram.y				- Parser for the replication commands
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/repl_gram.y
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/xlogdefs.h"
#include "nodes/makefuncs.h"
#include "nodes/replnodes.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"

/* Result of the parsing is returned here */
Node *replication_parse_result;

/*
 * Bison doesn't allocate anything that needs to live across parser calls,
 * so we can easily have it use palloc instead of malloc.  This prevents
 * memory leaks if we error out during parsing.  Note this only works with
 * bison >= 2.0.  However, in bison 1.875 the default is to use alloca()
 * if possible, so there's not really much problem anyhow, at least if
 * you're building with gcc.
 */
#define YYMALLOC palloc


#line 119 "repl_gram.c"

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else

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

/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
extern int replication_yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype {
  YYEMPTY = -2,
  YYEOF = 0,                       /* "end of file"  */
  YYerror = 256,                   /* error  */
  YYUNDEF = 257,                   /* "invalid token"  */
  SCONST = 258,                    /* SCONST  */
  IDENT = 259,                     /* IDENT  */
  UCONST = 260,                    /* UCONST  */
  RECPTR = 261,                    /* RECPTR  */
  K_BASE_BACKUP = 262,             /* K_BASE_BACKUP  */
  K_IDENTIFY_SYSTEM = 263,         /* K_IDENTIFY_SYSTEM  */
  K_SHOW = 264,                    /* K_SHOW  */
  K_START_REPLICATION = 265,       /* K_START_REPLICATION  */
  K_CREATE_REPLICATION_SLOT = 266, /* K_CREATE_REPLICATION_SLOT  */
  K_DROP_REPLICATION_SLOT = 267,   /* K_DROP_REPLICATION_SLOT  */
  K_TIMELINE_HISTORY = 268,        /* K_TIMELINE_HISTORY  */
  K_LABEL = 269,                   /* K_LABEL  */
  K_PROGRESS = 270,                /* K_PROGRESS  */
  K_FAST = 271,                    /* K_FAST  */
  K_WAIT = 272,                    /* K_WAIT  */
  K_NOWAIT = 273,                  /* K_NOWAIT  */
  K_MAX_RATE = 274,                /* K_MAX_RATE  */
  K_WAL = 275,                     /* K_WAL  */
  K_TABLESPACE_MAP = 276,          /* K_TABLESPACE_MAP  */
  K_NOVERIFY_CHECKSUMS = 277,      /* K_NOVERIFY_CHECKSUMS  */
  K_TIMELINE = 278,                /* K_TIMELINE  */
  K_PHYSICAL = 279,                /* K_PHYSICAL  */
  K_LOGICAL = 280,                 /* K_LOGICAL  */
  K_SLOT = 281,                    /* K_SLOT  */
  K_RESERVE_WAL = 282,             /* K_RESERVE_WAL  */
  K_TEMPORARY = 283,               /* K_TEMPORARY  */
  K_EXPORT_SNAPSHOT = 284,         /* K_EXPORT_SNAPSHOT  */
  K_NOEXPORT_SNAPSHOT = 285,       /* K_NOEXPORT_SNAPSHOT  */
  K_USE_SNAPSHOT = 286             /* K_USE_SNAPSHOT  */
};
typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
union YYSTYPE {
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

/* Symbol kind.  */
enum yysymbol_kind_t {
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_SCONST = 3,                     /* SCONST  */
  YYSYMBOL_IDENT = 4,                      /* IDENT  */
  YYSYMBOL_UCONST = 5,                     /* UCONST  */
  YYSYMBOL_RECPTR = 6,                     /* RECPTR  */
  YYSYMBOL_K_BASE_BACKUP = 7,              /* K_BASE_BACKUP  */
  YYSYMBOL_K_IDENTIFY_SYSTEM = 8,          /* K_IDENTIFY_SYSTEM  */
  YYSYMBOL_K_SHOW = 9,                     /* K_SHOW  */
  YYSYMBOL_K_START_REPLICATION = 10,       /* K_START_REPLICATION  */
  YYSYMBOL_K_CREATE_REPLICATION_SLOT = 11, /* K_CREATE_REPLICATION_SLOT  */
  YYSYMBOL_K_DROP_REPLICATION_SLOT = 12,   /* K_DROP_REPLICATION_SLOT  */
  YYSYMBOL_K_TIMELINE_HISTORY = 13,        /* K_TIMELINE_HISTORY  */
  YYSYMBOL_K_LABEL = 14,                   /* K_LABEL  */
  YYSYMBOL_K_PROGRESS = 15,                /* K_PROGRESS  */
  YYSYMBOL_K_FAST = 16,                    /* K_FAST  */
  YYSYMBOL_K_WAIT = 17,                    /* K_WAIT  */
  YYSYMBOL_K_NOWAIT = 18,                  /* K_NOWAIT  */
  YYSYMBOL_K_MAX_RATE = 19,                /* K_MAX_RATE  */
  YYSYMBOL_K_WAL = 20,                     /* K_WAL  */
  YYSYMBOL_K_TABLESPACE_MAP = 21,          /* K_TABLESPACE_MAP  */
  YYSYMBOL_K_NOVERIFY_CHECKSUMS = 22,      /* K_NOVERIFY_CHECKSUMS  */
  YYSYMBOL_K_TIMELINE = 23,                /* K_TIMELINE  */
  YYSYMBOL_K_PHYSICAL = 24,                /* K_PHYSICAL  */
  YYSYMBOL_K_LOGICAL = 25,                 /* K_LOGICAL  */
  YYSYMBOL_K_SLOT = 26,                    /* K_SLOT  */
  YYSYMBOL_K_RESERVE_WAL = 27,             /* K_RESERVE_WAL  */
  YYSYMBOL_K_TEMPORARY = 28,               /* K_TEMPORARY  */
  YYSYMBOL_K_EXPORT_SNAPSHOT = 29,         /* K_EXPORT_SNAPSHOT  */
  YYSYMBOL_K_NOEXPORT_SNAPSHOT = 30,       /* K_NOEXPORT_SNAPSHOT  */
  YYSYMBOL_K_USE_SNAPSHOT = 31,            /* K_USE_SNAPSHOT  */
  YYSYMBOL_32_ = 32,                       /* ';'  */
  YYSYMBOL_33_ = 33,                       /* '.'  */
  YYSYMBOL_34_ = 34,                       /* '('  */
  YYSYMBOL_35_ = 35,                       /* ')'  */
  YYSYMBOL_36_ = 36,                       /* ','  */
  YYSYMBOL_YYACCEPT = 37,                  /* $accept  */
  YYSYMBOL_firstcmd = 38,                  /* firstcmd  */
  YYSYMBOL_opt_semicolon = 39,             /* opt_semicolon  */
  YYSYMBOL_command = 40,                   /* command  */
  YYSYMBOL_identify_system = 41,           /* identify_system  */
  YYSYMBOL_show = 42,                      /* show  */
  YYSYMBOL_var_name = 43,                  /* var_name  */
  YYSYMBOL_base_backup = 44,               /* base_backup  */
  YYSYMBOL_base_backup_opt_list = 45,      /* base_backup_opt_list  */
  YYSYMBOL_base_backup_opt = 46,           /* base_backup_opt  */
  YYSYMBOL_create_replication_slot = 47,   /* create_replication_slot  */
  YYSYMBOL_create_slot_opt_list = 48,      /* create_slot_opt_list  */
  YYSYMBOL_create_slot_opt = 49,           /* create_slot_opt  */
  YYSYMBOL_drop_replication_slot = 50,     /* drop_replication_slot  */
  YYSYMBOL_start_replication = 51,         /* start_replication  */
  YYSYMBOL_start_logical_replication = 52, /* start_logical_replication  */
  YYSYMBOL_timeline_history = 53,          /* timeline_history  */
  YYSYMBOL_opt_physical = 54,              /* opt_physical  */
  YYSYMBOL_opt_temporary = 55,             /* opt_temporary  */
  YYSYMBOL_opt_slot = 56,                  /* opt_slot  */
  YYSYMBOL_opt_timeline = 57,              /* opt_timeline  */
  YYSYMBOL_plugin_options = 58,            /* plugin_options  */
  YYSYMBOL_plugin_opt_list = 59,           /* plugin_opt_list  */
  YYSYMBOL_plugin_opt_elem = 60,           /* plugin_opt_elem  */
  YYSYMBOL_plugin_opt_arg = 61             /* plugin_opt_arg  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;

#ifdef short
#undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
#include <limits.h> /* INFRINGES ON USER NAME SPACE */
#if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#define YY_STDINT_H
#endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

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

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
#undef UINT_LEAST8_MAX
#undef UINT_LEAST16_MAX
#define UINT_LEAST8_MAX 255
#define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H &&                  \
       UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H &&                 \
       UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
#if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__

#define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
#elif defined PTRDIFF_MAX
#ifndef ptrdiff_t
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
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
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#define YYSIZE_T size_t
#else
#define YYSIZE_T unsigned
#endif
#endif

#define YYSIZE_MAXIMUM                                                         \
  YY_CAST(YYPTRDIFF_T,                                                         \
          (YYPTRDIFF_MAXIMUM < YY_CAST(YYSIZE_T, -1) ? YYPTRDIFF_MAXIMUM       \
                                                     : YY_CAST(YYSIZE_T, -1)))



/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#define YY_(Msgid) dgettext("bison-runtime", Msgid)
#endif
#endif
#ifndef YY_

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

/* Suppress unused-variable warnings by "using" E.  */
#if !defined lint || defined __GNUC__

#else
#define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && !defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                    \
  _Pragma("GCC diagnostic push")                                               \
      _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")                    \
          _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN                                           \
  _Pragma("GCC diagnostic push")                                               \
      _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif



#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

#ifdef YYSTACK_USE_ALLOCA
#if YYSTACK_USE_ALLOCA
#ifdef __GNUC__
#define YYSTACK_ALLOC __builtin_alloca
#elif defined __BUILTIN_VA_ARG_INCR
#include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#elif defined _AIX
#define YYSTACK_ALLOC __alloca
#elif defined _MSC_VER
#include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#define alloca _alloca
#else
#define YYSTACK_ALLOC alloca
#if !defined _ALLOCA_H && !defined EXIT_SUCCESS
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
/* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#endif
#endif
#endif

#ifdef YYSTACK_ALLOC
/* Pacify GCC's 'empty if-body' warning.  */
#define YYSTACK_FREE(Ptr)                                                      \
  do { /* empty */                                                             \
    ;                                                                          \
  } while (0)
#ifndef YYSTACK_ALLOC_MAXIMUM
/* The OS might guarantee only one guard page at the bottom of the stack,
   and a page size can be as small as 4096 bytes.  So we cannot safely
   invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
   to allow for a few compiler-allocated temporary stack slots.  */
#define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#endif
#else
#define YYSTACK_ALLOC YYMALLOC

#ifndef YYSTACK_ALLOC_MAXIMUM
#define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#endif
#if (defined __cplusplus && !defined EXIT_SUCCESS &&                           \
     !((defined YYMALLOC || defined malloc) &&                                 \
       (defined YYFREE || defined free)))
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#ifndef YYMALLOC
#define YYMALLOC malloc
#if !defined malloc && !defined EXIT_SUCCESS
void *malloc(YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#ifndef YYFREE
#define YYFREE free
#if !defined free && !defined EXIT_SUCCESS
void
free(void *); /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#endif
#endif /* !defined yyoverflow */

#if (!defined yyoverflow &&                                                    \
     (!defined __cplusplus ||                                                  \
      (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc {
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */


/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
#define YYSTACK_BYTES(N)                                                       \
  ((N) * (YYSIZEOF(yy_state_t) + YYSIZEOF(YYSTYPE)) + YYSTACK_GAP_MAXIMUM)

#define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
#define YYSTACK_RELOCATE(Stack_alloc, Stack)                                   \








#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
#ifndef YYCOPY
#if defined __GNUC__ && 1 < __GNUC__
#define YYCOPY(Dst, Src, Count)                                                \

#else
#define YYCOPY(Dst, Src, Count)                                                \
  do {                                                                         \
    YYPTRDIFF_T yyi;                                                           \
    for (yyi = 0; yyi < (Count); yyi++)                                        \
      (Dst)[yyi] = (Src)[yyi];                                                 \
  } while (0)
#endif
#endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */

/* YYLAST -- Last index in YYTABLE.  */


/* YYNTOKENS -- Number of terminals.  */

/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS 25
/* YYNRULES -- Number of rules.  */
#define YYNRULES 55
/* YYNSTATES -- Number of states.  */
#define YYNSTATES 74

/* YYMAXUTOK -- Last valid token kind.  */


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                       \




/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] = {
    0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  34, 35, 2,  2,  36, 2,  33, 2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  32, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] = {
    0,   105, 105, 111, 112, 116, 117, 118, 119, 120, 121, 122, 123, 130,
    140, 147, 148, 157, 166, 169, 173, 178, 183, 188, 193, 198, 203, 208,
    217, 228, 242, 245, 249, 254, 259, 264, 273, 281, 295, 310, 325, 342,
    343, 347, 348, 352, 355, 359, 367, 372, 373, 377, 381, 388, 395, 396};
#endif

/** Accessing symbol of state STATE.  */


#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *
yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] = {"\"end of file\"",
                                      "error",
                                      "\"invalid token\"",
                                      "SCONST",
                                      "IDENT",
                                      "UCONST",
                                      "RECPTR",
                                      "K_BASE_BACKUP",
                                      "K_IDENTIFY_SYSTEM",
                                      "K_SHOW",
                                      "K_START_REPLICATION",
                                      "K_CREATE_REPLICATION_SLOT",
                                      "K_DROP_REPLICATION_SLOT",
                                      "K_TIMELINE_HISTORY",
                                      "K_LABEL",
                                      "K_PROGRESS",
                                      "K_FAST",
                                      "K_WAIT",
                                      "K_NOWAIT",
                                      "K_MAX_RATE",
                                      "K_WAL",
                                      "K_TABLESPACE_MAP",
                                      "K_NOVERIFY_CHECKSUMS",
                                      "K_TIMELINE",
                                      "K_PHYSICAL",
                                      "K_LOGICAL",
                                      "K_SLOT",
                                      "K_RESERVE_WAL",
                                      "K_TEMPORARY",
                                      "K_EXPORT_SNAPSHOT",
                                      "K_NOEXPORT_SNAPSHOT",
                                      "K_USE_SNAPSHOT",
                                      "';'",
                                      "'.'",
                                      "'('",
                                      "')'",
                                      "','",
                                      "$accept",
                                      "firstcmd",
                                      "opt_semicolon",
                                      "command",
                                      "identify_system",
                                      "show",
                                      "var_name",
                                      "base_backup",
                                      "base_backup_opt_list",
                                      "base_backup_opt",
                                      "create_replication_slot",
                                      "create_slot_opt_list",
                                      "create_slot_opt",
                                      "drop_replication_slot",
                                      "start_replication",
                                      "start_logical_replication",
                                      "timeline_history",
                                      "opt_physical",
                                      "opt_temporary",
                                      "opt_slot",
                                      "opt_timeline",
                                      "plugin_options",
                                      "plugin_opt_list",
                                      "plugin_opt_elem",
                                      "plugin_opt_arg",
                                      YY_NULLPTR};

static const char *
yysymbol_name(yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] = {
    0,   256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267,
    268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280,
    281, 282, 283, 284, 285, 286, 59,  46,  40,  41,  44};
#endif





#define YYTABLE_NINF (-1)



/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int8 yypact[] = {
    2,   -26, -26, -1,  -9,  21,  22,  23,  27,  -2,  -26, -26, -26, -26, -26,
    -26, -26, -26, -14, -26, -4,  28,  7,   5,   17,  -26, -26, -26, -26, 32,
    -26, -26, -26, 31,  -26, -26, -26, -26, 33,  13,  -26, 34,  -26, -3,  -26,
    -26, -26, -26, 35,  16,  -26, 38,  9,   39,  -26, -11, -26, 41,  -26, -26,
    -26, -26, -26, -26, -26, -11, 43,  -12, -26, -26, -26, -26, 41,  -26};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] = {
    0,  19, 13, 0,  46, 0,  0,  0,  0,  4,  5, 12, 6,  9,  10, 7,  8,  11, 17,
    15, 14, 0,  42, 44, 36, 40, 1,  3,  2,  0, 21, 22, 24, 0,  23, 26, 27, 18,
    0,  45, 41, 0,  43, 0,  37, 20, 25, 16, 0, 48, 31, 0,  50, 0,  38, 28, 31,
    0,  39, 47, 35, 32, 33, 34, 30, 29, 55, 0, 51, 54, 53, 49, 0,  52};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] = {
    -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -8, -26,
    -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -25, -26};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] = {0,  8,  28, 9,  10, 11, 20, 12, 18,
                                        37, 13, 55, 64, 14, 15, 16, 17, 41,
                                        43, 22, 54, 58, 67, 68, 70};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] = {
    29, 30, 31, 19, 32, 33, 34, 35, 36, 1,  2,  3,  4,  5,  6,  7,  60,
    21, 61, 62, 63, 50, 51, 71, 72, 23, 24, 26, 25, 38, 27, 40, 39, 42,
    44, 45, 46, 47, 48, 53, 49, 52, 56, 57, 59, 66, 69, 73, 65};

static const yytype_int8 yycheck[] = {
    14, 15, 16, 4,  18, 19, 20, 21, 22, 7,  8, 9, 10, 11, 12, 13, 27,
    26, 29, 30, 31, 24, 25, 35, 36, 4,  4,  0, 5, 33, 32, 24, 4,  28,
    17, 3,  5,  4,  25, 23, 6,  6,  4,  34, 5, 4, 3,  72, 56};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] = {
    0,  7,  8,  9,  10, 11, 12, 13, 38, 40, 41, 42, 44, 47, 50, 51, 52, 53, 45,
    4,  43, 26, 56, 4,  4,  5,  0,  32, 39, 14, 15, 16, 18, 19, 20, 21, 22, 46,
    33, 4,  24, 54, 28, 55, 17, 3,  5,  4,  25, 6,  24, 25, 6,  23, 57, 48, 4,
    34, 58, 5,  27, 29, 30, 31, 49, 48, 4,  59, 60, 3,  61, 35, 36, 60};

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] = {
    0,  37, 38, 39, 39, 40, 40, 40, 40, 40, 40, 40, 40, 41, 42, 43, 43, 44, 45,
    45, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 48, 48, 49, 49, 49, 49, 50, 50,
    51, 52, 53, 54, 54, 55, 55, 56, 56, 57, 57, 58, 58, 59, 59, 60, 61, 61};

/* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] = {0, 2, 2, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                   2, 1, 3, 2, 2, 0, 2, 1, 1, 1, 1, 2, 1, 1,
                                   5, 6, 2, 0, 1, 1, 1, 1, 2, 3, 5, 6, 2, 1,
                                   0, 1, 0, 2, 0, 2, 0, 3, 0, 1, 3, 2, 1, 0};

enum { YYENOMEM = -2 };

#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = YYEMPTY)





#define YYRECOVERING() (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                                 \
  do                                                                           \
    if (yychar == YYEMPTY) {                                                   \
      yychar = (Token);                                                        \
      yylval = (Value);                                                        \
      YYPOPSTACK(yylen);                                                       \
      yystate = *yyssp;                                                        \
      goto yybackup;                                                           \
    } else {                                                                   \
      yyerror(YY_("syntax error: cannot back up"));                            \
      YYERROR;                                                                 \
    }                                                                          \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* Enable debugging if requested.  */
#if YYDEBUG

#ifndef YYFPRINTF
#include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#define YYFPRINTF fprintf
#endif

#define YYDPRINTF(Args)                                                        \
  do {                                                                         \
    if (yydebug)                                                               \
      YYFPRINTF Args;                                                          \
  } while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
#define YY_LOCATION_PRINT(File, Loc) ((void)0)
#endif

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                          \
  do {                                                                         \
    if (yydebug) {                                                             \
      YYFPRINTF(stderr, "%s ", Title);                                         \
      yy_symbol_print(stderr, Kind, Value);                                    \
      YYFPRINTF(stderr, "\n");                                                 \
    }                                                                          \
  } while (0)

/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind,
                      YYSTYPE const *const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE(yyoutput);
  if (!yyvaluep) {
    return;
  }
#ifdef YYPRINT
  if (yykind < YYNTOKENS) {
    YYPRINT(yyo, yytoknum[yykind], *yyvaluep);
  }
#endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind,
                YYSTYPE const *const yyvaluep)
{
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm",
            yysymbol_name(yykind));

  yy_symbol_value_print(yyo, yykind, yyvaluep);
  YYFPRINTF(yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print(yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF(stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++) {
    int yybot = *yybottom;
    YYFPRINTF(stderr, " %d", yybot);
  }
  YYFPRINTF(stderr, "\n");
}

#define YY_STACK_PRINT(Bottom, Top)                                            \
  do {                                                                         \
    if (yydebug)                                                               \
      yy_stack_print((Bottom), (Top));                                         \
  } while (0)

/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1,
            yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++) {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]),
                    &yyvsp[(yyi + 1) - (yynrhs)]);
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)                                                  \
  do {                                                                         \
    if (yydebug)                                                               \
      yy_reduce_print(yyssp, yyvsp, Rule);                                     \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
#define YY_STACK_PRINT(Bottom, Top)
#define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH

#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH

#endif

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct(const char *yymsg, yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{









}

/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;

/*----------.
| yyparse.  |
`----------*/

int
yyparse(void)





























































#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else




#if defined yyoverflow
    {
      /* Give user a chance to reallocate the stack.  Use copies of
         these so that the &'s don't force the real ones into
         memory.  */
      yy_state_t *yyss1 = yyss;
      YYSTYPE *yyvs1 = yyvs;

      /* Each stack pointer address is followed by the size of the
         data in use in that stack, in bytes.  This used to be a
         conditional around just the two extra args, but that might
         be undefined if yyoverflow is a macro.  */
      yyoverflow(YY_("memory exhausted"), &yyss1, yysize * YYSIZEOF(*yyssp),
                 &yyvs1, yysize * YYSIZEOF(*yyvsp), &yystacksize);
      yyss = yyss1;
      yyvs = yyvs1;
    }
#else /* defined YYSTACK_RELOCATE */
    /* Extend the stack our own way.  */
















































































































































































































































































































































































































































































































































































































































































































































#line 399 "repl_gram.y"

#include "repl_scanner.c"
