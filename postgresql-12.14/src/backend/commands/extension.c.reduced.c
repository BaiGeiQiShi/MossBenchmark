/*-------------------------------------------------------------------------
 *
 * extension.c
 *	  Commands to manipulate extensions
 *
 * Extensions in PostgreSQL allow management of collections of SQL objects.
 *
 * All we need internally to manage an extension is an OID so that the
 * dependent objects can be associated with it.  An extension is created by
 * populating the pg_extension catalog from a "control" file.
 * The extension control file is parsed with the same parser we use for
 * postgresql.conf.  An extension also has an installation script file,
 * containing SQL commands to create the extension's objects.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/extension.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <dirent.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/extension.h"
#include "commands/schemacmds.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "storage/fd.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/varlena.h"

/* Globally visible state variables */
bool creating_extension = false;
Oid CurrentExtensionObject = InvalidOid;

/*
 * Internal data structure to hold the results of parsing a control file
 */
typedef struct ExtensionControlFile {
  char *name;            /* name of the extension */
  char *directory;       /* directory for script files */
  char *default_version; /* default install target version, if any */
  char *module_pathname; /* string to substitute for
                          * MODULE_PATHNAME */
  char *comment;         /* comment, if any */
  char *schema;          /* target schema (allowed if !relocatable) */
  bool relocatable;      /* is ALTER EXTENSION SET SCHEMA supported? */
  bool superuser;        /* must be superuser to install? */
  int encoding;          /* encoding of the script file, or -1 */
  List *
    requires; /* names of prerequisite extensions */
} ExtensionControlFile;

/*
 * Internal data structure for update path information
 */
typedef struct ExtensionVersionInfo {
  char *name;       /* name of the starting version */
  List *reachable;  /* List of ExtensionVersionInfo's */
  bool installable; /* does this version have an install script? */
  /* working state for Dijkstra's algorithm: */
  bool distance_known;                   /* is distance from start known yet? */
  int distance;                          /* current worst-case distance estimate */
  struct ExtensionVersionInfo *previous; /* current best predecessor */
} ExtensionVersionInfo;

/* Local functions */
static List *
find_update_path(List *evi_list, ExtensionVersionInfo *evi_start, ExtensionVersionInfo *evi_target, bool reject_indirect, bool reinitialize);
static Oid
get_required_extension(char *reqExtensionName, char *extensionName, char *origSchemaName, bool cascade, List *parents, bool is_create);
static void
get_available_versions_for_extension(ExtensionControlFile *pcontrol, Tuplestorestate *tupstore, TupleDesc tupdesc);
static Datum
convert_requires_to_datum(List *requires);
static void
ApplyExtensionUpdates(Oid extensionOid, ExtensionControlFile *pcontrol, const char *initialVersion, List *updateVersions, char *origSchemaName, bool cascade, bool is_create);
static char *
read_whole_file(const char *filename, int *length);

/*
 * get_extension_oid - given an extension name, look up the OID
 *
 * If missing_ok is false, throw an error if extension name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_extension_oid(const char *extname, bool missing_ok)
{
  Oid result;
  Relation rel;
  SysScanDesc scandesc;
  HeapTuple tuple;
  ScanKeyData entry[1];

  rel = table_open(ExtensionRelationId, AccessShareLock);

  ScanKeyInit(&entry[0], Anum_pg_extension_extname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(extname));

  scandesc = systable_beginscan(rel, ExtensionNameIndexId, true, NULL, 1, entry);

  tuple = systable_getnext(scandesc);

  /* We assume that there can be at most one matching tuple */
  if (HeapTupleIsValid(tuple)) {
    result = ((Form_pg_extension)GETSTRUCT(tuple))->oid;
  } else {
    result = InvalidOid;
  }

  systable_endscan(scandesc);

  table_close(rel, AccessShareLock);

  if (!OidIsValid(result) && !missing_ok) {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("extension \"%s\" does not exist", extname)));
  }

  return result;
}

/*
 * get_extension_name - given an extension OID, look up the name
 *
 * Returns a palloc'd string, or NULL if no such extension.
 */
char *
get_extension_name(Oid ext_oid)
{


























}

/*
 * get_extension_schema - given an extension OID, fetch its extnamespace
 *
 * Returns InvalidOid if no such extension.
 */
static Oid
get_extension_schema(Oid ext_oid)
{


























}

/*
 * Utility functions to check validity of extension and version names
 */
static void
check_valid_extension_name(const char *extensionname)
{
  int namelen = strlen(extensionname);

  /*
   * Disallow empty names (the parser rejects empty identifiers anyway, but
   * let's check).
   */
  if (namelen == 0) {

  }

  /*
   * No double dashes, since that would make script filenames ambiguous.
   */
  if (strstr(extensionname, "--")) {

  }

  /*
   * No leading or trailing dash either.  (We could probably allow this, but
   * it would require much care in filename parsing and would make filenames
   * visually if not formally ambiguous.  Since there's no real-world use
   * case, let's just forbid it.)
   */
  if (extensionname[0] == '-' || extensionname[namelen - 1] == '-') {

  }

  /*
   * No directory separators either (this is sufficient to prevent ".."
   * style attacks).
   */
  if (first_dir_separator(extensionname) != NULL) {

  }
}

static void
check_valid_version_name(const char *versionname)
{
  int namelen = strlen(versionname);

  /*
   * Disallow empty names (we could possibly allow this, but there seems
   * little point).
   */
  if (namelen == 0) {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid extension version name: \"%s\"", versionname), errdetail("Version names must not be empty.")));
  }

  /*
   * No double dashes, since that would make script filenames ambiguous.
   */
  if (strstr(versionname, "--")) {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid extension version name: \"%s\"", versionname), errdetail("Version names must not contain \"--\".")));
  }

  /*
   * No leading or trailing dash either.
   */
  if (versionname[0] == '-' || versionname[namelen - 1] == '-') {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid extension version name: \"%s\"", versionname), errdetail("Version names must not begin or end with \"-\".")));
  }

  /*
   * No directory separators either (this is sufficient to prevent ".."
   * style attacks).
   */
  if (first_dir_separator(versionname) != NULL) {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid extension version name: \"%s\"", versionname),                       errdetail("Version names must not contain directory separator characters.")));
  }
}

/*
 * Utility functions to handle extension-related path names
 */
static bool
is_extension_control_filename(const char *filename)
{
  const char *extension = strrchr(filename, '.');

  return (extension != NULL) && (strcmp(extension, ".control") == 0);
}

static bool
is_extension_script_filename(const char *filename)
{
  const char *extension = strrchr(filename, '.');

  return (extension != NULL) && (strcmp(extension, ".sql") == 0);
}

