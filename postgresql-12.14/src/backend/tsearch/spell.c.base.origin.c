/*-------------------------------------------------------------------------
 *
 * spell.c
 *		Normalizing word with ISpell
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * Ispell dictionary
 * -----------------
 *
 * Rules of dictionaries are defined in two files with .affix and .dict
 * extensions. They are used by spell checker programs Ispell and Hunspell.
 *
 * An .affix file declares morphological rules to get a basic form of words.
 * The format of an .affix file has different structure for Ispell and Hunspell
 * dictionaries. The Hunspell format is more complicated. But when an .affix
 * file is imported and compiled, it is stored in the same structure AffixNode.
 *
 * A .dict file stores a list of basic forms of words with references to
 * affix rules. The format of a .dict file has the same structure for Ispell
 * and Hunspell dictionaries.
 *
 * Compilation of a dictionary
 * ---------------------------
 *
 * A compiled dictionary is stored in the IspellDict structure. Compilation of
 * a dictionary is divided into the several steps:
 *	- NIImportDictionary() - stores each word of a .dict file in the
 *	  temporary Spell field.
 *	- NIImportAffixes() - stores affix rules of an .affix file in the
 *	  Affix field (not temporary) if an .affix file has the Ispell format.
 *	  -> NIImportOOAffixes() - stores affix rules if an .affix file has the
 *		 Hunspell format. The AffixData field is initialized if AF
 *parameter is defined.
 *	- NISortDictionary() - builds a prefix tree (Trie) from the words list
 *	  and stores it in the Dictionary field. The words list is got from the
 *	  Spell field. The AffixData field is initialized if AF parameter is not
 *	  defined.
 *	- NISortAffixes():
 *	  - builds a list of compound affixes from the affix list and stores it
 *		in the CompoundAffix.
 *	  - builds prefix trees (Trie) from the affix list for prefixes and
 *suffixes and stores them in Suffix and Prefix fields. The affix list is got
 *from the Affix field.
 *
 * Memory management
 * -----------------
 *
 * The IspellDict structure has the Spell field which is used only in compile
 * time. The Spell field stores a words list. It can take a lot of memory.
 * Therefore when a dictionary is compiled this field is cleared by
 * NIFinishBuild().
 *
 * All resources which should cleared by NIFinishBuild() is initialized using
 * tmpalloc() and tmpalloc0().
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/spell.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "miscadmin.h"
#include "tsearch/dicts/spell.h"
#include "tsearch/ts_locale.h"
#include "utils/memutils.h"

/*
 * Initialization requires a lot of memory that's not needed
 * after the initialization is done.  During initialization,
 * CurrentMemoryContext is the long-lived memory context associated
 * with the dictionary cache entry.  We keep the short-lived stuff
 * in the Conf->buildCxt context.
 */
#define tmpalloc(sz) MemoryContextAlloc(Conf->buildCxt, (sz))
#define tmpalloc0(sz) MemoryContextAllocZero(Conf->buildCxt, (sz))

/*
 * Prepare for constructing an ISpell dictionary.
 *
 * The IspellDict struct is assumed to be zeroed when allocated.
 */
void
NIStartBuild(IspellDict *Conf)
{





}

/*
 * Clean up when dictionary construction is complete.
 */
void
NIFinishBuild(IspellDict *Conf)
{







}

/*
 * "Compact" palloc: allocate without extra palloc overhead.
 *
 * Since we have no need to free the ispell data items individually, there's
 * not much value in the per-chunk overhead normally consumed by palloc.
 * Getting rid of it is helpful since ispell can allocate a lot of small nodes.
 *
 * We currently pre-zero all data allocated this way, even though some of it
 * doesn't need that.  The cpalloc and cpalloc0 macros are just documentation
 * to indicate which allocations actually require zeroing.
 */
#define COMPACT_ALLOC_CHUNK 8192 /* amount to get from palloc at once */
#define COMPACT_MAX_REQ 1024     /* must be < COMPACT_ALLOC_CHUNK */

static void *
compact_palloc0(IspellDict *Conf, size_t size)
{


























}