static char *
get_extension_control_directory(void)
{
  char sharepath[MAXPGPATH];
  char *result;

  get_share_path(my_exec_path, sharepath);
  result = (char *)palloc(MAXPGPATH);
  snprintf(result, MAXPGPATH, "%s/extension", sharepath);

  return result;
}

static char *
get_extension_control_filename(const char *extname)
{
  char sharepath[MAXPGPATH];
  char *result;

  get_share_path(my_exec_path, sharepath);
  result = (char *)palloc(MAXPGPATH);
  snprintf(result, MAXPGPATH, "%s/extension/%s.control", sharepath, extname);

  return result;
}

static char *
get_extension_script_directory(ExtensionControlFile *control)
{
  char sharepath[MAXPGPATH];
  char *result;

  /*
   * The directory parameter can be omitted, absolute, or relative to the
   * installation's share directory.
   */
  if (!control->directory) {
    return get_extension_control_directory();
  }

  if (is_absolute_path(control->directory)) {

  }






}

static char *
get_extension_aux_control_filename(ExtensionControlFile *control, const char *version)
{
  char *result;
  char *scriptdir;

  scriptdir = get_extension_script_directory(control);

  result = (char *)palloc(MAXPGPATH);
  snprintf(result, MAXPGPATH, "%s/%s--%s.control", scriptdir, control->name, version);

  pfree(scriptdir);

  return result;
}

static char *
get_extension_script_filename(ExtensionControlFile *control, const char *from_version, const char *version)
{
  char *result;
  char *scriptdir;

  scriptdir = get_extension_script_directory(control);

  result = (char *)palloc(MAXPGPATH);
  if (from_version) {
    snprintf(result, MAXPGPATH, "%s/%s--%s--%s.sql", scriptdir, control->name, from_version, version);
  } else {

  }

  pfree(scriptdir);

  return result;
}

/*
 * Parse contents of primary or auxiliary control file, and fill in
 * fields of *control.  We parse primary file if version == NULL,
 * else the optional auxiliary file for that version.
 *
 * Control files are supposed to be very short, half a dozen lines,
 * so we don't worry about memory allocation risks here.  Also we don't
 * worry about what encoding it's in; all values are expected to be ASCII.
 */
static void
parse_extension_control_file(ExtensionControlFile *control, const char *version)
{
  char *filename;
  FILE *file;
  ConfigVariable *item, *head = NULL, *tail = NULL;

  /*
   * Locate the file to read.  Auxiliary files are optional.
   */
  if (version) {
    filename = get_extension_aux_control_filename(control, version);
  } else {
    filename = get_extension_control_filename(control->name);
  }

  if ((file = AllocateFile(filename, "r")) == NULL) {
    if (version && errno == ENOENT) {
      /* no auxiliary file for this version */
      pfree(filename);
      return;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open extension control file \"%s\": %m", filename)));
  }

  /*
   * Parse the file content, using GUC's file parsing code.  We need not
   * check the return value since any errors will be thrown at ERROR level.
   */
  (void)ParseConfigFp(file, filename, 0, ERROR, &head, &tail);

  FreeFile(file);

  /*
   * Convert the ConfigVariable list into ExtensionControlFile entries.
   */
  for (item = head; item != NULL; item = item->next) {
    if (strcmp(item->name, "directory") == 0) {
      if (version) {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("parameter \"%s\" cannot be set in a secondary extension control file",                                                           item->name)));
      }


    } else if (strcmp(item->name, "default_version") == 0) {





    } else if (strcmp(item->name, "module_pathname") == 0) {

    } else if (strcmp(item->name, "comment") == 0) {
      control->comment = pstrdup(item->value);
    } else if (strcmp(item->name, "schema") == 0) {
      control->schema = pstrdup(item->value);
    } else if (strcmp(item->name, "relocatable") == 0) {
      if (!parse_bool(item->value, &control->relocatable)) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("parameter \"%s\" requires a Boolean value", item->name)));
      }
    } else if (strcmp(item->name, "superuser") == 0) {
      if (!parse_bool(item->value, &control->superuser)) {

      }
    } else if (strcmp(item->name, "encoding") == 0) {
      control->encoding = pg_valid_server_encoding(item->value);















  }



  if (control->relocatable && control->schema != NULL) {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("parameter \"schema\" cannot be specified when \"relocatable\" is true")));
  }

  pfree(filename);
}

/*
 * Read the primary control file for the specified extension.
 */
static ExtensionControlFile *
read_extension_control_file(const char *extname)
{
  ExtensionControlFile *control;

  /*
   * Set up default values.  Pointer fields are initially null.
   */
  control = (ExtensionControlFile *)palloc0(sizeof(ExtensionControlFile));
  control->name = pstrdup(extname);
  control->relocatable = false;
  control->superuser = true;
  control->encoding = -1;

  /*
   * Parse the primary control file.
   */
  parse_extension_control_file(control, NULL);

  return control;
}

/*
 * Read the auxiliary control file for the specified extension and version.
 *
 * Returns a new modified ExtensionControlFile struct; the original struct
 * (reflecting just the primary control file) is not modified.
 */
static ExtensionControlFile *
read_extension_aux_control_file(const ExtensionControlFile *pcontrol, const char *version)
{
  ExtensionControlFile *acontrol;

  /*
   * Flat-copy the struct.  Pointer fields share values with original.
   */
  acontrol = (ExtensionControlFile *)palloc(sizeof(ExtensionControlFile));
  memcpy(acontrol, pcontrol, sizeof(ExtensionControlFile));

  /*
   * Parse the auxiliary control file, overwriting struct fields
   */
  parse_extension_control_file(acontrol, version);

  return acontrol;
}

/*
 * Read an SQL script file into a string, and convert to database encoding
 */
static char *
read_extension_script_file(const ExtensionControlFile *control, const char *filename)
{
  int src_encoding;
  char *src_str;
  char *dest_str;
  int len;

  src_str = read_whole_file(filename, &len);

  /* use database encoding if not given */
  if (control->encoding < 0) {
    src_encoding = GetDatabaseEncoding();
  } else {
    src_encoding = control->encoding;
  }

  /* make sure that source string is valid in the expected encoding */


  /*
   * Convert the encoding to the database encoding. read_whole_file
   * null-terminated the string, so if no conversion happens the string is
   * valid as is.
   */
  dest_str = pg_any_to_server(src_str, len, src_encoding);

  return dest_str;
}

/*
 * Execute given SQL string.
 *
 * Note: it's tempting to just use SPI to execute the string, but that does
 * not work very well.  The really serious problem is that SPI will parse,
 * analyze, and plan the whole string before executing any of it; of course
 * this fails if there are any plannable statements referring to objects
 * created earlier in the script.  A lesser annoyance is that SPI insists
 * on printing the whole string as errcontext in case of any error, and that
 * could be very long.
 */
static void
execute_sql_string(const char *sql)
{
  List *raw_parsetree_list;
  DestReceiver *dest;
  ListCell *lc1;

  /*
   * Parse the SQL string into a list of raw parse trees.
   */
  raw_parsetree_list = pg_parse_query(sql);

  /* All output from SELECTs goes to the bit bucket */
  dest = CreateDestReceiver(DestNone);

  /*
   * Do parse analysis, rule rewrite, planning, and execution for each raw
   * parsetree.  We must fully execute each query before beginning parse
   * analysis on the next one, since there may be interdependencies.
   */
  foreach (lc1, raw_parsetree_list) {
    RawStmt *parsetree = lfirst_node(RawStmt, lc1);
    List *stmt_list;
    ListCell *lc2;

    /* Be sure parser can see any DDL done so far */
    CommandCounterIncrement();

    stmt_list = pg_analyze_and_rewrite(parsetree, sql, NULL, 0, NULL);
    stmt_list = pg_plan_queries(stmt_list, CURSOR_OPT_PARALLEL_OK, NULL);

    foreach (lc2, stmt_list) {
      PlannedStmt *stmt = lfirst_node(PlannedStmt, lc2);

      CommandCounterIncrement();

      PushActiveSnapshot(GetTransactionSnapshot());

      if (stmt->utilityStmt == NULL) {
        QueryDesc *qdesc;

        qdesc = CreateQueryDesc(stmt, sql, GetActiveSnapshot(), NULL, dest, NULL, NULL, 0);







      } else {




        ProcessUtility(stmt, sql, PROCESS_UTILITY_QUERY, NULL, NULL, dest, NULL);
      }

      PopActiveSnapshot();
    }
  }

  /* Be sure to advance the command counter after the last script command */
  CommandCounterIncrement();
}

/*
 * Execute the appropriate script file for installing or updating the extension
 *
 * If from_version isn't NULL, it's an update
 */
static void
execute_extension_script(Oid extensionOid, ExtensionControlFile *control, const char *from_version, const char *version, List *requiredSchemas, const char *schemaName, Oid schemaOid)
{
  char *filename;
  int save_nestlevel;
  StringInfoData pathbuf;
  ListCell *lc;

  /*
   * Enforce superuser-ness if appropriate.  We postpone this check until
   * here so that the flag is correctly associated with the right script(s)
   * if it's set in secondary control files.
   */
  if (control->superuser && !superuser()) {
    if (from_version == NULL) {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create extension \"%s\"", control->name), errhint("Must be superuser to create this extension.")));
    } else {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to update extension \"%s\"", control->name), errhint("Must be superuser to update this extension.")));
    }
  }



  /*
   * Force client_min_messages and log_min_messages to be at least WARNING,
   * so that we won't spam the user with useless NOTICE messages from common
   * script actions like creating shell types.
   *
   * We use the equivalent of a function SET option to allow the setting to
   * persist for exactly the duration of the script execution.  guc.c also
   * takes care of undoing the setting on error.
   */
  save_nestlevel = NewGUCNestLevel();

  if (client_min_messages < WARNING) {
    (void)set_config_option("client_min_messages", "warning", PGC_USERSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);
  }
  if (log_min_messages < WARNING) {
    (void)set_config_option("log_min_messages", "warning", PGC_SUSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);
  }

  /*
   * Similarly disable check_function_bodies, to ensure that SQL functions
   * won't be parsed during creation.
   */
  if (check_function_bodies) {
    (void)set_config_option("check_function_bodies", "off", PGC_USERSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);
  }

  /*
   * Set up the search path to have the target schema first, making it be
   * the default creation target namespace.  Then add the schemas of any
   * prerequisite extensions, unless they are in pg_catalog which would be
   * searched anyway.  (Listing pg_catalog explicitly in a non-first
   * position would be bad for security.)  Finally add pg_temp to ensure
   * that temp objects can't take precedence over others.
   *
   * Note: it might look tempting to use PushOverrideSearchPath for this,
   * but we cannot do that.  We have to actually set the search_path GUC in
   * case the extension script examines or changes it.  In any case, the
   * GUC_ACTION_SAVE method is just as convenient.
   */
  initStringInfo(&pathbuf);
  appendStringInfoString(&pathbuf, quote_identifier(schemaName));
  foreach (lc, requiredSchemas) {
    Oid reqschema = lfirst_oid(lc);
    char *reqname = get_namespace_name(reqschema);

    if (reqname && strcmp(reqname, "pg_catalog") != 0) {

    }
  }




  /*
   * Set creating_extension and related variables so that
   * recordDependencyOnCurrentExtension and other functions do the right
   * things.  On failure, ensure we reset these variables.
   */
  creating_extension = true;
  CurrentExtensionObject = extensionOid;
  PG_TRY();
  {
    char *c_sql = read_extension_script_file(control, filename);
    Datum t_sql;

    /* We use various functions that want to operate on text datums */
    t_sql = CStringGetTextDatum(c_sql);

    /*
     * Reduce any lines beginning with "\echo" to empty.  This allows
     * scripts to contain messages telling people not to run them via
     * psql, which has been found to be necessary due to old habits.
     */
    t_sql = DirectFunctionCall4Coll(textregexreplace, C_COLLATION_OID, t_sql, CStringGetTextDatum("^\\\\echo.*$"), CStringGetTextDatum(""), CStringGetTextDatum("ng"));

    /*
     * If it's not relocatable, substitute the target schema name for
     * occurrences of @extschema@.
     *
     * For a relocatable extension, we needn't do this.  There cannot be
     * any need for @extschema@, else it wouldn't be relocatable.
     */
    if (!control->relocatable) {
      const char *qSchemaName = quote_identifier(schemaName);

      t_sql = DirectFunctionCall3Coll(replace_text, C_COLLATION_OID, t_sql, CStringGetTextDatum("@extschema@"), CStringGetTextDatum(qSchemaName));
    }

    /*
     * If module_pathname was set in the control file, substitute its
     * value for occurrences of MODULE_PATHNAME.
     */
    if (control->module_pathname) {
      t_sql = DirectFunctionCall3Coll(replace_text, C_COLLATION_OID, t_sql, CStringGetTextDatum("MODULE_PATHNAME"), CStringGetTextDatum(control->module_pathname));
    }

    /* And now back to C string */
    c_sql = text_to_cstring(DatumGetTextPP(t_sql));

    execute_sql_string(c_sql);
  }
  PG_CATCH();
  {
    creating_extension = false;
    CurrentExtensionObject = InvalidOid;
    PG_RE_THROW();
  }





  /*
   * Restore the GUC variables we set above.
   */
  AtEOXact_GUC(true, save_nestlevel);
}