#define cpalloc(size) compact_palloc0(Conf, size)
#define cpalloc0(size) compact_palloc0(Conf, size)

static char *
cpstrdup(IspellDict *Conf, const char *str)
{




}

/*
 * Apply lowerstr(), producing a temporary result (in the buildCxt).
 */
static char *
lowerstr_ctx(IspellDict *Conf, const char *src)
{








}

#define MAX_NORM 1024
#define MAXNORMLEN 256

#define STRNCMP(s, p) strncmp((s), (p), strlen(p))
#define GETWCHAR(W, L, N, T) (((const uint8 *)(W))[((T) == FF_PREFIX) ? (N) : ((L)-1 - (N))])
#define GETCHAR(A, N, T) GETWCHAR((A)->repl, (A)->replen, N, T)

static char *VoidString = "";

static int
cmpspell(const void *s1, const void *s2)
{

}

static int
cmpspellaffix(const void *s1, const void *s2)
{

}

static int
cmpcmdflag(const void *f1, const void *f2)
{















}

static char *
findchar(char *str, int c)
{










}

static char *
findchar2(char *str, int c1, int c2)
{










}

/* backward string compare for suffix tree operations */
static int
strbcmp(const unsigned char *s1, const unsigned char *s2)
{

























}

static int
strbncmp(const unsigned char *s1, const unsigned char *s2, size_t count)
{





























}

/*
 * Compares affixes.
 * First compares the type of an affix. Prefixes should go before affixes.
 * If types are equal then compares replaceable string.
 */
static int
cmpaffix(const void *s1, const void *s2)
{



















}

/*
 * Gets an affix flag from the set of affix flags (sflagset).
 *
 * Several flags can be stored in a single string. Flags can be represented by:
 * - 1 character (FM_CHAR). A character may be Unicode.
 * - 2 characters (FM_LONG). A character may be Unicode.
 * - numbers from 1 to 65000 (FM_NUM).
 *
 * Depending on the flagMode an affix string can have the following format:
 * - FM_CHAR: ABCD
 *	 Here we have 4 flags: A, B, C and D
 * - FM_LONG: ABCDE*
 *	 Here we have 3 flags: AB, CD and E*
 * - FM_NUM: 200,205,50
 *	 Here we have 3 flags: 200, 205 and 50
 *
 * Conf: current dictionary.
 * sflagset: the set of affix flags. Returns a reference to the start of a next
 *			 affix flag.
 * sflag: returns an affix flag from sflagset.
 */
static void
getNextFlagFromString(IspellDict *Conf, char **sflagset, char *sflag)
{

















































































}

/*
 * Checks if the affix set Conf->AffixData[affix] contains affixflag.
 * Conf->AffixData[affix] does not contain affixflag if this flag is not used
 * actually by the .dict file.
 *
 * Conf: current dictionary.
 * affix: index of the Conf->AffixData array.
 * affixflag: the affix flag.
 *
 * Returns true if the string Conf->AffixData[affix] contains affixflag,
 * otherwise returns false.
 */
static bool
IsAffixFlagInUse(IspellDict *Conf, int affix, const char *affixflag)
{
























}

/*
 * Adds the new word into the temporary array Spell.
 *
 * Conf: current dictionary.
 * word: new word.
 * flag: set of affix flags. Single flag can be get by getNextFlagFromString().
 */
static void
NIAddSpell(IspellDict *Conf, const char *word, const char *flag)
{

















}

/*
 * Imports dictionary into the temporary array Spell.
 *
 * Note caller must already have applied get_tsearch_config_filename.
 *
 * Conf: current dictionary.
 * filename: path to the .dict file.
 */
void
NIImportDictionary(IspellDict *Conf, const char *filename)
{



























































}

/*
 * Searches a basic form of word in the prefix tree. This word was generated
 * using an affix rule. This rule may not be presented in an affix set of
 * a basic form of word.
 *
 * For example, we have the entry in the .dict file:
 * meter/GMD
 *
 * The affix rule with the flag S:
 * SFX S   y	 ies		[^aeiou]y
 * is not presented here.
 *
 * The affix rule with the flag M:
 * SFX M   0	 's         .
 * is presented here.
 *
 * Conf: current dictionary.
 * word: basic form of word.
 * affixflag: affix flag, by which a basic form of word was generated.
 * flag: compound flag used to compare with StopMiddle->compoundflag.
 *
 * Returns 1 if the word was found in the prefix tree, else returns 0.
 */