/*
 * Find or create an ExtensionVersionInfo for the specified version name
 *
 * Currently, we just use a List of the ExtensionVersionInfo's.  Searching
 * for them therefore uses about O(N^2) time when there are N versions of
 * the extension.  We could change the data structure to a hash table if
 * this ever becomes a bottleneck.
 */
static ExtensionVersionInfo *
get_ext_ver_info(const char *versionname, List **evi_list)
{
  ExtensionVersionInfo *evi;
  ListCell *lc;

  foreach (lc, *evi_list) {
    evi = (ExtensionVersionInfo *)lfirst(lc);
    if (strcmp(evi->name, versionname) == 0) {
      return evi;
    }
  }

  evi = (ExtensionVersionInfo *)palloc(sizeof(ExtensionVersionInfo));
  evi->name = pstrdup(versionname);
  evi->reachable = NIL;
  evi->installable = false;
  /* initialize for later application of Dijkstra's algorithm */
  evi->distance_known = false;
  evi->distance = INT_MAX;
  evi->previous = NULL;

  *evi_list = lappend(*evi_list, evi);

  return evi;
}

/*
 * Locate the nearest unprocessed ExtensionVersionInfo
 *
 * This part of the algorithm is also about O(N^2).  A priority queue would
 * make it much faster, but for now there's no need.
 */
static ExtensionVersionInfo *
get_nearest_unprocessed_vertex(List *evi_list)
{
  ExtensionVersionInfo *evi = NULL;
  ListCell *lc;

  foreach (lc, evi_list) {
    ExtensionVersionInfo *evi2 = (ExtensionVersionInfo *)lfirst(lc);

    /* only vertices whose distance is still uncertain are candidates */
    if (evi2->distance_known) {
      continue;
    }
    /* remember the closest such vertex */
    if (evi == NULL || evi->distance > evi2->distance) {
      evi = evi2;
    }
  }

  return evi;
}

/*
 * Obtain information about the set of update scripts available for the
 * specified extension.  The result is a List of ExtensionVersionInfo
 * structs, each with a subsidiary list of the ExtensionVersionInfos for
 * the versions that can be reached in one step from that version.
 */
static List *
get_ext_ver_list(ExtensionControlFile *control)
{
  List *evi_list = NIL;
  int extnamelen = strlen(control->name);
  char *location;
  DIR *dir;
  struct dirent *de;

  location = get_extension_script_directory(control);
  dir = AllocateDir(location);
  while ((de = ReadDir(dir, location)) != NULL) {
    char *vername;
    char *vername2;
    ExtensionVersionInfo *evi;
    ExtensionVersionInfo *evi2;

    /* must be a .sql file ... */
    if (!is_extension_script_filename(de->d_name)) {
      continue;
    }

    /* ... matching extension name followed by separator */
    if (strncmp(de->d_name, control->name, extnamelen) != 0 || de->d_name[extnamelen] != '-' || de->d_name[extnamelen + 1] != '-') {
      continue;
    }

    /* extract version name(s) from 'extname--something.sql' filename */
    vername = pstrdup(de->d_name + extnamelen + 2);
    *strrchr(vername, '.') = '\0';
    vername2 = strstr(vername, "--");
    if (!vername2) {
      /* It's an install, not update, script; record its version name */
      evi = get_ext_ver_info(vername, &evi_list);
      evi->installable = true;
      continue;
    }
    *vername2 = '\0'; /* terminate first version */
    vername2 += 2;    /* and point to second */

    /* if there's a third --, it's bogus, ignore it */
    if (strstr(vername2, "--")) {
      continue;
    }

    /* Create ExtensionVersionInfos and link them together */


    evi->reachable = lappend(evi->reachable, evi2);
  }
  FreeDir(dir);

  return evi_list;
}

/*
 * Given an initial and final version name, identify the sequence of update
 * scripts that have to be applied to perform that update.
 *
 * Result is a List of names of versions to transition through (the initial
 * version is *not* included).
 */
static List *
identify_update_path(ExtensionControlFile *control, const char *oldVersion, const char *newVersion)
{
  List *result;
  List *evi_list;
  ExtensionVersionInfo *evi_start;


  /* Extract the version update graph from the script directory */


  /* Initialize start and end vertices */



  /* Find shortest path */







}

/*
 * Apply Dijkstra's algorithm to find the shortest path from evi_start to
 * evi_target.
 *
 * If reject_indirect is true, ignore paths that go through installable
 * versions.  This saves work when the caller will consider starting from
 * all installable versions anyway.
 *
 * If reinitialize is false, assume the ExtensionVersionInfo list has not
 * been used for this before, and the initialization done by get_ext_ver_info
 * is still good.  Otherwise, reinitialize all transient fields used here.
 *
 * Result is a List of names of versions to transition through (the initial
 * version is *not* included).  Returns NIL if no such path.
 */
static List *
find_update_path(List *evi_list, ExtensionVersionInfo *evi_start, ExtensionVersionInfo *evi_target, bool reject_indirect, bool reinitialize)
{
  List *result;
  ExtensionVersionInfo *evi;
  ListCell *lc;

  /* Caller error if start == target */
  Assert(evi_start != evi_target);
  /* Caller error if reject_indirect and target is installable */
  Assert(!(reject_indirect && evi_target->installable));

  if (reinitialize) {
    foreach (lc, evi_list) {
      evi = (ExtensionVersionInfo *)lfirst(lc);
      evi->distance_known = false;
      evi->distance = INT_MAX;
      evi->previous = NULL;
    }
  }

  evi_start->distance = 0;

  while ((evi = get_nearest_unprocessed_vertex(evi_list)) != NULL) {
    if (evi->distance == INT_MAX) {
      break; /* all remaining vertices are unreachable */
    }
    evi->distance_known = true;
    if (evi == evi_target) {
      break; /* found shortest path to target */
    }
    foreach (lc, evi->reachable) {
      ExtensionVersionInfo *evi2 = (ExtensionVersionInfo *)lfirst(lc);
      int newdist;

      /* if reject_indirect, treat installable versions as unreachable */
      if (reject_indirect && evi2->installable) {

      }















    }
  }

  /* Return NIL if target is not reachable from start */




  /* Build and return list of version names representing the update path */
  result = NIL;
  for (evi = evi_target; evi != evi_start; evi = evi->previous) {
    result = lcons(evi->name, result);
  }


}

/*
 * Given a target version that is not directly installable, find the
 * best installation sequence starting from a directly-installable version.
 *
 * evi_list: previously-collected version update graph
 * evi_target: member of that list that we want to reach
 *
 * Returns the best starting-point version, or NULL if there is none.
 * On success, *best_path is set to the path from the start point.
 *
 * If there's more than one possible start point, prefer shorter update paths,
 * and break any ties arbitrarily on the basis of strcmp'ing the starting
 * versions' names.
 */
static ExtensionVersionInfo *
find_install_path(List *evi_list, ExtensionVersionInfo *evi_target, List **best_path)
{
  ExtensionVersionInfo *evi_start = NULL;
  ListCell *lc;

  *best_path = NIL;

  /*
   * We don't expect to be called for an installable target, but if we are,
   * the answer is easy: just start from there, with an empty update path.
   */
  if (evi_target->installable) {
    return evi_target;
  }

  /* Consider all installable versions as start points */
  foreach (lc, evi_list) {



    if (!evi1->installable) {
      continue;
    }

    /*
     * Find shortest path from evi1 to evi_target; but no need to consider
     * paths going through other installable versions.
     */
    path = find_update_path(evi_list, evi1, evi_target, true, true);
    if (path == NIL) {
      continue;
    }

    /* Remember best path */
    if (evi_start == NULL || list_length(path) < list_length(*best_path) || (list_length(path) == list_length(*best_path) && strcmp(evi_start->name, evi1->name) < 0)) {
      evi_start = evi1;
      *best_path = path;
    }
  }


}

/*
 * CREATE EXTENSION worker
 *
 * When CASCADE is specified, CreateExtensionInternal() recurses if required
 * extensions need to be installed.  To sanely handle cyclic dependencies,
 * the "parents" list contains a list of names of extensions already being
 * installed, allowing us to error out if we recurse to one of those.
 */
static ObjectAddress
CreateExtensionInternal(char *extensionName, char *schemaName, const char *versionName, const char *oldVersionName, bool cascade, List *parents, bool is_create)
{
  char *origSchemaName = schemaName;
  Oid schemaOid = InvalidOid;
  Oid extowner = GetUserId();
  ExtensionControlFile *pcontrol;
  ExtensionControlFile *control;
  List *updateVersions;
  List *requiredExtensions;
  List *requiredSchemas;
  Oid extensionOid;
  ObjectAddress address;
  ListCell *lc;

  /*
   * Read the primary control file.  Note we assume that it does not contain
   * any non-ASCII data, so there is no need to worry about encoding at this
   * point.
   */
  pcontrol = read_extension_control_file(extensionName);

  /*
   * Determine the version to install
   */
  if (versionName == NULL) {
    if (pcontrol->default_version) {
      versionName = pcontrol->default_version;
    } else {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("version to install must be specified")));
    }
  }
  check_valid_version_name(versionName);

  /*
   * Figure out which script(s) we need to run to install the desired
   * version of the extension.  If we do not have a script that directly
   * does what is needed, we try to find a sequence of update scripts that
   * will get us there.
   */
  if (oldVersionName) {
    /*
     * "FROM old_version" was specified, indicating that we're trying to
     * update from some unpackaged version of the extension.  Locate a
     * series of update scripts that will do it.
     */
























  } else {
    /*
     * No FROM, so we're installing from scratch.  If there is an install
     * script for the desired version, we only need to run that one.
     */

    struct stat fst;

    oldVersionName = NULL;

    filename = get_extension_script_filename(pcontrol, NULL, versionName);
    if (stat(filename, &fst) == 0) {
      /* Easy, no extra scripts */
      updateVersions = NIL;
    } else {
      /* Look for best way to install this version */
      List *evi_list;
      ExtensionVersionInfo *evi_start;
      ExtensionVersionInfo *evi_target;

      /* Extract the version update graph from the script directory */


      /* Identify the target version */


      /* Identify best path to reach target */


      /* Fail if no path ... */




      /* Otherwise, install best starting point and then upgrade */

    }
  }

  /*
   * Fetch control parameters for installation target version
   */


  /*
   * Determine the target schema to install the extension into
   */
  if (schemaName) {
    /* If the user is giving us the schema name, it must exist already. */
    schemaOid = get_namespace_oid(schemaName, false);
  }

  if (control->schema != NULL) {
    /*
     * The extension is not relocatable and the author gave us a schema
     * for it.
     *
     * Unless CASCADE parameter was given, it's an error to give a schema
     * different from control->schema if control->schema is specified.
     */
    if (schemaName && strcmp(control->schema, schemaName) != 0 && !cascade) {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("extension \"%s\" must be installed in schema \"%s\"", control->name, control->schema)));
    }

    /* Always use the schema from control file for current extension. */
    schemaName = control->schema;

    /* Find or create the schema in case it does not exist. */


    if (!OidIsValid(schemaOid)) {
      CreateSchemaStmt *csstmt = makeNode(CreateSchemaStmt);

      csstmt->schemaname = schemaName;
      csstmt->authrole = NULL; /* will be created by current user */
      csstmt->schemaElts = NIL;
      csstmt->if_not_exists = false;
      CreateSchemaCommand(csstmt, "(generated CREATE SCHEMA command)", -1, -1);

      /*
       * CreateSchemaCommand includes CommandCounterIncrement, so new
       * schema is now visible.
       */

    }
  } else if (!OidIsValid(schemaOid)) {
    /*
     * Neither user nor author of the extension specified schema; use the
     * current default creation namespace, which is the first explicit
     * entry in the search_path.
     */












  }

  /*
   * Make note if a temporary namespace has been accessed in this
   * transaction.
   */




  /*
   * We don't check creation rights on the target namespace here.  If the
   * extension script actually creates any objects there, it will fail if
   * the user doesn't have such permissions.  But there are cases such as
   * procedural languages where it's convenient to set schema = pg_catalog
   * yet we don't want to restrict the command to users with ACL_CREATE for
   * pg_catalog.
   */

  /*
   * Look up the prerequisite extensions, install them if necessary, and
   * build lists of their OIDs and the OIDs of their target schemas.
   */
  requiredExtensions = NIL;
  requiredSchemas = NIL;
  foreach (lc, control->requires) {
    char *curreq = (char *)lfirst(lc);
    Oid reqext;
    Oid reqschema;

    reqext = get_required_extension(curreq, extensionName, origSchemaName, cascade, parents, is_create);
    reqschema = get_extension_schema(reqext);
    requiredExtensions = lappend_oid(requiredExtensions, reqext);

  }

  /*
   * Insert new tuple into pg_extension, and create dependency entries.
   */



  /*
   * Apply any control-file comment on extension
   */
  if (control->comment != NULL) {
    CreateComments(extensionOid, ExtensionRelationId, 0, control->comment);
  }

  /*
   * Execute the installation script file
   */
  execute_extension_script(extensionOid, control, oldVersionName, versionName, requiredSchemas, schemaName, schemaOid);

  /*
   * If additional update scripts have to be executed, apply the updates as
   * though a series of ALTER EXTENSION UPDATE commands were given
   */
  ApplyExtensionUpdates(extensionOid, pcontrol, versionName, updateVersions, origSchemaName, cascade, is_create);

  return address;
}