static int
FindWord(IspellDict *Conf, const char *word, const char *affixflag, int flag)
{






























































}

/*
 * Context reset/delete callback for a regular expression used in an affix
 */
static void
regex_affix_deletion_callback(void *arg)
{



}

/*
 * Adds a new affix rule to the Affix field.
 *
 * Conf: current dictionary.
 * flag: affix flag ('\' in the below example).
 * flagflags: set of flags from the flagval field for this affix rule. This set
 *			  is listed after '/' character in the added string
 *(repl).
 *
 *			  For example L flag in the hunspell_sample.affix:
 *			  SFX \   0 Y/L [^Y]
 *
 * mask: condition for search ('[^Y]' in the above example).
 * find: stripping characters from beginning (at prefix) or end (at suffix)
 *		 of the word ('0' in the above example, 0 means that there is
 *not stripping character). repl: adding string after stripping ('Y' in the
 *above example). type: FF_SUFFIX or FF_PREFIX.
 */
static void
NIAddAffix(IspellDict *Conf, const char *flag, char flagflags, const char *mask, const char *find, const char *repl, int type)
{





































































































}

/* Parsing states for parse_affentry() and friends */
#define PAE_WAIT_MASK 0
#define PAE_INMASK 1
#define PAE_WAIT_FIND 2
#define PAE_INFIND 3
#define PAE_WAIT_REPL 4
#define PAE_INREPL 5
#define PAE_WAIT_TYPE 6
#define PAE_WAIT_FLAG 7

/*
 * Parse next space-separated field of an .affix file line.
 *
 * *str is the input pointer (will be advanced past field)
 * next is where to copy the field value to, with null termination
 *
 * The buffer at "next" must be of size BUFSIZ; we truncate the input to fit.
 *
 * Returns true if we found a field, false if not.
 */
static bool
get_nextfield(char **str, char *next)
{

















































}

/*
 * Parses entry of an .affix file of MySpell or Hunspell format.
 *
 * An .affix file entry has the following format:
 * - header
 *	 <type>  <flag>  <cross_flag>  <flag_count>
 * - fields after header:
 *	 <type>  <flag>  <find>  <replace>	<mask>
 *
 * str is the input line
 * field values are returned to type etc, which must be buffers of size BUFSIZ.
 *
 * Returns number of fields found; any omitted fields are set to empty strings.
 */
static int
parse_ooaffentry(char *str, char *type, char *flag, char *find, char *repl, char *mask)
{

















































}

/*
 * Parses entry of an .affix file of Ispell format
 *
 * An .affix file entry has the following format:
 * <mask>  >  [-<find>,]<replace>
 */
static bool
parse_affentry(char *str, char *mask, char *find, char *repl)
{
















































































































}

/*
 * Sets a Hunspell options depending on flag type.
 */
static void
setCompoundAffixFlagValue(IspellDict *Conf, CompoundAffixFlag *entry, char *s, uint32 val)
{
























}

/*
 * Sets up a correspondence for the affix parameter with the affix flag.
 *
 * Conf: current dictionary.
 * s: affix flag in string.
 * val: affix parameter.
 */
static void
addCompoundAffixFlagValue(IspellDict *Conf, char *s, uint32 val)
{















































}

/*
 * Returns a set of affix parameters which correspondence to the set of affix
 * flags s.
 */
static int
getCompoundAffixFlagValue(IspellDict *Conf, char *s)
{
























}

/*
 * Returns a flag set using the s parameter.
 *
 * If Conf->useFlagAliases is true then the s parameter is index of the
 * Conf->AffixData array and function returns its entry.
 * Else function returns the s parameter.
 */
static char *
getAffixFlagSet(IspellDict *Conf, char *s)
{






























}