/*
 * Get the OID of an extension listed in "requires", possibly creating it.
 */
static Oid
get_required_extension(char *reqExtensionName, char *extensionName, char *origSchemaName, bool cascade, List *parents, bool is_create)
{
  Oid reqExtensionOid;

  reqExtensionOid = get_extension_oid(reqExtensionName, true);
  if (!OidIsValid(reqExtensionOid)) {
    if (cascade) {
      /* Must install it. */




      /* Check extension name validity before trying to cascade. */


      /* Check for cyclic dependency between extensions. */










      /* Add current extension to list of parents to pass down. */


      /*
       * Create the required extension.  We propagate the SCHEMA option
       * if any, and CASCADE, but no other options.
       */


      /* Get its newly-assigned OID. */

    } else {

    }
  }


}

/*
 * CREATE EXTENSION
 */
ObjectAddress
CreateExtension(ParseState *pstate, CreateExtensionStmt *stmt)
{



  DefElem *d_cascade = NULL;
  char *schemaName = NULL;
  char *versionName = NULL;
  char *oldVersionName = NULL;
  bool cascade = false;
  ListCell *lc;

  /* Check extension name validity before any filesystem access */
  check_valid_extension_name(stmt->extname);

  /*
   * Check for duplicate extension name.  The unique index on
   * pg_extension.extname would catch this anyway, and serves as a backstop
   * in case of race conditions; but this is a friendlier error message, and
   * besides we need a check to support IF NOT EXISTS.
   */
  if (get_extension_oid(stmt->extname, true) != InvalidOid) {
    if (stmt->if_not_exists) {
      ereport(NOTICE, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("extension \"%s\" already exists, skipping", stmt->extname)));
      return InvalidObjectAddress;
    } else {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("extension \"%s\" already exists", stmt->extname)));
    }
  }

  /*
   * We use global variables to track the extension being created, so we can
   * create only one extension at the same time.
   */




  /* Deconstruct the statement option list */
  foreach (lc, stmt->options) {
    DefElem *defel = (DefElem *)lfirst(lc);

    if (strcmp(defel->defname, "schema") == 0) {
      if (d_schema) {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }

      schemaName = defGetString(d_schema);
    } else if (strcmp(defel->defname, "new_version") == 0) {
      if (d_new_version) {

      }

















  }

  /* Call CreateExtensionInternal to do the real work. */

}

/*
 * InsertExtensionTuple
 *
 * Insert the new pg_extension row, and create extension's dependency entries.
 * Return the OID assigned to the new row.
 *
 * This is exported for the benefit of pg_upgrade, which has to create a
 * pg_extension entry (and the extension-level dependencies) without
 * actually running the extension's script.
 *
 * extConfig and extCondition should be arrays or PointerGetDatum(NULL).
 * We declare them as plain Datum to avoid needing array.h in extension.h.
 */
ObjectAddress
InsertExtensionTuple(const char *extName, Oid extOwner, Oid schemaOid, bool relocatable, const char *extVersion, Datum extConfig, Datum extCondition, List *requiredExtensions)
{
  Oid extensionOid;
  Relation rel;
  Datum values[Natts_pg_extension];
  bool nulls[Natts_pg_extension];
  HeapTuple tuple;
  ObjectAddress myself;
  ObjectAddress nsp;
  ListCell *lc;

  /*
   * Build and insert the pg_extension tuple
   */
  rel = table_open(ExtensionRelationId, RowExclusiveLock);

  memset(values, 0, sizeof(values));
  memset(nulls, 0, sizeof(nulls));

  extensionOid = GetNewOidWithIndex(rel, ExtensionOidIndexId, Anum_pg_extension_oid);
  values[Anum_pg_extension_oid - 1] = ObjectIdGetDatum(extensionOid);
  values[Anum_pg_extension_extname - 1] = DirectFunctionCall1(namein, CStringGetDatum(extName));
  values[Anum_pg_extension_extowner - 1] = ObjectIdGetDatum(extOwner);
  values[Anum_pg_extension_extnamespace - 1] = ObjectIdGetDatum(schemaOid);
  values[Anum_pg_extension_extrelocatable - 1] = BoolGetDatum(relocatable);
  values[Anum_pg_extension_extversion - 1] = CStringGetTextDatum(extVersion);

  if (extConfig == PointerGetDatum(NULL)) {
    nulls[Anum_pg_extension_extconfig - 1] = true;
  } else {
    values[Anum_pg_extension_extconfig - 1] = extConfig;
  }

  if (extCondition == PointerGetDatum(NULL)) {
    nulls[Anum_pg_extension_extcondition - 1] = true;
  } else {
    values[Anum_pg_extension_extcondition - 1] = extCondition;
  }

  tuple = heap_form_tuple(rel->rd_att, values, nulls);



  heap_freetuple(tuple);
  table_close(rel, RowExclusiveLock);

  /*
   * Record dependencies on owner, schema, and prerequisite extensions
   */
  recordDependencyOnOwner(ExtensionRelationId, extensionOid, extOwner);

  myself.classId = ExtensionRelationId;
  myself.objectId = extensionOid;
  myself.objectSubId = 0;

  nsp.classId = NamespaceRelationId;
  nsp.objectId = schemaOid;
  nsp.objectSubId = 0;

  recordDependencyOn(&myself, &nsp, DEPENDENCY_NORMAL);

  foreach (lc, requiredExtensions) {
    Oid reqext = lfirst_oid(lc);
    ObjectAddress otherext;

    otherext.classId = ExtensionRelationId;
    otherext.objectId = reqext;
    otherext.objectSubId = 0;

    recordDependencyOn(&myself, &otherext, DEPENDENCY_NORMAL);
  }
  /* Post creation hook for new extension */



}

/*
 * Guts of extension deletion.
 *
 * All we need do here is remove the pg_extension tuple itself.  Everything
 * else is taken care of by the dependency infrastructure.
 */
void
RemoveExtensionById(Oid extId)
{
  Relation rel;
  SysScanDesc scandesc;
  HeapTuple tuple;
  ScanKeyData entry[1];

  /*
   * Disallow deletion of any extension that's currently open for insertion;
   * else subsequent executions of recordDependencyOnCurrentExtension()
   * could create dangling pg_depend records that refer to a no-longer-valid
   * pg_extension OID.  This is needed not so much because we think people
   * might write "DROP EXTENSION foo" in foo's own script files, as because
   * errors in dependency management in extension script files could give
   * rise to cases where an extension is dropped as a result of recursing
   * from some contained object.  Because of that, we must test for the case
   * here, not at some higher level of the DROP EXTENSION command.
   */
  if (extId == CurrentExtensionObject) {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot drop extension \"%s\" because it is being modified", get_extension_name(extId))));
  }

  rel = table_open(ExtensionRelationId, RowExclusiveLock);

  ScanKeyInit(&entry[0], Anum_pg_extension_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(extId));
  scandesc = systable_beginscan(rel, ExtensionOidIndexId, true, NULL, 1, entry);

  tuple = systable_getnext(scandesc);

  /* We assume that there can be at most one matching tuple */
  if (HeapTupleIsValid(tuple)) {
    CatalogTupleDelete(rel, &tuple->t_self);
  }

  systable_endscan(scandesc);

  table_close(rel, RowExclusiveLock);
}

/*
 * This function lists the available extensions (one row per primary control
 * file in the control directory).  We parse each control file and report the
 * interesting fields.
 *
 * The system view pg_available_extensions provides a user interface to this
 * SRF, adding information about whether the extensions are installed in the
 * current DB.
 */
Datum
pg_available_extensions(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  char *location;
  DIR *dir;
  struct dirent *de;

  /* check to see if caller supports us returning a tuplestore */
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo)) {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize)) {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }

  /* Build a tuple descriptor for our result type */
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
    elog(ERROR, "return type must be a row type");
  }

  /* Build tuplestore to hold the result rows */
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;


  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;


  MemoryContextSwitchTo(oldcontext);

  location = get_extension_control_directory();
  dir = AllocateDir(location);

  /*
   * If the control directory doesn't exist, we want to silently return an
   * empty set.  Any other error will be reported by ReadDir.
   */
  if (dir == NULL && errno == ENOENT) {
    /* do nothing */
  } else {
    while ((de = ReadDir(dir, location)) != NULL) {
      ExtensionControlFile *control;
      char *extname;
      Datum values[3];
      bool nulls[3];

      if (!is_extension_control_filename(de->d_name)) {
        continue;
      }

      /* extract extension name from 'name.control' filename */
      extname = pstrdup(de->d_name);
      *strrchr(extname, '.') = '\0';

      /* ignore it if it's an auxiliary control file */
      if (strstr(extname, "--")) {
        continue;
      }

      control = read_extension_control_file(extname);

      memset(values, 0, sizeof(values));
      memset(nulls, 0, sizeof(nulls));

      /* name */
      values[0] = DirectFunctionCall1(namein, CStringGetDatum(control->name));
      /* default_version */
      if (control->default_version == NULL) {
        nulls[1] = true;
      } else {
        values[1] = CStringGetTextDatum(control->default_version);
      }
      /* comment */
      if (control->comment == NULL) {
        nulls[2] = true;
      } else {
        values[2] = CStringGetTextDatum(control->comment);
      }

      tuplestore_putvalues(tupstore, tupdesc, values, nulls);
    }

    FreeDir(dir);
  }

  /* clean up and return the tuplestore */
  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

/*
 * This function lists the available extension versions (one row per
 * extension installation script).  For each version, we parse the related
 * control file(s) and report the interesting fields.
 *
 * The system view pg_available_extension_versions provides a user interface
 * to this SRF, adding information about which versions are installed in the
 * current DB.
 */
Datum
pg_available_extension_versions(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  char *location;
  DIR *dir;
  struct dirent *de;

  /* check to see if caller supports us returning a tuplestore */
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo)) {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize)) {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }

  /* Build a tuple descriptor for our result type */
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
    elog(ERROR, "return type must be a row type");
  }

  /* Build tuplestore to hold the result rows */
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;


  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;


  MemoryContextSwitchTo(oldcontext);

  location = get_extension_control_directory();
  dir = AllocateDir(location);

  /*
   * If the control directory doesn't exist, we want to silently return an
   * empty set.  Any other error will be reported by ReadDir.
   */
  if (dir == NULL && errno == ENOENT) {
    /* do nothing */
  } else {
    while ((de = ReadDir(dir, location)) != NULL) {
      ExtensionControlFile *control;
      char *extname;

      if (!is_extension_control_filename(de->d_name)) {
        continue;
      }

      /* extract extension name from 'name.control' filename */
      extname = pstrdup(de->d_name);
      *strrchr(extname, '.') = '\0';

      /* ignore it if it's an auxiliary control file */
      if (strstr(extname, "--")) {
        continue;
      }

      /* read the control file */
      control = read_extension_control_file(extname);

      /* scan extension's script directory for install scripts */
      get_available_versions_for_extension(control, tupstore, tupdesc);
    }


  }

  /* clean up and return the tuplestore */
  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

/*
 * Inner loop for pg_available_extension_versions:
 *		read versions of one extension, add rows to tupstore
 */
static void
get_available_versions_for_extension(ExtensionControlFile *pcontrol, Tuplestorestate *tupstore, TupleDesc tupdesc)
{
  List *evi_list;
  ListCell *lc;

  /* Extract the version update graph from the script directory */
  evi_list = get_ext_ver_list(pcontrol);

  /* For each installable version ... */
  foreach (lc, evi_list) {
    ExtensionVersionInfo *evi = (ExtensionVersionInfo *)lfirst(lc);
    ExtensionControlFile *control;
    Datum values[7];
    bool nulls[7];
    ListCell *lc2;

    if (!evi->installable) {
      continue;
    }

    /*
     * Fetch parameters for specific version (pcontrol is not changed)
     */
    control = read_extension_aux_control_file(pcontrol, evi->name);

    memset(values, 0, sizeof(values));
    memset(nulls, 0, sizeof(nulls));

    /* name */
    values[0] = DirectFunctionCall1(namein, CStringGetDatum(control->name));
    /* version */
    values[1] = CStringGetTextDatum(evi->name);
    /* superuser */
    values[2] = BoolGetDatum(control->superuser);
    /* relocatable */
    values[3] = BoolGetDatum(control->relocatable);
    /* schema */
    if (control->schema == NULL) {
      nulls[4] = true;
    } else {
      values[4] = DirectFunctionCall1(namein, CStringGetDatum(control->schema));
    }
    /* requires */
    if (control->requires == NIL) {
      nulls[5] = true;
    } else {
      values[5] = convert_requires_to_datum(control->requires);
    }
    /* comment */
    if (control->comment == NULL) {
      nulls[6] = true;
    } else {
      values[6] = CStringGetTextDatum(control->comment);
    }

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);

    /*
     * Find all non-directly-installable versions that would be installed
     * starting from this version, and report them, inheriting the
     * parameters that aren't changed in updates from this version.
     */
    foreach (lc2, evi_list) {
      ExtensionVersionInfo *evi2 = (ExtensionVersionInfo *)lfirst(lc2);
      List *best_path;

      if (evi2->installable) {
        continue;
      }
      if (find_install_path(evi_list, evi2, &best_path) == evi) {
        /*
         * Fetch parameters for this version (pcontrol is not changed)
         */
        control = read_extension_aux_control_file(pcontrol, evi2->name);

        /* name stays the same */
        /* version */
        values[1] = CStringGetTextDatum(evi2->name);
        /* superuser */
        values[2] = BoolGetDatum(control->superuser);
        /* relocatable */

        /* schema stays the same */
        /* requires */






        /* comment stays the same */


      }
    }
  }
}