/*
 * Import an affix file that follows MySpell or Hunspell format.
 *
 * Conf: current dictionary.
 * filename: path to the .affix file.
 */
static void
NIImportOOAffixes(IspellDict *Conf, const char *filename)
{










































































































































































































































}

/*
 * import affixes
 *
 * Note caller must already have applied get_tsearch_config_filename
 *
 * This function is responsible for parsing ispell ("old format") affix files.
 * If we realize that the file contains new-format commands, we pass off the
 * work to NIImportOOAffixes(), which will re-read the whole file.
 */
void
NIImportAffixes(IspellDict *Conf, const char *filename)
{



















































































































































}

/*
 * Merges two affix flag sets and stores a new affix flag set into
 * Conf->AffixData.
 *
 * Returns index of a new affix flag set.
 */
static int
MergeAffix(IspellDict *Conf, int a1, int a2)
{




































}

/*
 * Returns a set of affix parameters which correspondence to the set of affix
 * flags with the given index.
 */
static uint32
makeCompoundFlags(IspellDict *Conf, int affix)
{



}

/*
 * Makes a prefix tree for the given level.
 *
 * Conf: current dictionary.
 * low: lower index of the Conf->Spell array.
 * high: upper index of the Conf->Spell array.
 * level: current prefix tree level.
 */
static SPNode *
mkSPNode(IspellDict *Conf, int low, int high, int level)
{


















































































}

/*
 * Builds the Conf->Dictionary tree and AffixData from the imported dictionary
 * and affixes.
 */
void
NISortDictionary(IspellDict *Conf)
{























































































}

/*
 * Makes a prefix tree for the given level using the repl string of an affix
 * rule. Affixes with empty replace string do not include in the prefix tree.
 * This affixes are included by mkVoidAffix().
 *
 * Conf: current dictionary.
 * low: lower index of the Conf->Affix array.
 * high: upper index of the Conf->Affix array.
 * level: current prefix tree level.
 * type: FF_SUFFIX or FF_PREFIX.
 */
static AffixNode *
mkANode(IspellDict *Conf, int low, int high, int level, int type)
{










































































}

/*
 * Makes the root void node in the prefix tree. The root void node is created
 * for affixes which have empty replace string ("repl" field).
 */
static void
mkVoidAffix(IspellDict *Conf, bool issuffix, int startsuffix)
{














































}

/*
 * Checks if the affixflag is used by dictionary. Conf->AffixData does not
 * contain affixflag if this flag is not used actually by the .dict file.
 *
 * Conf: current dictionary.
 * affixflag: affix flag.
 *
 * Returns true if the Conf->AffixData array contains affixflag, otherwise
 * returns false.
 */
static bool
isAffixInUse(IspellDict *Conf, char *affixflag)
{











}

/*
 * Builds Conf->Prefix and Conf->Suffix trees from the imported affixes.
 */
void
NISortAffixes(IspellDict *Conf)
{
















































}

static AffixNodeData *
FindAffixes(AffixNode *node, const char *word, int wrdlen, int *level, int type)
{














































}

static char *
CheckAffix(const char *word, size_t len, AFFIX *Affix, int flagflags, char *newword, int *baselen)
{











































































































}

static int
addToResult(char **forms, char **cur, char *word)
{












}

static char **
NormalizeSubWord(IspellDict *Conf, char *word, int flag)
{
















































































































}

typedef struct SplitVar
{
  int nstem;
  int lenstem;
  char **stem;
  struct SplitVar *next;
} SplitVar;

static int
CheckCompoundAffixes(CMPDAffix **ptr, char *word, int len, bool CheckInPlace)
{







































}

static SplitVar *
CopyVar(SplitVar *s, int makedup)
{






















}

static void
AddStem(SplitVar *v, char *word)
{








}

static SplitVar *
SplitToVariants(IspellDict *Conf, SPNode *snode, SplitVar *orig, char *word, int wordlen, int startpos, int minpos)
{















































































































































































}

static void
addNorm(TSLexeme **lres, TSLexeme **lcur, char *word, int flags, uint16 NVariant)
{













}

TSLexeme *
NINormalizeWord(IspellDict *Conf, char *word)
{
































































}