/*
 * Convert a list of extension names to a name[] Datum
 */
static Datum
convert_requires_to_datum(List *requires)
{
  Datum *datums;
  int ndatums;
  ArrayType *a;
  ListCell *lc;

  ndatums = list_length(requires);
  datums = (Datum *)palloc(ndatums * sizeof(Datum));
  ndatums = 0;
  foreach (lc, requires) {



  }


}

/*
 * This function reports the version update paths that exist for the
 * specified extension.
 */
Datum
pg_extension_update_paths(PG_FUNCTION_ARGS)
{

































































































}

/*
 * pg_extension_config_dump
 *
 * Record information about a configuration table that belongs to an
 * extension being created, but whose contents should be dumped in whole
 * or in part during pg_dump.
 */
Datum
pg_extension_config_dump(PG_FUNCTION_ARGS)
{







































































































































}

/*
 * extension_config_remove
 *
 * Remove the specified table OID from extension's extconfig, if present.
 * This is not currently exposed as a function, but it could be;
 * for now, we just invoke it from ALTER EXTENSION DROP.
 */
static void
extension_config_remove(Oid extensionoid, Oid tableoid)
{





































































































































}

/*
 * Execute ALTER EXTENSION SET SCHEMA
 */
ObjectAddress
AlterExtensionNamespace(const char *extensionName, const char *newschema, Oid *oldschema)
{
























































































































































}

/*
 * Execute ALTER EXTENSION UPDATE
 */
ObjectAddress
ExecAlterExtensionStmt(ParseState *pstate, AlterExtensionStmt *stmt)
{




















































































































}

/*
 * Apply a series of update scripts as though individual ALTER EXTENSION
 * UPDATE commands had been given, including altering the pg_extension row
 * and dependencies each time.
 *
 * This might be more work than necessary, but it ensures that old update
 * scripts don't break if newer versions have different control parameters.
 */
static void
ApplyExtensionUpdates(Oid extensionOid, ExtensionControlFile *pcontrol, const char *initialVersion, List *updateVersions, char *origSchemaName, bool cascade, bool is_create)
{
  const char *oldVersionName = initialVersion;
  ListCell *lcv;

  foreach (lcv, updateVersions) {
    char *versionName = (char *)lfirst(lcv);
    ExtensionControlFile *control;
    char *schemaName;
    Oid schemaOid;
    List *requiredExtensions;
    List *requiredSchemas;
    Relation extRel;
    ScanKeyData key[1];
    SysScanDesc extScan;
    HeapTuple extTup;
    Form_pg_extension extForm;
    Datum values[Natts_pg_extension];





    /*
     * Fetch parameters for specific version (pcontrol is not changed)
     */


    /* Find the pg_extension tuple */














    /*
     * Determine the target schema (set by original install)
     */



    /*
     * Modify extrelocatable and extversion in the pg_extension tuple
     */

















    /*
     * Look up the prerequisite extensions for this version, install them
     * if necessary, and build lists of their OIDs and the OIDs of their
     * target schemas.
     */













    /*
     * Remove and recreate dependencies on prerequisite extensions
     */



















    /*
     * Finally, execute the update script file
     */


    /*
     * Update prior-version name and loop around.  Since
     * execute_sql_string did a final CommandCounterIncrement, we can
     * update the pg_extension row again.
     */

  }
}

/*
 * Execute ALTER EXTENSION ADD/DROP
 *
 * Return value is the address of the altered extension.
 *
 * objAddr is an output argument which, if not NULL, is set to the address of
 * the added/dropped object.
 */
ObjectAddress
ExecAlterExtensionContentsStmt(AlterExtensionContentsStmt *stmt, ObjectAddress *objAddr)
{
  ObjectAddress extension;
  ObjectAddress object;
  Relation relation;
  Oid oldExtension;

  /*
   * Find the extension and acquire a lock on it, to ensure it doesn't get
   * dropped concurrently.  A sharable lock seems sufficient: there's no
   * reason not to allow other sorts of manipulations, such as add/drop of
   * other objects, to occur concurrently.  Concurrently adding/dropping the
   * *same* object would be bad, but we prevent that by using a non-sharable
   * lock on the individual object, below.
   */


  /* Permission check: must own extension */




  /*
   * Translate the parser representation that identifies the object into an
   * ObjectAddress.  get_object_address() will throw an error if the object
   * does not exist, and will also acquire a lock on the object to guard
   * against concurrent DROP and ALTER EXTENSION ADD/DROP operations.
   */







  /* Permission check: must own target object, too */


  /*
   * Check existing extension membership.
   */




































































  /*
   * If get_object_address() opened the relation for us, we close it to keep
   * the reference count correct - but we retain any locks acquired by
   * get_object_address() until commit time, to guard against concurrent
   * activity.
   */





}

/*
 * Read the whole of file into memory.
 *
 * The file contents are returned as a single palloc'd chunk. For convenience
 * of the callers, an extra \0 byte is added to the end.
 */
static char *
read_whole_file(const char *filename, int *length)
{



  struct stat fst;

  if (stat(filename, &fst) < 0) {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", filename)));
  }

  if (fst.st_size > (MaxAllocSize - 1)) {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("file \"%s\" is too large", filename)));
  }
  bytes_to_read = (size_t)fst.st_size;

  if ((file = AllocateFile(filename, PG_BINARY_R)) == NULL) {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" for reading: %m", filename)));
  }

  buf = (char *)palloc(bytes_to_read + 1);



  if (ferror(file)) {

  }

  FreeFile(file);



}