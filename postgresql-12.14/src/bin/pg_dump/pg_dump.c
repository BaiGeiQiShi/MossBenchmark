                                                                            
   
             
                                                              
                         
   
                                                                         
                                                                        
   
                                                                      
                                                                        
                 
   
                                                                      
                                                                     
                                                                       
                                                                      
                                                                   
                                                                        
                                                                        
                                                                         
                                                                            
                      
   
                                                                  
   
                  
                               
   
                                                                            
   
#include "postgres_fe.h"

#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "getopt_long.h"

#include "access/attnum.h"
#include "access/sysattr.h"
#include "access/transam.h"
#include "catalog/pg_aggregate_d.h"
#include "catalog/pg_am_d.h"
#include "catalog/pg_attribute_d.h"
#include "catalog/pg_cast_d.h"
#include "catalog/pg_class_d.h"
#include "catalog/pg_default_acl_d.h"
#include "catalog/pg_largeobject_d.h"
#include "catalog/pg_largeobject_metadata_d.h"
#include "catalog/pg_proc_d.h"
#include "catalog/pg_trigger_d.h"
#include "catalog/pg_type_d.h"
#include "libpq/libpq-fs.h"
#include "storage/block.h"

#include "dumputils.h"
#include "parallel.h"
#include "pg_backup_db.h"
#include "pg_backup_utils.h"
#include "pg_dump.h"
#include "fe_utils/connect.h"
#include "fe_utils/string_utils.h"

typedef struct
{
  const char *descr;                            
  Oid classoid;                                      
  Oid objoid;                        
  int objsubid;                                      
} CommentItem;

typedef struct
{
  const char *provider;                                            
  const char *label;                                      
  Oid classoid;                                         
  Oid objoid;                           
  int objsubid;                                         
} SecLabelItem;

typedef enum OidOptions
{
  zeroAsOpaque = 1,
  zeroAsAny = 2,
  zeroAsStar = 4,
  zeroAsNone = 8
} OidOptions;

                  
static bool dosync = true;                                                  

                                                                
static const char *username_subquery;

   
                                                                         
                            
   
static Oid g_last_builtin_oid;                                    

                                                                      
static int strict_names = 0;

   
                                    
   
                                                                        
                                                               
   
static SimpleStringList schema_include_patterns = {NULL, NULL};
static SimpleOidList schema_include_oids = {NULL, NULL};
static SimpleStringList schema_exclude_patterns = {NULL, NULL};
static SimpleOidList schema_exclude_oids = {NULL, NULL};

static SimpleStringList table_include_patterns = {NULL, NULL};
static SimpleOidList table_include_oids = {NULL, NULL};
static SimpleStringList table_exclude_patterns = {NULL, NULL};
static SimpleOidList table_exclude_oids = {NULL, NULL};
static SimpleStringList tabledata_exclude_patterns = {NULL, NULL};
static SimpleOidList tabledata_exclude_oids = {NULL, NULL};

char g_opaque_type[10];                               

                                                  
char g_comment_start[10];
char g_comment_end[10];

static const CatalogId nilCatalogId = {0, 0};

                                                      
static bool have_extra_float_digits = false;
static int extra_float_digits;

   
                                              
                                                    
   
#define DUMP_DEFAULT_ROWS_PER_INSERT 1

   
                                                                           
   
#define fmtQualifiedDumpable(obj) fmtQualifiedId((obj)->dobj.namespace->dobj.name, (obj)->dobj.name)

static void
help(const char *progname);
static void
setup_connection(Archive *AH, const char *dumpencoding, const char *dumpsnapshot, char *use_role);
static ArchiveFormat
parseArchiveFormat(const char *format, ArchiveMode *mode);
static void
expand_schema_name_patterns(Archive *fout, SimpleStringList *patterns, SimpleOidList *oids, bool strict_names);
static void
expand_table_name_patterns(Archive *fout, SimpleStringList *patterns, SimpleOidList *oids, bool strict_names);
static NamespaceInfo *
findNamespace(Archive *fout, Oid nsoid);
static void
dumpTableData(Archive *fout, TableDataInfo *tdinfo);
static void
refreshMatViewData(Archive *fout, TableDataInfo *tdinfo);
static void
guessConstraintInheritance(TableInfo *tblinfo, int numTables);
static void
dumpComment(Archive *fout, const char *type, const char *name, const char *namespace, const char *owner, CatalogId catalogId, int subid, DumpId dumpId);
static int
findComments(Archive *fout, Oid classoid, Oid objoid, CommentItem **items);
static int
collectComments(Archive *fout, CommentItem **items);
static void
dumpSecLabel(Archive *fout, const char *type, const char *name, const char *namespace, const char *owner, CatalogId catalogId, int subid, DumpId dumpId);
static int
findSecLabels(Archive *fout, Oid classoid, Oid objoid, SecLabelItem **items);
static int
collectSecLabels(Archive *fout, SecLabelItem **items);
static void
dumpDumpableObject(Archive *fout, DumpableObject *dobj);
static void
dumpNamespace(Archive *fout, NamespaceInfo *nspinfo);
static void
dumpExtension(Archive *fout, ExtensionInfo *extinfo);
static void
dumpType(Archive *fout, TypeInfo *tyinfo);
static void
dumpBaseType(Archive *fout, TypeInfo *tyinfo);
static void
dumpEnumType(Archive *fout, TypeInfo *tyinfo);
static void
dumpRangeType(Archive *fout, TypeInfo *tyinfo);
static void
dumpUndefinedType(Archive *fout, TypeInfo *tyinfo);
static void
dumpDomain(Archive *fout, TypeInfo *tyinfo);
static void
dumpCompositeType(Archive *fout, TypeInfo *tyinfo);
static void
dumpCompositeTypeColComments(Archive *fout, TypeInfo *tyinfo);
static void
dumpShellType(Archive *fout, ShellTypeInfo *stinfo);
static void
dumpProcLang(Archive *fout, ProcLangInfo *plang);
static void
dumpFunc(Archive *fout, FuncInfo *finfo);
static void
dumpCast(Archive *fout, CastInfo *cast);
static void
dumpTransform(Archive *fout, TransformInfo *transform);
static void
dumpOpr(Archive *fout, OprInfo *oprinfo);
static void
dumpAccessMethod(Archive *fout, AccessMethodInfo *oprinfo);
static void
dumpOpclass(Archive *fout, OpclassInfo *opcinfo);
static void
dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo);
static void
dumpCollation(Archive *fout, CollInfo *collinfo);
static void
dumpConversion(Archive *fout, ConvInfo *convinfo);
static void
dumpRule(Archive *fout, RuleInfo *rinfo);
static void
dumpAgg(Archive *fout, AggInfo *agginfo);
static void
dumpTrigger(Archive *fout, TriggerInfo *tginfo);
static void
dumpEventTrigger(Archive *fout, EventTriggerInfo *evtinfo);
static void
dumpTable(Archive *fout, TableInfo *tbinfo);
static void
dumpTableSchema(Archive *fout, TableInfo *tbinfo);
static void
dumpAttrDef(Archive *fout, AttrDefInfo *adinfo);
static void
dumpSequence(Archive *fout, TableInfo *tbinfo);
static void
dumpSequenceData(Archive *fout, TableDataInfo *tdinfo);
static void
dumpIndex(Archive *fout, IndxInfo *indxinfo);
static void
dumpIndexAttach(Archive *fout, IndexAttachInfo *attachinfo);
static void
dumpStatisticsExt(Archive *fout, StatsExtInfo *statsextinfo);
static void
dumpConstraint(Archive *fout, ConstraintInfo *coninfo);
static void
dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo);
static void
dumpTSParser(Archive *fout, TSParserInfo *prsinfo);
static void
dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo);
static void
dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo);
static void
dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo);
static void
dumpForeignDataWrapper(Archive *fout, FdwInfo *fdwinfo);
static void
dumpForeignServer(Archive *fout, ForeignServerInfo *srvinfo);
static void
dumpUserMappings(Archive *fout, const char *servername, const char *namespace, const char *owner, CatalogId catalogId, DumpId dumpId);
static void
dumpDefaultACL(Archive *fout, DefaultACLInfo *daclinfo);

static DumpId
dumpACL(Archive *fout, DumpId objDumpId, DumpId altDumpId, const char *type, const char *name, const char *subname, const char *nspname, const char *owner, const char *acls, const char *racls, const char *initacls, const char *initracls);

static void
getDependencies(Archive *fout);
static void
BuildArchiveDependencies(Archive *fout);
static void
findDumpableDependencies(ArchiveHandle *AH, DumpableObject *dobj, DumpId **dependencies, int *nDeps, int *allocDeps);

static DumpableObject *
createBoundaryObjects(void);
static void
addBoundaryDependencies(DumpableObject **dobjs, int numObjs, DumpableObject *boundaryObjs);

static void
addConstrChildIdxDeps(DumpableObject *dobj, IndxInfo *refidx);
static void
getDomainConstraints(Archive *fout, TypeInfo *tyinfo);
static void
getTableData(DumpOptions *dopt, TableInfo *tblinfo, int numTables, char relkind);
static void
makeTableDataInfo(DumpOptions *dopt, TableInfo *tbinfo);
static void
buildMatViewRefreshDependencies(Archive *fout);
static void
getTableDataFKConstraints(void);
static char *
format_function_arguments(FuncInfo *finfo, char *funcargs, bool is_agg);
static char *
format_function_arguments_old(Archive *fout, FuncInfo *finfo, int nallargs, char **allargtypes, char **argmodes, char **argnames);
static char *
format_function_signature(Archive *fout, FuncInfo *finfo, bool honor_quotes);
static char *
convertRegProcReference(Archive *fout, const char *proc);
static char *
getFormattedOperatorName(Archive *fout, const char *oproid);
static char *
convertTSFunction(Archive *fout, Oid funcOid);
static Oid
findLastBuiltinOid_V71(Archive *fout);
static const char *
getFormattedTypeName(Archive *fout, Oid oid, OidOptions opts);
static void
getBlobs(Archive *fout);
static void
dumpBlob(Archive *fout, BlobInfo *binfo);
static int
dumpBlobs(Archive *fout, void *arg);
static void
dumpPolicy(Archive *fout, PolicyInfo *polinfo);
static void
dumpPublication(Archive *fout, PublicationInfo *pubinfo);
static void
dumpPublicationTable(Archive *fout, PublicationRelInfo *pubrinfo);
static void
dumpSubscription(Archive *fout, SubscriptionInfo *subinfo);
static void
dumpDatabase(Archive *AH);
static void
dumpDatabaseConfig(Archive *AH, PQExpBuffer outbuf, const char *dbname, Oid dboid);
static void
dumpEncoding(Archive *AH);
static void
dumpStdStrings(Archive *AH);
static void
dumpSearchPath(Archive *AH);
static void
binary_upgrade_set_type_oids_by_type_oid(Archive *fout, PQExpBuffer upgrade_buffer, Oid pg_type_oid, bool force_array_type);
static bool
binary_upgrade_set_type_oids_by_rel_oid(Archive *fout, PQExpBuffer upgrade_buffer, Oid pg_rel_oid);
static void
binary_upgrade_set_pg_class_oids(Archive *fout, PQExpBuffer upgrade_buffer, Oid pg_class_oid, bool is_index);
static void
binary_upgrade_extension_member(PQExpBuffer upgrade_buffer, DumpableObject *dobj, const char *objtype, const char *objname, const char *objnamespace);
static const char *
getAttrName(int attrnum, TableInfo *tblInfo);
static const char *
fmtCopyColumnList(const TableInfo *ti, PQExpBuffer buffer);
static bool
nonemptyReloptions(const char *reloptions);
static void
appendReloptionsArrayAH(PQExpBuffer buffer, const char *reloptions, const char *prefix, Archive *fout);
static char *
get_synchronized_snapshot(Archive *fout);
static void
setupDumpWorker(Archive *AHX);
static TableInfo *
getRootTableInfo(TableInfo *tbinfo);

int
main(int argc, char **argv)
{
  int c;
  const char *filename = NULL;
  const char *format = "p";
  TableInfo *tblinfo;
  int numTables;
  DumpableObject **dobjs;
  int numObjs;
  DumpableObject *boundaryObjs;
  int i;
  int optindex;
  char *endptr;
  RestoreOptions *ropt;
  Archive *fout;                      
  bool g_verbose = false;
  const char *dumpencoding = NULL;
  const char *dumpsnapshot = NULL;
  char *use_role = NULL;
  long rowsPerInsert;
  int numWorkers = 1;
  int compressLevel = -1;
  int plainText = 0;
  ArchiveFormat archiveFormat = archUnknown;
  ArchiveMode archiveMode;

  static DumpOptions dopt;

  static struct option long_options[] = {{"data-only", no_argument, NULL, 'a'}, {"blobs", no_argument, NULL, 'b'}, {"no-blobs", no_argument, NULL, 'B'}, {"clean", no_argument, NULL, 'c'}, {"create", no_argument, NULL, 'C'}, {"dbname", required_argument, NULL, 'd'}, {"file", required_argument, NULL, 'f'}, {"format", required_argument, NULL, 'F'}, {"host", required_argument, NULL, 'h'}, {"jobs", 1, NULL, 'j'}, {"no-reconnect", no_argument, NULL, 'R'}, {"no-owner", no_argument, NULL, 'O'}, {"port", required_argument, NULL, 'p'}, {"schema", required_argument, NULL, 'n'}, {"exclude-schema", required_argument, NULL, 'N'}, {"schema-only", no_argument, NULL, 's'}, {"superuser", required_argument, NULL, 'S'}, {"table", required_argument, NULL, 't'}, {"exclude-table", required_argument, NULL, 'T'}, {"no-password", no_argument, NULL, 'w'}, {"password", no_argument, NULL, 'W'}, {"username", required_argument, NULL, 'U'}, {"verbose", no_argument, NULL, 'v'},
      {"no-privileges", no_argument, NULL, 'x'}, {"no-acl", no_argument, NULL, 'x'}, {"compress", required_argument, NULL, 'Z'}, {"encoding", required_argument, NULL, 'E'}, {"help", no_argument, NULL, '?'}, {"version", no_argument, NULL, 'V'},

         
                                                                            
         
      {"attribute-inserts", no_argument, &dopt.column_inserts, 1}, {"binary-upgrade", no_argument, &dopt.binary_upgrade, 1}, {"column-inserts", no_argument, &dopt.column_inserts, 1}, {"disable-dollar-quoting", no_argument, &dopt.disable_dollar_quoting, 1}, {"disable-triggers", no_argument, &dopt.disable_triggers, 1}, {"enable-row-security", no_argument, &dopt.enable_row_security, 1}, {"exclude-table-data", required_argument, NULL, 4}, {"extra-float-digits", required_argument, NULL, 8}, {"if-exists", no_argument, &dopt.if_exists, 1}, {"inserts", no_argument, NULL, 9}, {"lock-wait-timeout", required_argument, NULL, 2}, {"no-tablespaces", no_argument, &dopt.outputNoTablespaces, 1}, {"quote-all-identifiers", no_argument, &quote_all_identifiers, 1}, {"load-via-partition-root", no_argument, &dopt.load_via_partition_root, 1}, {"role", required_argument, NULL, 3}, {"section", required_argument, NULL, 5}, {"serializable-deferrable", no_argument, &dopt.serializable_deferrable, 1},
      {"snapshot", required_argument, NULL, 6}, {"strict-names", no_argument, &strict_names, 1}, {"use-set-session-authorization", no_argument, &dopt.use_setsessauth, 1}, {"no-comments", no_argument, &dopt.no_comments, 1}, {"no-publications", no_argument, &dopt.no_publications, 1}, {"no-security-labels", no_argument, &dopt.no_security_labels, 1}, {"no-synchronized-snapshots", no_argument, &dopt.no_synchronized_snapshots, 1}, {"no-unlogged-table-data", no_argument, &dopt.no_unlogged_table_data, 1}, {"no-subscriptions", no_argument, &dopt.no_subscriptions, 1}, {"no-sync", no_argument, NULL, 7}, {"on-conflict-do-nothing", no_argument, &dopt.do_nothing, 1}, {"rows-per-insert", required_argument, NULL, 10},

      {NULL, 0, NULL, 0}};

  pg_logging_init(argv[0]);
  pg_logging_set_level(PG_LOG_WARNING);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_dump"));

     
                                                                           
                         
     
  init_parallel_dump_utils();

  strcpy(g_comment_start, "-- ");
  g_comment_end[0] = '\0';
  strcpy(g_opaque_type, "opaque");

  progname = get_progname(argv[0]);

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      help(progname);
      exit_nicely(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      puts("pg_dump (PostgreSQL) " PG_VERSION);
      exit_nicely(0);
    }
  }

  InitDumpOptions(&dopt);

  while ((c = getopt_long(argc, argv, "abBcCd:E:f:F:h:j:n:N:Op:RsS:t:T:U:vwWxZ:", long_options, &optindex)) != -1)
  {
    switch (c)
    {
    case 'a':                     
      dopt.dataOnly = true;
      break;

    case 'b':                 
      dopt.outputBlobs = true;
      break;

    case 'B':                       
      dopt.dontOutputBlobs = true;
      break;

    case 'c':                                                
      dopt.outputClean = 1;
      break;

    case 'C':                
      dopt.outputCreateDB = 1;
      break;

    case 'd':                    
      dopt.cparams.dbname = pg_strdup(optarg);
      break;

    case 'E':                    
      dumpencoding = pg_strdup(optarg);
      break;

    case 'f':
      filename = pg_strdup(optarg);
      break;

    case 'F':
      format = pg_strdup(optarg);
      break;

    case 'h':                  
      dopt.cparams.pghost = pg_strdup(optarg);
      break;

    case 'j':                          
      numWorkers = atoi(optarg);
      break;

    case 'n':                        
      simple_string_list_append(&schema_include_patterns, optarg);
      dopt.include_everything = false;
      break;

    case 'N':                        
      simple_string_list_append(&schema_exclude_patterns, optarg);
      break;

    case 'O':                                     
      dopt.outputNoOwner = 1;
      break;

    case 'p':                  
      dopt.cparams.pgport = pg_strdup(optarg);
      break;

    case 'R':
                                                             
      break;

    case 's':                       
      dopt.schemaOnly = true;
      break;

    case 'S':                                                  
      dopt.outputSuperuser = pg_strdup(optarg);
      break;

    case 't':                       
      simple_string_list_append(&table_include_patterns, optarg);
      dopt.include_everything = false;
      break;

    case 'T':                       
      simple_string_list_append(&table_exclude_patterns, optarg);
      break;

    case 'U':
      dopt.cparams.username = pg_strdup(optarg);
      break;

    case 'v':              
      g_verbose = true;
      pg_logging_set_level(PG_LOG_INFO);
      break;

    case 'w':
      dopt.cparams.promptPassword = TRI_NO;
      break;

    case 'W':
      dopt.cparams.promptPassword = TRI_YES;
      break;

    case 'x':                    
      dopt.aclsSkip = true;
      break;

    case 'Z':                        
      compressLevel = atoi(optarg);
      if (compressLevel < 0 || compressLevel > 9)
      {
        pg_log_error("compression level must be in range 0..9");
        exit_nicely(1);
      }
      break;

    case 0:
                                         
      break;

    case 2:                        
      dopt.lockWaitTimeout = pg_strdup(optarg);
      break;

    case 3:               
      use_role = pg_strdup(optarg);
      break;

    case 4:                            
      simple_string_list_append(&tabledata_exclude_patterns, optarg);
      break;

    case 5:              
      set_dump_section(optarg, &dopt.dumpSections);
      break;

    case 6:               
      dumpsnapshot = pg_strdup(optarg);
      break;

    case 7:              
      dosync = false;
      break;

    case 8:
      have_extra_float_digits = true;
      extra_float_digits = atoi(optarg);
      if (extra_float_digits < -15 || extra_float_digits > 3)
      {
        pg_log_error("extra_float_digits must be in range -15..3");
        exit_nicely(1);
      }
      break;

    case 9:              

         
                                                                    
                         
         
      if (dopt.dump_inserts == 0)
      {
        dopt.dump_inserts = DUMP_DEFAULT_ROWS_PER_INSERT;
      }
      break;

    case 10:                      
      errno = 0;
      rowsPerInsert = strtol(optarg, &endptr, 10);

      if (endptr == optarg || *endptr != '\0' || rowsPerInsert <= 0 || rowsPerInsert > INT_MAX || errno == ERANGE)
      {
        pg_log_error("rows-per-insert must be in range %d..%d", 1, INT_MAX);
        exit_nicely(1);
      }
      dopt.dump_inserts = (int)rowsPerInsert;
      break;

    default:
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit_nicely(1);
    }
  }

     
                                                                      
                                          
     
  if (optind < argc && dopt.cparams.dbname == NULL)
  {
    dopt.cparams.dbname = argv[optind++];
  }

                                        
  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

                                          
  if (dopt.column_inserts && dopt.dump_inserts == 0)
  {
    dopt.dump_inserts = DUMP_DEFAULT_ROWS_PER_INSERT;
  }

     
                                                                           
                                                                        
                             
     
  if (dopt.binary_upgrade)
  {
    dopt.sequence_data = 1;
  }

  if (dopt.dataOnly && dopt.schemaOnly)
  {
    pg_log_error("options -s/--schema-only and -a/--data-only cannot be used together");
    exit_nicely(1);
  }

  if (dopt.dataOnly && dopt.outputClean)
  {
    pg_log_error("options -c/--clean and -a/--data-only cannot be used together");
    exit_nicely(1);
  }

  if (dopt.if_exists && !dopt.outputClean)
  {
    fatal("option --if-exists requires option -c/--clean");
  }

     
                                                                
                                       
     
  if (dopt.do_nothing && dopt.dump_inserts == 0)
  {
    fatal("option --on-conflict-do-nothing requires option --inserts, --rows-per-insert, or --column-inserts");
  }

                                       
  archiveFormat = parseArchiveFormat(format, &archiveMode);

                                    
  if (archiveFormat == archNull)
  {
    plainText = 1;
  }

                                                                          
  if (compressLevel == -1)
  {
#ifdef HAVE_LIBZ
    if (archiveFormat == archCustom || archiveFormat == archDirectory)
    {
      compressLevel = Z_DEFAULT_COMPRESSION;
    }
    else
#endif
      compressLevel = 0;
  }

#ifndef HAVE_LIBZ
  if (compressLevel != 0)
  {
    pg_log_warning("requested compression not available in this installation -- archive will be uncompressed");
  }
  compressLevel = 0;
#endif

     
                                                                            
                                                       
     
  if (!plainText)
  {
    dopt.outputCreateDB = 1;
  }

     
                                                                             
                                                            
                                    
     
  if (numWorkers <= 0
#ifdef WIN32
      || numWorkers > MAXIMUM_WAIT_OBJECTS
#endif
  )
    fatal("invalid number of parallel jobs");

                                                                   
  if (archiveFormat != archDirectory && numWorkers > 1)
  {
    fatal("parallel backup only supported by the directory format");
  }

                            
  fout = CreateArchive(filename, archiveFormat, compressLevel, dosync, archiveMode, setupDumpWorker);

                                               
  SetArchiveOptions(fout, &dopt, NULL);

                                 
  on_exit_close_archive(fout);

                                             
  fout->verbose = g_verbose;

     
                                                                           
                                                                       
     
  fout->minRemoteVersion = 80000;
  fout->maxRemoteVersion = (PG_VERSION_NUM / 100) * 100 + 99;

  fout->numWorkers = numWorkers;

     
                                                                             
            
     
  ConnectDatabase(fout, &dopt.cparams, false);
  setup_connection(fout, dumpencoding, dumpsnapshot, use_role);

     
                                                                         
                                                
     
  if (fout->remoteVersion < 90100)
  {
    dopt.no_security_labels = 1;
  }

     
                                                                           
                          
     
  if (fout->isStandby)
  {
    dopt.no_unlogged_table_data = true;
  }

                                                                    
  if (fout->remoteVersion >= 80100)
  {
    username_subquery = "SELECT rolname FROM pg_catalog.pg_roles WHERE oid =";
  }
  else
  {
    username_subquery = "SELECT usename FROM pg_catalog.pg_user WHERE usesysid =";
  }

                                                                
  if (numWorkers > 1 && fout->remoteVersion < 90200 && !dopt.no_synchronized_snapshots)
  {
    fatal("Synchronized snapshots are not supported by this server version.\n"
          "Run with --no-synchronized-snapshots instead if you do not need\n"
          "synchronized snapshots.");
  }

                                                                         
  if (dumpsnapshot && fout->remoteVersion < 90200)
  {
    fatal("Exported snapshots are not supported by this server version.");
  }

     
                                                          
     
                                                                  
     
  if (fout->remoteVersion < 80100)
  {
    g_last_builtin_oid = findLastBuiltinOid_V71(fout);
  }
  else
  {
    g_last_builtin_oid = FirstNormalObjectId - 1;
  }

  pg_log_info("last built-in OID is %u", g_last_builtin_oid);

                                                       
  if (schema_include_patterns.head != NULL)
  {
    expand_schema_name_patterns(fout, &schema_include_patterns, &schema_include_oids, strict_names);
    if (schema_include_oids.head == NULL)
    {
      fatal("no matching schemas were found");
    }
  }
  expand_schema_name_patterns(fout, &schema_exclude_patterns, &schema_exclude_oids, false);
                                                       

                                                      
  if (table_include_patterns.head != NULL)
  {
    expand_table_name_patterns(fout, &table_include_patterns, &table_include_oids, strict_names);
    if (table_include_oids.head == NULL)
    {
      fatal("no matching tables were found");
    }
  }
  expand_table_name_patterns(fout, &table_exclude_patterns, &table_exclude_oids, false);

  expand_table_name_patterns(fout, &tabledata_exclude_patterns, &tabledata_exclude_oids, false);

                                                       

     
                                                                             
                                                                           
                                                                     
                               
     
                                                                        
                                    
     
  if (dopt.include_everything && !dopt.schemaOnly && !dopt.dontOutputBlobs)
  {
    dopt.outputBlobs = true;
  }

     
                                                                         
                                
     
  tblinfo = getSchemaData(fout, &numTables);

  if (fout->remoteVersion < 80400)
  {
    guessConstraintInheritance(tblinfo, numTables);
  }

  if (!dopt.schemaOnly)
  {
    getTableData(&dopt, tblinfo, numTables, 0);
    buildMatViewRefreshDependencies(fout);
    if (dopt.dataOnly)
    {
      getTableDataFKConstraints();
    }
  }

  if (dopt.schemaOnly && dopt.sequence_data)
  {
    getTableData(&dopt, tblinfo, numTables, RELKIND_SEQUENCE);
  }

     
                                                                           
                                                                            
                                                   
     
                                                                     
                                                                         
     
  if (dopt.outputBlobs || dopt.binary_upgrade)
  {
    getBlobs(fout);
  }

     
                                                                
     
  getDependencies(fout);

                                                                        
  boundaryObjs = createBoundaryObjects();

                                                     
  getDumpableObjects(&dobjs, &numObjs);

     
                                                                  
     
  addBoundaryDependencies(dobjs, numObjs, boundaryObjs);

     
                                                                      
     
                                                                             
                                                                          
                                                                    
     
  sortDumpableObjectsByTypeName(dobjs, numObjs);

  sortDumpableObjects(dobjs, numObjs, boundaryObjs[0].dumpId, boundaryObjs[1].dumpId);

     
                                                                            
            
     

                                                                       
  dumpEncoding(fout);
  dumpStdStrings(fout);
  dumpSearchPath(fout);

                                                                            
  if (dopt.outputCreateDB)
  {
    dumpDatabase(fout);
  }

                                      
  for (i = 0; i < numObjs; i++)
  {
    dumpDumpableObject(fout, dobjs[i]);
  }

     
                                                         
     
  ropt = NewRestoreOptions();
  ropt->filename = filename;

                                                                  
  ropt->cparams.dbname = dopt.cparams.dbname ? pg_strdup(dopt.cparams.dbname) : NULL;
  ropt->cparams.pgport = dopt.cparams.pgport ? pg_strdup(dopt.cparams.pgport) : NULL;
  ropt->cparams.pghost = dopt.cparams.pghost ? pg_strdup(dopt.cparams.pghost) : NULL;
  ropt->cparams.username = dopt.cparams.username ? pg_strdup(dopt.cparams.username) : NULL;
  ropt->cparams.promptPassword = dopt.cparams.promptPassword;
  ropt->dropSchema = dopt.outputClean;
  ropt->dataOnly = dopt.dataOnly;
  ropt->schemaOnly = dopt.schemaOnly;
  ropt->if_exists = dopt.if_exists;
  ropt->column_inserts = dopt.column_inserts;
  ropt->dumpSections = dopt.dumpSections;
  ropt->aclsSkip = dopt.aclsSkip;
  ropt->superuser = dopt.outputSuperuser;
  ropt->createDB = dopt.outputCreateDB;
  ropt->noOwner = dopt.outputNoOwner;
  ropt->noTablespace = dopt.outputNoTablespaces;
  ropt->disable_triggers = dopt.disable_triggers;
  ropt->use_setsessauth = dopt.use_setsessauth;
  ropt->disable_dollar_quoting = dopt.disable_dollar_quoting;
  ropt->dump_inserts = dopt.dump_inserts;
  ropt->no_comments = dopt.no_comments;
  ropt->no_publications = dopt.no_publications;
  ropt->no_security_labels = dopt.no_security_labels;
  ropt->no_subscriptions = dopt.no_subscriptions;
  ropt->lockWaitTimeout = dopt.lockWaitTimeout;
  ropt->include_everything = dopt.include_everything;
  ropt->enable_row_security = dopt.enable_row_security;
  ropt->sequence_data = dopt.sequence_data;
  ropt->binary_upgrade = dopt.binary_upgrade;

  if (compressLevel == -1)
  {
    ropt->compression = 0;
  }
  else
  {
    ropt->compression = compressLevel;
  }

  ropt->suppressDumpWarnings = true;                               

  SetArchiveOptions(fout, &dopt, ropt);

                                           
  ProcessArchiveRestoreOptions(fout);

     
                                                                             
                                                                             
                                              
     
  if (!plainText)
  {
    BuildArchiveDependencies(fout);
  }

     
                                              
     
                                                                         
                                                                          
                
     
  if (plainText)
  {
    RestoreArchive(fout);
  }

  CloseArchive(fout);

  exit_nicely(0);
}

static void
help(const char *progname)
{
  printf(_("%s dumps a database as a text file or to other formats.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]... [DBNAME]\n"), progname);

  printf(_("\nGeneral options:\n"));
  printf(_("  -f, --file=FILENAME          output file or directory name\n"));
  printf(_("  -F, --format=c|d|t|p         output file format (custom, directory, tar,\n"
           "                               plain text (default))\n"));
  printf(_("  -j, --jobs=NUM               use this many parallel jobs to dump\n"));
  printf(_("  -v, --verbose                verbose mode\n"));
  printf(_("  -V, --version                output version information, then exit\n"));
  printf(_("  -Z, --compress=0-9           compression level for compressed formats\n"));
  printf(_("  --lock-wait-timeout=TIMEOUT  fail after waiting TIMEOUT for a table lock\n"));
  printf(_("  --no-sync                    do not wait for changes to be written safely to disk\n"));
  printf(_("  -?, --help                   show this help, then exit\n"));

  printf(_("\nOptions controlling the output content:\n"));
  printf(_("  -a, --data-only              dump only the data, not the schema\n"));
  printf(_("  -b, --blobs                  include large objects in dump\n"));
  printf(_("  -B, --no-blobs               exclude large objects in dump\n"));
  printf(_("  -c, --clean                  clean (drop) database objects before recreating\n"));
  printf(_("  -C, --create                 include commands to create database in dump\n"));
  printf(_("  -E, --encoding=ENCODING      dump the data in encoding ENCODING\n"));
  printf(_("  -n, --schema=PATTERN         dump the specified schema(s) only\n"));
  printf(_("  -N, --exclude-schema=PATTERN do NOT dump the specified schema(s)\n"));
  printf(_("  -O, --no-owner               skip restoration of object ownership in\n"
           "                               plain-text format\n"));
  printf(_("  -s, --schema-only            dump only the schema, no data\n"));
  printf(_("  -S, --superuser=NAME         superuser user name to use in plain-text format\n"));
  printf(_("  -t, --table=PATTERN          dump the specified table(s) only\n"));
  printf(_("  -T, --exclude-table=PATTERN  do NOT dump the specified table(s)\n"));
  printf(_("  -x, --no-privileges          do not dump privileges (grant/revoke)\n"));
  printf(_("  --binary-upgrade             for use by upgrade utilities only\n"));
  printf(_("  --column-inserts             dump data as INSERT commands with column names\n"));
  printf(_("  --disable-dollar-quoting     disable dollar quoting, use SQL standard quoting\n"));
  printf(_("  --disable-triggers           disable triggers during data-only restore\n"));
  printf(_("  --enable-row-security        enable row security (dump only content user has\n"
           "                               access to)\n"));
  printf(_("  --exclude-table-data=PATTERN do NOT dump data for the specified table(s)\n"));
  printf(_("  --extra-float-digits=NUM     override default setting for extra_float_digits\n"));
  printf(_("  --if-exists                  use IF EXISTS when dropping objects\n"));
  printf(_("  --inserts                    dump data as INSERT commands, rather than COPY\n"));
  printf(_("  --load-via-partition-root    load partitions via the root table\n"));
  printf(_("  --no-comments                do not dump comments\n"));
  printf(_("  --no-publications            do not dump publications\n"));
  printf(_("  --no-security-labels         do not dump security label assignments\n"));
  printf(_("  --no-subscriptions           do not dump subscriptions\n"));
  printf(_("  --no-synchronized-snapshots  do not use synchronized snapshots in parallel jobs\n"));
  printf(_("  --no-tablespaces             do not dump tablespace assignments\n"));
  printf(_("  --no-unlogged-table-data     do not dump unlogged table data\n"));
  printf(_("  --on-conflict-do-nothing     add ON CONFLICT DO NOTHING to INSERT commands\n"));
  printf(_("  --quote-all-identifiers      quote all identifiers, even if not key words\n"));
  printf(_("  --rows-per-insert=NROWS      number of rows per INSERT; implies --inserts\n"));
  printf(_("  --section=SECTION            dump named section (pre-data, data, or post-data)\n"));
  printf(_("  --serializable-deferrable    wait until the dump can run without anomalies\n"));
  printf(_("  --snapshot=SNAPSHOT          use given snapshot for the dump\n"));
  printf(_("  --strict-names               require table and/or schema include patterns to\n"
           "                               match at least one entity each\n"));
  printf(_("  --use-set-session-authorization\n"
           "                               use SET SESSION AUTHORIZATION commands instead of\n"
           "                               ALTER OWNER commands to set ownership\n"));

  printf(_("\nConnection options:\n"));
  printf(_("  -d, --dbname=DBNAME      database to dump\n"));
  printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
  printf(_("  -p, --port=PORT          database server port number\n"));
  printf(_("  -U, --username=NAME      connect as specified database user\n"));
  printf(_("  -w, --no-password        never prompt for password\n"));
  printf(_("  -W, --password           force password prompt (should happen automatically)\n"));
  printf(_("  --role=ROLENAME          do SET ROLE before dump\n"));

  printf(_("\nIf no database name is supplied, then the PGDATABASE environment\n"
           "variable value is used.\n\n"));
  printf(_("Report bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

static void
setup_connection(Archive *AH, const char *dumpencoding, const char *dumpsnapshot, char *use_role)
{
  DumpOptions *dopt = AH->dopt;
  PGconn *conn = GetConnection(AH);
  const char *std_strings;

  PQclear(ExecuteSqlQueryForSingleRow(AH, ALWAYS_SECURE_SEARCH_PATH_SQL));

     
                                           
     
  if (dumpencoding)
  {
    if (PQsetClientEncoding(conn, dumpencoding) < 0)
    {
      fatal("invalid client encoding \"%s\" specified", dumpencoding);
    }
  }

     
                                                                             
                                    
     
  AH->encoding = PQclientEncoding(conn);

  std_strings = PQparameterStatus(conn, "standard_conforming_strings");
  AH->std_strings = (std_strings && strcmp(std_strings, "on") == 0);

     
                                                                            
                                                                             
                                         
     
  if (!use_role && AH->use_role)
  {
    use_role = AH->use_role;
  }

                                 
  if (use_role && AH->remoteVersion >= 80100)
  {
    PQExpBuffer query = createPQExpBuffer();

    appendPQExpBuffer(query, "SET ROLE %s", fmtId(use_role));
    ExecuteSqlStatement(AH, query->data);
    destroyPQExpBuffer(query);

                                                            
    if (!AH->use_role)
    {
      AH->use_role = pg_strdup(use_role);
    }
  }

                                                                 
  ExecuteSqlStatement(AH, "SET DATESTYLE = ISO");

                                                        
  if (AH->remoteVersion >= 80400)
  {
    ExecuteSqlStatement(AH, "SET INTERVALSTYLE = POSTGRES");
  }

     
                                                                             
                                                                      
                                                                   
     
  if (have_extra_float_digits)
  {
    PQExpBuffer q = createPQExpBuffer();

    appendPQExpBuffer(q, "SET extra_float_digits TO %d", extra_float_digits);
    ExecuteSqlStatement(AH, q->data);
    destroyPQExpBuffer(q);
  }
  else if (AH->remoteVersion >= 90000)
  {
    ExecuteSqlStatement(AH, "SET extra_float_digits TO 3");
  }
  else
  {
    ExecuteSqlStatement(AH, "SET extra_float_digits TO 2");
  }

     
                                                                   
                                                                     
     
  if (AH->remoteVersion >= 80300)
  {
    ExecuteSqlStatement(AH, "SET synchronize_seqscans TO off");
  }

     
                                    
     
  ExecuteSqlStatement(AH, "SET statement_timeout = 0");
  if (AH->remoteVersion >= 90300)
  {
    ExecuteSqlStatement(AH, "SET lock_timeout = 0");
  }
  if (AH->remoteVersion >= 90600)
  {
    ExecuteSqlStatement(AH, "SET idle_in_transaction_session_timeout = 0");
  }

     
                                          
     
  if (quote_all_identifiers && AH->remoteVersion >= 90100)
  {
    ExecuteSqlStatement(AH, "SET quote_all_identifiers = true");
  }

     
                                             
     
  if (AH->remoteVersion >= 90500)
  {
    if (dopt->enable_row_security)
    {
      ExecuteSqlStatement(AH, "SET row_security = on");
    }
    else
    {
      ExecuteSqlStatement(AH, "SET row_security = off");
    }
  }

     
                                                                          
     
  ExecuteSqlStatement(AH, "BEGIN");
  if (AH->remoteVersion >= 90100)
  {
       
                                                                           
                                                                         
                                                                    
                                                                         
                                                                      
                                                                 
       
    if (dopt->serializable_deferrable && AH->sync_snapshot_id == NULL)
    {
      ExecuteSqlStatement(AH, "SET TRANSACTION ISOLATION LEVEL "
                              "SERIALIZABLE, READ ONLY, DEFERRABLE");
    }
    else
    {
      ExecuteSqlStatement(AH, "SET TRANSACTION ISOLATION LEVEL "
                              "REPEATABLE READ, READ ONLY");
    }
  }
  else
  {
    ExecuteSqlStatement(AH, "SET TRANSACTION ISOLATION LEVEL "
                            "SERIALIZABLE, READ ONLY");
  }

     
                                                                           
                                                                            
                                                                          
     
  if (dumpsnapshot)
  {
    AH->sync_snapshot_id = pg_strdup(dumpsnapshot);
  }

  if (AH->sync_snapshot_id)
  {
    PQExpBuffer query = createPQExpBuffer();

    appendPQExpBuffer(query, "SET TRANSACTION SNAPSHOT ");
    appendStringLiteralConn(query, AH->sync_snapshot_id, conn);
    ExecuteSqlStatement(AH, query->data);
    destroyPQExpBuffer(query);
  }
  else if (AH->numWorkers > 1 && AH->remoteVersion >= 90200 && !dopt->no_synchronized_snapshots)
  {
    if (AH->isStandby && AH->remoteVersion < 100000)
    {
      fatal("Synchronized snapshots on standby servers are not supported by this server version.\n"
            "Run with --no-synchronized-snapshots instead if you do not need\n"
            "synchronized snapshots.");
    }

    AH->sync_snapshot_id = get_synchronized_snapshot(AH);
  }
}

                                                     
static void
setupDumpWorker(Archive *AH)
{
     
                                                                       
                                                            
                                                                         
                                                                            
     
  setup_connection(AH, pg_encoding_to_char(AH->encoding), NULL, NULL);
}

static char *
get_synchronized_snapshot(Archive *fout)
{
  char *query = "SELECT pg_catalog.pg_export_snapshot()";
  char *result;
  PGresult *res;

  res = ExecuteSqlQueryForSingleRow(fout, query);
  result = pg_strdup(PQgetvalue(res, 0, 0));
  PQclear(res);

  return result;
}

static ArchiveFormat
parseArchiveFormat(const char *format, ArchiveMode *mode)
{
  ArchiveFormat archiveFormat;

  *mode = archModeWrite;

  if (pg_strcasecmp(format, "a") == 0 || pg_strcasecmp(format, "append") == 0)
  {
                                                           
    archiveFormat = archNull;
    *mode = archModeAppend;
  }
  else if (pg_strcasecmp(format, "c") == 0)
  {
    archiveFormat = archCustom;
  }
  else if (pg_strcasecmp(format, "custom") == 0)
  {
    archiveFormat = archCustom;
  }
  else if (pg_strcasecmp(format, "d") == 0)
  {
    archiveFormat = archDirectory;
  }
  else if (pg_strcasecmp(format, "directory") == 0)
  {
    archiveFormat = archDirectory;
  }
  else if (pg_strcasecmp(format, "p") == 0)
  {
    archiveFormat = archNull;
  }
  else if (pg_strcasecmp(format, "plain") == 0)
  {
    archiveFormat = archNull;
  }
  else if (pg_strcasecmp(format, "t") == 0)
  {
    archiveFormat = archTar;
  }
  else if (pg_strcasecmp(format, "tar") == 0)
  {
    archiveFormat = archTar;
  }
  else
  {
    fatal("invalid output format \"%s\" specified", format);
  }
  return archiveFormat;
}

   
                                                                     
                                          
   
static void
expand_schema_name_patterns(Archive *fout, SimpleStringList *patterns, SimpleOidList *oids, bool strict_names)
{
  PQExpBuffer query;
  PGresult *res;
  SimpleStringListCell *cell;
  int i;

  if (patterns->head == NULL)
  {
    return;                    
  }

  query = createPQExpBuffer();

     
                                                                    
                                                           
     

  for (cell = patterns->head; cell; cell = cell->next)
  {
    appendPQExpBuffer(query, "SELECT oid FROM pg_catalog.pg_namespace n\n");
    processSQLNamePattern(GetConnection(fout), query, cell->val, false, false, NULL, "n.nspname", NULL, NULL);

    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
    if (strict_names && PQntuples(res) == 0)
    {
      fatal("no matching schemas were found for pattern \"%s\"", cell->val);
    }

    for (i = 0; i < PQntuples(res); i++)
    {
      simple_oid_list_append(oids, atooid(PQgetvalue(res, i, 0)));
    }

    PQclear(res);
    resetPQExpBuffer(query);
  }

  destroyPQExpBuffer(query);
}

   
                                                                    
                                                                            
                   
   
static void
expand_table_name_patterns(Archive *fout, SimpleStringList *patterns, SimpleOidList *oids, bool strict_names)
{
  PQExpBuffer query;
  PGresult *res;
  SimpleStringListCell *cell;
  int i;

  if (patterns->head == NULL)
  {
    return;                    
  }

  query = createPQExpBuffer();

     
                                                                           
                    
     

  for (cell = patterns->head; cell; cell = cell->next)
  {
       
                                                                       
                                                                           
                             
       
    appendPQExpBuffer(query,
        "SELECT c.oid"
        "\nFROM pg_catalog.pg_class c"
        "\n     LEFT JOIN pg_catalog.pg_namespace n"
        "\n     ON n.oid OPERATOR(pg_catalog.=) c.relnamespace"
        "\nWHERE c.relkind OPERATOR(pg_catalog.=) ANY"
        "\n    (array['%c', '%c', '%c', '%c', '%c', '%c'])\n",
        RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE, RELKIND_PARTITIONED_TABLE);
    processSQLNamePattern(GetConnection(fout), query, cell->val, true, false, "n.nspname", "c.relname", NULL, "pg_catalog.pg_table_is_visible(c.oid)");

    ExecuteSqlStatement(fout, "RESET search_path");
    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
    PQclear(ExecuteSqlQueryForSingleRow(fout, ALWAYS_SECURE_SEARCH_PATH_SQL));
    if (strict_names && PQntuples(res) == 0)
    {
      fatal("no matching tables were found for pattern \"%s\"", cell->val);
    }

    for (i = 0; i < PQntuples(res); i++)
    {
      simple_oid_list_append(oids, atooid(PQgetvalue(res, i, 0)));
    }

    PQclear(res);
    resetPQExpBuffer(query);
  }

  destroyPQExpBuffer(query);
}

   
                            
                                                                
                                                                     
   
                                                                          
                                                                       
                                                                                 
   
                                                              
   
static bool
checkExtensionMembership(DumpableObject *dobj, Archive *fout)
{
  ExtensionInfo *ext = findOwningExtension(dobj->catId);

  if (ext == NULL)
  {
    return false;
  }

  dobj->ext_member = true;

                                                                        
  addObjectDependency(dobj, ext->dobj.dumpId);

     
                                                                           
                                           
     
                                                                            
                                                                             
                                                                             
                                                        
     
                                                                      
     
                                                                     
                                                                       
                                                                        
                
     
  if (fout->dopt->binary_upgrade)
  {
    dobj->dump = ext->dobj.dump;
  }
  else
  {
    if (fout->remoteVersion < 90600)
    {
      dobj->dump = DUMP_COMPONENT_NONE;
    }
    else
    {
      dobj->dump = ext->dobj.dump_contains & (DUMP_COMPONENT_ACL | DUMP_COMPONENT_SECLABEL | DUMP_COMPONENT_POLICY);
    }
  }

  return true;
}

   
                                                      
                                            
   
static void
selectDumpableNamespace(NamespaceInfo *nsinfo, Archive *fout)
{
     
                                                                   
                                                                          
                                                            
     
  if (table_include_oids.head != NULL)
  {
    nsinfo->dobj.dump_contains = nsinfo->dobj.dump = DUMP_COMPONENT_NONE;
  }
  else if (schema_include_oids.head != NULL)
  {
    nsinfo->dobj.dump_contains = nsinfo->dobj.dump = simple_oid_list_member(&schema_include_oids, nsinfo->dobj.catId.oid) ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
  else if (fout->remoteVersion >= 90600 && strcmp(nsinfo->dobj.name, "pg_catalog") == 0)
  {
       
                                                                        
                                                                         
                                        
       
    nsinfo->dobj.dump_contains = nsinfo->dobj.dump = DUMP_COMPONENT_ACL;
  }
  else if (strncmp(nsinfo->dobj.name, "pg_", 3) == 0 || strcmp(nsinfo->dobj.name, "information_schema") == 0)
  {
                                               
    nsinfo->dobj.dump_contains = nsinfo->dobj.dump = DUMP_COMPONENT_NONE;
  }
  else if (strcmp(nsinfo->dobj.name, "public") == 0)
  {
       
                                                                   
                                                                         
                                                                       
                                                                          
                                                                     
                                                
       
    nsinfo->dobj.dump = DUMP_COMPONENT_ACL;
    nsinfo->dobj.dump_contains = DUMP_COMPONENT_ALL;
  }
  else
  {
    nsinfo->dobj.dump_contains = nsinfo->dobj.dump = DUMP_COMPONENT_ALL;
  }

     
                                                                     
     
  if (nsinfo->dobj.dump_contains && simple_oid_list_member(&schema_exclude_oids, nsinfo->dobj.catId.oid))
  {
    nsinfo->dobj.dump_contains = nsinfo->dobj.dump = DUMP_COMPONENT_NONE;
  }

     
                                                                          
                                                                           
                                                                            
                                                                       
                                       
     
  (void)checkExtensionMembership(&nsinfo->dobj, fout);
}

   
                                                  
                                        
   
static void
selectDumpableTable(TableInfo *tbinfo, Archive *fout)
{
  if (checkExtensionMembership(&tbinfo->dobj, fout))
  {
    return;                                              
  }

     
                                                                             
                                                    
     
  if (table_include_oids.head != NULL)
  {
    tbinfo->dobj.dump = simple_oid_list_member(&table_include_oids, tbinfo->dobj.catId.oid) ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
  else
  {
    tbinfo->dobj.dump = tbinfo->dobj.namespace->dobj.dump_contains;
  }

     
                                                                 
     
  if (tbinfo->dobj.dump && simple_oid_list_member(&table_exclude_oids, tbinfo->dobj.catId.oid))
  {
    tbinfo->dobj.dump = DUMP_COMPONENT_NONE;
  }
}

   
                                                 
                                       
   
                                                                             
                                                                              
                                                                               
                                                                            
                                                                            
                                                                              
                                                                               
                                    
   
static void
selectDumpableType(TypeInfo *tyinfo, Archive *fout)
{
                                                                 
  if (OidIsValid(tyinfo->typrelid) && tyinfo->typrelkind != RELKIND_COMPOSITE_TYPE)
  {
    TableInfo *tytable = findTableByOid(tyinfo->typrelid);

    tyinfo->dobj.objType = DO_DUMMY_TYPE;
    if (tytable != NULL)
    {
      tyinfo->dobj.dump = tytable->dobj.dump;
    }
    else
    {
      tyinfo->dobj.dump = DUMP_COMPONENT_NONE;
    }
    return;
  }

                                       
  if (tyinfo->isArray)
  {
    tyinfo->dobj.objType = DO_DUMMY_TYPE;

       
                                                                        
                                                                       
                                                                    
                                                
       
  }

  if (checkExtensionMembership(&tyinfo->dobj, fout))
  {
    return;                                              
  }

                                                                       
  tyinfo->dobj.dump = tyinfo->dobj.namespace->dobj.dump_contains;
}

   
                                                       
                                              
   
                                                                    
                                                                      
                                        
   
static void
selectDumpableDefaultACL(DefaultACLInfo *dinfo, DumpOptions *dopt)
{
                                               

  if (dinfo->dobj.namespace)
  {
                                                           
    dinfo->dobj.dump = dinfo->dobj.namespace->dobj.dump_contains;
  }
  else
  {
    dinfo->dobj.dump = dopt->include_everything ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
}

   
                                                 
                                       
   
                                                                           
                                                                              
                                                                           
                                            
   
static void
selectDumpableCast(CastInfo *cast, Archive *fout)
{
  if (checkExtensionMembership(&cast->dobj, fout))
  {
    return;                                              
  }

     
                                                                             
                             
     
  if (cast->dobj.catId.oid <= (Oid)g_last_builtin_oid)
  {
    cast->dobj.dump = DUMP_COMPONENT_NONE;
  }
  else
  {
    cast->dobj.dump = fout->dopt->include_everything ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
}

   
                                                     
                                                      
   
                                                                       
                                                                       
                                                       
   
static void
selectDumpableProcLang(ProcLangInfo *plang, Archive *fout)
{
  if (checkExtensionMembership(&plang->dobj, fout))
  {
    return;                                              
  }

     
                                                                       
     
                                                                           
                                                                             
                                
     
  if (!fout->dopt->include_everything)
  {
    plang->dobj.dump = DUMP_COMPONENT_NONE;
  }
  else
  {
    if (plang->dobj.catId.oid <= (Oid)g_last_builtin_oid)
    {
      plang->dobj.dump = fout->remoteVersion < 90600 ? DUMP_COMPONENT_NONE : DUMP_COMPONENT_ACL;
    }
    else
    {
      plang->dobj.dump = DUMP_COMPONENT_ALL;
    }
  }
}

   
                                                         
                                                 
   
                                                                          
                                                                   
                                                     
   
static void
selectDumpableAccessMethod(AccessMethodInfo *method, Archive *fout)
{
  if (checkExtensionMembership(&method->dobj, fout))
  {
    return;                                              
  }

     
                                                                          
                                         
     
  if (method->dobj.catId.oid <= (Oid)g_last_builtin_oid)
  {
    method->dobj.dump = DUMP_COMPONENT_NONE;
  }
  else
  {
    method->dobj.dump = fout->dopt->include_everything ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
}

   
                                                      
                                             
   
                                                                            
                                                                               
                                                                          
                                                                    
                                                                               
   
static void
selectDumpableExtension(ExtensionInfo *extinfo, DumpOptions *dopt)
{
     
                                                                       
                                                                           
                              
     
  if (extinfo->dobj.catId.oid <= (Oid)g_last_builtin_oid)
  {
    extinfo->dobj.dump = extinfo->dobj.dump_contains = DUMP_COMPONENT_ACL;
  }
  else
  {
    extinfo->dobj.dump = extinfo->dobj.dump_contains = dopt->include_everything ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
}

   
                                                             
                                                    
   
                                                                              
                                                                        
   
static void
selectDumpablePublicationTable(DumpableObject *dobj, Archive *fout)
{
  if (checkExtensionMembership(dobj, fout))
  {
    return;                                              
  }

  dobj->dump = fout->dopt->include_everything ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
}

   
                                                   
                                                          
   
                                                                        
   
static void
selectDumpableObject(DumpableObject *dobj, Archive *fout)
{
  if (checkExtensionMembership(dobj, fout))
  {
    return;                                              
  }

     
                                                                       
                                                                         
     
  if (dobj->namespace)
  {
    dobj->dump = dobj->namespace->dobj.dump_contains;
  }
  else
  {
    dobj->dump = fout->dopt->include_everything ? DUMP_COMPONENT_ALL : DUMP_COMPONENT_NONE;
  }
}

   
                                                              
                                                                    
                   
   

static int
dumpTableData_copy(Archive *fout, void *dcontext)
{
  TableDataInfo *tdinfo = (TableDataInfo *)dcontext;
  TableInfo *tbinfo = tdinfo->tdtable;
  const char *classname = tbinfo->dobj.name;
  PQExpBuffer q = createPQExpBuffer();

     
                                                                           
                            
     
  PQExpBuffer clistBuf = createPQExpBuffer();
  PGconn *conn = GetConnection(fout);
  PGresult *res;
  int ret;
  char *copybuf;
  const char *column_list;

  pg_log_info("dumping contents of table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, classname);

     
                                                                          
                                                                     
                                                                       
                                            
     
  column_list = fmtCopyColumnList(tbinfo, clistBuf);

  if (tdinfo->filtercond)
  {
                                                           
    appendPQExpBufferStr(q, "COPY (SELECT ");
                                                     
    if (strlen(column_list) > 2)
    {
      appendPQExpBufferStr(q, column_list + 1);
      q->data[q->len - 1] = ' ';
    }
    else
    {
      appendPQExpBufferStr(q, "* ");
    }
    appendPQExpBuffer(q, "FROM %s %s) TO stdout;", fmtQualifiedDumpable(tbinfo), tdinfo->filtercond);
  }
  else
  {
    appendPQExpBuffer(q, "COPY %s %s TO stdout;", fmtQualifiedDumpable(tbinfo), column_list);
  }
  res = ExecuteSqlQuery(fout, q->data, PGRES_COPY_OUT);
  PQclear(res);
  destroyPQExpBuffer(clistBuf);

  for (;;)
  {
    ret = PQgetCopyData(conn, &copybuf, 0);

    if (ret < 0)
    {
      break;                    
    }

    if (copybuf)
    {
      WriteData(fout, copybuf, ret);
      PQfreemem(copybuf);
    }

                  
                 
       
                                                                      
                                                                          
                                                                      
                                        
       
                                                                         
                                                                           
                                                                        
                                                              
       
                                     
                                           
                                                     
                                          
               
                     
              
             
       
                                                                        
                                                    
       
                                                                       
                                                                         
                                                                
                                                                         
                          
       
                                                                
       
                                                                           
                                                                         
                                                                       
                                                                         
                                                    
       
                                          
       
                                                                           
                                                                       
                  
       
  }
  archprintf(fout, "\\.\n\n\n");

  if (ret == -2)
  {
                                   
    pg_log_error("Dumping the contents of table \"%s\" failed: PQgetCopyData() failed.", classname);
    pg_log_error("Error message from server: %s", PQerrorMessage(conn));
    pg_log_error("The command was: %s", q->data);
    exit_nicely(1);
  }

                                                             
  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    pg_log_error("Dumping the contents of table \"%s\" failed: PQgetResult() failed.", classname);
    pg_log_error("Error message from server: %s", PQerrorMessage(conn));
    pg_log_error("The command was: %s", q->data);
    exit_nicely(1);
  }
  PQclear(res);

                                                               
  if (PQgetResult(conn) != NULL)
  {
    pg_log_warning("unexpected extra results during COPY of table \"%s\"", classname);
  }

  destroyPQExpBuffer(q);
  return 1;
}

   
                                          
   
                                                                         
                                                                 
                                                                             
                                                                             
   
static int
dumpTableData_insert(Archive *fout, void *dcontext)
{
  TableDataInfo *tdinfo = (TableDataInfo *)dcontext;
  TableInfo *tbinfo = tdinfo->tdtable;
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer insertStmt = NULL;
  char *attgenerated;
  PGresult *res;
  int nfields, i;
  int rows_per_statement = dopt->dump_inserts;
  int rows_this_statement = 0;

     
                                                                          
                                                                          
                                                                           
                                                                            
                                                
     
  attgenerated = (char *)pg_malloc(tbinfo->numatts * sizeof(char));
  appendPQExpBufferStr(q, "DECLARE _pg_dump_cursor CURSOR FOR SELECT ");
  nfields = 0;
  for (i = 0; i < tbinfo->numatts; i++)
  {
    if (tbinfo->attisdropped[i])
    {
      continue;
    }
    if (tbinfo->attgenerated[i] && dopt->column_inserts)
    {
      continue;
    }
    if (nfields > 0)
    {
      appendPQExpBufferStr(q, ", ");
    }
    if (tbinfo->attgenerated[i])
    {
      appendPQExpBufferStr(q, "NULL");
    }
    else
    {
      appendPQExpBufferStr(q, fmtId(tbinfo->attnames[i]));
    }
    attgenerated[nfields] = tbinfo->attgenerated[i];
    nfields++;
  }
                                                                 
  if (nfields == 0)
  {
    appendPQExpBufferStr(q, "NULL");
  }
  appendPQExpBuffer(q, " FROM ONLY %s", fmtQualifiedDumpable(tbinfo));
  if (tdinfo->filtercond)
  {
    appendPQExpBuffer(q, " %s", tdinfo->filtercond);
  }

  ExecuteSqlStatement(fout, q->data);

  while (1)
  {
    res = ExecuteSqlQuery(fout, "FETCH 100 FROM _pg_dump_cursor", PGRES_TUPLES_OK);

                                                                 
    if (nfields != PQnfields(res) && !(nfields == 0 && PQnfields(res) == 1))
    {
      fatal("wrong number of fields retrieved from table \"%s\"", tbinfo->dobj.name);
    }

       
                                                                       
                                                                       
                                                                          
                                                                   
                                                                      
       
    if (insertStmt == NULL)
    {
      TableInfo *targettab;

      insertStmt = createPQExpBuffer();

         
                                                                      
                                                                         
                     
         
      if (dopt->load_via_partition_root && tbinfo->ispartition)
      {
        targettab = getRootTableInfo(tbinfo);
      }
      else
      {
        targettab = tbinfo;
      }

      appendPQExpBuffer(insertStmt, "INSERT INTO %s ", fmtQualifiedDumpable(targettab));

                                             
      if (nfields == 0)
      {
        appendPQExpBufferStr(insertStmt, "DEFAULT VALUES;\n");
      }
      else
      {
                                                         
        if (dopt->column_inserts)
        {
          appendPQExpBufferChar(insertStmt, '(');
          for (int field = 0; field < nfields; field++)
          {
            if (field > 0)
            {
              appendPQExpBufferStr(insertStmt, ", ");
            }
            appendPQExpBufferStr(insertStmt, fmtId(PQfname(res, field)));
          }
          appendPQExpBufferStr(insertStmt, ") ");
        }

        if (tbinfo->needs_override)
        {
          appendPQExpBufferStr(insertStmt, "OVERRIDING SYSTEM VALUE ");
        }

        appendPQExpBufferStr(insertStmt, "VALUES");
      }
    }

    for (int tuple = 0; tuple < PQntuples(res); tuple++)
    {
                                                                        
      if (rows_this_statement == 0)
      {
        archputs(insertStmt->data, fout);
      }

         
                                                                   
                                                             
                                                                        
                                                                    
                                                                     
                                                                 
                                 
         
      if (nfields == 0)
      {
        continue;
      }

                              
      if (rows_per_statement == 1)
      {
        archputs(" (", fout);
      }
      else if (rows_this_statement > 0)
      {
        archputs(",\n\t(", fout);
      }
      else
      {
        archputs("\n\t(", fout);
      }

      for (int field = 0; field < nfields; field++)
      {
        if (field > 0)
        {
          archputs(", ", fout);
        }
        if (attgenerated[field])
        {
          archputs("DEFAULT", fout);
          continue;
        }
        if (PQgetisnull(res, tuple, field))
        {
          archputs("NULL", fout);
          continue;
        }

                                                                  
        switch (PQftype(res, field))
        {
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case OIDOID:
        case FLOAT4OID:
        case FLOAT8OID:
        case NUMERICOID:
        {
             
                                                           
                                                             
                                                        
                                                          
                                     
             
                                                       
                                                            
                                          
             
          const char *s = PQgetvalue(res, tuple, field);

          if (strspn(s, "0123456789 +-eE.") == strlen(s))
          {
            archputs(s, fout);
          }
          else
          {
            archprintf(fout, "'%s'", s);
          }
        }
        break;

        case BITOID:
        case VARBITOID:
          archprintf(fout, "B'%s'", PQgetvalue(res, tuple, field));
          break;

        case BOOLOID:
          if (strcmp(PQgetvalue(res, tuple, field), "t") == 0)
          {
            archputs("true", fout);
          }
          else
          {
            archputs("false", fout);
          }
          break;

        default:
                                                               
          resetPQExpBuffer(q);
          appendStringLiteralAH(q, PQgetvalue(res, tuple, field), fout);
          archputs(q->data, fout);
          break;
        }
      }

                                 
      archputs(")", fout);

                                                                       
      if (++rows_this_statement >= rows_per_statement)
      {
        if (dopt->do_nothing)
        {
          archputs(" ON CONFLICT DO NOTHING;\n", fout);
        }
        else
        {
          archputs(";\n", fout);
        }
                                   
        rows_this_statement = 0;
      }
    }

    if (PQntuples(res) <= 0)
    {
      PQclear(res);
      break;
    }
    PQclear(res);
  }

                                                                
  if (rows_this_statement > 0)
  {
    if (dopt->do_nothing)
    {
      archputs(" ON CONFLICT DO NOTHING;\n", fout);
    }
    else
    {
      archputs(";\n", fout);
    }
  }

  archputs("\n\n", fout);

  ExecuteSqlStatement(fout, "CLOSE _pg_dump_cursor");

  destroyPQExpBuffer(q);
  if (insertStmt != NULL)
  {
    destroyPQExpBuffer(insertStmt);
  }
  free(attgenerated);

  return 1;
}

   
                     
                                                             
   
static TableInfo *
getRootTableInfo(TableInfo *tbinfo)
{
  TableInfo *parentTbinfo;

  Assert(tbinfo->ispartition);
  Assert(tbinfo->numParents == 1);

  parentTbinfo = tbinfo->parents[0];
  while (parentTbinfo->ispartition)
  {
    Assert(parentTbinfo->numParents == 1);
    parentTbinfo = parentTbinfo->parents[0];
  }

  return parentTbinfo;
}

   
                   
                                         
   
                                                                     
   
static void
dumpTableData(Archive *fout, TableDataInfo *tdinfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = tdinfo->tdtable;
  PQExpBuffer copyBuf = createPQExpBuffer();
  PQExpBuffer clistBuf = createPQExpBuffer();
  DataDumperPtr dumpFn;
  char *copyStmt;
  const char *copyFrom;

                                                                     
  Assert(tbinfo->interesting);

  if (!dopt->dump_inserts)
  {
                                 
    dumpFn = dumpTableData_copy;

       
                                                                        
                                                                        
              
       
    if (dopt->load_via_partition_root && tbinfo->ispartition)
    {
      TableInfo *parentTbinfo;

      parentTbinfo = getRootTableInfo(tbinfo);
      copyFrom = fmtQualifiedDumpable(parentTbinfo);
    }
    else
    {
      copyFrom = fmtQualifiedDumpable(tbinfo);
    }

                                                            
    appendPQExpBuffer(copyBuf, "COPY %s ", copyFrom);
    appendPQExpBuffer(copyBuf, "%s FROM stdin;\n", fmtCopyColumnList(tbinfo, clistBuf));
    copyStmt = copyBuf->data;
  }
  else
  {
                              
    dumpFn = dumpTableData_insert;
    copyStmt = NULL;
  }

     
                                                                             
                                                                           
                                                
     
  if (tdinfo->dobj.dump & DUMP_COMPONENT_DATA)
  {
    TocEntry *te;

    te = ArchiveEntry(fout, tdinfo->dobj.catId, tdinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tbinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "TABLE DATA", .section = SECTION_DATA, .copyStmt = copyStmt, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1, .dumpFn = dumpFn, .dumpArg = tdinfo));

       
                                                                          
                                                                        
                                                                       
                                                                         
                                                                          
                                                                   
                                
       
    te->dataLength = (BlockNumber)tbinfo->relpages;
  }

  destroyPQExpBuffer(copyBuf);
  destroyPQExpBuffer(clistBuf);
}

   
                        
                                                                
   
                                                                               
              
   
static void
refreshMatViewData(Archive *fout, TableDataInfo *tdinfo)
{
  TableInfo *tbinfo = tdinfo->tdtable;
  PQExpBuffer q;

                                                                        
  if (!tbinfo->relispopulated)
  {
    return;
  }

  q = createPQExpBuffer();

  appendPQExpBuffer(q, "REFRESH MATERIALIZED VIEW %s;\n", fmtQualifiedDumpable(tbinfo));

  if (tdinfo->dobj.dump & DUMP_COMPONENT_DATA)
  {
    ArchiveEntry(fout, tdinfo->dobj.catId,                 
        tdinfo->dobj.dumpId,                            
        ARCHIVE_OPTS(.tag = tbinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "MATERIALIZED VIEW DATA", .section = SECTION_POST_DATA, .createStmt = q->data, .deps = tdinfo->dobj.dependencies, .nDeps = tdinfo->dobj.nDeps));
  }

  destroyPQExpBuffer(q);
}

   
                  
                                                                 
   
static void
getTableData(DumpOptions *dopt, TableInfo *tblinfo, int numTables, char relkind)
{
  int i;

  for (i = 0; i < numTables; i++)
  {
    if (tblinfo[i].dobj.dump & DUMP_COMPONENT_DATA && (!relkind || tblinfo[i].relkind == relkind))
    {
      makeTableDataInfo(dopt, &(tblinfo[i]));
    }
  }
}

   
                                                              
   
                                                                         
                                                           
   
static void
makeTableDataInfo(DumpOptions *dopt, TableInfo *tbinfo)
{
  TableDataInfo *tdinfo;

     
                                                                       
                                 
     
  if (tbinfo->dataObj != NULL)
  {
    return;
  }

                                    
  if (tbinfo->relkind == RELKIND_VIEW)
  {
    return;
  }
                                             
  if (tbinfo->relkind == RELKIND_FOREIGN_TABLE)
  {
    return;
  }
                                                    
  if (tbinfo->relkind == RELKIND_PARTITIONED_TABLE)
  {
    return;
  }

                                                           
  if (tbinfo->relpersistence == RELPERSISTENCE_UNLOGGED && dopt->no_unlogged_table_data)
  {
    return;
  }

                                                      
  if (simple_oid_list_member(&tabledata_exclude_oids, tbinfo->dobj.catId.oid))
  {
    return;
  }

                         
  tdinfo = (TableDataInfo *)pg_malloc(sizeof(TableDataInfo));

  if (tbinfo->relkind == RELKIND_MATVIEW)
  {
    tdinfo->dobj.objType = DO_REFRESH_MATVIEW;
  }
  else if (tbinfo->relkind == RELKIND_SEQUENCE)
  {
    tdinfo->dobj.objType = DO_SEQUENCE_SET;
  }
  else
  {
    tdinfo->dobj.objType = DO_TABLE_DATA;
  }

     
                                                                    
                                                
     
  tdinfo->dobj.catId.tableoid = 0;
  tdinfo->dobj.catId.oid = tbinfo->dobj.catId.oid;
  AssignDumpId(&tdinfo->dobj);
  tdinfo->dobj.name = tbinfo->dobj.name;
  tdinfo->dobj.namespace = tbinfo->dobj.namespace;
  tdinfo->tdtable = tbinfo;
  tdinfo->filtercond = NULL;                          
  addObjectDependency(&tdinfo->dobj, tbinfo->dobj.dumpId);

  tbinfo->dataObj = tdinfo;

                                                                    
  tbinfo->interesting = true;
}

   
                                                                            
                                                        
   
                                                                              
           
   
static void
buildMatViewRefreshDependencies(Archive *fout)
{
  PQExpBuffer query;
  PGresult *res;
  int ntups, i;
  int i_classid, i_objid, i_refobjid;

                                
  if (fout->remoteVersion < 90300)
  {
    return;
  }

  query = createPQExpBuffer();

  appendPQExpBufferStr(query, "WITH RECURSIVE w AS "
                              "( "
                              "SELECT d1.objid, d2.refobjid, c2.relkind AS refrelkind "
                              "FROM pg_depend d1 "
                              "JOIN pg_class c1 ON c1.oid = d1.objid "
                              "AND c1.relkind = " CppAsString2(RELKIND_MATVIEW) " JOIN pg_rewrite r1 ON r1.ev_class = d1.objid "
                                                                                "JOIN pg_depend d2 ON d2.classid = 'pg_rewrite'::regclass "
                                                                                "AND d2.objid = r1.oid "
                                                                                "AND d2.refobjid <> d1.objid "
                                                                                "JOIN pg_class c2 ON c2.oid = d2.refobjid "
                                                                                "AND c2.relkind IN (" CppAsString2(RELKIND_MATVIEW) "," CppAsString2(RELKIND_VIEW) ") "
                                                                                                                                                                   "WHERE d1.classid = 'pg_class'::regclass "
                                                                                                                                                                   "UNION "
                                                                                                                                                                   "SELECT w.objid, d3.refobjid, c3.relkind "
                                                                                                                                                                   "FROM w "
                                                                                                                                                                   "JOIN pg_rewrite r3 ON r3.ev_class = w.refobjid "
                                                                                                                                                                   "JOIN pg_depend d3 ON d3.classid = 'pg_rewrite'::regclass "
                                                                                                                                                                   "AND d3.objid = r3.oid "
                                                                                                                                                                   "AND d3.refobjid <> w.refobjid "
                                                                                                                                                                   "JOIN pg_class c3 ON c3.oid = d3.refobjid "
                                                                                                                                                                   "AND c3.relkind IN (" CppAsString2(RELKIND_MATVIEW) "," CppAsString2(RELKIND_VIEW) ") "
                                                                                                                                                                                                                                                      ") "
                                                                                                                                                                                                                                                      "SELECT 'pg_class'::regclass::oid AS classid, objid, refobjid "
                                                                                                                                                                                                                                                      "FROM w "
                                                                                                                                                                                                                                                      "WHERE refrelkind = " CppAsString2(RELKIND_MATVIEW));

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_classid = PQfnumber(res, "classid");
  i_objid = PQfnumber(res, "objid");
  i_refobjid = PQfnumber(res, "refobjid");

  for (i = 0; i < ntups; i++)
  {
    CatalogId objId;
    CatalogId refobjId;
    DumpableObject *dobj;
    DumpableObject *refdobj;
    TableInfo *tbinfo;
    TableInfo *reftbinfo;

    objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
    objId.oid = atooid(PQgetvalue(res, i, i_objid));
    refobjId.tableoid = objId.tableoid;
    refobjId.oid = atooid(PQgetvalue(res, i, i_refobjid));

    dobj = findObjectByCatalogId(objId);
    if (dobj == NULL)
    {
      continue;
    }

    Assert(dobj->objType == DO_TABLE);
    tbinfo = (TableInfo *)dobj;
    Assert(tbinfo->relkind == RELKIND_MATVIEW);
    dobj = (DumpableObject *)tbinfo->dataObj;
    if (dobj == NULL)
    {
      continue;
    }
    Assert(dobj->objType == DO_REFRESH_MATVIEW);

    refdobj = findObjectByCatalogId(refobjId);
    if (refdobj == NULL)
    {
      continue;
    }

    Assert(refdobj->objType == DO_TABLE);
    reftbinfo = (TableInfo *)refdobj;
    Assert(reftbinfo->relkind == RELKIND_MATVIEW);
    refdobj = (DumpableObject *)reftbinfo->dataObj;
    if (refdobj == NULL)
    {
      continue;
    }
    Assert(refdobj->objType == DO_REFRESH_MATVIEW);

    addObjectDependency(dobj, refdobj->dumpId);

    if (!reftbinfo->relispopulated)
    {
      tbinfo->relispopulated = false;
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);
}

   
                               
                                                                    
   
                                                                           
                                                                         
                                                                       
                                                                        
                                                                          
                                                                           
                                             
   
static void
getTableDataFKConstraints(void)
{
  DumpableObject **dobjs;
  int numObjs;
  int i;

                                                                  
  getDumpableObjects(&dobjs, &numObjs);
  for (i = 0; i < numObjs; i++)
  {
    if (dobjs[i]->objType == DO_FK_CONSTRAINT)
    {
      ConstraintInfo *cinfo = (ConstraintInfo *)dobjs[i];
      TableInfo *ftable;

                                                               
      if (cinfo->contable == NULL || cinfo->contable->dataObj == NULL)
      {
        continue;
      }
      ftable = findTableByOid(cinfo->confrelid);
      if (ftable == NULL || ftable->dataObj == NULL)
      {
        continue;
      }

         
                                                                        
                                               
         
      addObjectDependency(&cinfo->contable->dataObj->dobj, ftable->dataObj->dobj.dumpId);
    }
  }
  free(dobjs);
}

   
                               
                                                                     
                                                                         
                                                                           
                                                                        
                                                                            
                                                                         
                                                       
   
                                                                     
                                           
   
                                                                        
                                                              
   
static void
guessConstraintInheritance(TableInfo *tblinfo, int numTables)
{
  int i, j, k;

  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &(tblinfo[i]);
    int numParents;
    TableInfo **parents;
    TableInfo *parent;

                                                
    if (tbinfo->relkind == RELKIND_SEQUENCE || tbinfo->relkind == RELKIND_VIEW)
    {
      continue;
    }

                                                                       
    if (!(tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
    {
      continue;
    }

    numParents = tbinfo->numParents;
    parents = tbinfo->parents;

    if (numParents == 0)
    {
      continue;                                      
    }

                                              
    for (j = 0; j < tbinfo->ncheck; j++)
    {
      ConstraintInfo *constr;

      constr = &(tbinfo->checkexprs[j]);

      for (k = 0; k < numParents; k++)
      {
        int l;

        parent = parents[k];
        for (l = 0; l < parent->ncheck; l++)
        {
          ConstraintInfo *pconstr = &(parent->checkexprs[l]);

          if (strcmp(pconstr->dobj.name, constr->dobj.name) == 0)
          {
            constr->conislocal = false;
            break;
          }
        }
        if (!constr->conislocal)
        {
          break;
        }
      }
    }
  }
}

   
                 
                                
   
static void
dumpDatabase(Archive *fout)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer dbQry = createPQExpBuffer();
  PQExpBuffer delQry = createPQExpBuffer();
  PQExpBuffer creaQry = createPQExpBuffer();
  PQExpBuffer labelq = createPQExpBuffer();
  PGconn *conn = GetConnection(fout);
  PGresult *res;
  int i_tableoid, i_oid, i_datname, i_dba, i_encoding, i_collate, i_ctype, i_frozenxid, i_minmxid, i_datacl, i_rdatacl, i_datistemplate, i_datconnlimit, i_tablespace;
  CatalogId dbCatId;
  DumpId dbDumpId;
  const char *datname, *dba, *encoding, *collate, *ctype, *datacl, *rdatacl, *datistemplate, *datconnlimit, *tablespace;
  uint32 frozenxid, minmxid;
  char *qdatname;

  pg_log_info("saving database definition");

     
                                                            
     
                                                                         
                                                                             
                                                                         
                                                                          
                                                                          
                                    
     
  if (fout->remoteVersion >= 90600)
  {
    appendPQExpBuffer(dbQry,
        "SELECT tableoid, oid, datname, "
        "(%s datdba) AS dba, "
        "pg_encoding_to_char(encoding) AS encoding, "
        "datcollate, datctype, datfrozenxid, datminmxid, "
        "(SELECT array_agg(acl ORDER BY row_n) FROM "
        "  (SELECT acl, row_n FROM "
        "     unnest(coalesce(datacl,acldefault('d',datdba))) "
        "     WITH ORDINALITY AS perm(acl,row_n) "
        "   WHERE NOT EXISTS ( "
        "     SELECT 1 "
        "     FROM unnest(acldefault('d',datdba)) "
        "       AS init(init_acl) "
        "     WHERE acl = init_acl)) AS datacls) "
        " AS datacl, "
        "(SELECT array_agg(acl ORDER BY row_n) FROM "
        "  (SELECT acl, row_n FROM "
        "     unnest(acldefault('d',datdba)) "
        "     WITH ORDINALITY AS initp(acl,row_n) "
        "   WHERE NOT EXISTS ( "
        "     SELECT 1 "
        "     FROM unnest(coalesce(datacl,acldefault('d',datdba))) "
        "       AS permp(orig_acl) "
        "     WHERE acl = orig_acl)) AS rdatacls) "
        " AS rdatacl, "
        "datistemplate, datconnlimit, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
        "shobj_description(oid, 'pg_database') AS description "

        "FROM pg_database "
        "WHERE datname = current_database()",
        username_subquery);
  }
  else if (fout->remoteVersion >= 90300)
  {
    appendPQExpBuffer(dbQry,
        "SELECT tableoid, oid, datname, "
        "(%s datdba) AS dba, "
        "pg_encoding_to_char(encoding) AS encoding, "
        "datcollate, datctype, datfrozenxid, datminmxid, "
        "datacl, '' as rdatacl, datistemplate, datconnlimit, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
        "shobj_description(oid, 'pg_database') AS description "

        "FROM pg_database "
        "WHERE datname = current_database()",
        username_subquery);
  }
  else if (fout->remoteVersion >= 80400)
  {
    appendPQExpBuffer(dbQry,
        "SELECT tableoid, oid, datname, "
        "(%s datdba) AS dba, "
        "pg_encoding_to_char(encoding) AS encoding, "
        "datcollate, datctype, datfrozenxid, 0 AS datminmxid, "
        "datacl, '' as rdatacl, datistemplate, datconnlimit, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
        "shobj_description(oid, 'pg_database') AS description "

        "FROM pg_database "
        "WHERE datname = current_database()",
        username_subquery);
  }
  else if (fout->remoteVersion >= 80200)
  {
    appendPQExpBuffer(dbQry,
        "SELECT tableoid, oid, datname, "
        "(%s datdba) AS dba, "
        "pg_encoding_to_char(encoding) AS encoding, "
        "NULL AS datcollate, NULL AS datctype, datfrozenxid, 0 AS datminmxid, "
        "datacl, '' as rdatacl, datistemplate, datconnlimit, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
        "shobj_description(oid, 'pg_database') AS description "

        "FROM pg_database "
        "WHERE datname = current_database()",
        username_subquery);
  }
  else
  {
    appendPQExpBuffer(dbQry,
        "SELECT tableoid, oid, datname, "
        "(%s datdba) AS dba, "
        "pg_encoding_to_char(encoding) AS encoding, "
        "NULL AS datcollate, NULL AS datctype, datfrozenxid, 0 AS datminmxid, "
        "datacl, '' as rdatacl, datistemplate, "
        "-1 as datconnlimit, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace "
        "FROM pg_database "
        "WHERE datname = current_database()",
        username_subquery);
  }

  res = ExecuteSqlQueryForSingleRow(fout, dbQry->data);

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_datname = PQfnumber(res, "datname");
  i_dba = PQfnumber(res, "dba");
  i_encoding = PQfnumber(res, "encoding");
  i_collate = PQfnumber(res, "datcollate");
  i_ctype = PQfnumber(res, "datctype");
  i_frozenxid = PQfnumber(res, "datfrozenxid");
  i_minmxid = PQfnumber(res, "datminmxid");
  i_datacl = PQfnumber(res, "datacl");
  i_rdatacl = PQfnumber(res, "rdatacl");
  i_datistemplate = PQfnumber(res, "datistemplate");
  i_datconnlimit = PQfnumber(res, "datconnlimit");
  i_tablespace = PQfnumber(res, "tablespace");

  dbCatId.tableoid = atooid(PQgetvalue(res, 0, i_tableoid));
  dbCatId.oid = atooid(PQgetvalue(res, 0, i_oid));
  datname = PQgetvalue(res, 0, i_datname);
  dba = PQgetvalue(res, 0, i_dba);
  encoding = PQgetvalue(res, 0, i_encoding);
  collate = PQgetvalue(res, 0, i_collate);
  ctype = PQgetvalue(res, 0, i_ctype);
  frozenxid = atooid(PQgetvalue(res, 0, i_frozenxid));
  minmxid = atooid(PQgetvalue(res, 0, i_minmxid));
  datacl = PQgetvalue(res, 0, i_datacl);
  rdatacl = PQgetvalue(res, 0, i_rdatacl);
  datistemplate = PQgetvalue(res, 0, i_datistemplate);
  datconnlimit = PQgetvalue(res, 0, i_datconnlimit);
  tablespace = PQgetvalue(res, 0, i_tablespace);

  qdatname = pg_strdup(fmtId(datname));

     
                                                                             
                                                                             
                                                                            
                                          
     
  appendPQExpBuffer(creaQry, "CREATE DATABASE %s WITH TEMPLATE = template0", qdatname);
  if (strlen(encoding) > 0)
  {
    appendPQExpBufferStr(creaQry, " ENCODING = ");
    appendStringLiteralAH(creaQry, encoding, fout);
  }
  if (strlen(collate) > 0)
  {
    appendPQExpBufferStr(creaQry, " LC_COLLATE = ");
    appendStringLiteralAH(creaQry, collate, fout);
  }
  if (strlen(ctype) > 0)
  {
    appendPQExpBufferStr(creaQry, " LC_CTYPE = ");
    appendStringLiteralAH(creaQry, ctype, fout);
  }

     
                                                                             
                                                                             
                                                                            
                                                                     
                                                                         
                                                          
     
  if (strlen(tablespace) > 0 && strcmp(tablespace, "pg_default") != 0 && !dopt->outputNoTablespaces)
  {
    appendPQExpBuffer(creaQry, " TABLESPACE = %s", fmtId(tablespace));
  }
  appendPQExpBufferStr(creaQry, ";\n");

  appendPQExpBuffer(delQry, "DROP DATABASE %s;\n", qdatname);

  dbDumpId = createDumpId();

  ArchiveEntry(fout, dbCatId,                 
      dbDumpId,                            
      ARCHIVE_OPTS(.tag = datname, .owner = dba, .description = "DATABASE", .section = SECTION_PRE_DATA, .createStmt = creaQry->data, .dropStmt = delQry->data));

                                             
  appendPQExpBuffer(labelq, "DATABASE %s", qdatname);

                              
  if (fout->remoteVersion >= 80200)
  {
       
                                                                           
                                                                          
                                                                        
       
    char *comment = PQgetvalue(res, 0, PQfnumber(res, "description"));

    if (comment && *comment && !dopt->no_comments)
    {
      resetPQExpBuffer(dbQry);

         
                                                                
                   
         
      appendPQExpBuffer(dbQry, "COMMENT ON DATABASE %s IS ", qdatname);
      appendStringLiteralAH(dbQry, comment, fout);
      appendPQExpBufferStr(dbQry, ";\n");

      ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = labelq->data, .owner = dba, .description = "COMMENT", .section = SECTION_NONE, .createStmt = dbQry->data, .deps = &dbDumpId, .nDeps = 1));
    }
  }
  else
  {
    dumpComment(fout, "DATABASE", qdatname, NULL, dba, dbCatId, 0, dbDumpId);
  }

                                          
  if (!dopt->no_security_labels && fout->remoteVersion >= 90200)
  {
    PGresult *shres;
    PQExpBuffer seclabelQry;

    seclabelQry = createPQExpBuffer();

    buildShSecLabelQuery(conn, "pg_database", dbCatId.oid, seclabelQry);
    shres = ExecuteSqlQuery(fout, seclabelQry->data, PGRES_TUPLES_OK);
    resetPQExpBuffer(seclabelQry);
    emitShSecLabels(conn, shres, seclabelQry, "DATABASE", datname);
    if (seclabelQry->len > 0)
    {
      ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = labelq->data, .owner = dba, .description = "SECURITY LABEL", .section = SECTION_NONE, .createStmt = seclabelQry->data, .deps = &dbDumpId, .nDeps = 1));
    }
    destroyPQExpBuffer(seclabelQry);
    PQclear(shres);
  }

     
                                                                      
                                   
     
  dumpACL(fout, dbDumpId, InvalidDumpId, "DATABASE", qdatname, NULL, NULL, dba, datacl, rdatacl, "", "");

     
                                                                      
                                                                      
                                                                         
                                                                            
                                                                          
                                
     
  resetPQExpBuffer(creaQry);
  resetPQExpBuffer(delQry);

  if (strlen(datconnlimit) > 0 && strcmp(datconnlimit, "-1") != 0)
  {
    appendPQExpBuffer(creaQry, "ALTER DATABASE %s CONNECTION LIMIT = %s;\n", qdatname, datconnlimit);
  }

  if (strcmp(datistemplate, "t") == 0)
  {
    appendPQExpBuffer(creaQry, "ALTER DATABASE %s IS_TEMPLATE = true;\n", qdatname);

       
                                                                          
                                                                           
                                                                           
                                                                           
                              
       
    appendPQExpBufferStr(delQry, "UPDATE pg_catalog.pg_database "
                                 "SET datistemplate = false WHERE datname = ");
    appendStringLiteralAH(delQry, datname, fout);
    appendPQExpBufferStr(delQry, ";\n");
  }

                                         
  dumpDatabaseConfig(fout, creaQry, datname, dbCatId.oid);

     
                                                                             
                                             
     
  if (dopt->binary_upgrade)
  {
    appendPQExpBufferStr(creaQry, "\n-- For binary upgrade, set datfrozenxid and datminmxid.\n");
    appendPQExpBuffer(creaQry,
        "UPDATE pg_catalog.pg_database\n"
        "SET datfrozenxid = '%u', datminmxid = '%u'\n"
        "WHERE datname = ",
        frozenxid, minmxid);
    appendStringLiteralAH(creaQry, datname, fout);
    appendPQExpBufferStr(creaQry, ";\n");
  }

  if (creaQry->len > 0)
  {
    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = datname, .owner = dba, .description = "DATABASE PROPERTIES", .section = SECTION_PRE_DATA, .createStmt = creaQry->data, .dropStmt = delQry->data, .deps = &dbDumpId));
  }

     
                                                                 
                                    
     
  if (dopt->binary_upgrade)
  {
    PGresult *lo_res;
    PQExpBuffer loFrozenQry = createPQExpBuffer();
    PQExpBuffer loOutQry = createPQExpBuffer();
    int i_relfrozenxid, i_relminmxid;

       
                      
       
    if (fout->remoteVersion >= 90300)
    {
      appendPQExpBuffer(loFrozenQry,
          "SELECT relfrozenxid, relminmxid\n"
          "FROM pg_catalog.pg_class\n"
          "WHERE oid = %u;\n",
          LargeObjectRelationId);
    }
    else
    {
      appendPQExpBuffer(loFrozenQry,
          "SELECT relfrozenxid, 0 AS relminmxid\n"
          "FROM pg_catalog.pg_class\n"
          "WHERE oid = %u;\n",
          LargeObjectRelationId);
    }

    lo_res = ExecuteSqlQueryForSingleRow(fout, loFrozenQry->data);

    i_relfrozenxid = PQfnumber(lo_res, "relfrozenxid");
    i_relminmxid = PQfnumber(lo_res, "relminmxid");

    appendPQExpBufferStr(loOutQry, "\n-- For binary upgrade, set pg_largeobject relfrozenxid and relminmxid\n");
    appendPQExpBuffer(loOutQry,
        "UPDATE pg_catalog.pg_class\n"
        "SET relfrozenxid = '%u', relminmxid = '%u'\n"
        "WHERE oid = %u;\n",
        atooid(PQgetvalue(lo_res, 0, i_relfrozenxid)), atooid(PQgetvalue(lo_res, 0, i_relminmxid)), LargeObjectRelationId);
    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = "pg_largeobject", .description = "pg_largeobject", .section = SECTION_PRE_DATA, .createStmt = loOutQry->data));

    PQclear(lo_res);

    destroyPQExpBuffer(loFrozenQry);
    destroyPQExpBuffer(loOutQry);
  }

  PQclear(res);

  free(qdatname);
  destroyPQExpBuffer(dbQry);
  destroyPQExpBuffer(delQry);
  destroyPQExpBuffer(creaQry);
  destroyPQExpBuffer(labelq);
}

   
                                                                           
                                                 
   
static void
dumpDatabaseConfig(Archive *AH, PQExpBuffer outbuf, const char *dbname, Oid dboid)
{
  PGconn *conn = GetConnection(AH);
  PQExpBuffer buf = createPQExpBuffer();
  PGresult *res;
  int count = 1;

     
                                                                            
                                                                          
     
  for (;;)
  {
    if (AH->remoteVersion >= 90000)
    {
      printfPQExpBuffer(buf,
          "SELECT setconfig[%d] FROM pg_db_role_setting "
          "WHERE setrole = 0 AND setdatabase = '%u'::oid",
          count, dboid);
    }
    else
    {
      printfPQExpBuffer(buf, "SELECT datconfig[%d] FROM pg_database WHERE oid = '%u'::oid", count, dboid);
    }

    res = ExecuteSqlQuery(AH, buf->data, PGRES_TUPLES_OK);

    if (PQntuples(res) == 1 && !PQgetisnull(res, 0, 0))
    {
      makeAlterConfigCommand(conn, PQgetvalue(res, 0, 0), "DATABASE", dbname, NULL, NULL, outbuf);
      PQclear(res);
      count++;
    }
    else
    {
      PQclear(res);
      break;
    }
  }

                                                       
  if (AH->remoteVersion >= 90000)
  {
                                             
    printfPQExpBuffer(buf,
        "SELECT rolname, unnest(setconfig) "
        "FROM pg_db_role_setting s, pg_roles r "
        "WHERE setrole = r.oid AND setdatabase = '%u'::oid",
        dboid);

    res = ExecuteSqlQuery(AH, buf->data, PGRES_TUPLES_OK);

    if (PQntuples(res) > 0)
    {
      int i;

      for (i = 0; i < PQntuples(res); i++)
      {
        makeAlterConfigCommand(conn, PQgetvalue(res, i, 1), "ROLE", PQgetvalue(res, i, 0), "DATABASE", dbname, outbuf);
      }
    }

    PQclear(res);
  }

  destroyPQExpBuffer(buf);
}

   
                                                           
   
static void
dumpEncoding(Archive *AH)
{
  const char *encname = pg_encoding_to_char(AH->encoding);
  PQExpBuffer qry = createPQExpBuffer();

  pg_log_info("saving encoding = %s", encname);

  appendPQExpBufferStr(qry, "SET client_encoding = ");
  appendStringLiteralAH(qry, encname, AH);
  appendPQExpBufferStr(qry, ";\n");

  ArchiveEntry(AH, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = "ENCODING", .description = "ENCODING", .section = SECTION_PRE_DATA, .createStmt = qry->data));

  destroyPQExpBuffer(qry);
}

   
                                                                           
   
static void
dumpStdStrings(Archive *AH)
{
  const char *stdstrings = AH->std_strings ? "on" : "off";
  PQExpBuffer qry = createPQExpBuffer();

  pg_log_info("saving standard_conforming_strings = %s", stdstrings);

  appendPQExpBuffer(qry, "SET standard_conforming_strings = '%s';\n", stdstrings);

  ArchiveEntry(AH, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = "STDSTRINGS", .description = "STDSTRINGS", .section = SECTION_PRE_DATA, .createStmt = qry->data));

  destroyPQExpBuffer(qry);
}

   
                                                                
   
static void
dumpSearchPath(Archive *AH)
{
  PQExpBuffer qry = createPQExpBuffer();
  PQExpBuffer path = createPQExpBuffer();
  PGresult *res;
  char **schemanames = NULL;
  int nschemanames = 0;
  int i;

     
                                                                      
                                                                       
                                                                            
                                                                            
                                           
     
  res = ExecuteSqlQueryForSingleRow(AH, "SELECT pg_catalog.current_schemas(false)");

  if (!parsePGArray(PQgetvalue(res, 0, 0), &schemanames, &nschemanames))
  {
    fatal("could not parse result of current_schemas()");
  }

     
                                                                          
                                                                            
                                                                            
                                                                          
     
  for (i = 0; i < nschemanames; i++)
  {
    if (i > 0)
    {
      appendPQExpBufferStr(path, ", ");
    }
    appendPQExpBufferStr(path, fmtId(schemanames[i]));
  }

  appendPQExpBufferStr(qry, "SELECT pg_catalog.set_config('search_path', ");
  appendStringLiteralAH(qry, path->data, AH);
  appendPQExpBufferStr(qry, ", false);\n");

  pg_log_info("saving search_path = %s", path->data);

  ArchiveEntry(AH, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = "SEARCHPATH", .description = "SEARCHPATH", .section = SECTION_PRE_DATA, .createStmt = qry->data));

                                                                           
  AH->searchpath = pg_strdup(qry->data);

  if (schemanames)
  {
    free(schemanames);
  }
  PQclear(res);
  destroyPQExpBuffer(qry);
  destroyPQExpBuffer(path);
}

   
             
                                                 
   
static void
getBlobs(Archive *fout)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer blobQry = createPQExpBuffer();
  BlobInfo *binfo;
  DumpableObject *bdata;
  PGresult *res;
  int ntups;
  int i;
  int i_oid;
  int i_lomowner;
  int i_lomacl;
  int i_rlomacl;
  int i_initlomacl;
  int i_initrlomacl;

  pg_log_info("reading large objects");

                                                     
  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer init_acl_subquery = createPQExpBuffer();
    PQExpBuffer init_racl_subquery = createPQExpBuffer();

    buildACLQueries(acl_subquery, racl_subquery, init_acl_subquery, init_racl_subquery, "l.lomacl", "l.lomowner", "'L'", dopt->binary_upgrade);

    appendPQExpBuffer(blobQry,
        "SELECT l.oid, (%s l.lomowner) AS rolname, "
        "%s AS lomacl, "
        "%s AS rlomacl, "
        "%s AS initlomacl, "
        "%s AS initrlomacl "
        "FROM pg_largeobject_metadata l "
        "LEFT JOIN pg_init_privs pip ON "
        "(l.oid = pip.objoid "
        "AND pip.classoid = 'pg_largeobject'::regclass "
        "AND pip.objsubid = 0) ",
        username_subquery, acl_subquery->data, racl_subquery->data, init_acl_subquery->data, init_racl_subquery->data);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(init_acl_subquery);
    destroyPQExpBuffer(init_racl_subquery);
  }
  else if (fout->remoteVersion >= 90000)
  {
    appendPQExpBuffer(blobQry,
        "SELECT oid, (%s lomowner) AS rolname, lomacl, "
        "NULL AS rlomacl, NULL AS initlomacl, "
        "NULL AS initrlomacl "
        " FROM pg_largeobject_metadata",
        username_subquery);
  }
  else
  {
    appendPQExpBufferStr(blobQry, "SELECT DISTINCT loid AS oid, "
                                  "NULL::name AS rolname, NULL::oid AS lomacl, "
                                  "NULL::oid AS rlomacl, NULL::oid AS initlomacl, "
                                  "NULL::oid AS initrlomacl "
                                  " FROM pg_largeobject");
  }

  res = ExecuteSqlQuery(fout, blobQry->data, PGRES_TUPLES_OK);

  i_oid = PQfnumber(res, "oid");
  i_lomowner = PQfnumber(res, "rolname");
  i_lomacl = PQfnumber(res, "lomacl");
  i_rlomacl = PQfnumber(res, "rlomacl");
  i_initlomacl = PQfnumber(res, "initlomacl");
  i_initrlomacl = PQfnumber(res, "initrlomacl");

  ntups = PQntuples(res);

     
                                                       
     
  binfo = (BlobInfo *)pg_malloc(ntups * sizeof(BlobInfo));

  for (i = 0; i < ntups; i++)
  {
    binfo[i].dobj.objType = DO_BLOB;
    binfo[i].dobj.catId.tableoid = LargeObjectRelationId;
    binfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&binfo[i].dobj);

    binfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_oid));
    binfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_lomowner));
    binfo[i].blobacl = pg_strdup(PQgetvalue(res, i, i_lomacl));
    binfo[i].rblobacl = pg_strdup(PQgetvalue(res, i, i_rlomacl));
    binfo[i].initblobacl = pg_strdup(PQgetvalue(res, i, i_initlomacl));
    binfo[i].initrblobacl = pg_strdup(PQgetvalue(res, i, i_initrlomacl));

    if (PQgetisnull(res, i, i_lomacl) && PQgetisnull(res, i, i_rlomacl) && PQgetisnull(res, i, i_initlomacl) && PQgetisnull(res, i, i_initrlomacl))
    {
      binfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }

       
                                                                       
                                                                         
                                                                       
                                                               
                                                            
       
    if (dopt->binary_upgrade)
    {
      binfo[i].dobj.dump &= ~DUMP_COMPONENT_DATA;
    }
  }

     
                                                                           
                                                                
     
  if (ntups > 0)
  {
    bdata = (DumpableObject *)pg_malloc(sizeof(DumpableObject));
    bdata->objType = DO_BLOB_DATA;
    bdata->catId = nilCatalogId;
    AssignDumpId(bdata);
    bdata->name = pg_strdup("BLOBS");
  }

  PQclear(res);
  destroyPQExpBuffer(blobQry);
}

   
            
   
                                                            
   
static void
dumpBlob(Archive *fout, BlobInfo *binfo)
{
  PQExpBuffer cquery = createPQExpBuffer();
  PQExpBuffer dquery = createPQExpBuffer();

  appendPQExpBuffer(cquery, "SELECT pg_catalog.lo_create('%s');\n", binfo->dobj.name);

  appendPQExpBuffer(dquery, "SELECT pg_catalog.lo_unlink('%s');\n", binfo->dobj.name);

  if (binfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, binfo->dobj.catId, binfo->dobj.dumpId, ARCHIVE_OPTS(.tag = binfo->dobj.name, .owner = binfo->rolname, .description = "BLOB", .section = SECTION_PRE_DATA, .createStmt = cquery->data, .dropStmt = dquery->data));
  }

                           
  if (binfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "LARGE OBJECT", binfo->dobj.name, NULL, binfo->rolname, binfo->dobj.catId, 0, binfo->dobj.dumpId);
  }

                                  
  if (binfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "LARGE OBJECT", binfo->dobj.name, NULL, binfo->rolname, binfo->dobj.catId, 0, binfo->dobj.dumpId);
  }

                       
  if (binfo->blobacl && (binfo->dobj.dump & DUMP_COMPONENT_ACL))
  {
    dumpACL(fout, binfo->dobj.dumpId, InvalidDumpId, "LARGE OBJECT", binfo->dobj.name, NULL, NULL, binfo->rolname, binfo->blobacl, binfo->rblobacl, binfo->initblobacl, binfo->initrblobacl);
  }

  destroyPQExpBuffer(cquery);
  destroyPQExpBuffer(dquery);
}

   
              
                                               
   
static int
dumpBlobs(Archive *fout, void *arg)
{
  const char *blobQry;
  const char *blobFetchQry;
  PGconn *conn = GetConnection(fout);
  PGresult *res;
  char buf[LOBBUFSIZE];
  int ntups;
  int i;
  int cnt;

  pg_log_info("saving large objects");

     
                                                                             
                                                       
     
  if (fout->remoteVersion >= 90000)
  {
    blobQry = "DECLARE bloboid CURSOR FOR "
              "SELECT oid FROM pg_largeobject_metadata ORDER BY 1";
  }
  else
  {
    blobQry = "DECLARE bloboid CURSOR FOR "
              "SELECT DISTINCT loid FROM pg_largeobject ORDER BY 1";
  }

  ExecuteSqlStatement(fout, blobQry);

                                    
  blobFetchQry = "FETCH 1000 IN bloboid";

  do
  {
                    
    res = ExecuteSqlQuery(fout, blobFetchQry, PGRES_TUPLES_OK);

                                    
    ntups = PQntuples(res);
    for (i = 0; i < ntups; i++)
    {
      Oid blobOid;
      int loFd;

      blobOid = atooid(PQgetvalue(res, i, 0));
                         
      loFd = lo_open(conn, blobOid, INV_READ);
      if (loFd == -1)
      {
        fatal("could not open large object %u: %s", blobOid, PQerrorMessage(conn));
      }

      StartBlob(fout, blobOid);

                                                          
      do
      {
        cnt = lo_read(conn, loFd, buf, LOBBUFSIZE);
        if (cnt < 0)
        {
          fatal("error reading large object %u: %s", blobOid, PQerrorMessage(conn));
        }

        WriteData(fout, buf, cnt);
      } while (cnt > 0);

      lo_close(conn, loFd);

      EndBlob(fout, blobOid);
    }

    PQclear(res);
  } while (ntups > 0);

  return 1;
}

   
               
                                                                
   
void
getPolicies(Archive *fout, TableInfo tblinfo[], int numTables)
{
  PQExpBuffer query;
  PQExpBuffer tbloids;
  PGresult *res;
  PolicyInfo *polinfo;
  int i_oid;
  int i_tableoid;
  int i_polrelid;
  int i_polname;
  int i_polcmd;
  int i_polpermissive;
  int i_polroles;
  int i_polqual;
  int i_polwithcheck;
  int i, j, ntups;

                              
  if (fout->remoteVersion < 90500)
  {
    return;
  }

  query = createPQExpBuffer();
  tbloids = createPQExpBuffer();

     
                                                                         
     
  appendPQExpBufferChar(tbloids, '{');
  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &tblinfo[i];

                                                        
    if (!(tbinfo->dobj.dump & DUMP_COMPONENT_POLICY))
    {
      continue;
    }

                                                           
    if (tbinfo->relkind != RELKIND_RELATION && tbinfo->relkind != RELKIND_PARTITIONED_TABLE)
    {
      continue;
    }

                                                             
    if (tbloids->len > 1)                                    
    {
      appendPQExpBufferChar(tbloids, ',');
    }
    appendPQExpBuffer(tbloids, "%u", tbinfo->dobj.catId.oid);

                                                                         
    if (tbinfo->rowsec)
    {
         
                                                                 
                                              
         
                                                                        
                                                    
         
      polinfo = pg_malloc(sizeof(PolicyInfo));
      polinfo->dobj.objType = DO_POLICY;
      polinfo->dobj.catId.tableoid = 0;
      polinfo->dobj.catId.oid = tbinfo->dobj.catId.oid;
      AssignDumpId(&polinfo->dobj);
      polinfo->dobj.namespace = tbinfo->dobj.namespace;
      polinfo->dobj.name = pg_strdup(tbinfo->dobj.name);
      polinfo->poltable = tbinfo;
      polinfo->polname = NULL;
      polinfo->polcmd = '\0';
      polinfo->polpermissive = 0;
      polinfo->polroles = NULL;
      polinfo->polqual = NULL;
      polinfo->polwithcheck = NULL;
    }
  }
  appendPQExpBufferChar(tbloids, '}');

     
                                                                         
                                                                        
                                                                            
                                       
     
  pg_log_info("reading row-level security policies");

  printfPQExpBuffer(query, "SELECT pol.oid, pol.tableoid, pol.polrelid, pol.polname, pol.polcmd, ");
  if (fout->remoteVersion >= 100000)
  {
    appendPQExpBuffer(query, "pol.polpermissive, ");
  }
  else
  {
    appendPQExpBuffer(query, "'t' as polpermissive, ");
  }
  appendPQExpBuffer(query,
      "CASE WHEN pol.polroles = '{0}' THEN NULL ELSE "
      "   pg_catalog.array_to_string(ARRAY(SELECT pg_catalog.quote_ident(rolname) from pg_catalog.pg_roles WHERE oid = ANY(pol.polroles)), ', ') END AS polroles, "
      "pg_catalog.pg_get_expr(pol.polqual, pol.polrelid) AS polqual, "
      "pg_catalog.pg_get_expr(pol.polwithcheck, pol.polrelid) AS polwithcheck "
      "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
      "JOIN pg_catalog.pg_policy pol ON (src.tbloid = pol.polrelid)",
      tbloids->data);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  if (ntups > 0)
  {
    i_oid = PQfnumber(res, "oid");
    i_tableoid = PQfnumber(res, "tableoid");
    i_polrelid = PQfnumber(res, "polrelid");
    i_polname = PQfnumber(res, "polname");
    i_polcmd = PQfnumber(res, "polcmd");
    i_polpermissive = PQfnumber(res, "polpermissive");
    i_polroles = PQfnumber(res, "polroles");
    i_polqual = PQfnumber(res, "polqual");
    i_polwithcheck = PQfnumber(res, "polwithcheck");

    polinfo = pg_malloc(ntups * sizeof(PolicyInfo));

    for (j = 0; j < ntups; j++)
    {
      Oid polrelid = atooid(PQgetvalue(res, j, i_polrelid));
      TableInfo *tbinfo = findTableByOid(polrelid);

      polinfo[j].dobj.objType = DO_POLICY;
      polinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
      polinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
      AssignDumpId(&polinfo[j].dobj);
      polinfo[j].dobj.namespace = tbinfo->dobj.namespace;
      polinfo[j].poltable = tbinfo;
      polinfo[j].polname = pg_strdup(PQgetvalue(res, j, i_polname));
      polinfo[j].dobj.name = pg_strdup(polinfo[j].polname);

      polinfo[j].polcmd = *(PQgetvalue(res, j, i_polcmd));
      polinfo[j].polpermissive = *(PQgetvalue(res, j, i_polpermissive)) == 't';

      if (PQgetisnull(res, j, i_polroles))
      {
        polinfo[j].polroles = NULL;
      }
      else
      {
        polinfo[j].polroles = pg_strdup(PQgetvalue(res, j, i_polroles));
      }

      if (PQgetisnull(res, j, i_polqual))
      {
        polinfo[j].polqual = NULL;
      }
      else
      {
        polinfo[j].polqual = pg_strdup(PQgetvalue(res, j, i_polqual));
      }

      if (PQgetisnull(res, j, i_polwithcheck))
      {
        polinfo[j].polwithcheck = NULL;
      }
      else
      {
        polinfo[j].polwithcheck = pg_strdup(PQgetvalue(res, j, i_polwithcheck));
      }
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(tbloids);
}

   
              
                                             
   
static void
dumpPolicy(Archive *fout, PolicyInfo *polinfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = polinfo->poltable;
  PQExpBuffer query;
  PQExpBuffer delqry;
  PQExpBuffer polprefix;
  char *qtabname;
  const char *cmd;
  char *tag;

  if (dopt->dataOnly)
  {
    return;
  }

     
                                                                            
                                                                           
                         
     
  if (polinfo->polname == NULL)
  {
    query = createPQExpBuffer();

    appendPQExpBuffer(query, "ALTER TABLE %s ENABLE ROW LEVEL SECURITY;", fmtQualifiedDumpable(tbinfo));

       
                                                                      
                                                                           
                                               
       
    if (polinfo->dobj.dump & DUMP_COMPONENT_POLICY)
    {
      ArchiveEntry(fout, polinfo->dobj.catId, polinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = polinfo->dobj.name, .namespace = polinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "ROW SECURITY", .section = SECTION_POST_DATA, .createStmt = query->data, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1));
    }

    destroyPQExpBuffer(query);
    return;
  }

  if (polinfo->polcmd == '*')
  {
    cmd = "";
  }
  else if (polinfo->polcmd == 'r')
  {
    cmd = " FOR SELECT";
  }
  else if (polinfo->polcmd == 'a')
  {
    cmd = " FOR INSERT";
  }
  else if (polinfo->polcmd == 'w')
  {
    cmd = " FOR UPDATE";
  }
  else if (polinfo->polcmd == 'd')
  {
    cmd = " FOR DELETE";
  }
  else
  {
    pg_log_error("unexpected policy command type: %c", polinfo->polcmd);
    exit_nicely(1);
  }

  query = createPQExpBuffer();
  delqry = createPQExpBuffer();
  polprefix = createPQExpBuffer();

  qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

  appendPQExpBuffer(query, "CREATE POLICY %s", fmtId(polinfo->polname));

  appendPQExpBuffer(query, " ON %s%s%s", fmtQualifiedDumpable(tbinfo), !polinfo->polpermissive ? " AS RESTRICTIVE" : "", cmd);

  if (polinfo->polroles != NULL)
  {
    appendPQExpBuffer(query, " TO %s", polinfo->polroles);
  }

  if (polinfo->polqual != NULL)
  {
    appendPQExpBuffer(query, " USING (%s)", polinfo->polqual);
  }

  if (polinfo->polwithcheck != NULL)
  {
    appendPQExpBuffer(query, " WITH CHECK (%s)", polinfo->polwithcheck);
  }

  appendPQExpBuffer(query, ";\n");

  appendPQExpBuffer(delqry, "DROP POLICY %s", fmtId(polinfo->polname));
  appendPQExpBuffer(delqry, " ON %s;\n", fmtQualifiedDumpable(tbinfo));

  appendPQExpBuffer(polprefix, "POLICY %s ON", fmtId(polinfo->polname));

  tag = psprintf("%s %s", tbinfo->dobj.name, polinfo->dobj.name);

  if (polinfo->dobj.dump & DUMP_COMPONENT_POLICY)
  {
    ArchiveEntry(fout, polinfo->dobj.catId, polinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = polinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "POLICY", .section = SECTION_POST_DATA, .createStmt = query->data, .dropStmt = delqry->data));
  }

  if (polinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, polprefix->data, qtabname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, polinfo->dobj.catId, 0, polinfo->dobj.dumpId);
  }

  free(tag);
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(delqry);
  destroyPQExpBuffer(polprefix);
  free(qtabname);
}

   
                   
                                        
   
PublicationInfo *
getPublications(Archive *fout, int *numPublications)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PGresult *res;
  PublicationInfo *pubinfo;
  int i_tableoid;
  int i_oid;
  int i_pubname;
  int i_rolname;
  int i_puballtables;
  int i_pubinsert;
  int i_pubupdate;
  int i_pubdelete;
  int i_pubtruncate;
  int i, ntups;

  if (dopt->no_publications || fout->remoteVersion < 100000)
  {
    *numPublications = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  resetPQExpBuffer(query);

                             
  if (fout->remoteVersion >= 110000)
  {
    appendPQExpBuffer(query,
        "SELECT p.tableoid, p.oid, p.pubname, "
        "(%s p.pubowner) AS rolname, "
        "p.puballtables, p.pubinsert, p.pubupdate, p.pubdelete, p.pubtruncate "
        "FROM pg_publication p",
        username_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT p.tableoid, p.oid, p.pubname, "
        "(%s p.pubowner) AS rolname, "
        "p.puballtables, p.pubinsert, p.pubupdate, p.pubdelete, false AS pubtruncate "
        "FROM pg_publication p",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_pubname = PQfnumber(res, "pubname");
  i_rolname = PQfnumber(res, "rolname");
  i_puballtables = PQfnumber(res, "puballtables");
  i_pubinsert = PQfnumber(res, "pubinsert");
  i_pubupdate = PQfnumber(res, "pubupdate");
  i_pubdelete = PQfnumber(res, "pubdelete");
  i_pubtruncate = PQfnumber(res, "pubtruncate");

  pubinfo = pg_malloc(ntups * sizeof(PublicationInfo));

  for (i = 0; i < ntups; i++)
  {
    pubinfo[i].dobj.objType = DO_PUBLICATION;
    pubinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    pubinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&pubinfo[i].dobj);
    pubinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_pubname));
    pubinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    pubinfo[i].puballtables = (strcmp(PQgetvalue(res, i, i_puballtables), "t") == 0);
    pubinfo[i].pubinsert = (strcmp(PQgetvalue(res, i, i_pubinsert), "t") == 0);
    pubinfo[i].pubupdate = (strcmp(PQgetvalue(res, i, i_pubupdate), "t") == 0);
    pubinfo[i].pubdelete = (strcmp(PQgetvalue(res, i, i_pubdelete), "t") == 0);
    pubinfo[i].pubtruncate = (strcmp(PQgetvalue(res, i, i_pubtruncate), "t") == 0);

    if (strlen(pubinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of publication \"%s\" appears to be invalid", pubinfo[i].dobj.name);
    }

                                           
    selectDumpableObject(&(pubinfo[i].dobj), fout);
  }
  PQclear(res);

  destroyPQExpBuffer(query);

  *numPublications = ntups;
  return pubinfo;
}

   
                   
                                                  
   
static void
dumpPublication(Archive *fout, PublicationInfo *pubinfo)
{
  PQExpBuffer delq;
  PQExpBuffer query;
  char *qpubname;
  bool first = true;

  if (!(pubinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
  {
    return;
  }

  delq = createPQExpBuffer();
  query = createPQExpBuffer();

  qpubname = pg_strdup(fmtId(pubinfo->dobj.name));

  appendPQExpBuffer(delq, "DROP PUBLICATION %s;\n", qpubname);

  appendPQExpBuffer(query, "CREATE PUBLICATION %s", qpubname);

  if (pubinfo->puballtables)
  {
    appendPQExpBufferStr(query, " FOR ALL TABLES");
  }

  appendPQExpBufferStr(query, " WITH (publish = '");
  if (pubinfo->pubinsert)
  {
    appendPQExpBufferStr(query, "insert");
    first = false;
  }

  if (pubinfo->pubupdate)
  {
    if (!first)
    {
      appendPQExpBufferStr(query, ", ");
    }

    appendPQExpBufferStr(query, "update");
    first = false;
  }

  if (pubinfo->pubdelete)
  {
    if (!first)
    {
      appendPQExpBufferStr(query, ", ");
    }

    appendPQExpBufferStr(query, "delete");
    first = false;
  }

  if (pubinfo->pubtruncate)
  {
    if (!first)
    {
      appendPQExpBufferStr(query, ", ");
    }

    appendPQExpBufferStr(query, "truncate");
    first = false;
  }

  appendPQExpBufferStr(query, "');\n");

  ArchiveEntry(fout, pubinfo->dobj.catId, pubinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = pubinfo->dobj.name, .owner = pubinfo->rolname, .description = "PUBLICATION", .section = SECTION_POST_DATA, .createStmt = query->data, .dropStmt = delq->data));

  if (pubinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "PUBLICATION", qpubname, NULL, pubinfo->rolname, pubinfo->dobj.catId, 0, pubinfo->dobj.dumpId);
  }

  if (pubinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "PUBLICATION", qpubname, NULL, pubinfo->rolname, pubinfo->dobj.catId, 0, pubinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qpubname);
}

   
                        
                                                                       
   
void
getPublicationTables(Archive *fout, TableInfo tblinfo[], int numTables)
{
  PQExpBuffer query;
  PGresult *res;
  PublicationRelInfo *pubrinfo;
  DumpOptions *dopt = fout->dopt;
  int i_tableoid;
  int i_oid;
  int i_prpubid;
  int i_prrelid;
  int i, j, ntups;

  if (dopt->no_publications || fout->remoteVersion < 100000)
  {
    return;
  }

  query = createPQExpBuffer();

                                                
  appendPQExpBufferStr(query, "SELECT tableoid, oid, prpubid, prrelid "
                              "FROM pg_catalog.pg_publication_rel");
  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_prpubid = PQfnumber(res, "prpubid");
  i_prrelid = PQfnumber(res, "prrelid");

                                                
  pubrinfo = pg_malloc(ntups * sizeof(PublicationRelInfo));
  j = 0;

  for (i = 0; i < ntups; i++)
  {
    Oid prpubid = atooid(PQgetvalue(res, i, i_prpubid));
    Oid prrelid = atooid(PQgetvalue(res, i, i_prrelid));
    PublicationInfo *pubinfo;
    TableInfo *tbinfo;

       
                                                                       
                               
       
    pubinfo = findPublicationByOid(prpubid);
    if (pubinfo == NULL)
    {
      continue;
    }
    tbinfo = findTableByOid(prrelid);
    if (tbinfo == NULL)
    {
      continue;
    }

       
                                                                         
                     
       
    if (!(tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
    {
      continue;
    }

                                                         
    pubrinfo[j].dobj.objType = DO_PUBLICATION_REL;
    pubrinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    pubrinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&pubrinfo[j].dobj);
    pubrinfo[j].dobj.namespace = tbinfo->dobj.namespace;
    pubrinfo[j].dobj.name = tbinfo->dobj.name;
    pubrinfo[j].publication = pubinfo;
    pubrinfo[j].pubtable = tbinfo;

                                           
    selectDumpablePublicationTable(&(pubrinfo[j].dobj), fout);

    j++;
  }

  PQclear(res);
  destroyPQExpBuffer(query);
}

   
                        
                                                                
   
static void
dumpPublicationTable(Archive *fout, PublicationRelInfo *pubrinfo)
{
  PublicationInfo *pubinfo = pubrinfo->publication;
  TableInfo *tbinfo = pubrinfo->pubtable;
  PQExpBuffer query;
  char *tag;

  if (!(pubrinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
  {
    return;
  }

  tag = psprintf("%s %s", pubinfo->dobj.name, tbinfo->dobj.name);

  query = createPQExpBuffer();

  appendPQExpBuffer(query, "ALTER PUBLICATION %s ADD TABLE ONLY", fmtId(pubinfo->dobj.name));
  appendPQExpBuffer(query, " %s;\n", fmtQualifiedDumpable(tbinfo));

     
                                                                             
                                                                      
                                                                         
                                                                         
                           
     
  ArchiveEntry(fout, pubrinfo->dobj.catId, pubrinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = pubinfo->rolname, .description = "PUBLICATION TABLE", .section = SECTION_POST_DATA, .createStmt = query->data));

  free(tag);
  destroyPQExpBuffer(query);
}

   
                                                
   
static bool
is_superuser(Archive *fout)
{
  ArchiveHandle *AH = (ArchiveHandle *)fout;
  const char *val;

  val = PQparameterStatus(AH->connection, "is_superuser");

  if (val && strcmp(val, "on") == 0)
  {
    return true;
  }

  return false;
}

   
                    
                                         
   
void
getSubscriptions(Archive *fout)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PGresult *res;
  SubscriptionInfo *subinfo;
  int i_tableoid;
  int i_oid;
  int i_subname;
  int i_rolname;
  int i_subconninfo;
  int i_subslotname;
  int i_subsynccommit;
  int i_subpublications;
  int i, ntups;

  if (dopt->no_subscriptions || fout->remoteVersion < 100000)
  {
    return;
  }

  if (!is_superuser(fout))
  {
    int n;

    res = ExecuteSqlQuery(fout,
        "SELECT count(*) FROM pg_subscription "
        "WHERE subdbid = (SELECT oid FROM pg_database"
        "                 WHERE datname = current_database())",
        PGRES_TUPLES_OK);
    n = atoi(PQgetvalue(res, 0, 0));
    if (n > 0)
    {
      pg_log_warning("subscriptions not dumped because current user is not a superuser");
    }
    PQclear(res);
    return;
  }

  query = createPQExpBuffer();

  resetPQExpBuffer(query);

                                                  
  appendPQExpBuffer(query,
      "SELECT s.tableoid, s.oid, s.subname,"
      "(%s s.subowner) AS rolname, "
      " s.subconninfo, s.subslotname, s.subsynccommit, "
      " s.subpublications "
      "FROM pg_subscription s "
      "WHERE s.subdbid = (SELECT oid FROM pg_database"
      "                   WHERE datname = current_database())",
      username_subquery);
  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_subname = PQfnumber(res, "subname");
  i_rolname = PQfnumber(res, "rolname");
  i_subconninfo = PQfnumber(res, "subconninfo");
  i_subslotname = PQfnumber(res, "subslotname");
  i_subsynccommit = PQfnumber(res, "subsynccommit");
  i_subpublications = PQfnumber(res, "subpublications");

  subinfo = pg_malloc(ntups * sizeof(SubscriptionInfo));

  for (i = 0; i < ntups; i++)
  {
    subinfo[i].dobj.objType = DO_SUBSCRIPTION;
    subinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    subinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&subinfo[i].dobj);
    subinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_subname));
    subinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    subinfo[i].subconninfo = pg_strdup(PQgetvalue(res, i, i_subconninfo));
    if (PQgetisnull(res, i, i_subslotname))
    {
      subinfo[i].subslotname = NULL;
    }
    else
    {
      subinfo[i].subslotname = pg_strdup(PQgetvalue(res, i, i_subslotname));
    }
    subinfo[i].subsynccommit = pg_strdup(PQgetvalue(res, i, i_subsynccommit));
    subinfo[i].subpublications = pg_strdup(PQgetvalue(res, i, i_subpublications));

    if (strlen(subinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of subscription \"%s\" appears to be invalid", subinfo[i].dobj.name);
    }

                                           
    selectDumpableObject(&(subinfo[i].dobj), fout);
  }
  PQclear(res);

  destroyPQExpBuffer(query);
}

   
                    
                                                   
   
static void
dumpSubscription(Archive *fout, SubscriptionInfo *subinfo)
{
  PQExpBuffer delq;
  PQExpBuffer query;
  PQExpBuffer publications;
  char *qsubname;
  char **pubnames = NULL;
  int npubnames = 0;
  int i;

  if (!(subinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
  {
    return;
  }

  delq = createPQExpBuffer();
  query = createPQExpBuffer();

  qsubname = pg_strdup(fmtId(subinfo->dobj.name));

  appendPQExpBuffer(delq, "DROP SUBSCRIPTION %s;\n", qsubname);

  appendPQExpBuffer(query, "CREATE SUBSCRIPTION %s CONNECTION ", qsubname);
  appendStringLiteralAH(query, subinfo->subconninfo, fout);

                                                                   
  if (!parsePGArray(subinfo->subpublications, &pubnames, &npubnames))
  {
    pg_log_warning("could not parse subpublications array");
    if (pubnames)
    {
      free(pubnames);
    }
    pubnames = NULL;
    npubnames = 0;
  }

  publications = createPQExpBuffer();
  for (i = 0; i < npubnames; i++)
  {
    if (i > 0)
    {
      appendPQExpBufferStr(publications, ", ");
    }

    appendPQExpBufferStr(publications, fmtId(pubnames[i]));
  }

  appendPQExpBuffer(query, " PUBLICATION %s WITH (connect = false, slot_name = ", publications->data);
  if (subinfo->subslotname)
  {
    appendStringLiteralAH(query, subinfo->subslotname, fout);
  }
  else
  {
    appendPQExpBufferStr(query, "NONE");
  }

  if (strcmp(subinfo->subsynccommit, "off") != 0)
  {
    appendPQExpBuffer(query, ", synchronous_commit = %s", fmtId(subinfo->subsynccommit));
  }

  appendPQExpBufferStr(query, ");\n");

  ArchiveEntry(fout, subinfo->dobj.catId, subinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = subinfo->dobj.name, .owner = subinfo->rolname, .description = "SUBSCRIPTION", .section = SECTION_POST_DATA, .createStmt = query->data, .dropStmt = delq->data));

  if (subinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "SUBSCRIPTION", qsubname, NULL, subinfo->rolname, subinfo->dobj.catId, 0, subinfo->dobj.dumpId);
  }

  if (subinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "SUBSCRIPTION", qsubname, NULL, subinfo->rolname, subinfo->dobj.catId, 0, subinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(publications);
  if (pubnames)
  {
    free(pubnames);
  }

  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qsubname);
}

   
                                                                            
                     
   
static void
append_depends_on_extension(Archive *fout, PQExpBuffer create, DumpableObject *dobj, const char *catalog, const char *keyword, const char *objname)
{
  if (dobj->depends_on_ext)
  {
    char *nm;
    PGresult *res;
    PQExpBuffer query;
    int ntups;
    int i_extname;
    int i;

                                      
    nm = pg_strdup(objname);

    query = createPQExpBuffer();
    appendPQExpBuffer(query,
        "SELECT e.extname "
        "FROM pg_catalog.pg_depend d, pg_catalog.pg_extension e "
        "WHERE d.refobjid = e.oid AND classid = '%s'::pg_catalog.regclass "
        "AND objid = '%u'::pg_catalog.oid AND deptype = 'x' "
        "AND refclassid = 'pg_catalog.pg_extension'::pg_catalog.regclass",
        catalog, dobj->catId.oid);
    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
    ntups = PQntuples(res);
    i_extname = PQfnumber(res, "extname");
    for (i = 0; i < ntups; i++)
    {
      appendPQExpBuffer(create, "ALTER %s %s DEPENDS ON EXTENSION %s;\n", keyword, nm, fmtId(PQgetvalue(res, i, i_extname)));
    }

    PQclear(res);
    destroyPQExpBuffer(query);
    pg_free(nm);
  }
}

static void
binary_upgrade_set_type_oids_by_type_oid(Archive *fout, PQExpBuffer upgrade_buffer, Oid pg_type_oid, bool force_array_type)
{
  PQExpBuffer upgrade_query = createPQExpBuffer();
  PGresult *res;
  Oid pg_type_array_oid;

  appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
  appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_pg_type_oid('%u'::pg_catalog.oid);\n\n", pg_type_oid);

                                                      
  appendPQExpBuffer(upgrade_query,
      "SELECT typarray "
      "FROM pg_catalog.pg_type "
      "WHERE oid = '%u'::pg_catalog.oid;",
      pg_type_oid);

  res = ExecuteSqlQueryForSingleRow(fout, upgrade_query->data);

  pg_type_array_oid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typarray")));

  PQclear(res);

  if (!OidIsValid(pg_type_array_oid) && force_array_type)
  {
       
                                                                           
                                                                          
                                                                       
       
                                                                      
                                                                   
       
    static Oid next_possible_free_oid = FirstNormalObjectId;
    bool is_dup;

    do
    {
      ++next_possible_free_oid;
      printfPQExpBuffer(upgrade_query,
          "SELECT EXISTS(SELECT 1 "
          "FROM pg_catalog.pg_type "
          "WHERE oid = '%u'::pg_catalog.oid);",
          next_possible_free_oid);
      res = ExecuteSqlQueryForSingleRow(fout, upgrade_query->data);
      is_dup = (PQgetvalue(res, 0, 0)[0] == 't');
      PQclear(res);
    } while (is_dup);

    pg_type_array_oid = next_possible_free_oid;
  }

  if (OidIsValid(pg_type_array_oid))
  {
    appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type array oid\n");
    appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_array_pg_type_oid('%u'::pg_catalog.oid);\n\n", pg_type_array_oid);
  }

  destroyPQExpBuffer(upgrade_query);
}

static bool
binary_upgrade_set_type_oids_by_rel_oid(Archive *fout, PQExpBuffer upgrade_buffer, Oid pg_rel_oid)
{
  PQExpBuffer upgrade_query = createPQExpBuffer();
  PGresult *upgrade_res;
  Oid pg_type_oid;
  bool toast_set = false;

     
                                                     
     
                                                                             
                                                                          
                                 
     
  appendPQExpBuffer(upgrade_query,
      "SELECT c.reltype AS crel, t.reltype AS trel "
      "FROM pg_catalog.pg_class c "
      "LEFT JOIN pg_catalog.pg_class t ON "
      "  (c.reltoastrelid = t.oid AND c.relkind <> '%c') "
      "WHERE c.oid = '%u'::pg_catalog.oid;",
      RELKIND_PARTITIONED_TABLE, pg_rel_oid);

  upgrade_res = ExecuteSqlQueryForSingleRow(fout, upgrade_query->data);

  pg_type_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "crel")));

  binary_upgrade_set_type_oids_by_type_oid(fout, upgrade_buffer, pg_type_oid, false);

  if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "trel")))
  {
                                                     
    Oid pg_type_toast_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "trel")));

    appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type toast oid\n");
    appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_toast_pg_type_oid('%u'::pg_catalog.oid);\n\n", pg_type_toast_oid);

    toast_set = true;
  }

  PQclear(upgrade_res);
  destroyPQExpBuffer(upgrade_query);

  return toast_set;
}

static void
binary_upgrade_set_pg_class_oids(Archive *fout, PQExpBuffer upgrade_buffer, Oid pg_class_oid, bool is_index)
{
  PQExpBuffer upgrade_query = createPQExpBuffer();
  PGresult *upgrade_res;
  Oid pg_class_reltoastrelid;
  Oid pg_index_indexrelid;

  appendPQExpBuffer(upgrade_query,
      "SELECT c.reltoastrelid, i.indexrelid "
      "FROM pg_catalog.pg_class c LEFT JOIN "
      "pg_catalog.pg_index i ON (c.reltoastrelid = i.indrelid AND i.indisvalid) "
      "WHERE c.oid = '%u'::pg_catalog.oid;",
      pg_class_oid);

  upgrade_res = ExecuteSqlQueryForSingleRow(fout, upgrade_query->data);

  pg_class_reltoastrelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "reltoastrelid")));
  pg_index_indexrelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "indexrelid")));

  appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_class oids\n");

  if (!is_index)
  {
    appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_heap_pg_class_oid('%u'::pg_catalog.oid);\n", pg_class_oid);
                                                    
    if (OidIsValid(pg_class_reltoastrelid))
    {
         
                                                                       
                                                                       
                                                                    
                                                                   
                                                                         
                                                           
         

      appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_toast_pg_class_oid('%u'::pg_catalog.oid);\n", pg_class_reltoastrelid);

                                          
      appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_index_pg_class_oid('%u'::pg_catalog.oid);\n", pg_index_indexrelid);
    }
  }
  else
  {
    appendPQExpBuffer(upgrade_buffer, "SELECT pg_catalog.binary_upgrade_set_next_index_pg_class_oid('%u'::pg_catalog.oid);\n", pg_class_oid);
  }

  appendPQExpBufferChar(upgrade_buffer, '\n');

  PQclear(upgrade_res);
  destroyPQExpBuffer(upgrade_query);
}

   
                                                                     
                                                                           
   
                                                                      
                                  
   
static void
binary_upgrade_extension_member(PQExpBuffer upgrade_buffer, DumpableObject *dobj, const char *objtype, const char *objname, const char *objnamespace)
{
  DumpableObject *extobj = NULL;
  int i;

  if (!dobj->ext_member)
  {
    return;
  }

     
                                                                            
                                                                           
                                                                       
                                                               
     
  for (i = 0; i < dobj->nDeps; i++)
  {
    extobj = findObjectByDumpId(dobj->dependencies[i]);
    if (extobj && extobj->objType == DO_EXTENSION)
    {
      break;
    }
    extobj = NULL;
  }
  if (extobj == NULL)
  {
    fatal("could not find parent extension for %s %s", objtype, objname);
  }

  appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, handle extension membership the hard way\n");
  appendPQExpBuffer(upgrade_buffer, "ALTER EXTENSION %s ADD %s ", fmtId(extobj->name), objtype);
  if (objnamespace && *objnamespace)
  {
    appendPQExpBuffer(upgrade_buffer, "%s.", fmtId(objnamespace));
  }
  appendPQExpBuffer(upgrade_buffer, "%s;\n", objname);
}

   
                  
                                                                       
                            
   
                                                            
   
NamespaceInfo *
getNamespaces(Archive *fout, int *numNamespaces)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  NamespaceInfo *nsinfo;
  int i_tableoid;
  int i_oid;
  int i_nspname;
  int i_rolname;
  int i_nspacl;
  int i_rnspacl;
  int i_initnspacl;
  int i_initrnspacl;

  query = createPQExpBuffer();

     
                                                                            
                                                      
     
  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer init_acl_subquery = createPQExpBuffer();
    PQExpBuffer init_racl_subquery = createPQExpBuffer();

    buildACLQueries(acl_subquery, racl_subquery, init_acl_subquery, init_racl_subquery, "n.nspacl", "n.nspowner", "'n'", dopt->binary_upgrade);

    appendPQExpBuffer(query,
        "SELECT n.tableoid, n.oid, n.nspname, "
        "(%s nspowner) AS rolname, "
        "%s as nspacl, "
        "%s as rnspacl, "
        "%s as initnspacl, "
        "%s as initrnspacl "
        "FROM pg_namespace n "
        "LEFT JOIN pg_init_privs pip "
        "ON (n.oid = pip.objoid "
        "AND pip.classoid = 'pg_namespace'::regclass "
        "AND pip.objsubid = 0",
        username_subquery, acl_subquery->data, racl_subquery->data, init_acl_subquery->data, init_racl_subquery->data);

    appendPQExpBuffer(query, ") ");

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(init_acl_subquery);
    destroyPQExpBuffer(init_racl_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, nspname, "
        "(%s nspowner) AS rolname, "
        "nspacl, NULL as rnspacl, "
        "NULL AS initnspacl, NULL as initrnspacl "
        "FROM pg_namespace",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  nsinfo = (NamespaceInfo *)pg_malloc(ntups * sizeof(NamespaceInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_nspname = PQfnumber(res, "nspname");
  i_rolname = PQfnumber(res, "rolname");
  i_nspacl = PQfnumber(res, "nspacl");
  i_rnspacl = PQfnumber(res, "rnspacl");
  i_initnspacl = PQfnumber(res, "initnspacl");
  i_initrnspacl = PQfnumber(res, "initrnspacl");

  for (i = 0; i < ntups; i++)
  {
    nsinfo[i].dobj.objType = DO_NAMESPACE;
    nsinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    nsinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&nsinfo[i].dobj);
    nsinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_nspname));
    nsinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    nsinfo[i].nspacl = pg_strdup(PQgetvalue(res, i, i_nspacl));
    nsinfo[i].rnspacl = pg_strdup(PQgetvalue(res, i, i_rnspacl));
    nsinfo[i].initnspacl = pg_strdup(PQgetvalue(res, i, i_initnspacl));
    nsinfo[i].initrnspacl = pg_strdup(PQgetvalue(res, i, i_initrnspacl));

                                               
    selectDumpableNamespace(&nsinfo[i], fout);

       
                                                                  
       
                                                                  
                                                                       
                                                                        
                                                
       
    if (PQgetisnull(res, i, i_nspacl) && PQgetisnull(res, i, i_rnspacl) && PQgetisnull(res, i, i_initnspacl) && PQgetisnull(res, i, i_initrnspacl))
    {
      nsinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }

    if (strlen(nsinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of schema \"%s\" appears to be invalid", nsinfo[i].dobj.name);
    }
  }

  PQclear(res);
  destroyPQExpBuffer(query);

  *numNamespaces = ntups;

  return nsinfo;
}

   
                  
                                                                  
   
static NamespaceInfo *
findNamespace(Archive *fout, Oid nsoid)
{
  NamespaceInfo *nsinfo;

  nsinfo = findNamespaceByOid(nsoid);
  if (nsinfo == NULL)
  {
    fatal("schema with OID %u does not exist", nsoid);
  }
  return nsinfo;
}

   
                  
                                                                       
                            
   
                                                            
   
ExtensionInfo *
getExtensions(Archive *fout, int *numExtensions)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  ExtensionInfo *extinfo;
  int i_tableoid;
  int i_oid;
  int i_extname;
  int i_nspname;
  int i_extrelocatable;
  int i_extversion;
  int i_extconfig;
  int i_extcondition;

     
                                          
     
  if (fout->remoteVersion < 90100)
  {
    *numExtensions = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  appendPQExpBufferStr(query, "SELECT x.tableoid, x.oid, "
                              "x.extname, n.nspname, x.extrelocatable, x.extversion, x.extconfig, x.extcondition "
                              "FROM pg_extension x "
                              "JOIN pg_namespace n ON n.oid = x.extnamespace");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  extinfo = (ExtensionInfo *)pg_malloc(ntups * sizeof(ExtensionInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_extname = PQfnumber(res, "extname");
  i_nspname = PQfnumber(res, "nspname");
  i_extrelocatable = PQfnumber(res, "extrelocatable");
  i_extversion = PQfnumber(res, "extversion");
  i_extconfig = PQfnumber(res, "extconfig");
  i_extcondition = PQfnumber(res, "extcondition");

  for (i = 0; i < ntups; i++)
  {
    extinfo[i].dobj.objType = DO_EXTENSION;
    extinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    extinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&extinfo[i].dobj);
    extinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_extname));
    extinfo[i].namespace = pg_strdup(PQgetvalue(res, i, i_nspname));
    extinfo[i].relocatable = *(PQgetvalue(res, i, i_extrelocatable)) == 't';
    extinfo[i].extversion = pg_strdup(PQgetvalue(res, i, i_extversion));
    extinfo[i].extconfig = pg_strdup(PQgetvalue(res, i, i_extconfig));
    extinfo[i].extcondition = pg_strdup(PQgetvalue(res, i, i_extcondition));

                                           
    selectDumpableExtension(&(extinfo[i]), dopt);
  }

  PQclear(res);
  destroyPQExpBuffer(query);

  *numExtensions = ntups;

  return extinfo;
}

   
             
                                                                  
                       
   
                                                  
   
                                                                  
                    
   
TypeInfo *
getTypes(Archive *fout, int *numTypes)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  TypeInfo *tyinfo;
  ShellTypeInfo *stinfo;
  int i_tableoid;
  int i_oid;
  int i_typname;
  int i_typnamespace;
  int i_typacl;
  int i_rtypacl;
  int i_inittypacl;
  int i_initrtypacl;
  int i_rolname;
  int i_typelem;
  int i_typrelid;
  int i_typrelkind;
  int i_typtype;
  int i_typisdefined;
  int i_isarray;

     
                                                                           
                                    
     
                                                                 
     
                                                               
     
                                                                 
                                                                        
                                                                         
                                                                         
                                                                             
                                                                      
     

  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "t.typacl", "t.typowner", "'T'", dopt->binary_upgrade);

    appendPQExpBuffer(query,
        "SELECT t.tableoid, t.oid, t.typname, "
        "t.typnamespace, "
        "%s AS typacl, "
        "%s AS rtypacl, "
        "%s AS inittypacl, "
        "%s AS initrtypacl, "
        "(%s t.typowner) AS rolname, "
        "t.typelem, t.typrelid, "
        "CASE WHEN t.typrelid = 0 THEN ' '::\"char\" "
        "ELSE (SELECT relkind FROM pg_class WHERE oid = t.typrelid) END AS typrelkind, "
        "t.typtype, t.typisdefined, "
        "t.typname[0] = '_' AND t.typelem != 0 AND "
        "(SELECT typarray FROM pg_type te WHERE oid = t.typelem) = t.oid AS isarray "
        "FROM pg_type t "
        "LEFT JOIN pg_init_privs pip ON "
        "(t.oid = pip.objoid "
        "AND pip.classoid = 'pg_type'::regclass "
        "AND pip.objsubid = 0) ",
        acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data, username_subquery);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else if (fout->remoteVersion >= 90200)
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, typname, "
        "typnamespace, typacl, NULL as rtypacl, "
        "NULL AS inittypacl, NULL AS initrtypacl, "
        "(%s typowner) AS rolname, "
        "typelem, typrelid, "
        "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
        "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
        "typtype, typisdefined, "
        "typname[0] = '_' AND typelem != 0 AND "
        "(SELECT typarray FROM pg_type te WHERE oid = pg_type.typelem) = oid AS isarray "
        "FROM pg_type",
        username_subquery);
  }
  else if (fout->remoteVersion >= 80300)
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, typname, "
        "typnamespace, NULL AS typacl, NULL as rtypacl, "
        "NULL AS inittypacl, NULL AS initrtypacl, "
        "(%s typowner) AS rolname, "
        "typelem, typrelid, "
        "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
        "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
        "typtype, typisdefined, "
        "typname[0] = '_' AND typelem != 0 AND "
        "(SELECT typarray FROM pg_type te WHERE oid = pg_type.typelem) = oid AS isarray "
        "FROM pg_type",
        username_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, typname, "
        "typnamespace, NULL AS typacl, NULL as rtypacl, "
        "NULL AS inittypacl, NULL AS initrtypacl, "
        "(%s typowner) AS rolname, "
        "typelem, typrelid, "
        "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
        "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
        "typtype, typisdefined, "
        "typname[0] = '_' AND typelem != 0 AS isarray "
        "FROM pg_type",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  tyinfo = (TypeInfo *)pg_malloc(ntups * sizeof(TypeInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_typname = PQfnumber(res, "typname");
  i_typnamespace = PQfnumber(res, "typnamespace");
  i_typacl = PQfnumber(res, "typacl");
  i_rtypacl = PQfnumber(res, "rtypacl");
  i_inittypacl = PQfnumber(res, "inittypacl");
  i_initrtypacl = PQfnumber(res, "initrtypacl");
  i_rolname = PQfnumber(res, "rolname");
  i_typelem = PQfnumber(res, "typelem");
  i_typrelid = PQfnumber(res, "typrelid");
  i_typrelkind = PQfnumber(res, "typrelkind");
  i_typtype = PQfnumber(res, "typtype");
  i_typisdefined = PQfnumber(res, "typisdefined");
  i_isarray = PQfnumber(res, "isarray");

  for (i = 0; i < ntups; i++)
  {
    tyinfo[i].dobj.objType = DO_TYPE;
    tyinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    tyinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&tyinfo[i].dobj);
    tyinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_typname));
    tyinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_typnamespace)));
    tyinfo[i].ftypname = NULL;                           
    tyinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    tyinfo[i].typacl = pg_strdup(PQgetvalue(res, i, i_typacl));
    tyinfo[i].rtypacl = pg_strdup(PQgetvalue(res, i, i_rtypacl));
    tyinfo[i].inittypacl = pg_strdup(PQgetvalue(res, i, i_inittypacl));
    tyinfo[i].initrtypacl = pg_strdup(PQgetvalue(res, i, i_initrtypacl));
    tyinfo[i].typelem = atooid(PQgetvalue(res, i, i_typelem));
    tyinfo[i].typrelid = atooid(PQgetvalue(res, i, i_typrelid));
    tyinfo[i].typrelkind = *PQgetvalue(res, i, i_typrelkind);
    tyinfo[i].typtype = *PQgetvalue(res, i, i_typtype);
    tyinfo[i].shellType = NULL;

    if (strcmp(PQgetvalue(res, i, i_typisdefined), "t") == 0)
    {
      tyinfo[i].isDefined = true;
    }
    else
    {
      tyinfo[i].isDefined = false;
    }

    if (strcmp(PQgetvalue(res, i, i_isarray), "t") == 0)
    {
      tyinfo[i].isArray = true;
    }
    else
    {
      tyinfo[i].isArray = false;
    }

                                           
    selectDumpableType(&tyinfo[i], fout);

                                                  
    if (PQgetisnull(res, i, i_typacl) && PQgetisnull(res, i, i_rtypacl) && PQgetisnull(res, i, i_inittypacl) && PQgetisnull(res, i, i_initrtypacl))
    {
      tyinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }

       
                                                                  
       
    tyinfo[i].nDomChecks = 0;
    tyinfo[i].domChecks = NULL;
    if ((tyinfo[i].dobj.dump & DUMP_COMPONENT_DEFINITION) && tyinfo[i].typtype == TYPTYPE_DOMAIN)
    {
      getDomainConstraints(fout, &(tyinfo[i]));
    }

       
                                                                       
                                                                           
                                                                    
                                                             
       
                                                                      
                                                                        
                                                            
       
    if ((tyinfo[i].dobj.dump & DUMP_COMPONENT_DEFINITION) && (tyinfo[i].typtype == TYPTYPE_BASE || tyinfo[i].typtype == TYPTYPE_RANGE))
    {
      stinfo = (ShellTypeInfo *)pg_malloc(sizeof(ShellTypeInfo));
      stinfo->dobj.objType = DO_SHELL_TYPE;
      stinfo->dobj.catId = nilCatalogId;
      AssignDumpId(&stinfo->dobj);
      stinfo->dobj.name = pg_strdup(tyinfo[i].dobj.name);
      stinfo->dobj.namespace = tyinfo[i].dobj.namespace;
      stinfo->baseType = &(tyinfo[i]);
      tyinfo[i].shellType = stinfo;

         
                                                                        
                                                                         
                                                           
         
      stinfo->dobj.dump = DUMP_COMPONENT_NONE;
    }

    if (strlen(tyinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of data type \"%s\" appears to be invalid", tyinfo[i].dobj.name);
    }
  }

  *numTypes = ntups;

  PQclear(res);

  destroyPQExpBuffer(query);

  return tyinfo;
}

   
                 
                                                                      
                      
   
                                                     
   
OprInfo *
getOperators(Archive *fout, int *numOprs)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  OprInfo *oprinfo;
  int i_tableoid;
  int i_oid;
  int i_oprname;
  int i_oprnamespace;
  int i_rolname;
  int i_oprkind;
  int i_oprcode;

     
                                                                    
                                                
     

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, oprname, "
      "oprnamespace, "
      "(%s oprowner) AS rolname, "
      "oprkind, "
      "oprcode::oid AS oprcode "
      "FROM pg_operator",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numOprs = ntups;

  oprinfo = (OprInfo *)pg_malloc(ntups * sizeof(OprInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_oprname = PQfnumber(res, "oprname");
  i_oprnamespace = PQfnumber(res, "oprnamespace");
  i_rolname = PQfnumber(res, "rolname");
  i_oprkind = PQfnumber(res, "oprkind");
  i_oprcode = PQfnumber(res, "oprcode");

  for (i = 0; i < ntups; i++)
  {
    oprinfo[i].dobj.objType = DO_OPERATOR;
    oprinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    oprinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&oprinfo[i].dobj);
    oprinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_oprname));
    oprinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_oprnamespace)));
    oprinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    oprinfo[i].oprkind = (PQgetvalue(res, i, i_oprkind))[0];
    oprinfo[i].oprcode = atooid(PQgetvalue(res, i, i_oprcode));

                                           
    selectDumpableObject(&(oprinfo[i].dobj), fout);

                                               
    oprinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;

    if (strlen(oprinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of operator \"%s\" appears to be invalid", oprinfo[i].dobj.name);
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return oprinfo;
}

   
                  
                                                                       
                       
   
                                                            
   
CollInfo *
getCollations(Archive *fout, int *numCollations)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  CollInfo *collinfo;
  int i_tableoid;
  int i_oid;
  int i_collname;
  int i_collnamespace;
  int i_rolname;

                                       
  if (fout->remoteVersion < 90100)
  {
    *numCollations = 0;
    return NULL;
  }

  query = createPQExpBuffer();

     
                                                                      
                                                 
     

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, collname, "
      "collnamespace, "
      "(%s collowner) AS rolname "
      "FROM pg_collation",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numCollations = ntups;

  collinfo = (CollInfo *)pg_malloc(ntups * sizeof(CollInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_collname = PQfnumber(res, "collname");
  i_collnamespace = PQfnumber(res, "collnamespace");
  i_rolname = PQfnumber(res, "rolname");

  for (i = 0; i < ntups; i++)
  {
    collinfo[i].dobj.objType = DO_COLLATION;
    collinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    collinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&collinfo[i].dobj);
    collinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_collname));
    collinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_collnamespace)));
    collinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));

                                           
    selectDumpableObject(&(collinfo[i].dobj), fout);

                                                
    collinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return collinfo;
}

   
                   
                                                                        
                       
   
                                                              
   
ConvInfo *
getConversions(Archive *fout, int *numConversions)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  ConvInfo *convinfo;
  int i_tableoid;
  int i_oid;
  int i_conname;
  int i_connamespace;
  int i_rolname;

  query = createPQExpBuffer();

     
                                                                        
                                                  
     

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, conname, "
      "connamespace, "
      "(%s conowner) AS rolname "
      "FROM pg_conversion",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numConversions = ntups;

  convinfo = (ConvInfo *)pg_malloc(ntups * sizeof(ConvInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_conname = PQfnumber(res, "conname");
  i_connamespace = PQfnumber(res, "connamespace");
  i_rolname = PQfnumber(res, "rolname");

  for (i = 0; i < ntups; i++)
  {
    convinfo[i].dobj.objType = DO_CONVERSION;
    convinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    convinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&convinfo[i].dobj);
    convinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_conname));
    convinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_connamespace)));
    convinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));

                                           
    selectDumpableObject(&(convinfo[i].dobj), fout);

                                                 
    convinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return convinfo;
}

   
                     
                                                                            
                                             
   
                                                                   
   
AccessMethodInfo *
getAccessMethods(Archive *fout, int *numAccessMethods)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  AccessMethodInfo *aminfo;
  int i_tableoid;
  int i_oid;
  int i_amname;
  int i_amhandler;
  int i_amtype;

                                                            
  if (fout->remoteVersion < 90600)
  {
    *numAccessMethods = 0;
    return NULL;
  }

  query = createPQExpBuffer();

                                                  
  appendPQExpBuffer(query, "SELECT tableoid, oid, amname, amtype, "
                           "amhandler::pg_catalog.regproc AS amhandler "
                           "FROM pg_am");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numAccessMethods = ntups;

  aminfo = (AccessMethodInfo *)pg_malloc(ntups * sizeof(AccessMethodInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_amname = PQfnumber(res, "amname");
  i_amhandler = PQfnumber(res, "amhandler");
  i_amtype = PQfnumber(res, "amtype");

  for (i = 0; i < ntups; i++)
  {
    aminfo[i].dobj.objType = DO_ACCESS_METHOD;
    aminfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    aminfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&aminfo[i].dobj);
    aminfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_amname));
    aminfo[i].dobj.namespace = NULL;
    aminfo[i].amhandler = pg_strdup(PQgetvalue(res, i, i_amhandler));
    aminfo[i].amtype = *(PQgetvalue(res, i, i_amtype));

                                           
    selectDumpableAccessMethod(&(aminfo[i]), fout);

                                                    
    aminfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return aminfo;
}

   
                 
                                                                      
                          
   
                                                          
   
OpclassInfo *
getOpclasses(Archive *fout, int *numOpclasses)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  OpclassInfo *opcinfo;
  int i_tableoid;
  int i_oid;
  int i_opcname;
  int i_opcnamespace;
  int i_rolname;

     
                                                                    
                                                
     

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, opcname, "
      "opcnamespace, "
      "(%s opcowner) AS rolname "
      "FROM pg_opclass",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numOpclasses = ntups;

  opcinfo = (OpclassInfo *)pg_malloc(ntups * sizeof(OpclassInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_opcname = PQfnumber(res, "opcname");
  i_opcnamespace = PQfnumber(res, "opcnamespace");
  i_rolname = PQfnumber(res, "rolname");

  for (i = 0; i < ntups; i++)
  {
    opcinfo[i].dobj.objType = DO_OPCLASS;
    opcinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    opcinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&opcinfo[i].dobj);
    opcinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_opcname));
    opcinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_opcnamespace)));
    opcinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));

                                           
    selectDumpableObject(&(opcinfo[i].dobj), fout);

                                                
    opcinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;

    if (strlen(opcinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of operator class \"%s\" appears to be invalid", opcinfo[i].dobj.name);
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return opcinfo;
}

   
                  
                                                                       
                           
   
                                                            
   
OpfamilyInfo *
getOpfamilies(Archive *fout, int *numOpfamilies)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  OpfamilyInfo *opfinfo;
  int i_tableoid;
  int i_oid;
  int i_opfname;
  int i_opfnamespace;
  int i_rolname;

                                                              
  if (fout->remoteVersion < 80300)
  {
    *numOpfamilies = 0;
    return NULL;
  }

  query = createPQExpBuffer();

     
                                                                      
                                                 
     

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, opfname, "
      "opfnamespace, "
      "(%s opfowner) AS rolname "
      "FROM pg_opfamily",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numOpfamilies = ntups;

  opfinfo = (OpfamilyInfo *)pg_malloc(ntups * sizeof(OpfamilyInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_opfname = PQfnumber(res, "opfname");
  i_opfnamespace = PQfnumber(res, "opfnamespace");
  i_rolname = PQfnumber(res, "rolname");

  for (i = 0; i < ntups; i++)
  {
    opfinfo[i].dobj.objType = DO_OPFAMILY;
    opfinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    opfinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&opfinfo[i].dobj);
    opfinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_opfname));
    opfinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_opfnamespace)));
    opfinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));

                                           
    selectDumpableObject(&(opfinfo[i].dobj), fout);

                                                
    opfinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;

    if (strlen(opfinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of operator family \"%s\" appears to be invalid", opfinfo[i].dobj.name);
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return opfinfo;
}

   
                  
                                                                     
                                         
   
                                                      
   
AggInfo *
getAggregates(Archive *fout, int *numAggs)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  AggInfo *agginfo;
  int i_tableoid;
  int i_oid;
  int i_aggname;
  int i_aggnamespace;
  int i_pronargs;
  int i_proargtypes;
  int i_rolname;
  int i_aggacl;
  int i_raggacl;
  int i_initaggacl;
  int i_initraggacl;

     
                                                                         
                                           
     
  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();
    const char *agg_check;

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "p.proacl", "p.proowner", "'f'", dopt->binary_upgrade);

    agg_check = (fout->remoteVersion >= 110000 ? "p.prokind = 'a'" : "p.proisagg");

    appendPQExpBuffer(query,
        "SELECT p.tableoid, p.oid, "
        "p.proname AS aggname, "
        "p.pronamespace AS aggnamespace, "
        "p.pronargs, p.proargtypes, "
        "(%s p.proowner) AS rolname, "
        "%s AS aggacl, "
        "%s AS raggacl, "
        "%s AS initaggacl, "
        "%s AS initraggacl "
        "FROM pg_proc p "
        "LEFT JOIN pg_init_privs pip ON "
        "(p.oid = pip.objoid "
        "AND pip.classoid = 'pg_proc'::regclass "
        "AND pip.objsubid = 0) "
        "WHERE %s AND ("
        "p.pronamespace != "
        "(SELECT oid FROM pg_namespace "
        "WHERE nspname = 'pg_catalog') OR "
        "p.proacl IS DISTINCT FROM pip.initprivs",
        username_subquery, acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data, agg_check);
    if (dopt->binary_upgrade)
    {
      appendPQExpBufferStr(query, " OR EXISTS(SELECT 1 FROM pg_depend WHERE "
                                  "classid = 'pg_proc'::regclass AND "
                                  "objid = p.oid AND "
                                  "refclassid = 'pg_extension'::regclass AND "
                                  "deptype = 'e')");
    }
    appendPQExpBufferChar(query, ')');

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else if (fout->remoteVersion >= 80200)
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, proname AS aggname, "
        "pronamespace AS aggnamespace, "
        "pronargs, proargtypes, "
        "(%s proowner) AS rolname, "
        "proacl AS aggacl, "
        "NULL AS raggacl, "
        "NULL AS initaggacl, NULL AS initraggacl "
        "FROM pg_proc p "
        "WHERE proisagg AND ("
        "pronamespace != "
        "(SELECT oid FROM pg_namespace "
        "WHERE nspname = 'pg_catalog')",
        username_subquery);
    if (dopt->binary_upgrade && fout->remoteVersion >= 90100)
    {
      appendPQExpBufferStr(query, " OR EXISTS(SELECT 1 FROM pg_depend WHERE "
                                  "classid = 'pg_proc'::regclass AND "
                                  "objid = p.oid AND "
                                  "refclassid = 'pg_extension'::regclass AND "
                                  "deptype = 'e')");
    }
    appendPQExpBufferChar(query, ')');
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, proname AS aggname, "
        "pronamespace AS aggnamespace, "
        "CASE WHEN proargtypes[0] = 'pg_catalog.\"any\"'::pg_catalog.regtype THEN 0 ELSE 1 END AS pronargs, "
        "proargtypes, "
        "(%s proowner) AS rolname, "
        "proacl AS aggacl, "
        "NULL AS raggacl, "
        "NULL AS initaggacl, NULL AS initraggacl "
        "FROM pg_proc "
        "WHERE proisagg "
        "AND pronamespace != "
        "(SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog')",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numAggs = ntups;

  agginfo = (AggInfo *)pg_malloc(ntups * sizeof(AggInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_aggname = PQfnumber(res, "aggname");
  i_aggnamespace = PQfnumber(res, "aggnamespace");
  i_pronargs = PQfnumber(res, "pronargs");
  i_proargtypes = PQfnumber(res, "proargtypes");
  i_rolname = PQfnumber(res, "rolname");
  i_aggacl = PQfnumber(res, "aggacl");
  i_raggacl = PQfnumber(res, "raggacl");
  i_initaggacl = PQfnumber(res, "initaggacl");
  i_initraggacl = PQfnumber(res, "initraggacl");

  for (i = 0; i < ntups; i++)
  {
    agginfo[i].aggfn.dobj.objType = DO_AGG;
    agginfo[i].aggfn.dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    agginfo[i].aggfn.dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&agginfo[i].aggfn.dobj);
    agginfo[i].aggfn.dobj.name = pg_strdup(PQgetvalue(res, i, i_aggname));
    agginfo[i].aggfn.dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_aggnamespace)));
    agginfo[i].aggfn.rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    if (strlen(agginfo[i].aggfn.rolname) == 0)
    {
      pg_log_warning("owner of aggregate function \"%s\" appears to be invalid", agginfo[i].aggfn.dobj.name);
    }
    agginfo[i].aggfn.lang = InvalidOid;                                      
    agginfo[i].aggfn.prorettype = InvalidOid;                
    agginfo[i].aggfn.proacl = pg_strdup(PQgetvalue(res, i, i_aggacl));
    agginfo[i].aggfn.rproacl = pg_strdup(PQgetvalue(res, i, i_raggacl));
    agginfo[i].aggfn.initproacl = pg_strdup(PQgetvalue(res, i, i_initaggacl));
    agginfo[i].aggfn.initrproacl = pg_strdup(PQgetvalue(res, i, i_initraggacl));
    agginfo[i].aggfn.nargs = atoi(PQgetvalue(res, i, i_pronargs));
    if (agginfo[i].aggfn.nargs == 0)
    {
      agginfo[i].aggfn.argtypes = NULL;
    }
    else
    {
      agginfo[i].aggfn.argtypes = (Oid *)pg_malloc(agginfo[i].aggfn.nargs * sizeof(Oid));
      parseOidArray(PQgetvalue(res, i, i_proargtypes), agginfo[i].aggfn.argtypes, agginfo[i].aggfn.nargs);
    }

                                           
    selectDumpableObject(&(agginfo[i].aggfn.dobj), fout);

                                                  
    if (PQgetisnull(res, i, i_aggacl) && PQgetisnull(res, i, i_raggacl) && PQgetisnull(res, i, i_initaggacl) && PQgetisnull(res, i, i_initraggacl))
    {
      agginfo[i].aggfn.dobj.dump &= ~DUMP_COMPONENT_ACL;
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return agginfo;
}

   
             
                                                                    
                                          
   
                                                      
   
FuncInfo *
getFuncs(Archive *fout, int *numFuncs)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  FuncInfo *finfo;
  int i_tableoid;
  int i_oid;
  int i_proname;
  int i_pronamespace;
  int i_rolname;
  int i_prolang;
  int i_pronargs;
  int i_proargtypes;
  int i_prorettype;
  int i_proacl;
  int i_rproacl;
  int i_initproacl;
  int i_initrproacl;

     
                                                                 
     
                                                                
     
                                                                            
                                                                          
                                                                           
                                                                         
                                                                          
                                                                      
                       
     
                                                                             
                                                                          
                                                                           
                                                                            
                                                                          
                                                                   
                                                                   
                    
     
  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();
    const char *not_agg_check;

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "p.proacl", "p.proowner", "'f'", dopt->binary_upgrade);

    not_agg_check = (fout->remoteVersion >= 110000 ? "p.prokind <> 'a'" : "NOT p.proisagg");

    appendPQExpBuffer(query,
        "SELECT p.tableoid, p.oid, p.proname, p.prolang, "
        "p.pronargs, p.proargtypes, p.prorettype, "
        "%s AS proacl, "
        "%s AS rproacl, "
        "%s AS initproacl, "
        "%s AS initrproacl, "
        "p.pronamespace, "
        "(%s p.proowner) AS rolname "
        "FROM pg_proc p "
        "LEFT JOIN pg_init_privs pip ON "
        "(p.oid = pip.objoid "
        "AND pip.classoid = 'pg_proc'::regclass "
        "AND pip.objsubid = 0) "
        "WHERE %s"
        "\n  AND NOT EXISTS (SELECT 1 FROM pg_depend "
        "WHERE classid = 'pg_proc'::regclass AND "
        "objid = p.oid AND deptype = 'i')"
        "\n  AND ("
        "\n  pronamespace != "
        "(SELECT oid FROM pg_namespace "
        "WHERE nspname = 'pg_catalog')"
        "\n  OR EXISTS (SELECT 1 FROM pg_cast"
        "\n  WHERE pg_cast.oid > %u "
        "\n  AND p.oid = pg_cast.castfunc)"
        "\n  OR EXISTS (SELECT 1 FROM pg_transform"
        "\n  WHERE pg_transform.oid > %u AND "
        "\n  (p.oid = pg_transform.trffromsql"
        "\n  OR p.oid = pg_transform.trftosql))",
        acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data, username_subquery, not_agg_check, g_last_builtin_oid, g_last_builtin_oid);
    if (dopt->binary_upgrade)
    {
      appendPQExpBufferStr(query, "\n  OR EXISTS(SELECT 1 FROM pg_depend WHERE "
                                  "classid = 'pg_proc'::regclass AND "
                                  "objid = p.oid AND "
                                  "refclassid = 'pg_extension'::regclass AND "
                                  "deptype = 'e')");
    }
    appendPQExpBufferStr(query, "\n  OR p.proacl IS DISTINCT FROM pip.initprivs");
    appendPQExpBufferChar(query, ')');

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, proname, prolang, "
        "pronargs, proargtypes, prorettype, proacl, "
        "NULL as rproacl, "
        "NULL as initproacl, NULL AS initrproacl, "
        "pronamespace, "
        "(%s proowner) AS rolname "
        "FROM pg_proc p "
        "WHERE NOT proisagg",
        username_subquery);
    if (fout->remoteVersion >= 90200)
    {
      appendPQExpBufferStr(query, "\n  AND NOT EXISTS (SELECT 1 FROM pg_depend "
                                  "WHERE classid = 'pg_proc'::regclass AND "
                                  "objid = p.oid AND deptype = 'i')");
    }
    appendPQExpBuffer(query,
        "\n  AND ("
        "\n  pronamespace != "
        "(SELECT oid FROM pg_namespace "
        "WHERE nspname = 'pg_catalog')"
        "\n  OR EXISTS (SELECT 1 FROM pg_cast"
        "\n  WHERE pg_cast.oid > '%u'::oid"
        "\n  AND p.oid = pg_cast.castfunc)",
        g_last_builtin_oid);

    if (fout->remoteVersion >= 90500)
    {
      appendPQExpBuffer(query,
          "\n  OR EXISTS (SELECT 1 FROM pg_transform"
          "\n  WHERE pg_transform.oid > '%u'::oid"
          "\n  AND (p.oid = pg_transform.trffromsql"
          "\n  OR p.oid = pg_transform.trftosql))",
          g_last_builtin_oid);
    }

    if (dopt->binary_upgrade && fout->remoteVersion >= 90100)
    {
      appendPQExpBufferStr(query, "\n  OR EXISTS(SELECT 1 FROM pg_depend WHERE "
                                  "classid = 'pg_proc'::regclass AND "
                                  "objid = p.oid AND "
                                  "refclassid = 'pg_extension'::regclass AND "
                                  "deptype = 'e')");
    }
    appendPQExpBufferChar(query, ')');
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numFuncs = ntups;

  finfo = (FuncInfo *)pg_malloc0(ntups * sizeof(FuncInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_proname = PQfnumber(res, "proname");
  i_pronamespace = PQfnumber(res, "pronamespace");
  i_rolname = PQfnumber(res, "rolname");
  i_prolang = PQfnumber(res, "prolang");
  i_pronargs = PQfnumber(res, "pronargs");
  i_proargtypes = PQfnumber(res, "proargtypes");
  i_prorettype = PQfnumber(res, "prorettype");
  i_proacl = PQfnumber(res, "proacl");
  i_rproacl = PQfnumber(res, "rproacl");
  i_initproacl = PQfnumber(res, "initproacl");
  i_initrproacl = PQfnumber(res, "initrproacl");

  for (i = 0; i < ntups; i++)
  {
    finfo[i].dobj.objType = DO_FUNC;
    finfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    finfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&finfo[i].dobj);
    finfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_proname));
    finfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_pronamespace)));
    finfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    finfo[i].lang = atooid(PQgetvalue(res, i, i_prolang));
    finfo[i].prorettype = atooid(PQgetvalue(res, i, i_prorettype));
    finfo[i].proacl = pg_strdup(PQgetvalue(res, i, i_proacl));
    finfo[i].rproacl = pg_strdup(PQgetvalue(res, i, i_rproacl));
    finfo[i].initproacl = pg_strdup(PQgetvalue(res, i, i_initproacl));
    finfo[i].initrproacl = pg_strdup(PQgetvalue(res, i, i_initrproacl));
    finfo[i].nargs = atoi(PQgetvalue(res, i, i_pronargs));
    if (finfo[i].nargs == 0)
    {
      finfo[i].argtypes = NULL;
    }
    else
    {
      finfo[i].argtypes = (Oid *)pg_malloc(finfo[i].nargs * sizeof(Oid));
      parseOidArray(PQgetvalue(res, i, i_proargtypes), finfo[i].argtypes, finfo[i].nargs);
    }

                                           
    selectDumpableObject(&(finfo[i].dobj), fout);

                                                  
    if (PQgetisnull(res, i, i_proacl) && PQgetisnull(res, i, i_rproacl) && PQgetisnull(res, i, i_initproacl) && PQgetisnull(res, i, i_initrproacl))
    {
      finfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }

    if (strlen(finfo[i].rolname) == 0)
    {
      pg_log_warning("owner of function \"%s\" appears to be invalid", finfo[i].dobj.name);
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return finfo;
}

   
             
                                      
                                                                  
   
                                                    
   
TableInfo *
getTables(Archive *fout, int *numTables)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  TableInfo *tblinfo;
  int i_reltableoid;
  int i_reloid;
  int i_relname;
  int i_relnamespace;
  int i_relkind;
  int i_relacl;
  int i_rrelacl;
  int i_initrelacl;
  int i_initrrelacl;
  int i_rolname;
  int i_relchecks;
  int i_relhastriggers;
  int i_relhasindex;
  int i_relhasrules;
  int i_relrowsec;
  int i_relforcerowsec;
  int i_relhasoids;
  int i_relfrozenxid;
  int i_relminmxid;
  int i_toastoid;
  int i_toastfrozenxid;
  int i_toastminmxid;
  int i_relpersistence;
  int i_relispopulated;
  int i_relreplident;
  int i_owning_tab;
  int i_owning_col;
  int i_reltablespace;
  int i_reloptions;
  int i_checkoption;
  int i_toastreloptions;
  int i_reloftype;
  int i_relpages;
  int i_is_identity_sequence;
  int i_changed_acl;
  int i_ispartition;
  int i_amname;

     
                                                 
     
                                                                        
                                                                 
     
                                                                        
                                                                     
     
                                                                         
                                                             
     
                                                                          
                                                                             
                                                                         
                                                        
     
                                                                    
                                                                            
                                                                            
                                                                         
                                                                            
                                                       
     
                                                                             
                                                                          
                                 
     

  if (fout->remoteVersion >= 90600)
  {
    char *ispartition = "false";
    char *relhasoids = "c.relhasoids";

    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();

    PQExpBuffer attacl_subquery = createPQExpBuffer();
    PQExpBuffer attracl_subquery = createPQExpBuffer();
    PQExpBuffer attinitacl_subquery = createPQExpBuffer();
    PQExpBuffer attinitracl_subquery = createPQExpBuffer();

       
                                                                        
                      
       
    if (fout->remoteVersion >= 100000)
    {
      ispartition = "c.relispartition";
    }

                                                           
    if (fout->remoteVersion >= 120000)
    {
      relhasoids = "'f'::bool";
    }

       
                                                                       
                                                                      
       
                                                                          
                                                            
       

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "c.relacl", "c.relowner", "CASE WHEN c.relkind = " CppAsString2(RELKIND_SEQUENCE) " THEN 's' ELSE 'r' END::\"char\"", dopt->binary_upgrade);

    buildACLQueries(attacl_subquery, attracl_subquery, attinitacl_subquery, attinitracl_subquery, "at.attacl", "c.relowner", "'c'", dopt->binary_upgrade);

    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "%s AS relacl, %s as rrelacl, "
        "%s AS initrelacl, %s as initrrelacl, "
        "c.relkind, c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, %s AS relhasoids, "
        "c.relrowsecurity, c.relforcerowsecurity, "
        "c.relfrozenxid, c.relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "tc.relminmxid AS tminmxid, "
        "c.relpersistence, c.relispopulated, "
        "c.relreplident, c.relpages, am.amname, "
        "c.reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "array_remove(array_remove(c.reloptions,'check_option=local'),'check_option=cascaded') AS reloptions, "
        "CASE WHEN 'check_option=local' = ANY (c.reloptions) THEN 'LOCAL'::text "
        "WHEN 'check_option=cascaded' = ANY (c.reloptions) THEN 'CASCADED'::text ELSE NULL END AS checkoption, "
        "tc.reloptions AS toast_reloptions, "
        "c.relkind = '%c' AND EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = c.oid AND objsubid = 0 AND refclassid = 'pg_class'::regclass AND deptype = 'i') AS is_identity_sequence, "
        "EXISTS (SELECT 1 FROM pg_attribute at LEFT JOIN pg_init_privs pip ON "
        "(c.oid = pip.objoid "
        "AND pip.classoid = 'pg_class'::regclass "
        "AND pip.objsubid = at.attnum)"
        "WHERE at.attrelid = c.oid AND ("
        "%s IS NOT NULL "
        "OR %s IS NOT NULL "
        "OR %s IS NOT NULL "
        "OR %s IS NOT NULL"
        "))"
        "AS changed_acl, "
        "%s AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype IN ('a', 'i')) "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid AND c.relkind <> '%c') "
        "LEFT JOIN pg_am am ON (c.relam = am.oid) "
        "LEFT JOIN pg_init_privs pip ON "
        "(c.oid = pip.objoid "
        "AND pip.classoid = 'pg_class'::regclass "
        "AND pip.objsubid = 0) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data, username_subquery, relhasoids, RELKIND_SEQUENCE, attacl_subquery->data, attracl_subquery->data, attinitacl_subquery->data, attinitracl_subquery->data, ispartition, RELKIND_SEQUENCE, RELKIND_PARTITIONED_TABLE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE, RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE, RELKIND_PARTITIONED_TABLE);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);

    destroyPQExpBuffer(attacl_subquery);
    destroyPQExpBuffer(attracl_subquery);
    destroyPQExpBuffer(attinitacl_subquery);
    destroyPQExpBuffer(attinitracl_subquery);
  }
  else if (fout->remoteVersion >= 90500)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "c.relrowsecurity, c.relforcerowsecurity, "
        "c.relfrozenxid, c.relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "tc.relminmxid AS tminmxid, "
        "c.relpersistence, c.relispopulated, "
        "c.relreplident, c.relpages, "
        "NULL AS amname, "
        "c.reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "array_remove(array_remove(c.reloptions,'check_option=local'),'check_option=cascaded') AS reloptions, "
        "CASE WHEN 'check_option=local' = ANY (c.reloptions) THEN 'LOCAL'::text "
        "WHEN 'check_option=cascaded' = ANY (c.reloptions) THEN 'CASCADED'::text ELSE NULL END AS checkoption, "
        "tc.reloptions AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE, RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE);
  }
  else if (fout->remoteVersion >= 90400)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "c.relfrozenxid, c.relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "tc.relminmxid AS tminmxid, "
        "c.relpersistence, c.relispopulated, "
        "c.relreplident, c.relpages, "
        "NULL AS amname, "
        "c.reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "array_remove(array_remove(c.reloptions,'check_option=local'),'check_option=cascaded') AS reloptions, "
        "CASE WHEN 'check_option=local' = ANY (c.reloptions) THEN 'LOCAL'::text "
        "WHEN 'check_option=cascaded' = ANY (c.reloptions) THEN 'CASCADED'::text ELSE NULL END AS checkoption, "
        "tc.reloptions AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE, RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE);
  }
  else if (fout->remoteVersion >= 90300)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "c.relfrozenxid, c.relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "tc.relminmxid AS tminmxid, "
        "c.relpersistence, c.relispopulated, "
        "'d' AS relreplident, c.relpages, "
        "NULL AS amname, "
        "c.reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "array_remove(array_remove(c.reloptions,'check_option=local'),'check_option=cascaded') AS reloptions, "
        "CASE WHEN 'check_option=local' = ANY (c.reloptions) THEN 'LOCAL'::text "
        "WHEN 'check_option=cascaded' = ANY (c.reloptions) THEN 'CASCADED'::text ELSE NULL END AS checkoption, "
        "tc.reloptions AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE, RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE);
  }
  else if (fout->remoteVersion >= 90100)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "c.relfrozenxid, 0 AS relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "0 AS tminmxid, "
        "c.relpersistence, 't' as relispopulated, "
        "'d' AS relreplident, c.relpages, "
        "NULL AS amname, "
        "c.reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "c.reloptions AS reloptions, "
        "tc.reloptions AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE, RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE);
  }
  else if (fout->remoteVersion >= 90000)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "c.relfrozenxid, 0 AS relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "0 AS tminmxid, "
        "'p' AS relpersistence, 't' as relispopulated, "
        "'d' AS relreplident, c.relpages, "
        "NULL AS amname, "
        "c.reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "c.reloptions AS reloptions, "
        "tc.reloptions AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
  }
  else if (fout->remoteVersion >= 80400)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, c.relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "c.relfrozenxid, 0 AS relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "0 AS tminmxid, "
        "'p' AS relpersistence, 't' as relispopulated, "
        "'d' AS relreplident, c.relpages, "
        "NULL AS amname, "
        "0 AS reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "c.reloptions AS reloptions, "
        "tc.reloptions AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
  }
  else if (fout->remoteVersion >= 80200)
  {
       
                                                                       
                                                                      
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, c.relname, "
        "c.relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "c.relkind, "
        "c.relnamespace, "
        "(%s c.relowner) AS rolname, "
        "c.relchecks, (c.reltriggers <> 0) AS relhastriggers, "
        "c.relhasindex, c.relhasrules, c.relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "c.relfrozenxid, 0 AS relminmxid, tc.oid AS toid, "
        "tc.relfrozenxid AS tfrozenxid, "
        "0 AS tminmxid, "
        "'p' AS relpersistence, 't' as relispopulated, "
        "'d' AS relreplident, c.relpages, "
        "NULL AS amname, "
        "0 AS reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "c.reloptions AS reloptions, "
        "NULL AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'a') "
        "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
        "WHERE c.relkind in ('%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
  }
  else
  {
       
                                                                       
                             
       
    appendPQExpBuffer(query,
        "SELECT c.tableoid, c.oid, relname, "
        "relacl, NULL as rrelacl, "
        "NULL AS initrelacl, NULL AS initrrelacl, "
        "relkind, relnamespace, "
        "(%s relowner) AS rolname, "
        "relchecks, (reltriggers <> 0) AS relhastriggers, "
        "relhasindex, relhasrules, relhasoids, "
        "'f'::bool AS relrowsecurity, "
        "'f'::bool AS relforcerowsecurity, "
        "0 AS relfrozenxid, 0 AS relminmxid,"
        "0 AS toid, "
        "0 AS tfrozenxid, 0 AS tminmxid,"
        "'p' AS relpersistence, 't' as relispopulated, "
        "'d' AS relreplident, relpages, "
        "NULL AS amname, "
        "0 AS reloftype, "
        "d.refobjid AS owning_tab, "
        "d.refobjsubid AS owning_col, "
        "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
        "NULL AS reloptions, "
        "NULL AS toast_reloptions, "
        "NULL AS changed_acl, "
        "false AS ispartition "
        "FROM pg_class c "
        "LEFT JOIN pg_depend d ON "
        "(c.relkind = '%c' AND "
        "d.classid = c.tableoid AND d.objid = c.oid AND "
        "d.objsubid = 0 AND "
        "d.refclassid = c.tableoid AND d.deptype = 'i') "
        "WHERE relkind in ('%c', '%c', '%c', '%c') "
        "ORDER BY c.oid",
        username_subquery, RELKIND_SEQUENCE, RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numTables = ntups;

     
                                                                           
                                                                        
                         
     
                                                                             
                                                                           
                                    
     
  tblinfo = (TableInfo *)pg_malloc0(ntups * sizeof(TableInfo));

  i_reltableoid = PQfnumber(res, "tableoid");
  i_reloid = PQfnumber(res, "oid");
  i_relname = PQfnumber(res, "relname");
  i_relnamespace = PQfnumber(res, "relnamespace");
  i_relacl = PQfnumber(res, "relacl");
  i_rrelacl = PQfnumber(res, "rrelacl");
  i_initrelacl = PQfnumber(res, "initrelacl");
  i_initrrelacl = PQfnumber(res, "initrrelacl");
  i_relkind = PQfnumber(res, "relkind");
  i_rolname = PQfnumber(res, "rolname");
  i_relchecks = PQfnumber(res, "relchecks");
  i_relhastriggers = PQfnumber(res, "relhastriggers");
  i_relhasindex = PQfnumber(res, "relhasindex");
  i_relhasrules = PQfnumber(res, "relhasrules");
  i_relrowsec = PQfnumber(res, "relrowsecurity");
  i_relforcerowsec = PQfnumber(res, "relforcerowsecurity");
  i_relhasoids = PQfnumber(res, "relhasoids");
  i_relfrozenxid = PQfnumber(res, "relfrozenxid");
  i_relminmxid = PQfnumber(res, "relminmxid");
  i_toastoid = PQfnumber(res, "toid");
  i_toastfrozenxid = PQfnumber(res, "tfrozenxid");
  i_toastminmxid = PQfnumber(res, "tminmxid");
  i_relpersistence = PQfnumber(res, "relpersistence");
  i_relispopulated = PQfnumber(res, "relispopulated");
  i_relreplident = PQfnumber(res, "relreplident");
  i_relpages = PQfnumber(res, "relpages");
  i_owning_tab = PQfnumber(res, "owning_tab");
  i_owning_col = PQfnumber(res, "owning_col");
  i_reltablespace = PQfnumber(res, "reltablespace");
  i_reloptions = PQfnumber(res, "reloptions");
  i_checkoption = PQfnumber(res, "checkoption");
  i_toastreloptions = PQfnumber(res, "toast_reloptions");
  i_reloftype = PQfnumber(res, "reloftype");
  i_is_identity_sequence = PQfnumber(res, "is_identity_sequence");
  i_changed_acl = PQfnumber(res, "changed_acl");
  i_ispartition = PQfnumber(res, "ispartition");
  i_amname = PQfnumber(res, "amname");

  if (dopt->lockWaitTimeout)
  {
       
                                                                    
       
                                                                       
                                                                           
                                    
       
    resetPQExpBuffer(query);
    appendPQExpBufferStr(query, "SET statement_timeout = ");
    appendStringLiteralConn(query, dopt->lockWaitTimeout, GetConnection(fout));
    ExecuteSqlStatement(fout, query->data);
  }

  for (i = 0; i < ntups; i++)
  {
    tblinfo[i].dobj.objType = DO_TABLE;
    tblinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_reltableoid));
    tblinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_reloid));
    AssignDumpId(&tblinfo[i].dobj);
    tblinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_relname));
    tblinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_relnamespace)));
    tblinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    tblinfo[i].relacl = pg_strdup(PQgetvalue(res, i, i_relacl));
    tblinfo[i].rrelacl = pg_strdup(PQgetvalue(res, i, i_rrelacl));
    tblinfo[i].initrelacl = pg_strdup(PQgetvalue(res, i, i_initrelacl));
    tblinfo[i].initrrelacl = pg_strdup(PQgetvalue(res, i, i_initrrelacl));
    tblinfo[i].relkind = *(PQgetvalue(res, i, i_relkind));
    tblinfo[i].relpersistence = *(PQgetvalue(res, i, i_relpersistence));
    tblinfo[i].hasindex = (strcmp(PQgetvalue(res, i, i_relhasindex), "t") == 0);
    tblinfo[i].hasrules = (strcmp(PQgetvalue(res, i, i_relhasrules), "t") == 0);
    tblinfo[i].hastriggers = (strcmp(PQgetvalue(res, i, i_relhastriggers), "t") == 0);
    tblinfo[i].rowsec = (strcmp(PQgetvalue(res, i, i_relrowsec), "t") == 0);
    tblinfo[i].forcerowsec = (strcmp(PQgetvalue(res, i, i_relforcerowsec), "t") == 0);
    tblinfo[i].hasoids = (strcmp(PQgetvalue(res, i, i_relhasoids), "t") == 0);
    tblinfo[i].relispopulated = (strcmp(PQgetvalue(res, i, i_relispopulated), "t") == 0);
    tblinfo[i].relreplident = *(PQgetvalue(res, i, i_relreplident));
    tblinfo[i].relpages = atoi(PQgetvalue(res, i, i_relpages));
    tblinfo[i].frozenxid = atooid(PQgetvalue(res, i, i_relfrozenxid));
    tblinfo[i].minmxid = atooid(PQgetvalue(res, i, i_relminmxid));
    tblinfo[i].toast_oid = atooid(PQgetvalue(res, i, i_toastoid));
    tblinfo[i].toast_frozenxid = atooid(PQgetvalue(res, i, i_toastfrozenxid));
    tblinfo[i].toast_minmxid = atooid(PQgetvalue(res, i, i_toastminmxid));
    tblinfo[i].reloftype = atooid(PQgetvalue(res, i, i_reloftype));
    tblinfo[i].ncheck = atoi(PQgetvalue(res, i, i_relchecks));
    if (PQgetisnull(res, i, i_owning_tab))
    {
      tblinfo[i].owning_tab = InvalidOid;
      tblinfo[i].owning_col = 0;
    }
    else
    {
      tblinfo[i].owning_tab = atooid(PQgetvalue(res, i, i_owning_tab));
      tblinfo[i].owning_col = atoi(PQgetvalue(res, i, i_owning_col));
    }
    tblinfo[i].reltablespace = pg_strdup(PQgetvalue(res, i, i_reltablespace));
    tblinfo[i].reloptions = pg_strdup(PQgetvalue(res, i, i_reloptions));
    if (i_checkoption == -1 || PQgetisnull(res, i, i_checkoption))
    {
      tblinfo[i].checkoption = NULL;
    }
    else
    {
      tblinfo[i].checkoption = pg_strdup(PQgetvalue(res, i, i_checkoption));
    }
    tblinfo[i].toast_reloptions = pg_strdup(PQgetvalue(res, i, i_toastreloptions));
    if (PQgetisnull(res, i, i_amname))
    {
      tblinfo[i].amname = NULL;
    }
    else
    {
      tblinfo[i].amname = pg_strdup(PQgetvalue(res, i, i_amname));
    }

                                        

       
                                                  
       
    if (tblinfo[i].relkind == RELKIND_COMPOSITE_TYPE)
    {
      tblinfo[i].dobj.dump = DUMP_COMPONENT_NONE;
    }
    else
    {
      selectDumpableTable(&tblinfo[i], fout);
    }

       
                                                                       
                                                                           
                                                                    
                                                               
       
                                                                         
                                                                       
       
    if (PQgetisnull(res, i, i_relacl) && PQgetisnull(res, i, i_rrelacl) && PQgetisnull(res, i, i_initrelacl) && PQgetisnull(res, i, i_initrrelacl) && strcmp(PQgetvalue(res, i, i_changed_acl), "f") == 0)
    {
      tblinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }

    tblinfo[i].interesting = tblinfo[i].dobj.dump ? true : false;
    tblinfo[i].dummy_view = false;                                   
    tblinfo[i].postponed_def = false;                                

    tblinfo[i].is_identity_sequence = (i_is_identity_sequence >= 0 && strcmp(PQgetvalue(res, i, i_is_identity_sequence), "t") == 0);

                    
    tblinfo[i].ispartition = (strcmp(PQgetvalue(res, i, i_ispartition), "t") == 0);

       
                                                                           
                                                       
       
                                                                           
                                                                
                                     
       
                                                                      
                                                                      
                   
       
                                                                  
                 
       
    if (tblinfo[i].dobj.dump && (tblinfo[i].relkind == RELKIND_RELATION || tblinfo[i].relkind == RELKIND_PARTITIONED_TABLE) && (tblinfo[i].dobj.dump & DUMP_COMPONENTS_REQUIRING_LOCK))
    {
      resetPQExpBuffer(query);
      appendPQExpBuffer(query, "LOCK TABLE %s IN ACCESS SHARE MODE", fmtQualifiedDumpable(&tblinfo[i]));
      ExecuteSqlStatement(fout, query->data);
    }

                                              
    if (strlen(tblinfo[i].rolname) == 0)
    {
      pg_log_warning("owner of table \"%s\" appears to be invalid", tblinfo[i].dobj.name);
    }
  }

  if (dopt->lockWaitTimeout)
  {
    ExecuteSqlStatement(fout, "SET statement_timeout = 0");
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return tblinfo;
}

   
                
                                                                           
   
                                                                         
                                                   
   
void
getOwnedSeqs(Archive *fout, TableInfo tblinfo[], int numTables)
{
  int i;

     
                                                                             
                                         
     
  for (i = 0; i < numTables; i++)
  {
    TableInfo *seqinfo = &tblinfo[i];
    TableInfo *owning_tab;

    if (!OidIsValid(seqinfo->owning_tab))
    {
      continue;                            
    }

    owning_tab = findTableByOid(seqinfo->owning_tab);
    if (owning_tab == NULL)
    {
      fatal("failed sanity check, parent table with OID %u of sequence with OID %u not found", seqinfo->owning_tab, seqinfo->dobj.catId.oid);
    }

       
                                                                          
                      
       
    if (owning_tab->dobj.dump == DUMP_COMPONENT_NONE && seqinfo->is_identity_sequence)
    {
      seqinfo->dobj.dump = DUMP_COMPONENT_NONE;
      continue;
    }

       
                                                                          
                                                                     
                    
       
                                                                        
                                                                         
                                                                           
                                                                  
                                                                    
                
       
                                                                           
                                                                        
                                              
       
    seqinfo->dobj.dump = seqinfo->dobj.dump | owning_tab->dobj.dump;

    if (seqinfo->dobj.dump != DUMP_COMPONENT_NONE)
    {
      seqinfo->interesting = true;
    }
  }
}

   
               
                                          
                                                                  
   
                                                     
   
InhInfo *
getInherits(Archive *fout, int *numInherits)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  InhInfo *inhinfo;

  int i_inhrelid;
  int i_inhparent;

     
                                                                          
                                                                           
                                                                          
                   
     
  appendPQExpBufferStr(query, "SELECT inhrelid, inhparent FROM pg_inherits");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numInherits = ntups;

  inhinfo = (InhInfo *)pg_malloc(ntups * sizeof(InhInfo));

  i_inhrelid = PQfnumber(res, "inhrelid");
  i_inhparent = PQfnumber(res, "inhparent");

  for (i = 0; i < ntups; i++)
  {
    inhinfo[i].inhrelid = atooid(PQgetvalue(res, i, i_inhrelid));
    inhinfo[i].inhparent = atooid(PQgetvalue(res, i, i_inhparent));
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return inhinfo;
}

   
              
                                                           
   
                                                                   
                                                    
   
void
getIndexes(Archive *fout, TableInfo tblinfo[], int numTables)
{
  int i, j;
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  IndxInfo *indxinfo;
  ConstraintInfo *constrinfo;
  int i_tableoid, i_oid, i_indexname, i_parentidx, i_indexdef, i_indnkeyatts, i_indnatts, i_indkey, i_indisclustered, i_indisreplident, i_contype, i_conname, i_condeferrable, i_condeferred, i_contableoid, i_conoid, i_condef, i_tablespace, i_indreloptions, i_indstatcols, i_indstatvals;
  int ntups;

  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &tblinfo[i];

    if (!tbinfo->hasindex)
    {
      continue;
    }

       
                                                                        
       
                                                                           
                                                                  
       
    if (!(tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION) && !tbinfo->interesting)
    {
      continue;
    }

    pg_log_info("reading indexes for table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, tbinfo->dobj.name);

       
                                                                         
                                                                          
                                                                          
                                                                     
       
                                                                        
                                                                  
                                                                          
               
       
    resetPQExpBuffer(query);
    if (fout->remoteVersion >= 110000)
    {
      appendPQExpBuffer(query,
          "SELECT t.tableoid, t.oid, "
          "t.relname AS indexname, "
          "inh.inhparent AS parentidx, "
          "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
          "i.indnkeyatts AS indnkeyatts, "
          "i.indnatts AS indnatts, "
          "i.indkey, i.indisclustered, "
          "i.indisreplident, "
          "c.contype, c.conname, "
          "c.condeferrable, c.condeferred, "
          "c.tableoid AS contableoid, "
          "c.oid AS conoid, "
          "pg_catalog.pg_get_constraintdef(c.oid, false) AS condef, "
          "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
          "t.reloptions AS indreloptions, "
          "(SELECT pg_catalog.array_agg(attnum ORDER BY attnum) "
          "  FROM pg_catalog.pg_attribute "
          "  WHERE attrelid = i.indexrelid AND "
          "    attstattarget >= 0) AS indstatcols,"
          "(SELECT pg_catalog.array_agg(attstattarget ORDER BY attnum) "
          "  FROM pg_catalog.pg_attribute "
          "  WHERE attrelid = i.indexrelid AND "
          "    attstattarget >= 0) AS indstatvals "
          "FROM pg_catalog.pg_index i "
          "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
          "JOIN pg_catalog.pg_class t2 ON (t2.oid = i.indrelid) "
          "LEFT JOIN pg_catalog.pg_constraint c "
          "ON (i.indrelid = c.conrelid AND "
          "i.indexrelid = c.conindid AND "
          "c.contype IN ('p','u','x')) "
          "LEFT JOIN pg_catalog.pg_inherits inh "
          "ON (inh.inhrelid = indexrelid) "
          "WHERE i.indrelid = '%u'::pg_catalog.oid "
          "AND (i.indisvalid OR t2.relkind = 'p') "
          "AND i.indisready "
          "ORDER BY indexname",
          tbinfo->dobj.catId.oid);
    }
    else if (fout->remoteVersion >= 90400)
    {
         
                                                                     
                                
         
      appendPQExpBuffer(query,
          "SELECT t.tableoid, t.oid, "
          "t.relname AS indexname, "
          "0 AS parentidx, "
          "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
          "i.indnatts AS indnkeyatts, "
          "i.indnatts AS indnatts, "
          "i.indkey, i.indisclustered, "
          "i.indisreplident, "
          "c.contype, c.conname, "
          "c.condeferrable, c.condeferred, "
          "c.tableoid AS contableoid, "
          "c.oid AS conoid, "
          "pg_catalog.pg_get_constraintdef(c.oid, false) AS condef, "
          "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
          "t.reloptions AS indreloptions, "
          "'' AS indstatcols, "
          "'' AS indstatvals "
          "FROM pg_catalog.pg_index i "
          "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
          "LEFT JOIN pg_catalog.pg_constraint c "
          "ON (i.indrelid = c.conrelid AND "
          "i.indexrelid = c.conindid AND "
          "c.contype IN ('p','u','x')) "
          "WHERE i.indrelid = '%u'::pg_catalog.oid "
          "AND i.indisvalid AND i.indisready "
          "ORDER BY indexname",
          tbinfo->dobj.catId.oid);
    }
    else if (fout->remoteVersion >= 90000)
    {
         
                                                                     
                                
         
      appendPQExpBuffer(query,
          "SELECT t.tableoid, t.oid, "
          "t.relname AS indexname, "
          "0 AS parentidx, "
          "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
          "i.indnatts AS indnkeyatts, "
          "i.indnatts AS indnatts, "
          "i.indkey, i.indisclustered, "
          "false AS indisreplident, "
          "c.contype, c.conname, "
          "c.condeferrable, c.condeferred, "
          "c.tableoid AS contableoid, "
          "c.oid AS conoid, "
          "pg_catalog.pg_get_constraintdef(c.oid, false) AS condef, "
          "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
          "t.reloptions AS indreloptions, "
          "'' AS indstatcols, "
          "'' AS indstatvals "
          "FROM pg_catalog.pg_index i "
          "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
          "LEFT JOIN pg_catalog.pg_constraint c "
          "ON (i.indrelid = c.conrelid AND "
          "i.indexrelid = c.conindid AND "
          "c.contype IN ('p','u','x')) "
          "WHERE i.indrelid = '%u'::pg_catalog.oid "
          "AND i.indisvalid AND i.indisready "
          "ORDER BY indexname",
          tbinfo->dobj.catId.oid);
    }
    else if (fout->remoteVersion >= 80200)
    {
      appendPQExpBuffer(query,
          "SELECT t.tableoid, t.oid, "
          "t.relname AS indexname, "
          "0 AS parentidx, "
          "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
          "i.indnatts AS indnkeyatts, "
          "i.indnatts AS indnatts, "
          "i.indkey, i.indisclustered, "
          "false AS indisreplident, "
          "c.contype, c.conname, "
          "c.condeferrable, c.condeferred, "
          "c.tableoid AS contableoid, "
          "c.oid AS conoid, "
          "null AS condef, "
          "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
          "t.reloptions AS indreloptions, "
          "'' AS indstatcols, "
          "'' AS indstatvals "
          "FROM pg_catalog.pg_index i "
          "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
          "LEFT JOIN pg_catalog.pg_depend d "
          "ON (d.classid = t.tableoid "
          "AND d.objid = t.oid "
          "AND d.deptype = 'i') "
          "LEFT JOIN pg_catalog.pg_constraint c "
          "ON (d.refclassid = c.tableoid "
          "AND d.refobjid = c.oid) "
          "WHERE i.indrelid = '%u'::pg_catalog.oid "
          "AND i.indisvalid "
          "ORDER BY indexname",
          tbinfo->dobj.catId.oid);
    }
    else
    {
      appendPQExpBuffer(query,
          "SELECT t.tableoid, t.oid, "
          "t.relname AS indexname, "
          "0 AS parentidx, "
          "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
          "t.relnatts AS indnkeyatts, "
          "t.relnatts AS indnatts, "
          "i.indkey, i.indisclustered, "
          "false AS indisreplident, "
          "c.contype, c.conname, "
          "c.condeferrable, c.condeferred, "
          "c.tableoid AS contableoid, "
          "c.oid AS conoid, "
          "null AS condef, "
          "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
          "null AS indreloptions, "
          "'' AS indstatcols, "
          "'' AS indstatvals "
          "FROM pg_catalog.pg_index i "
          "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
          "LEFT JOIN pg_catalog.pg_depend d "
          "ON (d.classid = t.tableoid "
          "AND d.objid = t.oid "
          "AND d.deptype = 'i') "
          "LEFT JOIN pg_catalog.pg_constraint c "
          "ON (d.refclassid = c.tableoid "
          "AND d.refobjid = c.oid) "
          "WHERE i.indrelid = '%u'::pg_catalog.oid "
          "ORDER BY indexname",
          tbinfo->dobj.catId.oid);
    }

    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

    ntups = PQntuples(res);

    i_tableoid = PQfnumber(res, "tableoid");
    i_oid = PQfnumber(res, "oid");
    i_indexname = PQfnumber(res, "indexname");
    i_parentidx = PQfnumber(res, "parentidx");
    i_indexdef = PQfnumber(res, "indexdef");
    i_indnkeyatts = PQfnumber(res, "indnkeyatts");
    i_indnatts = PQfnumber(res, "indnatts");
    i_indkey = PQfnumber(res, "indkey");
    i_indisclustered = PQfnumber(res, "indisclustered");
    i_indisreplident = PQfnumber(res, "indisreplident");
    i_contype = PQfnumber(res, "contype");
    i_conname = PQfnumber(res, "conname");
    i_condeferrable = PQfnumber(res, "condeferrable");
    i_condeferred = PQfnumber(res, "condeferred");
    i_contableoid = PQfnumber(res, "contableoid");
    i_conoid = PQfnumber(res, "conoid");
    i_condef = PQfnumber(res, "condef");
    i_tablespace = PQfnumber(res, "tablespace");
    i_indreloptions = PQfnumber(res, "indreloptions");
    i_indstatcols = PQfnumber(res, "indstatcols");
    i_indstatvals = PQfnumber(res, "indstatvals");

    tbinfo->indexes = indxinfo = (IndxInfo *)pg_malloc(ntups * sizeof(IndxInfo));
    constrinfo = (ConstraintInfo *)pg_malloc(ntups * sizeof(ConstraintInfo));
    tbinfo->numIndexes = ntups;

    for (j = 0; j < ntups; j++)
    {
      char contype;

      indxinfo[j].dobj.objType = DO_INDEX;
      indxinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
      indxinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
      AssignDumpId(&indxinfo[j].dobj);
      indxinfo[j].dobj.dump = tbinfo->dobj.dump;
      indxinfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_indexname));
      indxinfo[j].dobj.namespace = tbinfo->dobj.namespace;
      indxinfo[j].indextable = tbinfo;
      indxinfo[j].indexdef = pg_strdup(PQgetvalue(res, j, i_indexdef));
      indxinfo[j].indnkeyattrs = atoi(PQgetvalue(res, j, i_indnkeyatts));
      indxinfo[j].indnattrs = atoi(PQgetvalue(res, j, i_indnatts));
      indxinfo[j].tablespace = pg_strdup(PQgetvalue(res, j, i_tablespace));
      indxinfo[j].indreloptions = pg_strdup(PQgetvalue(res, j, i_indreloptions));
      indxinfo[j].indstatcols = pg_strdup(PQgetvalue(res, j, i_indstatcols));
      indxinfo[j].indstatvals = pg_strdup(PQgetvalue(res, j, i_indstatvals));
      indxinfo[j].indkeys = (Oid *)pg_malloc(indxinfo[j].indnattrs * sizeof(Oid));
      parseOidArray(PQgetvalue(res, j, i_indkey), indxinfo[j].indkeys, indxinfo[j].indnattrs);
      indxinfo[j].indisclustered = (PQgetvalue(res, j, i_indisclustered)[0] == 't');
      indxinfo[j].indisreplident = (PQgetvalue(res, j, i_indisreplident)[0] == 't');
      indxinfo[j].parentidx = atooid(PQgetvalue(res, j, i_parentidx));
      indxinfo[j].partattaches = (SimplePtrList){NULL, NULL};
      contype = *(PQgetvalue(res, j, i_contype));

      if (contype == 'p' || contype == 'u' || contype == 'x')
      {
           
                                                                  
                         
           
        constrinfo[j].dobj.objType = DO_CONSTRAINT;
        constrinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_contableoid));
        constrinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_conoid));
        AssignDumpId(&constrinfo[j].dobj);
        constrinfo[j].dobj.dump = tbinfo->dobj.dump;
        constrinfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_conname));
        constrinfo[j].dobj.namespace = tbinfo->dobj.namespace;
        constrinfo[j].contable = tbinfo;
        constrinfo[j].condomain = NULL;
        constrinfo[j].contype = contype;
        if (contype == 'x')
        {
          constrinfo[j].condef = pg_strdup(PQgetvalue(res, j, i_condef));
        }
        else
        {
          constrinfo[j].condef = NULL;
        }
        constrinfo[j].confrelid = InvalidOid;
        constrinfo[j].conindex = indxinfo[j].dobj.dumpId;
        constrinfo[j].condeferrable = *(PQgetvalue(res, j, i_condeferrable)) == 't';
        constrinfo[j].condeferred = *(PQgetvalue(res, j, i_condeferred)) == 't';
        constrinfo[j].conislocal = true;
        constrinfo[j].separate = true;

        indxinfo[j].indexconstraint = constrinfo[j].dobj.dumpId;
      }
      else
      {
                                   
        indxinfo[j].indexconstraint = 0;
      }
    }

    PQclear(res);
  }

  destroyPQExpBuffer(query);
}

   
                         
                                                        
   
                                                                              
                                                       
   
void
getExtendedStatistics(Archive *fout)
{
  PQExpBuffer query;
  PGresult *res;
  StatsExtInfo *statsextinfo;
  int ntups;
  int i_tableoid;
  int i_oid;
  int i_stxname;
  int i_stxnamespace;
  int i_rolname;
  int i;

                                           
  if (fout->remoteVersion < 100000)
  {
    return;
  }

  query = createPQExpBuffer();

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, stxname, "
      "stxnamespace, (%s stxowner) AS rolname "
      "FROM pg_catalog.pg_statistic_ext",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_stxname = PQfnumber(res, "stxname");
  i_stxnamespace = PQfnumber(res, "stxnamespace");
  i_rolname = PQfnumber(res, "rolname");

  statsextinfo = (StatsExtInfo *)pg_malloc(ntups * sizeof(StatsExtInfo));

  for (i = 0; i < ntups; i++)
  {
    statsextinfo[i].dobj.objType = DO_STATSEXT;
    statsextinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    statsextinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&statsextinfo[i].dobj);
    statsextinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_stxname));
    statsextinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_stxnamespace)));
    statsextinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));

                                           
    selectDumpableObject(&(statsextinfo[i].dobj), fout);

                                                   
    statsextinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);
  destroyPQExpBuffer(query);
}

   
                  
   
                                                  
   
                                        
                                                                
                                                             
   
void
getConstraints(Archive *fout, TableInfo tblinfo[], int numTables)
{
  int i, j;
  ConstraintInfo *constrinfo;
  PQExpBuffer query;
  PGresult *res;
  int i_contableoid, i_conoid, i_conname, i_confrelid, i_conindid, i_condef;
  int ntups;

  query = createPQExpBuffer();

  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &tblinfo[i];

       
                                                                          
                                                                 
       
    if ((!tbinfo->hastriggers && tbinfo->relkind != RELKIND_PARTITIONED_TABLE) || !(tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
    {
      continue;
    }

    pg_log_info("reading foreign key constraints for table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, tbinfo->dobj.name);

    resetPQExpBuffer(query);
    if (fout->remoteVersion >= 110000)
    {
      appendPQExpBuffer(query,
          "SELECT tableoid, oid, conname, confrelid, conindid, "
          "pg_catalog.pg_get_constraintdef(oid) AS condef "
          "FROM pg_catalog.pg_constraint "
          "WHERE conrelid = '%u'::pg_catalog.oid "
          "AND conparentid = 0 "
          "AND contype = 'f'",
          tbinfo->dobj.catId.oid);
    }
    else
    {
      appendPQExpBuffer(query,
          "SELECT tableoid, oid, conname, confrelid, 0 as conindid, "
          "pg_catalog.pg_get_constraintdef(oid) AS condef "
          "FROM pg_catalog.pg_constraint "
          "WHERE conrelid = '%u'::pg_catalog.oid "
          "AND contype = 'f'",
          tbinfo->dobj.catId.oid);
    }
    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

    ntups = PQntuples(res);

    i_contableoid = PQfnumber(res, "tableoid");
    i_conoid = PQfnumber(res, "oid");
    i_conname = PQfnumber(res, "conname");
    i_confrelid = PQfnumber(res, "confrelid");
    i_conindid = PQfnumber(res, "conindid");
    i_condef = PQfnumber(res, "condef");

    constrinfo = (ConstraintInfo *)pg_malloc(ntups * sizeof(ConstraintInfo));

    for (j = 0; j < ntups; j++)
    {
      TableInfo *reftable;

      constrinfo[j].dobj.objType = DO_FK_CONSTRAINT;
      constrinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_contableoid));
      constrinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_conoid));
      AssignDumpId(&constrinfo[j].dobj);
      constrinfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_conname));
      constrinfo[j].dobj.namespace = tbinfo->dobj.namespace;
      constrinfo[j].contable = tbinfo;
      constrinfo[j].condomain = NULL;
      constrinfo[j].contype = 'f';
      constrinfo[j].condef = pg_strdup(PQgetvalue(res, j, i_condef));
      constrinfo[j].confrelid = atooid(PQgetvalue(res, j, i_confrelid));
      constrinfo[j].conindex = 0;
      constrinfo[j].condeferrable = false;
      constrinfo[j].condeferred = false;
      constrinfo[j].conislocal = true;
      constrinfo[j].separate = true;

         
                                                                     
                                                                   
                                                                     
                                        
         
      reftable = findTableByOid(constrinfo[j].confrelid);
      if (reftable && reftable->relkind == RELKIND_PARTITIONED_TABLE)
      {
        Oid indexOid = atooid(PQgetvalue(res, j, i_conindid));

        if (indexOid != InvalidOid)
        {
          for (int k = 0; k < reftable->numIndexes; k++)
          {
            IndxInfo *refidx;

                                
            if (reftable->indexes[k].dobj.catId.oid != indexOid)
            {
              continue;
            }

            refidx = &reftable->indexes[k];
            addConstrChildIdxDeps(&constrinfo[j].dobj, refidx);
            break;
          }
        }
      }
    }

    PQclear(res);
  }

  destroyPQExpBuffer(query);
}

   
                         
   
                                           
   
                                                                             
                                                                            
                                                                        
                                                                            
          
   
static void
addConstrChildIdxDeps(DumpableObject *dobj, IndxInfo *refidx)
{
  SimplePtrListCell *cell;

  Assert(dobj->objType == DO_FK_CONSTRAINT);

  for (cell = refidx->partattaches.head; cell; cell = cell->next)
  {
    DumpableObject *childobj = (DumpableObject *)cell->ptr;
    IndexAttachInfo *attach;

    addObjectDependency(dobj, childobj->dumpId);

    attach = (IndexAttachInfo *)childobj;
    if (attach->partitionIdx->partattaches.head != NULL)
    {
      addConstrChildIdxDeps(dobj, attach->partitionIdx);
    }
  }
}

   
                        
   
                                           
   
static void
getDomainConstraints(Archive *fout, TypeInfo *tyinfo)
{
  int i;
  ConstraintInfo *constrinfo;
  PQExpBuffer query;
  PGresult *res;
  int i_tableoid, i_oid, i_conname, i_consrc;
  int ntups;

  query = createPQExpBuffer();

  if (fout->remoteVersion >= 90100)
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, conname, "
        "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
        "convalidated "
        "FROM pg_catalog.pg_constraint "
        "WHERE contypid = '%u'::pg_catalog.oid "
        "ORDER BY conname",
        tyinfo->dobj.catId.oid);
  }

  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, conname, "
        "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
        "true as convalidated "
        "FROM pg_catalog.pg_constraint "
        "WHERE contypid = '%u'::pg_catalog.oid "
        "ORDER BY conname",
        tyinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_conname = PQfnumber(res, "conname");
  i_consrc = PQfnumber(res, "consrc");

  constrinfo = (ConstraintInfo *)pg_malloc(ntups * sizeof(ConstraintInfo));

  tyinfo->nDomChecks = ntups;
  tyinfo->domChecks = constrinfo;

  for (i = 0; i < ntups; i++)
  {
    bool validated = PQgetvalue(res, i, 4)[0] == 't';

    constrinfo[i].dobj.objType = DO_CONSTRAINT;
    constrinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    constrinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&constrinfo[i].dobj);
    constrinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_conname));
    constrinfo[i].dobj.namespace = tyinfo->dobj.namespace;
    constrinfo[i].contable = NULL;
    constrinfo[i].condomain = tyinfo;
    constrinfo[i].contype = 'c';
    constrinfo[i].condef = pg_strdup(PQgetvalue(res, i, i_consrc));
    constrinfo[i].confrelid = InvalidOid;
    constrinfo[i].conindex = 0;
    constrinfo[i].condeferrable = false;
    constrinfo[i].condeferred = false;
    constrinfo[i].conislocal = true;

    constrinfo[i].separate = !validated;

       
                                                                      
                                                                          
                                                                        
                                       
       
    if (validated)
    {
      addObjectDependency(&tyinfo->dobj, constrinfo[i].dobj.dumpId);
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);
}

   
            
                                                          
   
                                                  
   
RuleInfo *
getRules(Archive *fout, int *numRules)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  RuleInfo *ruleinfo;
  int i_tableoid;
  int i_oid;
  int i_rulename;
  int i_ruletable;
  int i_ev_type;
  int i_is_instead;
  int i_ev_enabled;

  if (fout->remoteVersion >= 80300)
  {
    appendPQExpBufferStr(query, "SELECT "
                                "tableoid, oid, rulename, "
                                "ev_class AS ruletable, ev_type, is_instead, "
                                "ev_enabled "
                                "FROM pg_rewrite "
                                "ORDER BY oid");
  }
  else
  {
    appendPQExpBufferStr(query, "SELECT "
                                "tableoid, oid, rulename, "
                                "ev_class AS ruletable, ev_type, is_instead, "
                                "'O'::char AS ev_enabled "
                                "FROM pg_rewrite "
                                "ORDER BY oid");
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numRules = ntups;

  ruleinfo = (RuleInfo *)pg_malloc(ntups * sizeof(RuleInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_rulename = PQfnumber(res, "rulename");
  i_ruletable = PQfnumber(res, "ruletable");
  i_ev_type = PQfnumber(res, "ev_type");
  i_is_instead = PQfnumber(res, "is_instead");
  i_ev_enabled = PQfnumber(res, "ev_enabled");

  for (i = 0; i < ntups; i++)
  {
    Oid ruletableoid;

    ruleinfo[i].dobj.objType = DO_RULE;
    ruleinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    ruleinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&ruleinfo[i].dobj);
    ruleinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_rulename));
    ruletableoid = atooid(PQgetvalue(res, i, i_ruletable));
    ruleinfo[i].ruletable = findTableByOid(ruletableoid);
    if (ruleinfo[i].ruletable == NULL)
    {
      fatal("failed sanity check, parent table with OID %u of pg_rewrite entry with OID %u not found", ruletableoid, ruleinfo[i].dobj.catId.oid);
    }
    ruleinfo[i].dobj.namespace = ruleinfo[i].ruletable->dobj.namespace;
    ruleinfo[i].dobj.dump = ruleinfo[i].ruletable->dobj.dump;
    ruleinfo[i].ev_type = *(PQgetvalue(res, i, i_ev_type));
    ruleinfo[i].is_instead = *(PQgetvalue(res, i, i_is_instead)) == 't';
    ruleinfo[i].ev_enabled = *(PQgetvalue(res, i, i_ev_enabled));
    if (ruleinfo[i].ruletable)
    {
         
                                                                   
                                                                  
                                                                       
                                                                   
                
         
      if ((ruleinfo[i].ruletable->relkind == RELKIND_VIEW || ruleinfo[i].ruletable->relkind == RELKIND_MATVIEW) && ruleinfo[i].ev_type == '1' && ruleinfo[i].is_instead)
      {
        addObjectDependency(&ruleinfo[i].ruletable->dobj, ruleinfo[i].dobj.dumpId);
                                                                
        ruleinfo[i].separate = false;
      }
      else
      {
        addObjectDependency(&ruleinfo[i].dobj, ruleinfo[i].ruletable->dobj.dumpId);
        ruleinfo[i].separate = true;
      }
    }
    else
    {
      ruleinfo[i].separate = true;
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return ruleinfo;
}

   
               
                                                             
   
                                                                     
                                                    
   
void
getTriggers(Archive *fout, TableInfo tblinfo[], int numTables)
{
  int i, j;
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  TriggerInfo *tginfo;
  int i_tableoid, i_oid, i_tgname, i_tgfname, i_tgtype, i_tgnargs, i_tgargs, i_tgisconstraint, i_tgconstrname, i_tgconstrrelid, i_tgconstrrelname, i_tgenabled, i_tgisinternal, i_tgdeferrable, i_tginitdeferred, i_tgdef;
  int ntups;

  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &tblinfo[i];

    if (!tbinfo->hastriggers || !(tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION))
    {
      continue;
    }

    pg_log_info("reading triggers for table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, tbinfo->dobj.name);

    resetPQExpBuffer(query);
    if (fout->remoteVersion >= 130000)
    {
         
                                                                    
                                                                      
                                        
         
                                                                         
                                                              
         
      appendPQExpBuffer(query,
          "SELECT t.tgname, "
          "t.tgfoid::pg_catalog.regproc AS tgfname, "
          "pg_catalog.pg_get_triggerdef(t.oid, false) AS tgdef, "
          "t.tgenabled, t.tableoid, t.oid, t.tgisinternal "
          "FROM pg_catalog.pg_trigger t "
          "LEFT JOIN pg_catalog.pg_trigger u ON u.oid = t.tgparentid "
          "WHERE t.tgrelid = '%u'::pg_catalog.oid "
          "AND (NOT t.tgisinternal OR t.tgenabled != u.tgenabled)",
          tbinfo->dobj.catId.oid);
    }
    else if (fout->remoteVersion >= 110000)
    {
         
                                                                         
                                                                 
                                                                   
                    
         
                                                           
         
      appendPQExpBuffer(query,
          "SELECT t.tgname, "
          "t.tgfoid::pg_catalog.regproc AS tgfname, "
          "pg_catalog.pg_get_triggerdef(t.oid, false) AS tgdef, "
          "t.tgenabled, t.tableoid, t.oid, t.tgisinternal "
          "FROM pg_catalog.pg_trigger t "
          "LEFT JOIN pg_catalog.pg_depend AS d ON "
          " d.classid = 'pg_catalog.pg_trigger'::pg_catalog.regclass AND "
          " d.refclassid = 'pg_catalog.pg_trigger'::pg_catalog.regclass AND "
          " d.objid = t.oid "
          "LEFT JOIN pg_catalog.pg_trigger AS pt ON pt.oid = refobjid "
          "WHERE t.tgrelid = '%u'::pg_catalog.oid "
          "AND (NOT t.tgisinternal%s)",
          tbinfo->dobj.catId.oid, tbinfo->ispartition ? " OR t.tgenabled != pt.tgenabled" : "");
    }
    else if (fout->remoteVersion >= 90000)
    {
                                                            
      appendPQExpBuffer(query,
          "SELECT t.tgname, "
          "t.tgfoid::pg_catalog.regproc AS tgfname, "
          "pg_catalog.pg_get_triggerdef(t.oid, false) AS tgdef, "
          "t.tgenabled, false as tgisinternal, "
          "t.tableoid, t.oid "
          "FROM pg_catalog.pg_trigger t "
          "WHERE tgrelid = '%u'::pg_catalog.oid "
          "AND NOT tgisinternal",
          tbinfo->dobj.catId.oid);
    }
    else if (fout->remoteVersion >= 80300)
    {
         
                                                                      
         
      appendPQExpBuffer(query,
          "SELECT tgname, "
          "tgfoid::pg_catalog.regproc AS tgfname, "
          "tgtype, tgnargs, tgargs, tgenabled, "
          "false as tgisinternal, "
          "tgisconstraint, tgconstrname, tgdeferrable, "
          "tgconstrrelid, tginitdeferred, tableoid, oid, "
          "tgconstrrelid::pg_catalog.regclass AS tgconstrrelname "
          "FROM pg_catalog.pg_trigger t "
          "WHERE tgrelid = '%u'::pg_catalog.oid "
          "AND tgconstraint = 0",
          tbinfo->dobj.catId.oid);
    }
    else
    {
         
                                                                       
                                                                       
                     
         
      appendPQExpBuffer(query,
          "SELECT tgname, "
          "tgfoid::pg_catalog.regproc AS tgfname, "
          "tgtype, tgnargs, tgargs, tgenabled, "
          "false as tgisinternal, "
          "tgisconstraint, tgconstrname, tgdeferrable, "
          "tgconstrrelid, tginitdeferred, tableoid, oid, "
          "tgconstrrelid::pg_catalog.regclass AS tgconstrrelname "
          "FROM pg_catalog.pg_trigger t "
          "WHERE tgrelid = '%u'::pg_catalog.oid "
          "AND (NOT tgisconstraint "
          " OR NOT EXISTS"
          "  (SELECT 1 FROM pg_catalog.pg_depend d "
          "   JOIN pg_catalog.pg_constraint c ON (d.refclassid = c.tableoid AND d.refobjid = c.oid) "
          "   WHERE d.classid = t.tableoid AND d.objid = t.oid AND d.deptype = 'i' AND c.contype = 'f'))",
          tbinfo->dobj.catId.oid);
    }

    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

    ntups = PQntuples(res);

    i_tableoid = PQfnumber(res, "tableoid");
    i_oid = PQfnumber(res, "oid");
    i_tgname = PQfnumber(res, "tgname");
    i_tgfname = PQfnumber(res, "tgfname");
    i_tgtype = PQfnumber(res, "tgtype");
    i_tgnargs = PQfnumber(res, "tgnargs");
    i_tgargs = PQfnumber(res, "tgargs");
    i_tgisconstraint = PQfnumber(res, "tgisconstraint");
    i_tgconstrname = PQfnumber(res, "tgconstrname");
    i_tgconstrrelid = PQfnumber(res, "tgconstrrelid");
    i_tgconstrrelname = PQfnumber(res, "tgconstrrelname");
    i_tgenabled = PQfnumber(res, "tgenabled");
    i_tgisinternal = PQfnumber(res, "tgisinternal");
    i_tgdeferrable = PQfnumber(res, "tgdeferrable");
    i_tginitdeferred = PQfnumber(res, "tginitdeferred");
    i_tgdef = PQfnumber(res, "tgdef");

    tginfo = (TriggerInfo *)pg_malloc(ntups * sizeof(TriggerInfo));

    tbinfo->numTriggers = ntups;
    tbinfo->triggers = tginfo;

    for (j = 0; j < ntups; j++)
    {
      tginfo[j].dobj.objType = DO_TRIGGER;
      tginfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
      tginfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
      AssignDumpId(&tginfo[j].dobj);
      tginfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_tgname));
      tginfo[j].dobj.namespace = tbinfo->dobj.namespace;
      tginfo[j].tgtable = tbinfo;
      tginfo[j].tgenabled = *(PQgetvalue(res, j, i_tgenabled));
      tginfo[j].tgisinternal = *(PQgetvalue(res, j, i_tgisinternal)) == 't';
      if (i_tgdef >= 0)
      {
        tginfo[j].tgdef = pg_strdup(PQgetvalue(res, j, i_tgdef));

                                                             
        tginfo[j].tgfname = NULL;
        tginfo[j].tgtype = 0;
        tginfo[j].tgnargs = 0;
        tginfo[j].tgargs = NULL;
        tginfo[j].tgisconstraint = false;
        tginfo[j].tgdeferrable = false;
        tginfo[j].tginitdeferred = false;
        tginfo[j].tgconstrname = NULL;
        tginfo[j].tgconstrrelid = InvalidOid;
        tginfo[j].tgconstrrelname = NULL;
      }
      else
      {
        tginfo[j].tgdef = NULL;

        tginfo[j].tgfname = pg_strdup(PQgetvalue(res, j, i_tgfname));
        tginfo[j].tgtype = atoi(PQgetvalue(res, j, i_tgtype));
        tginfo[j].tgnargs = atoi(PQgetvalue(res, j, i_tgnargs));
        tginfo[j].tgargs = pg_strdup(PQgetvalue(res, j, i_tgargs));
        tginfo[j].tgisconstraint = *(PQgetvalue(res, j, i_tgisconstraint)) == 't';
        tginfo[j].tgdeferrable = *(PQgetvalue(res, j, i_tgdeferrable)) == 't';
        tginfo[j].tginitdeferred = *(PQgetvalue(res, j, i_tginitdeferred)) == 't';

        if (tginfo[j].tgisconstraint)
        {
          tginfo[j].tgconstrname = pg_strdup(PQgetvalue(res, j, i_tgconstrname));
          tginfo[j].tgconstrrelid = atooid(PQgetvalue(res, j, i_tgconstrrelid));
          if (OidIsValid(tginfo[j].tgconstrrelid))
          {
            if (PQgetisnull(res, j, i_tgconstrrelname))
            {
              fatal("query produced null referenced table name for foreign key trigger \"%s\" on table \"%s\" (OID of table: %u)", tginfo[j].dobj.name, tbinfo->dobj.name, tginfo[j].tgconstrrelid);
            }
            tginfo[j].tgconstrrelname = pg_strdup(PQgetvalue(res, j, i_tgconstrrelname));
          }
          else
          {
            tginfo[j].tgconstrrelname = NULL;
          }
        }
        else
        {
          tginfo[j].tgconstrname = NULL;
          tginfo[j].tgconstrrelid = InvalidOid;
          tginfo[j].tgconstrrelname = NULL;
        }
      }
    }

    PQclear(res);
  }

  destroyPQExpBuffer(query);
}

   
                    
                                          
   
EventTriggerInfo *
getEventTriggers(Archive *fout, int *numEventTriggers)
{
  int i;
  PQExpBuffer query;
  PGresult *res;
  EventTriggerInfo *evtinfo;
  int i_tableoid, i_oid, i_evtname, i_evtevent, i_evtowner, i_evttags, i_evtfname, i_evtenabled;
  int ntups;

                                               
  if (fout->remoteVersion < 90300)
  {
    *numEventTriggers = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  appendPQExpBuffer(query,
      "SELECT e.tableoid, e.oid, evtname, evtenabled, "
      "evtevent, (%s evtowner) AS evtowner, "
      "array_to_string(array("
      "select quote_literal(x) "
      " from unnest(evttags) as t(x)), ', ') as evttags, "
      "e.evtfoid::regproc as evtfname "
      "FROM pg_event_trigger e "
      "ORDER BY e.oid",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numEventTriggers = ntups;

  evtinfo = (EventTriggerInfo *)pg_malloc(ntups * sizeof(EventTriggerInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_evtname = PQfnumber(res, "evtname");
  i_evtevent = PQfnumber(res, "evtevent");
  i_evtowner = PQfnumber(res, "evtowner");
  i_evttags = PQfnumber(res, "evttags");
  i_evtfname = PQfnumber(res, "evtfname");
  i_evtenabled = PQfnumber(res, "evtenabled");

  for (i = 0; i < ntups; i++)
  {
    evtinfo[i].dobj.objType = DO_EVENT_TRIGGER;
    evtinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    evtinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&evtinfo[i].dobj);
    evtinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_evtname));
    evtinfo[i].evtname = pg_strdup(PQgetvalue(res, i, i_evtname));
    evtinfo[i].evtevent = pg_strdup(PQgetvalue(res, i, i_evtevent));
    evtinfo[i].evtowner = pg_strdup(PQgetvalue(res, i, i_evtowner));
    evtinfo[i].evttags = pg_strdup(PQgetvalue(res, i, i_evttags));
    evtinfo[i].evtfname = pg_strdup(PQgetvalue(res, i, i_evtfname));
    evtinfo[i].evtenabled = *(PQgetvalue(res, i, i_evtenabled));

                                           
    selectDumpableObject(&(evtinfo[i].dobj), fout);

                                                    
    evtinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return evtinfo;
}

   
                
                                                                         
   
                                                      
   
                                                                  
                    
   
ProcLangInfo *
getProcLangs(Archive *fout, int *numProcLangs)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  ProcLangInfo *planginfo;
  int i_tableoid;
  int i_oid;
  int i_lanname;
  int i_lanpltrusted;
  int i_lanplcallfoid;
  int i_laninline;
  int i_lanvalidator;
  int i_lanacl;
  int i_rlanacl;
  int i_initlanacl;
  int i_initrlanacl;
  int i_lanowner;

  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "l.lanacl", "l.lanowner", "'l'", dopt->binary_upgrade);

                                            
    appendPQExpBuffer(query,
        "SELECT l.tableoid, l.oid, "
        "l.lanname, l.lanpltrusted, l.lanplcallfoid, "
        "l.laninline, l.lanvalidator, "
        "%s AS lanacl, "
        "%s AS rlanacl, "
        "%s AS initlanacl, "
        "%s AS initrlanacl, "
        "(%s l.lanowner) AS lanowner "
        "FROM pg_language l "
        "LEFT JOIN pg_init_privs pip ON "
        "(l.oid = pip.objoid "
        "AND pip.classoid = 'pg_language'::regclass "
        "AND pip.objsubid = 0) "
        "WHERE l.lanispl "
        "ORDER BY l.oid",
        acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data, username_subquery);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else if (fout->remoteVersion >= 90000)
  {
                                            
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, "
        "lanname, lanpltrusted, lanplcallfoid, "
        "laninline, lanvalidator, lanacl, NULL AS rlanacl, "
        "NULL AS initlanacl, NULL AS initrlanacl, "
        "(%s lanowner) AS lanowner "
        "FROM pg_language "
        "WHERE lanispl "
        "ORDER BY oid",
        username_subquery);
  }
  else if (fout->remoteVersion >= 80300)
  {
                                           
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, "
        "lanname, lanpltrusted, lanplcallfoid, "
        "0 AS laninline, lanvalidator, lanacl, "
        "NULL AS rlanacl, "
        "NULL AS initlanacl, NULL AS initrlanacl, "
        "(%s lanowner) AS lanowner "
        "FROM pg_language "
        "WHERE lanispl "
        "ORDER BY oid",
        username_subquery);
  }
  else if (fout->remoteVersion >= 80100)
  {
                                                                
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, "
        "lanname, lanpltrusted, lanplcallfoid, "
        "0 AS laninline, lanvalidator, lanacl, "
        "NULL AS rlanacl, "
        "NULL AS initlanacl, NULL AS initrlanacl, "
        "(%s '10') AS lanowner "
        "FROM pg_language "
        "WHERE lanispl "
        "ORDER BY oid",
        username_subquery);
  }
  else
  {
                                                                 
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, "
        "lanname, lanpltrusted, lanplcallfoid, "
        "0 AS laninline, lanvalidator, lanacl, "
        "NULL AS rlanacl, "
        "NULL AS initlanacl, NULL AS initrlanacl, "
        "(%s '1') AS lanowner "
        "FROM pg_language "
        "WHERE lanispl "
        "ORDER BY oid",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numProcLangs = ntups;

  planginfo = (ProcLangInfo *)pg_malloc(ntups * sizeof(ProcLangInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_lanname = PQfnumber(res, "lanname");
  i_lanpltrusted = PQfnumber(res, "lanpltrusted");
  i_lanplcallfoid = PQfnumber(res, "lanplcallfoid");
  i_laninline = PQfnumber(res, "laninline");
  i_lanvalidator = PQfnumber(res, "lanvalidator");
  i_lanacl = PQfnumber(res, "lanacl");
  i_rlanacl = PQfnumber(res, "rlanacl");
  i_initlanacl = PQfnumber(res, "initlanacl");
  i_initrlanacl = PQfnumber(res, "initrlanacl");
  i_lanowner = PQfnumber(res, "lanowner");

  for (i = 0; i < ntups; i++)
  {
    planginfo[i].dobj.objType = DO_PROCLANG;
    planginfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    planginfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&planginfo[i].dobj);

    planginfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_lanname));
    planginfo[i].lanpltrusted = *(PQgetvalue(res, i, i_lanpltrusted)) == 't';
    planginfo[i].lanplcallfoid = atooid(PQgetvalue(res, i, i_lanplcallfoid));
    planginfo[i].laninline = atooid(PQgetvalue(res, i, i_laninline));
    planginfo[i].lanvalidator = atooid(PQgetvalue(res, i, i_lanvalidator));
    planginfo[i].lanacl = pg_strdup(PQgetvalue(res, i, i_lanacl));
    planginfo[i].rlanacl = pg_strdup(PQgetvalue(res, i, i_rlanacl));
    planginfo[i].initlanacl = pg_strdup(PQgetvalue(res, i, i_initlanacl));
    planginfo[i].initrlanacl = pg_strdup(PQgetvalue(res, i, i_initrlanacl));
    planginfo[i].lanowner = pg_strdup(PQgetvalue(res, i, i_lanowner));

                                           
    selectDumpableProcLang(&(planginfo[i]), fout);

                                                  
    if (PQgetisnull(res, i, i_lanacl) && PQgetisnull(res, i, i_rlanacl) && PQgetisnull(res, i, i_initlanacl) && PQgetisnull(res, i, i_initrlanacl))
    {
      planginfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return planginfo;
}

   
            
                                                          
   
                                                  
   
CastInfo *
getCasts(Archive *fout, int *numCasts)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query = createPQExpBuffer();
  CastInfo *castinfo;
  int i_tableoid;
  int i_oid;
  int i_castsource;
  int i_casttarget;
  int i_castfunc;
  int i_castcontext;
  int i_castmethod;

  if (fout->remoteVersion >= 80400)
  {
    appendPQExpBufferStr(query, "SELECT tableoid, oid, "
                                "castsource, casttarget, castfunc, castcontext, "
                                "castmethod "
                                "FROM pg_cast ORDER BY 3,4");
  }
  else
  {
    appendPQExpBufferStr(query, "SELECT tableoid, oid, "
                                "castsource, casttarget, castfunc, castcontext, "
                                "CASE WHEN castfunc = 0 THEN 'b' ELSE 'f' END AS castmethod "
                                "FROM pg_cast ORDER BY 3,4");
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numCasts = ntups;

  castinfo = (CastInfo *)pg_malloc(ntups * sizeof(CastInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_castsource = PQfnumber(res, "castsource");
  i_casttarget = PQfnumber(res, "casttarget");
  i_castfunc = PQfnumber(res, "castfunc");
  i_castcontext = PQfnumber(res, "castcontext");
  i_castmethod = PQfnumber(res, "castmethod");

  for (i = 0; i < ntups; i++)
  {
    PQExpBufferData namebuf;
    TypeInfo *sTypeInfo;
    TypeInfo *tTypeInfo;

    castinfo[i].dobj.objType = DO_CAST;
    castinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    castinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&castinfo[i].dobj);
    castinfo[i].castsource = atooid(PQgetvalue(res, i, i_castsource));
    castinfo[i].casttarget = atooid(PQgetvalue(res, i, i_casttarget));
    castinfo[i].castfunc = atooid(PQgetvalue(res, i, i_castfunc));
    castinfo[i].castcontext = *(PQgetvalue(res, i, i_castcontext));
    castinfo[i].castmethod = *(PQgetvalue(res, i, i_castmethod));

       
                                                                         
                                                                          
                                
       
    initPQExpBuffer(&namebuf);
    sTypeInfo = findTypeByOid(castinfo[i].castsource);
    tTypeInfo = findTypeByOid(castinfo[i].casttarget);
    if (sTypeInfo && tTypeInfo)
    {
      appendPQExpBuffer(&namebuf, "%s %s", sTypeInfo->dobj.name, tTypeInfo->dobj.name);
    }
    castinfo[i].dobj.name = namebuf.data;

                                           
    selectDumpableCast(&(castinfo[i]), fout);

                                           
    castinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return castinfo;
}

static char *
get_language_name(Archive *fout, Oid langid)
{
  PQExpBuffer query;
  PGresult *res;
  char *lanname;

  query = createPQExpBuffer();
  appendPQExpBuffer(query, "SELECT lanname FROM pg_language WHERE oid = %u", langid);
  res = ExecuteSqlQueryForSingleRow(fout, query->data);
  lanname = pg_strdup(fmtId(PQgetvalue(res, 0, 0)));
  destroyPQExpBuffer(query);
  PQclear(res);

  return lanname;
}

   
                 
                                                               
   
                                                            
   
TransformInfo *
getTransforms(Archive *fout, int *numTransforms)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  TransformInfo *transforminfo;
  int i_tableoid;
  int i_oid;
  int i_trftype;
  int i_trflang;
  int i_trffromsql;
  int i_trftosql;

                                       
  if (fout->remoteVersion < 90500)
  {
    *numTransforms = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  appendPQExpBuffer(query, "SELECT tableoid, oid, "
                           "trftype, trflang, trffromsql::oid, trftosql::oid "
                           "FROM pg_transform "
                           "ORDER BY 3,4");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  *numTransforms = ntups;

  transforminfo = (TransformInfo *)pg_malloc(ntups * sizeof(TransformInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_trftype = PQfnumber(res, "trftype");
  i_trflang = PQfnumber(res, "trflang");
  i_trffromsql = PQfnumber(res, "trffromsql");
  i_trftosql = PQfnumber(res, "trftosql");

  for (i = 0; i < ntups; i++)
  {
    PQExpBufferData namebuf;
    TypeInfo *typeInfo;
    char *lanname;

    transforminfo[i].dobj.objType = DO_TRANSFORM;
    transforminfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    transforminfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&transforminfo[i].dobj);
    transforminfo[i].trftype = atooid(PQgetvalue(res, i, i_trftype));
    transforminfo[i].trflang = atooid(PQgetvalue(res, i, i_trflang));
    transforminfo[i].trffromsql = atooid(PQgetvalue(res, i, i_trffromsql));
    transforminfo[i].trftosql = atooid(PQgetvalue(res, i, i_trftosql));

       
                                                                         
                                                                      
                                                 
       
    initPQExpBuffer(&namebuf);
    typeInfo = findTypeByOid(transforminfo[i].trftype);
    lanname = get_language_name(fout, transforminfo[i].trflang);
    if (typeInfo && lanname)
    {
      appendPQExpBuffer(&namebuf, "%s %s", typeInfo->dobj.name, lanname);
    }
    transforminfo[i].dobj.name = namebuf.data;
    free(lanname);

                                           
    selectDumpableObject(&(transforminfo[i].dobj), fout);
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return transforminfo;
}

   
                   
                                                                
                                                            
   
                                                                    
                                                                              
                                                                        
                                                                       
                                         
   
                    
   
void
getTableAttrs(Archive *fout, TableInfo *tblinfo, int numTables)
{
  DumpOptions *dopt = fout->dopt;
  int i, j;
  PQExpBuffer q = createPQExpBuffer();
  int i_attnum;
  int i_attname;
  int i_atttypname;
  int i_atttypmod;
  int i_attstattarget;
  int i_attstorage;
  int i_typstorage;
  int i_attnotnull;
  int i_atthasdef;
  int i_attidentity;
  int i_attgenerated;
  int i_attisdropped;
  int i_attlen;
  int i_attalign;
  int i_attislocal;
  int i_attoptions;
  int i_attcollation;
  int i_attfdwoptions;
  int i_attmissingval;
  PGresult *res;
  int ntups;
  bool hasdefaults;

  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &tblinfo[i];

                                                    
    if (tbinfo->relkind == RELKIND_SEQUENCE)
    {
      continue;
    }

                                                        
    if (!tbinfo->interesting)
    {
      continue;
    }

                                                      

       
                                                                           
                                                                      
       
    pg_log_info("finding the columns and types of table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, tbinfo->dobj.name);

    resetPQExpBuffer(q);

    appendPQExpBuffer(q, "SELECT\n"
                         "a.attnum,\n"
                         "a.attname,\n"
                         "a.atttypmod,\n"
                         "a.attstattarget,\n"
                         "a.attstorage,\n"
                         "t.typstorage,\n"
                         "a.attnotnull,\n"
                         "a.atthasdef,\n"
                         "a.attisdropped,\n"
                         "a.attlen,\n"
                         "a.attalign,\n"
                         "a.attislocal,\n"
                         "pg_catalog.format_type(t.oid, a.atttypmod) AS atttypname,\n");

    if (fout->remoteVersion >= 120000)
    {
      appendPQExpBuffer(q, "a.attgenerated,\n");
    }
    else
    {
      appendPQExpBuffer(q, "'' AS attgenerated,\n");
    }

    if (fout->remoteVersion >= 110000)
    {
      appendPQExpBuffer(q, "CASE WHEN a.atthasmissing AND NOT a.attisdropped "
                           "THEN a.attmissingval ELSE null END AS attmissingval,\n");
    }
    else
    {
      appendPQExpBuffer(q, "NULL AS attmissingval,\n");
    }

    if (fout->remoteVersion >= 100000)
    {
      appendPQExpBuffer(q, "a.attidentity,\n");
    }
    else
    {
      appendPQExpBuffer(q, "'' AS attidentity,\n");
    }

    if (fout->remoteVersion >= 90200)
    {
      appendPQExpBuffer(q, "pg_catalog.array_to_string(ARRAY("
                           "SELECT pg_catalog.quote_ident(option_name) || "
                           "' ' || pg_catalog.quote_literal(option_value) "
                           "FROM pg_catalog.pg_options_to_table(attfdwoptions) "
                           "ORDER BY option_name"
                           "), E',\n    ') AS attfdwoptions,\n");
    }
    else
    {
      appendPQExpBuffer(q, "'' AS attfdwoptions,\n");
    }

    if (fout->remoteVersion >= 90100)
    {
         
                                                                         
                                                                         
                                                               
         
      appendPQExpBuffer(q, "CASE WHEN a.attcollation <> t.typcollation "
                           "THEN a.attcollation ELSE 0 END AS attcollation,\n");
    }
    else
    {
      appendPQExpBuffer(q, "0 AS attcollation,\n");
    }

    if (fout->remoteVersion >= 90000)
    {
      appendPQExpBuffer(q, "array_to_string(a.attoptions, ', ') AS attoptions\n");
    }
    else
    {
      appendPQExpBuffer(q, "'' AS attoptions\n");
    }

                                                                
    appendPQExpBuffer(q,
        "FROM pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_type t "
        "ON a.atttypid = t.oid\n"
        "WHERE a.attrelid = '%u'::pg_catalog.oid "
        "AND a.attnum > 0::pg_catalog.int2\n"
        "ORDER BY a.attnum",
        tbinfo->dobj.catId.oid);

    res = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);

    ntups = PQntuples(res);

    i_attnum = PQfnumber(res, "attnum");
    i_attname = PQfnumber(res, "attname");
    i_atttypname = PQfnumber(res, "atttypname");
    i_atttypmod = PQfnumber(res, "atttypmod");
    i_attstattarget = PQfnumber(res, "attstattarget");
    i_attstorage = PQfnumber(res, "attstorage");
    i_typstorage = PQfnumber(res, "typstorage");
    i_attnotnull = PQfnumber(res, "attnotnull");
    i_atthasdef = PQfnumber(res, "atthasdef");
    i_attidentity = PQfnumber(res, "attidentity");
    i_attgenerated = PQfnumber(res, "attgenerated");
    i_attisdropped = PQfnumber(res, "attisdropped");
    i_attlen = PQfnumber(res, "attlen");
    i_attalign = PQfnumber(res, "attalign");
    i_attislocal = PQfnumber(res, "attislocal");
    i_attoptions = PQfnumber(res, "attoptions");
    i_attcollation = PQfnumber(res, "attcollation");
    i_attfdwoptions = PQfnumber(res, "attfdwoptions");
    i_attmissingval = PQfnumber(res, "attmissingval");

    tbinfo->numatts = ntups;
    tbinfo->attnames = (char **)pg_malloc(ntups * sizeof(char *));
    tbinfo->atttypnames = (char **)pg_malloc(ntups * sizeof(char *));
    tbinfo->atttypmod = (int *)pg_malloc(ntups * sizeof(int));
    tbinfo->attstattarget = (int *)pg_malloc(ntups * sizeof(int));
    tbinfo->attstorage = (char *)pg_malloc(ntups * sizeof(char));
    tbinfo->typstorage = (char *)pg_malloc(ntups * sizeof(char));
    tbinfo->attidentity = (char *)pg_malloc(ntups * sizeof(char));
    tbinfo->attgenerated = (char *)pg_malloc(ntups * sizeof(char));
    tbinfo->attisdropped = (bool *)pg_malloc(ntups * sizeof(bool));
    tbinfo->attlen = (int *)pg_malloc(ntups * sizeof(int));
    tbinfo->attalign = (char *)pg_malloc(ntups * sizeof(char));
    tbinfo->attislocal = (bool *)pg_malloc(ntups * sizeof(bool));
    tbinfo->attoptions = (char **)pg_malloc(ntups * sizeof(char *));
    tbinfo->attcollation = (Oid *)pg_malloc(ntups * sizeof(Oid));
    tbinfo->attfdwoptions = (char **)pg_malloc(ntups * sizeof(char *));
    tbinfo->attmissingval = (char **)pg_malloc(ntups * sizeof(char *));
    tbinfo->notnull = (bool *)pg_malloc(ntups * sizeof(bool));
    tbinfo->inhNotNull = (bool *)pg_malloc(ntups * sizeof(bool));
    tbinfo->attrdefs = (AttrDefInfo **)pg_malloc(ntups * sizeof(AttrDefInfo *));
    hasdefaults = false;

    for (j = 0; j < ntups; j++)
    {
      if (j + 1 != atoi(PQgetvalue(res, j, i_attnum)))
      {
        fatal("invalid column numbering in table \"%s\"", tbinfo->dobj.name);
      }
      tbinfo->attnames[j] = pg_strdup(PQgetvalue(res, j, i_attname));
      tbinfo->atttypnames[j] = pg_strdup(PQgetvalue(res, j, i_atttypname));
      tbinfo->atttypmod[j] = atoi(PQgetvalue(res, j, i_atttypmod));
      tbinfo->attstattarget[j] = atoi(PQgetvalue(res, j, i_attstattarget));
      tbinfo->attstorage[j] = *(PQgetvalue(res, j, i_attstorage));
      tbinfo->typstorage[j] = *(PQgetvalue(res, j, i_typstorage));
      tbinfo->attidentity[j] = *(PQgetvalue(res, j, i_attidentity));
      tbinfo->attgenerated[j] = *(PQgetvalue(res, j, i_attgenerated));
      tbinfo->needs_override = tbinfo->needs_override || (tbinfo->attidentity[j] == ATTRIBUTE_IDENTITY_ALWAYS);
      tbinfo->attisdropped[j] = (PQgetvalue(res, j, i_attisdropped)[0] == 't');
      tbinfo->attlen[j] = atoi(PQgetvalue(res, j, i_attlen));
      tbinfo->attalign[j] = *(PQgetvalue(res, j, i_attalign));
      tbinfo->attislocal[j] = (PQgetvalue(res, j, i_attislocal)[0] == 't');
      tbinfo->notnull[j] = (PQgetvalue(res, j, i_attnotnull)[0] == 't');
      tbinfo->attoptions[j] = pg_strdup(PQgetvalue(res, j, i_attoptions));
      tbinfo->attcollation[j] = atooid(PQgetvalue(res, j, i_attcollation));
      tbinfo->attfdwoptions[j] = pg_strdup(PQgetvalue(res, j, i_attfdwoptions));
      tbinfo->attmissingval[j] = pg_strdup(PQgetvalue(res, j, i_attmissingval));
      tbinfo->attrdefs[j] = NULL;                
      if (PQgetvalue(res, j, i_atthasdef)[0] == 't')
      {
        hasdefaults = true;
      }
                                                     
      tbinfo->inhNotNull[j] = false;
    }

    PQclear(res);

       
                                      
       
    if (hasdefaults)
    {
      AttrDefInfo *attrdefs;
      int numDefaults;

      pg_log_info("finding default expressions of table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, tbinfo->dobj.name);

      printfPQExpBuffer(q,
          "SELECT tableoid, oid, adnum, "
          "pg_catalog.pg_get_expr(adbin, adrelid) AS adsrc "
          "FROM pg_catalog.pg_attrdef "
          "WHERE adrelid = '%u'::pg_catalog.oid",
          tbinfo->dobj.catId.oid);

      res = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);

      numDefaults = PQntuples(res);
      attrdefs = (AttrDefInfo *)pg_malloc(numDefaults * sizeof(AttrDefInfo));

      for (j = 0; j < numDefaults; j++)
      {
        int adnum;

        adnum = atoi(PQgetvalue(res, j, 2));

        if (adnum <= 0 || adnum > ntups)
        {
          fatal("invalid adnum value %d for table \"%s\"", adnum, tbinfo->dobj.name);
        }

           
                                                                      
                      
           
        if (tbinfo->attisdropped[adnum - 1])
        {
          continue;
        }

        attrdefs[j].dobj.objType = DO_ATTRDEF;
        attrdefs[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, 0));
        attrdefs[j].dobj.catId.oid = atooid(PQgetvalue(res, j, 1));
        AssignDumpId(&attrdefs[j].dobj);
        attrdefs[j].adtable = tbinfo;
        attrdefs[j].adnum = adnum;
        attrdefs[j].adef_expr = pg_strdup(PQgetvalue(res, j, 3));

        attrdefs[j].dobj.name = pg_strdup(tbinfo->dobj.name);
        attrdefs[j].dobj.namespace = tbinfo->dobj.namespace;

        attrdefs[j].dobj.dump = tbinfo->dobj.dump;

           
                                                                       
                                                                   
                                                                      
                                                                       
                                           
           
        if (tbinfo->attgenerated[adnum - 1])
        {
             
                                                            
                                                                 
                                                                  
                                                                   
                                                                  
                                                                   
                                                                   
                                                                  
                                                                   
                                                                    
                                                                
                                                                   
                     
             
          attrdefs[j].separate = false;
        }
        else if (tbinfo->relkind == RELKIND_VIEW)
        {
             
                                                                  
                                   
             
          attrdefs[j].separate = true;
        }
        else if (!shouldPrintColumn(dopt, tbinfo, adnum - 1))
        {
                                                                   
          attrdefs[j].separate = true;
        }
        else
        {
          attrdefs[j].separate = false;
        }

        if (!attrdefs[j].separate)
        {
             
                                                                     
                                                                    
                                                               
                                                                   
             
          addObjectDependency(&tbinfo->dobj, attrdefs[j].dobj.dumpId);
        }

        tbinfo->attrdefs[adnum - 1] = &attrdefs[j];
      }
      PQclear(res);
    }

       
                                              
       
    if (tbinfo->ncheck > 0)
    {
      ConstraintInfo *constrs;
      int numConstrs;

      pg_log_info("finding check constraints for table \"%s.%s\"", tbinfo->dobj.namespace->dobj.name, tbinfo->dobj.name);

      resetPQExpBuffer(q);
      if (fout->remoteVersion >= 90200)
      {
           
                                                                     
                                                                      
           
        appendPQExpBuffer(q,
            "SELECT tableoid, oid, conname, "
            "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
            "conislocal, convalidated "
            "FROM pg_catalog.pg_constraint "
            "WHERE conrelid = '%u'::pg_catalog.oid "
            "   AND contype = 'c' "
            "ORDER BY conname",
            tbinfo->dobj.catId.oid);
      }
      else if (fout->remoteVersion >= 80400)
      {
                                      
        appendPQExpBuffer(q,
            "SELECT tableoid, oid, conname, "
            "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
            "conislocal, true AS convalidated "
            "FROM pg_catalog.pg_constraint "
            "WHERE conrelid = '%u'::pg_catalog.oid "
            "   AND contype = 'c' "
            "ORDER BY conname",
            tbinfo->dobj.catId.oid);
      }
      else
      {
        appendPQExpBuffer(q,
            "SELECT tableoid, oid, conname, "
            "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
            "true AS conislocal, true AS convalidated "
            "FROM pg_catalog.pg_constraint "
            "WHERE conrelid = '%u'::pg_catalog.oid "
            "   AND contype = 'c' "
            "ORDER BY conname",
            tbinfo->dobj.catId.oid);
      }

      res = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);

      numConstrs = PQntuples(res);
      if (numConstrs != tbinfo->ncheck)
      {
        pg_log_error(ngettext("expected %d check constraint on table \"%s\" but found %d", "expected %d check constraints on table \"%s\" but found %d", tbinfo->ncheck), tbinfo->ncheck, tbinfo->dobj.name, numConstrs);
        pg_log_error("(The system catalogs might be corrupted.)");
        exit_nicely(1);
      }

      constrs = (ConstraintInfo *)pg_malloc(numConstrs * sizeof(ConstraintInfo));
      tbinfo->checkexprs = constrs;

      for (j = 0; j < numConstrs; j++)
      {
        bool validated = PQgetvalue(res, j, 5)[0] == 't';

        constrs[j].dobj.objType = DO_CONSTRAINT;
        constrs[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, 0));
        constrs[j].dobj.catId.oid = atooid(PQgetvalue(res, j, 1));
        AssignDumpId(&constrs[j].dobj);
        constrs[j].dobj.name = pg_strdup(PQgetvalue(res, j, 2));
        constrs[j].dobj.namespace = tbinfo->dobj.namespace;
        constrs[j].contable = tbinfo;
        constrs[j].condomain = NULL;
        constrs[j].contype = 'c';
        constrs[j].condef = pg_strdup(PQgetvalue(res, j, 3));
        constrs[j].confrelid = InvalidOid;
        constrs[j].conindex = 0;
        constrs[j].condeferrable = false;
        constrs[j].condeferred = false;
        constrs[j].conislocal = (PQgetvalue(res, j, 4)[0] == 't');

           
                                                                       
                                                                     
                           
           
        constrs[j].separate = !validated;

        constrs[j].dobj.dump = tbinfo->dobj.dump;

           
                                                                     
                                                             
                                                                  
                                                                    
                                                                       
                                                                      
                                                            
           
        if (!constrs[j].separate)
        {
          addObjectDependency(&tbinfo->dobj, constrs[j].dobj.dumpId);
        }

           
                                                                       
                                                                
                                                                   
           
      }
      PQclear(res);
    }
  }

  destroyPQExpBuffer(q);
}

   
                                                                            
                                
   
                                                                             
                                                                            
                                                                              
                                                                          
                                                               
   
                                                                             
                                                                          
          
   
                                                                           
                                            
   
bool
shouldPrintColumn(DumpOptions *dopt, TableInfo *tbinfo, int colno)
{
  if (dopt->binary_upgrade)
  {
    return true;
  }
  if (tbinfo->attisdropped[colno])
  {
    return false;
  }
  return (tbinfo->attislocal[colno] || tbinfo->ispartition);
}

   
                 
                                                                         
                                    
   
                                                        
   
TSParserInfo *
getTSParsers(Archive *fout, int *numTSParsers)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  TSParserInfo *prsinfo;
  int i_tableoid;
  int i_oid;
  int i_prsname;
  int i_prsnamespace;
  int i_prsstart;
  int i_prstoken;
  int i_prsend;
  int i_prsheadline;
  int i_prslextype;

                                                            
  if (fout->remoteVersion < 80300)
  {
    *numTSParsers = 0;
    return NULL;
  }

  query = createPQExpBuffer();

     
                                                                         
                                              
     

  appendPQExpBufferStr(query, "SELECT tableoid, oid, prsname, prsnamespace, "
                              "prsstart::oid, prstoken::oid, "
                              "prsend::oid, prsheadline::oid, prslextype::oid "
                              "FROM pg_ts_parser");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numTSParsers = ntups;

  prsinfo = (TSParserInfo *)pg_malloc(ntups * sizeof(TSParserInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_prsname = PQfnumber(res, "prsname");
  i_prsnamespace = PQfnumber(res, "prsnamespace");
  i_prsstart = PQfnumber(res, "prsstart");
  i_prstoken = PQfnumber(res, "prstoken");
  i_prsend = PQfnumber(res, "prsend");
  i_prsheadline = PQfnumber(res, "prsheadline");
  i_prslextype = PQfnumber(res, "prslextype");

  for (i = 0; i < ntups; i++)
  {
    prsinfo[i].dobj.objType = DO_TSPARSER;
    prsinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    prsinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&prsinfo[i].dobj);
    prsinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_prsname));
    prsinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_prsnamespace)));
    prsinfo[i].prsstart = atooid(PQgetvalue(res, i, i_prsstart));
    prsinfo[i].prstoken = atooid(PQgetvalue(res, i, i_prstoken));
    prsinfo[i].prsend = atooid(PQgetvalue(res, i, i_prsend));
    prsinfo[i].prsheadline = atooid(PQgetvalue(res, i, i_prsheadline));
    prsinfo[i].prslextype = atooid(PQgetvalue(res, i, i_prslextype));

                                           
    selectDumpableObject(&(prsinfo[i].dobj), fout);

                                                         
    prsinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return prsinfo;
}

   
                      
                                                                              
                                  
   
                                                           
   
TSDictInfo *
getTSDictionaries(Archive *fout, int *numTSDicts)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  TSDictInfo *dictinfo;
  int i_tableoid;
  int i_oid;
  int i_dictname;
  int i_dictnamespace;
  int i_rolname;
  int i_dicttemplate;
  int i_dictinitoption;

                                                            
  if (fout->remoteVersion < 80300)
  {
    *numTSDicts = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, dictname, "
      "dictnamespace, (%s dictowner) AS rolname, "
      "dicttemplate, dictinitoption "
      "FROM pg_ts_dict",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numTSDicts = ntups;

  dictinfo = (TSDictInfo *)pg_malloc(ntups * sizeof(TSDictInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_dictname = PQfnumber(res, "dictname");
  i_dictnamespace = PQfnumber(res, "dictnamespace");
  i_rolname = PQfnumber(res, "rolname");
  i_dictinitoption = PQfnumber(res, "dictinitoption");
  i_dicttemplate = PQfnumber(res, "dicttemplate");

  for (i = 0; i < ntups; i++)
  {
    dictinfo[i].dobj.objType = DO_TSDICT;
    dictinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    dictinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&dictinfo[i].dobj);
    dictinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_dictname));
    dictinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_dictnamespace)));
    dictinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    dictinfo[i].dicttemplate = atooid(PQgetvalue(res, i, i_dicttemplate));
    if (PQgetisnull(res, i, i_dictinitoption))
    {
      dictinfo[i].dictinitoption = NULL;
    }
    else
    {
      dictinfo[i].dictinitoption = pg_strdup(PQgetvalue(res, i, i_dictinitoption));
    }

                                           
    selectDumpableObject(&(dictinfo[i].dobj), fout);

                                                              
    dictinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return dictinfo;
}

   
                   
                                                                           
                                      
   
                                                            
   
TSTemplateInfo *
getTSTemplates(Archive *fout, int *numTSTemplates)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  TSTemplateInfo *tmplinfo;
  int i_tableoid;
  int i_oid;
  int i_tmplname;
  int i_tmplnamespace;
  int i_tmplinit;
  int i_tmpllexize;

                                                            
  if (fout->remoteVersion < 80300)
  {
    *numTSTemplates = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  appendPQExpBufferStr(query, "SELECT tableoid, oid, tmplname, "
                              "tmplnamespace, tmplinit::oid, tmpllexize::oid "
                              "FROM pg_ts_template");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numTSTemplates = ntups;

  tmplinfo = (TSTemplateInfo *)pg_malloc(ntups * sizeof(TSTemplateInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_tmplname = PQfnumber(res, "tmplname");
  i_tmplnamespace = PQfnumber(res, "tmplnamespace");
  i_tmplinit = PQfnumber(res, "tmplinit");
  i_tmpllexize = PQfnumber(res, "tmpllexize");

  for (i = 0; i < ntups; i++)
  {
    tmplinfo[i].dobj.objType = DO_TSTEMPLATE;
    tmplinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    tmplinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&tmplinfo[i].dobj);
    tmplinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_tmplname));
    tmplinfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_tmplnamespace)));
    tmplinfo[i].tmplinit = atooid(PQgetvalue(res, i, i_tmplinit));
    tmplinfo[i].tmpllexize = atooid(PQgetvalue(res, i, i_tmpllexize));

                                           
    selectDumpableObject(&(tmplinfo[i].dobj), fout);

                                                           
    tmplinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return tmplinfo;
}

   
                        
                                                                           
                                         
   
                                                               
   
TSConfigInfo *
getTSConfigurations(Archive *fout, int *numTSConfigs)
{
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  TSConfigInfo *cfginfo;
  int i_tableoid;
  int i_oid;
  int i_cfgname;
  int i_cfgnamespace;
  int i_rolname;
  int i_cfgparser;

                                                            
  if (fout->remoteVersion < 80300)
  {
    *numTSConfigs = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  appendPQExpBuffer(query,
      "SELECT tableoid, oid, cfgname, "
      "cfgnamespace, (%s cfgowner) AS rolname, cfgparser "
      "FROM pg_ts_config",
      username_subquery);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numTSConfigs = ntups;

  cfginfo = (TSConfigInfo *)pg_malloc(ntups * sizeof(TSConfigInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_cfgname = PQfnumber(res, "cfgname");
  i_cfgnamespace = PQfnumber(res, "cfgnamespace");
  i_rolname = PQfnumber(res, "rolname");
  i_cfgparser = PQfnumber(res, "cfgparser");

  for (i = 0; i < ntups; i++)
  {
    cfginfo[i].dobj.objType = DO_TSCONFIG;
    cfginfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    cfginfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&cfginfo[i].dobj);
    cfginfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_cfgname));
    cfginfo[i].dobj.namespace = findNamespace(fout, atooid(PQgetvalue(res, i, i_cfgnamespace)));
    cfginfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    cfginfo[i].cfgparser = atooid(PQgetvalue(res, i, i_cfgparser));

                                           
    selectDumpableObject(&(cfginfo[i].dobj), fout);

                                                                
    cfginfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return cfginfo;
}

   
                           
                                                                      
                                    
   
                                                               
   
FdwInfo *
getForeignDataWrappers(Archive *fout, int *numForeignDataWrappers)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  FdwInfo *fdwinfo;
  int i_tableoid;
  int i_oid;
  int i_fdwname;
  int i_rolname;
  int i_fdwhandler;
  int i_fdwvalidator;
  int i_fdwacl;
  int i_rfdwacl;
  int i_initfdwacl;
  int i_initrfdwacl;
  int i_fdwoptions;

                                                      
  if (fout->remoteVersion < 80400)
  {
    *numForeignDataWrappers = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "f.fdwacl", "f.fdwowner", "'F'", dopt->binary_upgrade);

    appendPQExpBuffer(query,
        "SELECT f.tableoid, f.oid, f.fdwname, "
        "(%s f.fdwowner) AS rolname, "
        "f.fdwhandler::pg_catalog.regproc, "
        "f.fdwvalidator::pg_catalog.regproc, "
        "%s AS fdwacl, "
        "%s AS rfdwacl, "
        "%s AS initfdwacl, "
        "%s AS initrfdwacl, "
        "array_to_string(ARRAY("
        "SELECT quote_ident(option_name) || ' ' || "
        "quote_literal(option_value) "
        "FROM pg_options_to_table(f.fdwoptions) "
        "ORDER BY option_name"
        "), E',\n    ') AS fdwoptions "
        "FROM pg_foreign_data_wrapper f "
        "LEFT JOIN pg_init_privs pip ON "
        "(f.oid = pip.objoid "
        "AND pip.classoid = 'pg_foreign_data_wrapper'::regclass "
        "AND pip.objsubid = 0) ",
        username_subquery, acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else if (fout->remoteVersion >= 90100)
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, fdwname, "
        "(%s fdwowner) AS rolname, "
        "fdwhandler::pg_catalog.regproc, "
        "fdwvalidator::pg_catalog.regproc, fdwacl, "
        "NULL as rfdwacl, "
        "NULL as initfdwacl, NULL AS initrfdwacl, "
        "array_to_string(ARRAY("
        "SELECT quote_ident(option_name) || ' ' || "
        "quote_literal(option_value) "
        "FROM pg_options_to_table(fdwoptions) "
        "ORDER BY option_name"
        "), E',\n    ') AS fdwoptions "
        "FROM pg_foreign_data_wrapper",
        username_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, fdwname, "
        "(%s fdwowner) AS rolname, "
        "'-' AS fdwhandler, "
        "fdwvalidator::pg_catalog.regproc, fdwacl, "
        "NULL as rfdwacl, "
        "NULL as initfdwacl, NULL AS initrfdwacl, "
        "array_to_string(ARRAY("
        "SELECT quote_ident(option_name) || ' ' || "
        "quote_literal(option_value) "
        "FROM pg_options_to_table(fdwoptions) "
        "ORDER BY option_name"
        "), E',\n    ') AS fdwoptions "
        "FROM pg_foreign_data_wrapper",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numForeignDataWrappers = ntups;

  fdwinfo = (FdwInfo *)pg_malloc(ntups * sizeof(FdwInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_fdwname = PQfnumber(res, "fdwname");
  i_rolname = PQfnumber(res, "rolname");
  i_fdwhandler = PQfnumber(res, "fdwhandler");
  i_fdwvalidator = PQfnumber(res, "fdwvalidator");
  i_fdwacl = PQfnumber(res, "fdwacl");
  i_rfdwacl = PQfnumber(res, "rfdwacl");
  i_initfdwacl = PQfnumber(res, "initfdwacl");
  i_initrfdwacl = PQfnumber(res, "initrfdwacl");
  i_fdwoptions = PQfnumber(res, "fdwoptions");

  for (i = 0; i < ntups; i++)
  {
    fdwinfo[i].dobj.objType = DO_FDW;
    fdwinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    fdwinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&fdwinfo[i].dobj);
    fdwinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_fdwname));
    fdwinfo[i].dobj.namespace = NULL;
    fdwinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    fdwinfo[i].fdwhandler = pg_strdup(PQgetvalue(res, i, i_fdwhandler));
    fdwinfo[i].fdwvalidator = pg_strdup(PQgetvalue(res, i, i_fdwvalidator));
    fdwinfo[i].fdwoptions = pg_strdup(PQgetvalue(res, i, i_fdwoptions));
    fdwinfo[i].fdwacl = pg_strdup(PQgetvalue(res, i, i_fdwacl));
    fdwinfo[i].rfdwacl = pg_strdup(PQgetvalue(res, i, i_rfdwacl));
    fdwinfo[i].initfdwacl = pg_strdup(PQgetvalue(res, i, i_initfdwacl));
    fdwinfo[i].initrfdwacl = pg_strdup(PQgetvalue(res, i, i_initrfdwacl));

                                           
    selectDumpableObject(&(fdwinfo[i].dobj), fout);

                                                  
    if (PQgetisnull(res, i, i_fdwacl) && PQgetisnull(res, i, i_rfdwacl) && PQgetisnull(res, i, i_initfdwacl) && PQgetisnull(res, i, i_initrfdwacl))
    {
      fdwinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return fdwinfo;
}

   
                      
                                                                
                                               
   
                                                             
   
ForeignServerInfo *
getForeignServers(Archive *fout, int *numForeignServers)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  int ntups;
  int i;
  PQExpBuffer query;
  ForeignServerInfo *srvinfo;
  int i_tableoid;
  int i_oid;
  int i_srvname;
  int i_rolname;
  int i_srvfdw;
  int i_srvtype;
  int i_srvversion;
  int i_srvacl;
  int i_rsrvacl;
  int i_initsrvacl;
  int i_initrsrvacl;
  int i_srvoptions;

                                                
  if (fout->remoteVersion < 80400)
  {
    *numForeignServers = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();

    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "f.srvacl", "f.srvowner", "'S'", dopt->binary_upgrade);

    appendPQExpBuffer(query,
        "SELECT f.tableoid, f.oid, f.srvname, "
        "(%s f.srvowner) AS rolname, "
        "f.srvfdw, f.srvtype, f.srvversion, "
        "%s AS srvacl, "
        "%s AS rsrvacl, "
        "%s AS initsrvacl, "
        "%s AS initrsrvacl, "
        "array_to_string(ARRAY("
        "SELECT quote_ident(option_name) || ' ' || "
        "quote_literal(option_value) "
        "FROM pg_options_to_table(f.srvoptions) "
        "ORDER BY option_name"
        "), E',\n    ') AS srvoptions "
        "FROM pg_foreign_server f "
        "LEFT JOIN pg_init_privs pip "
        "ON (f.oid = pip.objoid "
        "AND pip.classoid = 'pg_foreign_server'::regclass "
        "AND pip.objsubid = 0) ",
        username_subquery, acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT tableoid, oid, srvname, "
        "(%s srvowner) AS rolname, "
        "srvfdw, srvtype, srvversion, srvacl, "
        "NULL AS rsrvacl, "
        "NULL AS initsrvacl, NULL AS initrsrvacl, "
        "array_to_string(ARRAY("
        "SELECT quote_ident(option_name) || ' ' || "
        "quote_literal(option_value) "
        "FROM pg_options_to_table(srvoptions) "
        "ORDER BY option_name"
        "), E',\n    ') AS srvoptions "
        "FROM pg_foreign_server",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numForeignServers = ntups;

  srvinfo = (ForeignServerInfo *)pg_malloc(ntups * sizeof(ForeignServerInfo));

  i_tableoid = PQfnumber(res, "tableoid");
  i_oid = PQfnumber(res, "oid");
  i_srvname = PQfnumber(res, "srvname");
  i_rolname = PQfnumber(res, "rolname");
  i_srvfdw = PQfnumber(res, "srvfdw");
  i_srvtype = PQfnumber(res, "srvtype");
  i_srvversion = PQfnumber(res, "srvversion");
  i_srvacl = PQfnumber(res, "srvacl");
  i_rsrvacl = PQfnumber(res, "rsrvacl");
  i_initsrvacl = PQfnumber(res, "initsrvacl");
  i_initrsrvacl = PQfnumber(res, "initrsrvacl");
  i_srvoptions = PQfnumber(res, "srvoptions");

  for (i = 0; i < ntups; i++)
  {
    srvinfo[i].dobj.objType = DO_FOREIGN_SERVER;
    srvinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    srvinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&srvinfo[i].dobj);
    srvinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_srvname));
    srvinfo[i].dobj.namespace = NULL;
    srvinfo[i].rolname = pg_strdup(PQgetvalue(res, i, i_rolname));
    srvinfo[i].srvfdw = atooid(PQgetvalue(res, i, i_srvfdw));
    srvinfo[i].srvtype = pg_strdup(PQgetvalue(res, i, i_srvtype));
    srvinfo[i].srvversion = pg_strdup(PQgetvalue(res, i, i_srvversion));
    srvinfo[i].srvoptions = pg_strdup(PQgetvalue(res, i, i_srvoptions));
    srvinfo[i].srvacl = pg_strdup(PQgetvalue(res, i, i_srvacl));
    srvinfo[i].rsrvacl = pg_strdup(PQgetvalue(res, i, i_rsrvacl));
    srvinfo[i].initsrvacl = pg_strdup(PQgetvalue(res, i, i_initsrvacl));
    srvinfo[i].initrsrvacl = pg_strdup(PQgetvalue(res, i, i_initrsrvacl));

                                           
    selectDumpableObject(&(srvinfo[i].dobj), fout);

                                                  
    if (PQgetisnull(res, i, i_srvacl) && PQgetisnull(res, i, i_rsrvacl) && PQgetisnull(res, i, i_initsrvacl) && PQgetisnull(res, i, i_initrsrvacl))
    {
      srvinfo[i].dobj.dump &= ~DUMP_COMPONENT_ACL;
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return srvinfo;
}

   
                   
                                                                        
                                          
   
                                                       
   
DefaultACLInfo *
getDefaultACLs(Archive *fout, int *numDefaultACLs)
{
  DumpOptions *dopt = fout->dopt;
  DefaultACLInfo *daclinfo;
  PQExpBuffer query;
  PGresult *res;
  int i_oid;
  int i_tableoid;
  int i_defaclrole;
  int i_defaclnamespace;
  int i_defaclobjtype;
  int i_defaclacl;
  int i_rdefaclacl;
  int i_initdefaclacl;
  int i_initrdefaclacl;
  int i, ntups;

  if (fout->remoteVersion < 90000)
  {
    *numDefaultACLs = 0;
    return NULL;
  }

  query = createPQExpBuffer();

  if (fout->remoteVersion >= 90600)
  {
    PQExpBuffer acl_subquery = createPQExpBuffer();
    PQExpBuffer racl_subquery = createPQExpBuffer();
    PQExpBuffer initacl_subquery = createPQExpBuffer();
    PQExpBuffer initracl_subquery = createPQExpBuffer();

       
                                                                      
                                                                         
                                                                         
                                                                       
                                                                         
                                                                      
                                                                       
                                                                 
       
                                                                     
                                                                     
                         
       
    buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "defaclacl", "defaclrole",
        "CASE WHEN defaclnamespace = 0 THEN"
        "	  CASE WHEN defaclobjtype = 'S' THEN 's'::\"char\""
        "	  ELSE defaclobjtype END "
        "ELSE NULL END",
        dopt->binary_upgrade);

    appendPQExpBuffer(query,
        "SELECT d.oid, d.tableoid, "
        "(%s d.defaclrole) AS defaclrole, "
        "d.defaclnamespace, "
        "d.defaclobjtype, "
        "%s AS defaclacl, "
        "%s AS rdefaclacl, "
        "%s AS initdefaclacl, "
        "%s AS initrdefaclacl "
        "FROM pg_default_acl d "
        "LEFT JOIN pg_init_privs pip ON "
        "(d.oid = pip.objoid "
        "AND pip.classoid = 'pg_default_acl'::regclass "
        "AND pip.objsubid = 0) ",
        username_subquery, acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data);

    destroyPQExpBuffer(acl_subquery);
    destroyPQExpBuffer(racl_subquery);
    destroyPQExpBuffer(initacl_subquery);
    destroyPQExpBuffer(initracl_subquery);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT oid, tableoid, "
        "(%s defaclrole) AS defaclrole, "
        "defaclnamespace, "
        "defaclobjtype, "
        "defaclacl, "
        "NULL AS rdefaclacl, "
        "NULL AS initdefaclacl, "
        "NULL AS initrdefaclacl "
        "FROM pg_default_acl",
        username_subquery);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  *numDefaultACLs = ntups;

  daclinfo = (DefaultACLInfo *)pg_malloc(ntups * sizeof(DefaultACLInfo));

  i_oid = PQfnumber(res, "oid");
  i_tableoid = PQfnumber(res, "tableoid");
  i_defaclrole = PQfnumber(res, "defaclrole");
  i_defaclnamespace = PQfnumber(res, "defaclnamespace");
  i_defaclobjtype = PQfnumber(res, "defaclobjtype");
  i_defaclacl = PQfnumber(res, "defaclacl");
  i_rdefaclacl = PQfnumber(res, "rdefaclacl");
  i_initdefaclacl = PQfnumber(res, "initdefaclacl");
  i_initrdefaclacl = PQfnumber(res, "initrdefaclacl");

  for (i = 0; i < ntups; i++)
  {
    Oid nspid = atooid(PQgetvalue(res, i, i_defaclnamespace));

    daclinfo[i].dobj.objType = DO_DEFAULT_ACL;
    daclinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
    daclinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
    AssignDumpId(&daclinfo[i].dobj);
                                                                     
    daclinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_defaclobjtype));

    if (nspid != InvalidOid)
    {
      daclinfo[i].dobj.namespace = findNamespace(fout, nspid);
    }
    else
    {
      daclinfo[i].dobj.namespace = NULL;
    }

    daclinfo[i].defaclrole = pg_strdup(PQgetvalue(res, i, i_defaclrole));
    daclinfo[i].defaclobjtype = *(PQgetvalue(res, i, i_defaclobjtype));
    daclinfo[i].defaclacl = pg_strdup(PQgetvalue(res, i, i_defaclacl));
    daclinfo[i].rdefaclacl = pg_strdup(PQgetvalue(res, i, i_rdefaclacl));
    daclinfo[i].initdefaclacl = pg_strdup(PQgetvalue(res, i, i_initdefaclacl));
    daclinfo[i].initrdefaclacl = pg_strdup(PQgetvalue(res, i, i_initrdefaclacl));

                                           
    selectDumpableDefaultACL(&(daclinfo[i]), dopt);
  }

  PQclear(res);

  destroyPQExpBuffer(query);

  return daclinfo;
}

   
                  
   
                                                                 
                                                                    
                                                                        
                                                                          
                                                                          
                                                               
                                                              
   
                                                                            
                                                                               
                                                                          
               
   
                                                                       
                                                                        
                                                                            
                                                                            
                                                                              
                                                    
   
static void
dumpComment(Archive *fout, const char *type, const char *name, const char *namespace, const char *owner, CatalogId catalogId, int subid, DumpId dumpId)
{
  DumpOptions *dopt = fout->dopt;
  CommentItem *comments;
  int ncomments;

                                                
  if (dopt->no_comments)
  {
    return;
  }

                                                                      
  if (strcmp(type, "LARGE OBJECT") != 0)
  {
    if (dopt->dataOnly)
    {
      return;
    }
  }
  else
  {
                                                         
    if (dopt->schemaOnly && !dopt->binary_upgrade)
    {
      return;
    }
  }

                                                                  
  ncomments = findComments(fout, catalogId.tableoid, catalogId.oid, &comments);

                                        
  while (ncomments > 0)
  {
    if (comments->objsubid == subid)
    {
      break;
    }
    comments++;
    ncomments--;
  }

                                                       
  if (ncomments > 0)
  {
    PQExpBuffer query = createPQExpBuffer();
    PQExpBuffer tag = createPQExpBuffer();

    appendPQExpBuffer(query, "COMMENT ON %s ", type);
    if (namespace && *namespace)
    {
      appendPQExpBuffer(query, "%s.", fmtId(namespace));
    }
    appendPQExpBuffer(query, "%s IS ", name);
    appendStringLiteralAH(query, comments->descr, fout);
    appendPQExpBufferStr(query, ";\n");

    appendPQExpBuffer(tag, "%s %s", type, name);

       
                                                                          
                                                                 
                  
       
    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tag->data, .namespace = namespace, .owner = owner, .description = "COMMENT", .section = SECTION_NONE, .createStmt = query->data, .deps = &dumpId, .nDeps = 1));

    destroyPQExpBuffer(query);
    destroyPQExpBuffer(tag);
  }
}

   
                       
   
                                                                      
                    
   
static void
dumpTableComment(Archive *fout, TableInfo *tbinfo, const char *reltypename)
{
  DumpOptions *dopt = fout->dopt;
  CommentItem *comments;
  int ncomments;
  PQExpBuffer query;
  PQExpBuffer tag;

                                                
  if (dopt->no_comments)
  {
    return;
  }

                                    
  if (dopt->dataOnly)
  {
    return;
  }

                                                                 
  ncomments = findComments(fout, tbinfo->dobj.catId.tableoid, tbinfo->dobj.catId.oid, &comments);

                                                      
  if (ncomments <= 0)
  {
    return;
  }

  query = createPQExpBuffer();
  tag = createPQExpBuffer();

  while (ncomments > 0)
  {
    const char *descr = comments->descr;
    int objsubid = comments->objsubid;

    if (objsubid == 0)
    {
      resetPQExpBuffer(tag);
      appendPQExpBuffer(tag, "%s %s", reltypename, fmtId(tbinfo->dobj.name));

      resetPQExpBuffer(query);
      appendPQExpBuffer(query, "COMMENT ON %s %s IS ", reltypename, fmtQualifiedDumpable(tbinfo));
      appendStringLiteralAH(query, descr, fout);
      appendPQExpBufferStr(query, ";\n");

      ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tag->data, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "COMMENT", .section = SECTION_NONE, .createStmt = query->data, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1));
    }
    else if (objsubid > 0 && objsubid <= tbinfo->numatts)
    {
      resetPQExpBuffer(tag);
      appendPQExpBuffer(tag, "COLUMN %s.", fmtId(tbinfo->dobj.name));
      appendPQExpBufferStr(tag, fmtId(tbinfo->attnames[objsubid - 1]));

      resetPQExpBuffer(query);
      appendPQExpBuffer(query, "COMMENT ON COLUMN %s.", fmtQualifiedDumpable(tbinfo));
      appendPQExpBuffer(query, "%s IS ", fmtId(tbinfo->attnames[objsubid - 1]));
      appendStringLiteralAH(query, descr, fout);
      appendPQExpBufferStr(query, ";\n");

      ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tag->data, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "COMMENT", .section = SECTION_NONE, .createStmt = query->data, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1));
    }

    comments++;
    ncomments--;
  }

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(tag);
}

   
                   
   
                                                                           
                                                                            
               
   
static int
findComments(Archive *fout, Oid classoid, Oid objoid, CommentItem **items)
{
                                            
  static CommentItem *comments = NULL;
  static int ncomments = -1;

  CommentItem *middle = NULL;
  CommentItem *low;
  CommentItem *high;
  int nmatch;

                                         
  if (ncomments < 0)
  {
    ncomments = collectComments(fout, &comments);
  }

     
                                                             
     
  low = &comments[0];
  high = &comments[ncomments - 1];
  while (low <= high)
  {
    middle = low + (high - low) / 2;

    if (classoid < middle->classoid)
    {
      high = middle - 1;
    }
    else if (classoid > middle->classoid)
    {
      low = middle + 1;
    }
    else if (objoid < middle->objoid)
    {
      high = middle - 1;
    }
    else if (objoid > middle->objoid)
    {
      low = middle + 1;
    }
    else
    {
      break;                    
    }
  }

  if (low > high)                 
  {
    *items = NULL;
    return 0;
  }

     
                                                                     
                                                                            
            
     
  nmatch = 1;
  while (middle > low)
  {
    if (classoid != middle[-1].classoid || objoid != middle[-1].objoid)
    {
      break;
    }
    middle--;
    nmatch++;
  }

  *items = middle;

  middle += nmatch;
  while (middle <= high)
  {
    if (classoid != middle->classoid || objoid != middle->objoid)
    {
      break;
    }
    middle++;
    nmatch++;
  }

  return nmatch;
}

   
                      
   
                                                                     
                                                                           
                                                                        
               
   
                                                                       
   
static int
collectComments(Archive *fout, CommentItem **items)
{
  PGresult *res;
  PQExpBuffer query;
  int i_description;
  int i_classoid;
  int i_objoid;
  int i_objsubid;
  int ntups;
  int i;
  CommentItem *comments;

  query = createPQExpBuffer();

  appendPQExpBufferStr(query, "SELECT description, classoid, objoid, objsubid "
                              "FROM pg_catalog.pg_description "
                              "ORDER BY classoid, objoid, objsubid");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

                                                              

  i_description = PQfnumber(res, "description");
  i_classoid = PQfnumber(res, "classoid");
  i_objoid = PQfnumber(res, "objoid");
  i_objsubid = PQfnumber(res, "objsubid");

  ntups = PQntuples(res);

  comments = (CommentItem *)pg_malloc(ntups * sizeof(CommentItem));

  for (i = 0; i < ntups; i++)
  {
    comments[i].descr = PQgetvalue(res, i, i_description);
    comments[i].classoid = atooid(PQgetvalue(res, i, i_classoid));
    comments[i].objoid = atooid(PQgetvalue(res, i, i_objoid));
    comments[i].objsubid = atoi(PQgetvalue(res, i, i_objsubid));
  }

                                                                      
  destroyPQExpBuffer(query);

  *items = comments;
  return ntups;
}

   
                      
   
                                                                  
                                                              
   
static void
dumpDumpableObject(Archive *fout, DumpableObject *dobj)
{
  switch (dobj->objType)
  {
  case DO_NAMESPACE:
    dumpNamespace(fout, (NamespaceInfo *)dobj);
    break;
  case DO_EXTENSION:
    dumpExtension(fout, (ExtensionInfo *)dobj);
    break;
  case DO_TYPE:
    dumpType(fout, (TypeInfo *)dobj);
    break;
  case DO_SHELL_TYPE:
    dumpShellType(fout, (ShellTypeInfo *)dobj);
    break;
  case DO_FUNC:
    dumpFunc(fout, (FuncInfo *)dobj);
    break;
  case DO_AGG:
    dumpAgg(fout, (AggInfo *)dobj);
    break;
  case DO_OPERATOR:
    dumpOpr(fout, (OprInfo *)dobj);
    break;
  case DO_ACCESS_METHOD:
    dumpAccessMethod(fout, (AccessMethodInfo *)dobj);
    break;
  case DO_OPCLASS:
    dumpOpclass(fout, (OpclassInfo *)dobj);
    break;
  case DO_OPFAMILY:
    dumpOpfamily(fout, (OpfamilyInfo *)dobj);
    break;
  case DO_COLLATION:
    dumpCollation(fout, (CollInfo *)dobj);
    break;
  case DO_CONVERSION:
    dumpConversion(fout, (ConvInfo *)dobj);
    break;
  case DO_TABLE:
    dumpTable(fout, (TableInfo *)dobj);
    break;
  case DO_ATTRDEF:
    dumpAttrDef(fout, (AttrDefInfo *)dobj);
    break;
  case DO_INDEX:
    dumpIndex(fout, (IndxInfo *)dobj);
    break;
  case DO_INDEX_ATTACH:
    dumpIndexAttach(fout, (IndexAttachInfo *)dobj);
    break;
  case DO_STATSEXT:
    dumpStatisticsExt(fout, (StatsExtInfo *)dobj);
    break;
  case DO_REFRESH_MATVIEW:
    refreshMatViewData(fout, (TableDataInfo *)dobj);
    break;
  case DO_RULE:
    dumpRule(fout, (RuleInfo *)dobj);
    break;
  case DO_TRIGGER:
    dumpTrigger(fout, (TriggerInfo *)dobj);
    break;
  case DO_EVENT_TRIGGER:
    dumpEventTrigger(fout, (EventTriggerInfo *)dobj);
    break;
  case DO_CONSTRAINT:
    dumpConstraint(fout, (ConstraintInfo *)dobj);
    break;
  case DO_FK_CONSTRAINT:
    dumpConstraint(fout, (ConstraintInfo *)dobj);
    break;
  case DO_PROCLANG:
    dumpProcLang(fout, (ProcLangInfo *)dobj);
    break;
  case DO_CAST:
    dumpCast(fout, (CastInfo *)dobj);
    break;
  case DO_TRANSFORM:
    dumpTransform(fout, (TransformInfo *)dobj);
    break;
  case DO_SEQUENCE_SET:
    dumpSequenceData(fout, (TableDataInfo *)dobj);
    break;
  case DO_TABLE_DATA:
    dumpTableData(fout, (TableDataInfo *)dobj);
    break;
  case DO_DUMMY_TYPE:
                                                                    
    break;
  case DO_TSPARSER:
    dumpTSParser(fout, (TSParserInfo *)dobj);
    break;
  case DO_TSDICT:
    dumpTSDictionary(fout, (TSDictInfo *)dobj);
    break;
  case DO_TSTEMPLATE:
    dumpTSTemplate(fout, (TSTemplateInfo *)dobj);
    break;
  case DO_TSCONFIG:
    dumpTSConfig(fout, (TSConfigInfo *)dobj);
    break;
  case DO_FDW:
    dumpForeignDataWrapper(fout, (FdwInfo *)dobj);
    break;
  case DO_FOREIGN_SERVER:
    dumpForeignServer(fout, (ForeignServerInfo *)dobj);
    break;
  case DO_DEFAULT_ACL:
    dumpDefaultACL(fout, (DefaultACLInfo *)dobj);
    break;
  case DO_BLOB:
    dumpBlob(fout, (BlobInfo *)dobj);
    break;
  case DO_BLOB_DATA:
    if (dobj->dump & DUMP_COMPONENT_DATA)
    {
      TocEntry *te;

      te = ArchiveEntry(fout, dobj->catId, dobj->dumpId, ARCHIVE_OPTS(.tag = dobj->name, .description = "BLOBS", .section = SECTION_DATA, .dumpFn = dumpBlobs));

         
                                                              
                                                                  
                                                               
                                                                  
                                                                   
                                                                    
                                                                   
                                                                     
                                                                
                                                                     
                                                               
         
      te->dataLength = MaxBlockNumber;
    }
    break;
  case DO_POLICY:
    dumpPolicy(fout, (PolicyInfo *)dobj);
    break;
  case DO_PUBLICATION:
    dumpPublication(fout, (PublicationInfo *)dobj);
    break;
  case DO_PUBLICATION_REL:
    dumpPublicationTable(fout, (PublicationRelInfo *)dobj);
    break;
  case DO_SUBSCRIPTION:
    dumpSubscription(fout, (SubscriptionInfo *)dobj);
    break;
  case DO_PRE_DATA_BOUNDARY:
  case DO_POST_DATA_BOUNDARY:
                                     
    break;
  }
}

   
                 
                                                                         
   
static void
dumpNamespace(Archive *fout, NamespaceInfo *nspinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qnspname;

                                
  if (!nspinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qnspname = pg_strdup(fmtId(nspinfo->dobj.name));

  appendPQExpBuffer(delq, "DROP SCHEMA %s;\n", qnspname);

  appendPQExpBuffer(q, "CREATE SCHEMA %s;\n", qnspname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &nspinfo->dobj, "SCHEMA", qnspname, NULL);
  }

  if (nspinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = nspinfo->dobj.name, .owner = nspinfo->rolname, .description = "SCHEMA", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                                
  if (nspinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "SCHEMA", qnspname, NULL, nspinfo->rolname, nspinfo->dobj.catId, 0, nspinfo->dobj.dumpId);
  }

  if (nspinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "SCHEMA", qnspname, NULL, nspinfo->rolname, nspinfo->dobj.catId, 0, nspinfo->dobj.dumpId);
  }

  if (nspinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, nspinfo->dobj.dumpId, InvalidDumpId, "SCHEMA", qnspname, NULL, NULL, nspinfo->rolname, nspinfo->nspacl, nspinfo->rnspacl, nspinfo->initnspacl, nspinfo->initrnspacl);
  }

  free(qnspname);

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
}

   
                 
                                                             
   
static void
dumpExtension(Archive *fout, ExtensionInfo *extinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qextname;

                                
  if (!extinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qextname = pg_strdup(fmtId(extinfo->dobj.name));

  appendPQExpBuffer(delq, "DROP EXTENSION %s;\n", qextname);

  if (!dopt->binary_upgrade)
  {
       
                                                                        
                                                                        
                                
       
                                                                         
                                                                          
                                                                     
                                            
       
    appendPQExpBuffer(q, "CREATE EXTENSION IF NOT EXISTS %s WITH SCHEMA %s;\n", qextname, fmtId(extinfo->namespace));
  }
  else
  {
       
                                                                           
                                                                           
                                                                       
                                                                       
                                                                     
                                                 
       
    int i;
    int n;

    appendPQExpBufferStr(q, "-- For binary upgrade, create an empty extension and insert objects into it\n");

       
                                                                         
                                                                         
                                                                          
       
    appendPQExpBuffer(q, "DROP EXTENSION IF EXISTS %s;\n", qextname);

    appendPQExpBufferStr(q, "SELECT pg_catalog.binary_upgrade_create_empty_extension(");
    appendStringLiteralAH(q, extinfo->dobj.name, fout);
    appendPQExpBufferStr(q, ", ");
    appendStringLiteralAH(q, extinfo->namespace, fout);
    appendPQExpBufferStr(q, ", ");
    appendPQExpBuffer(q, "%s, ", extinfo->relocatable ? "true" : "false");
    appendStringLiteralAH(q, extinfo->extversion, fout);
    appendPQExpBufferStr(q, ", ");

       
                                                                  
                                                                         
                                    
       
    if (strlen(extinfo->extconfig) > 2)
    {
      appendStringLiteralAH(q, extinfo->extconfig, fout);
    }
    else
    {
      appendPQExpBufferStr(q, "NULL");
    }
    appendPQExpBufferStr(q, ", ");
    if (strlen(extinfo->extcondition) > 2)
    {
      appendStringLiteralAH(q, extinfo->extcondition, fout);
    }
    else
    {
      appendPQExpBufferStr(q, "NULL");
    }
    appendPQExpBufferStr(q, ", ");
    appendPQExpBufferStr(q, "ARRAY[");
    n = 0;
    for (i = 0; i < extinfo->dobj.nDeps; i++)
    {
      DumpableObject *extobj;

      extobj = findObjectByDumpId(extinfo->dobj.dependencies[i]);
      if (extobj && extobj->objType == DO_EXTENSION)
      {
        if (n++ > 0)
        {
          appendPQExpBufferChar(q, ',');
        }
        appendStringLiteralAH(q, extobj->name, fout);
      }
    }
    appendPQExpBufferStr(q, "]::pg_catalog.text[]");
    appendPQExpBufferStr(q, ");\n");
  }

  if (extinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, extinfo->dobj.catId, extinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = extinfo->dobj.name, .description = "EXTENSION", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                                   
  if (extinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "EXTENSION", qextname, NULL, "", extinfo->dobj.catId, 0, extinfo->dobj.dumpId);
  }

  if (extinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "EXTENSION", qextname, NULL, "", extinfo->dobj.catId, 0, extinfo->dobj.dumpId);
  }

  free(qextname);

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
}

   
            
                                                                    
   
static void
dumpType(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;

                                
  if (!tyinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

                                
  if (tyinfo->typtype == TYPTYPE_BASE)
  {
    dumpBaseType(fout, tyinfo);
  }
  else if (tyinfo->typtype == TYPTYPE_DOMAIN)
  {
    dumpDomain(fout, tyinfo);
  }
  else if (tyinfo->typtype == TYPTYPE_COMPOSITE)
  {
    dumpCompositeType(fout, tyinfo);
  }
  else if (tyinfo->typtype == TYPTYPE_ENUM)
  {
    dumpEnumType(fout, tyinfo);
  }
  else if (tyinfo->typtype == TYPTYPE_RANGE)
  {
    dumpRangeType(fout, tyinfo);
  }
  else if (tyinfo->typtype == TYPTYPE_PSEUDO && !tyinfo->isDefined)
  {
    dumpUndefinedType(fout, tyinfo);
  }
  else
  {
    pg_log_warning("typtype of data type \"%s\" appears to be invalid", tyinfo->dobj.name);
  }
}

   
                
                                                                         
   
static void
dumpEnumType(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  int num, i;
  Oid enum_oid;
  char *qtypname;
  char *qualtypname;
  char *label;

  if (fout->remoteVersion >= 90100)
  {
    appendPQExpBuffer(query,
        "SELECT oid, enumlabel "
        "FROM pg_catalog.pg_enum "
        "WHERE enumtypid = '%u'"
        "ORDER BY enumsortorder",
        tyinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT oid, enumlabel "
        "FROM pg_catalog.pg_enum "
        "WHERE enumtypid = '%u'"
        "ORDER BY oid",
        tyinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  num = PQntuples(res);

  qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
  qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

     
                                                                          
                                                   
     
  appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo->dobj.catId.oid, false);
  }

  appendPQExpBuffer(q, "CREATE TYPE %s AS ENUM (", qualtypname);

  if (!dopt->binary_upgrade)
  {
                                          
    for (i = 0; i < num; i++)
    {
      label = PQgetvalue(res, i, PQfnumber(res, "enumlabel"));
      if (i > 0)
      {
        appendPQExpBufferChar(q, ',');
      }
      appendPQExpBufferStr(q, "\n    ");
      appendStringLiteralAH(q, label, fout);
    }
  }

  appendPQExpBufferStr(q, "\n);\n");

  if (dopt->binary_upgrade)
  {
                                                    
    for (i = 0; i < num; i++)
    {
      enum_oid = atooid(PQgetvalue(res, i, PQfnumber(res, "oid")));
      label = PQgetvalue(res, i, PQfnumber(res, "enumlabel"));

      if (i == 0)
      {
        appendPQExpBufferStr(q, "\n-- For binary upgrade, must preserve pg_enum oids\n");
      }
      appendPQExpBuffer(q, "SELECT pg_catalog.binary_upgrade_set_next_pg_enum_oid('%u'::pg_catalog.oid);\n", enum_oid);
      appendPQExpBuffer(q, "ALTER TYPE %s ADD VALUE ", qualtypname);
      appendStringLiteralAH(q, label, fout);
      appendPQExpBufferStr(q, ";\n\n");
    }
  }

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tyinfo->dobj, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tyinfo->dobj.name, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "TYPE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                              
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, tyinfo->dobj.dumpId, InvalidDumpId, "TYPE", qtypname, NULL, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->typacl, tyinfo->rtypacl, tyinfo->inittypacl, tyinfo->initrtypacl);
  }

  PQclear(res);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qtypname);
  free(qualtypname);
}

   
                 
                                                                          
   
static void
dumpRangeType(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  Oid collationOid;
  char *qtypname;
  char *qualtypname;
  char *procname;

  appendPQExpBuffer(query,
      "SELECT pg_catalog.format_type(rngsubtype, NULL) AS rngsubtype, "
      "opc.opcname AS opcname, "
      "(SELECT nspname FROM pg_catalog.pg_namespace nsp "
      "  WHERE nsp.oid = opc.opcnamespace) AS opcnsp, "
      "opc.opcdefault, "
      "CASE WHEN rngcollation = st.typcollation THEN 0 "
      "     ELSE rngcollation END AS collation, "
      "rngcanonical, rngsubdiff "
      "FROM pg_catalog.pg_range r, pg_catalog.pg_type st, "
      "     pg_catalog.pg_opclass opc "
      "WHERE st.oid = rngsubtype AND opc.oid = rngsubopc AND "
      "rngtypid = '%u'",
      tyinfo->dobj.catId.oid);

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
  qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

     
                                                                          
                                                   
     
  appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo->dobj.catId.oid, false);
  }

  appendPQExpBuffer(q, "CREATE TYPE %s AS RANGE (", qualtypname);

  appendPQExpBuffer(q, "\n    subtype = %s", PQgetvalue(res, 0, PQfnumber(res, "rngsubtype")));

                                                             
  if (PQgetvalue(res, 0, PQfnumber(res, "opcdefault"))[0] != 't')
  {
    char *opcname = PQgetvalue(res, 0, PQfnumber(res, "opcname"));
    char *nspname = PQgetvalue(res, 0, PQfnumber(res, "opcnsp"));

    appendPQExpBuffer(q, ",\n    subtype_opclass = %s.", fmtId(nspname));
    appendPQExpBufferStr(q, fmtId(opcname));
  }

  collationOid = atooid(PQgetvalue(res, 0, PQfnumber(res, "collation")));
  if (OidIsValid(collationOid))
  {
    CollInfo *coll = findCollationByOid(collationOid);

    if (coll)
    {
      appendPQExpBuffer(q, ",\n    collation = %s", fmtQualifiedDumpable(coll));
    }
  }

  procname = PQgetvalue(res, 0, PQfnumber(res, "rngcanonical"));
  if (strcmp(procname, "-") != 0)
  {
    appendPQExpBuffer(q, ",\n    canonical = %s", procname);
  }

  procname = PQgetvalue(res, 0, PQfnumber(res, "rngsubdiff"));
  if (strcmp(procname, "-") != 0)
  {
    appendPQExpBuffer(q, ",\n    subtype_diff = %s", procname);
  }

  appendPQExpBufferStr(q, "\n);\n");

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tyinfo->dobj, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tyinfo->dobj.name, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "TYPE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                              
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, tyinfo->dobj.dumpId, InvalidDumpId, "TYPE", qtypname, NULL, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->typacl, tyinfo->rtypacl, tyinfo->inittypacl, tyinfo->initrtypacl);
  }

  PQclear(res);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qtypname);
  free(qualtypname);
}

   
                     
                                                                     
   
                                                                         
                                                                         
                                                                          
                    
   
static void
dumpUndefinedType(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  char *qtypname;
  char *qualtypname;

  qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
  qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

  appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo->dobj.catId.oid, false);
  }

  appendPQExpBuffer(q, "CREATE TYPE %s;\n", qualtypname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tyinfo->dobj, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tyinfo->dobj.name, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "TYPE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                              
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, tyinfo->dobj.dumpId, InvalidDumpId, "TYPE", qtypname, NULL, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->typacl, tyinfo->rtypacl, tyinfo->inittypacl, tyinfo->initrtypacl);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qtypname);
  free(qualtypname);
}

   
                
                                                                         
   
static void
dumpBaseType(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  char *qtypname;
  char *qualtypname;
  char *typlen;
  char *typinput;
  char *typoutput;
  char *typreceive;
  char *typsend;
  char *typmodin;
  char *typmodout;
  char *typanalyze;
  Oid typreceiveoid;
  Oid typsendoid;
  Oid typmodinoid;
  Oid typmodoutoid;
  Oid typanalyzeoid;
  char *typcategory;
  char *typispreferred;
  char *typdelim;
  char *typbyval;
  char *typalign;
  char *typstorage;
  char *typcollatable;
  char *typdefault;
  bool typdefault_is_literal = false;

                                   
  if (fout->remoteVersion >= 90100)
  {
    appendPQExpBuffer(query,
        "SELECT typlen, "
        "typinput, typoutput, typreceive, typsend, "
        "typmodin, typmodout, typanalyze, "
        "typreceive::pg_catalog.oid AS typreceiveoid, "
        "typsend::pg_catalog.oid AS typsendoid, "
        "typmodin::pg_catalog.oid AS typmodinoid, "
        "typmodout::pg_catalog.oid AS typmodoutoid, "
        "typanalyze::pg_catalog.oid AS typanalyzeoid, "
        "typcategory, typispreferred, "
        "typdelim, typbyval, typalign, typstorage, "
        "(typcollation <> 0) AS typcollatable, "
        "pg_catalog.pg_get_expr(typdefaultbin, 0) AS typdefaultbin, typdefault "
        "FROM pg_catalog.pg_type "
        "WHERE oid = '%u'::pg_catalog.oid",
        tyinfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80400)
  {
    appendPQExpBuffer(query,
        "SELECT typlen, "
        "typinput, typoutput, typreceive, typsend, "
        "typmodin, typmodout, typanalyze, "
        "typreceive::pg_catalog.oid AS typreceiveoid, "
        "typsend::pg_catalog.oid AS typsendoid, "
        "typmodin::pg_catalog.oid AS typmodinoid, "
        "typmodout::pg_catalog.oid AS typmodoutoid, "
        "typanalyze::pg_catalog.oid AS typanalyzeoid, "
        "typcategory, typispreferred, "
        "typdelim, typbyval, typalign, typstorage, "
        "false AS typcollatable, "
        "pg_catalog.pg_get_expr(typdefaultbin, 0) AS typdefaultbin, typdefault "
        "FROM pg_catalog.pg_type "
        "WHERE oid = '%u'::pg_catalog.oid",
        tyinfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80300)
  {
                                                                     
    appendPQExpBuffer(query,
        "SELECT typlen, "
        "typinput, typoutput, typreceive, typsend, "
        "typmodin, typmodout, typanalyze, "
        "typreceive::pg_catalog.oid AS typreceiveoid, "
        "typsend::pg_catalog.oid AS typsendoid, "
        "typmodin::pg_catalog.oid AS typmodinoid, "
        "typmodout::pg_catalog.oid AS typmodoutoid, "
        "typanalyze::pg_catalog.oid AS typanalyzeoid, "
        "'U' AS typcategory, false AS typispreferred, "
        "typdelim, typbyval, typalign, typstorage, "
        "false AS typcollatable, "
        "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
        "FROM pg_catalog.pg_type "
        "WHERE oid = '%u'::pg_catalog.oid",
        tyinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT typlen, "
        "typinput, typoutput, typreceive, typsend, "
        "'-' AS typmodin, '-' AS typmodout, "
        "typanalyze, "
        "typreceive::pg_catalog.oid AS typreceiveoid, "
        "typsend::pg_catalog.oid AS typsendoid, "
        "0 AS typmodinoid, 0 AS typmodoutoid, "
        "typanalyze::pg_catalog.oid AS typanalyzeoid, "
        "'U' AS typcategory, false AS typispreferred, "
        "typdelim, typbyval, typalign, typstorage, "
        "false AS typcollatable, "
        "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
        "FROM pg_catalog.pg_type "
        "WHERE oid = '%u'::pg_catalog.oid",
        tyinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  typlen = PQgetvalue(res, 0, PQfnumber(res, "typlen"));
  typinput = PQgetvalue(res, 0, PQfnumber(res, "typinput"));
  typoutput = PQgetvalue(res, 0, PQfnumber(res, "typoutput"));
  typreceive = PQgetvalue(res, 0, PQfnumber(res, "typreceive"));
  typsend = PQgetvalue(res, 0, PQfnumber(res, "typsend"));
  typmodin = PQgetvalue(res, 0, PQfnumber(res, "typmodin"));
  typmodout = PQgetvalue(res, 0, PQfnumber(res, "typmodout"));
  typanalyze = PQgetvalue(res, 0, PQfnumber(res, "typanalyze"));
  typreceiveoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typreceiveoid")));
  typsendoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typsendoid")));
  typmodinoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typmodinoid")));
  typmodoutoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typmodoutoid")));
  typanalyzeoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typanalyzeoid")));
  typcategory = PQgetvalue(res, 0, PQfnumber(res, "typcategory"));
  typispreferred = PQgetvalue(res, 0, PQfnumber(res, "typispreferred"));
  typdelim = PQgetvalue(res, 0, PQfnumber(res, "typdelim"));
  typbyval = PQgetvalue(res, 0, PQfnumber(res, "typbyval"));
  typalign = PQgetvalue(res, 0, PQfnumber(res, "typalign"));
  typstorage = PQgetvalue(res, 0, PQfnumber(res, "typstorage"));
  typcollatable = PQgetvalue(res, 0, PQfnumber(res, "typcollatable"));
  if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
  {
    typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
  }
  else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
  {
    typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
    typdefault_is_literal = true;                      
  }
  else
  {
    typdefault = NULL;
  }

  qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
  qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

     
                                                                           
                                                                             
                
     
  appendPQExpBuffer(delq, "DROP TYPE %s CASCADE;\n", qualtypname);

     
                                                                    
                                                                   
     
  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo->dobj.catId.oid, false);
  }

  appendPQExpBuffer(q,
      "CREATE TYPE %s (\n"
      "    INTERNALLENGTH = %s",
      qualtypname, (strcmp(typlen, "-1") == 0) ? "variable" : typlen);

                                                     
  appendPQExpBuffer(q, ",\n    INPUT = %s", typinput);
  appendPQExpBuffer(q, ",\n    OUTPUT = %s", typoutput);
  if (OidIsValid(typreceiveoid))
  {
    appendPQExpBuffer(q, ",\n    RECEIVE = %s", typreceive);
  }
  if (OidIsValid(typsendoid))
  {
    appendPQExpBuffer(q, ",\n    SEND = %s", typsend);
  }
  if (OidIsValid(typmodinoid))
  {
    appendPQExpBuffer(q, ",\n    TYPMOD_IN = %s", typmodin);
  }
  if (OidIsValid(typmodoutoid))
  {
    appendPQExpBuffer(q, ",\n    TYPMOD_OUT = %s", typmodout);
  }
  if (OidIsValid(typanalyzeoid))
  {
    appendPQExpBuffer(q, ",\n    ANALYZE = %s", typanalyze);
  }

  if (strcmp(typcollatable, "t") == 0)
  {
    appendPQExpBufferStr(q, ",\n    COLLATABLE = true");
  }

  if (typdefault != NULL)
  {
    appendPQExpBufferStr(q, ",\n    DEFAULT = ");
    if (typdefault_is_literal)
    {
      appendStringLiteralAH(q, typdefault, fout);
    }
    else
    {
      appendPQExpBufferStr(q, typdefault);
    }
  }

  if (OidIsValid(tyinfo->typelem))
  {
    appendPQExpBuffer(q, ",\n    ELEMENT = %s", getFormattedTypeName(fout, tyinfo->typelem, zeroAsOpaque));
  }

  if (strcmp(typcategory, "U") != 0)
  {
    appendPQExpBufferStr(q, ",\n    CATEGORY = ");
    appendStringLiteralAH(q, typcategory, fout);
  }

  if (strcmp(typispreferred, "t") == 0)
  {
    appendPQExpBufferStr(q, ",\n    PREFERRED = true");
  }

  if (typdelim && strcmp(typdelim, ",") != 0)
  {
    appendPQExpBufferStr(q, ",\n    DELIMITER = ");
    appendStringLiteralAH(q, typdelim, fout);
  }

  if (strcmp(typalign, "c") == 0)
  {
    appendPQExpBufferStr(q, ",\n    ALIGNMENT = char");
  }
  else if (strcmp(typalign, "s") == 0)
  {
    appendPQExpBufferStr(q, ",\n    ALIGNMENT = int2");
  }
  else if (strcmp(typalign, "i") == 0)
  {
    appendPQExpBufferStr(q, ",\n    ALIGNMENT = int4");
  }
  else if (strcmp(typalign, "d") == 0)
  {
    appendPQExpBufferStr(q, ",\n    ALIGNMENT = double");
  }

  if (strcmp(typstorage, "p") == 0)
  {
    appendPQExpBufferStr(q, ",\n    STORAGE = plain");
  }
  else if (strcmp(typstorage, "e") == 0)
  {
    appendPQExpBufferStr(q, ",\n    STORAGE = external");
  }
  else if (strcmp(typstorage, "x") == 0)
  {
    appendPQExpBufferStr(q, ",\n    STORAGE = extended");
  }
  else if (strcmp(typstorage, "m") == 0)
  {
    appendPQExpBufferStr(q, ",\n    STORAGE = main");
  }

  if (strcmp(typbyval, "t") == 0)
  {
    appendPQExpBufferStr(q, ",\n    PASSEDBYVALUE");
  }

  appendPQExpBufferStr(q, "\n);\n");

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tyinfo->dobj, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tyinfo->dobj.name, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "TYPE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                              
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, tyinfo->dobj.dumpId, InvalidDumpId, "TYPE", qtypname, NULL, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->typacl, tyinfo->rtypacl, tyinfo->inittypacl, tyinfo->initrtypacl);
  }

  PQclear(res);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qtypname);
  free(qualtypname);
}

   
              
                                                                      
   
static void
dumpDomain(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  int i;
  char *qtypname;
  char *qualtypname;
  char *typnotnull;
  char *typdefn;
  char *typdefault;
  Oid typcollation;
  bool typdefault_is_literal = false;

                                     
  if (fout->remoteVersion >= 90100)
  {
                                    
    appendPQExpBuffer(query,
        "SELECT t.typnotnull, "
        "pg_catalog.format_type(t.typbasetype, t.typtypmod) AS typdefn, "
        "pg_catalog.pg_get_expr(t.typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, "
        "t.typdefault, "
        "CASE WHEN t.typcollation <> u.typcollation "
        "THEN t.typcollation ELSE 0 END AS typcollation "
        "FROM pg_catalog.pg_type t "
        "LEFT JOIN pg_catalog.pg_type u ON (t.typbasetype = u.oid) "
        "WHERE t.oid = '%u'::pg_catalog.oid",
        tyinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT typnotnull, "
        "pg_catalog.format_type(typbasetype, typtypmod) AS typdefn, "
        "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, "
        "typdefault, 0 AS typcollation "
        "FROM pg_catalog.pg_type "
        "WHERE oid = '%u'::pg_catalog.oid",
        tyinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  typnotnull = PQgetvalue(res, 0, PQfnumber(res, "typnotnull"));
  typdefn = PQgetvalue(res, 0, PQfnumber(res, "typdefn"));
  if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
  {
    typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
  }
  else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
  {
    typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
    typdefault_is_literal = true;                      
  }
  else
  {
    typdefault = NULL;
  }
  typcollation = atooid(PQgetvalue(res, 0, PQfnumber(res, "typcollation")));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo->dobj.catId.oid, true);                       
  }

  qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
  qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

  appendPQExpBuffer(q, "CREATE DOMAIN %s AS %s", qualtypname, typdefn);

                                                                    
  if (OidIsValid(typcollation))
  {
    CollInfo *coll;

    coll = findCollationByOid(typcollation);
    if (coll)
    {
      appendPQExpBuffer(q, " COLLATE %s", fmtQualifiedDumpable(coll));
    }
  }

  if (typnotnull[0] == 't')
  {
    appendPQExpBufferStr(q, " NOT NULL");
  }

  if (typdefault != NULL)
  {
    appendPQExpBufferStr(q, " DEFAULT ");
    if (typdefault_is_literal)
    {
      appendStringLiteralAH(q, typdefault, fout);
    }
    else
    {
      appendPQExpBufferStr(q, typdefault);
    }
  }

  PQclear(res);

     
                                              
     
  for (i = 0; i < tyinfo->nDomChecks; i++)
  {
    ConstraintInfo *domcheck = &(tyinfo->domChecks[i]);

    if (!domcheck->separate)
    {
      appendPQExpBuffer(q, "\n\tCONSTRAINT %s %s", fmtId(domcheck->dobj.name), domcheck->condef);
    }
  }

  appendPQExpBufferStr(q, ";\n");

  appendPQExpBuffer(delq, "DROP DOMAIN %s;\n", qualtypname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tyinfo->dobj, "DOMAIN", qtypname, tyinfo->dobj.namespace->dobj.name);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tyinfo->dobj.name, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "DOMAIN", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                                
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "DOMAIN", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "DOMAIN", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, tyinfo->dobj.dumpId, InvalidDumpId, "TYPE", qtypname, NULL, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->typacl, tyinfo->rtypacl, tyinfo->inittypacl, tyinfo->initrtypacl);
  }

                                        
  for (i = 0; i < tyinfo->nDomChecks; i++)
  {
    ConstraintInfo *domcheck = &(tyinfo->domChecks[i]);
    PQExpBuffer conprefix = createPQExpBuffer();

    appendPQExpBuffer(conprefix, "CONSTRAINT %s ON DOMAIN", fmtId(domcheck->dobj.name));

    if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
    {
      dumpComment(fout, conprefix->data, qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, domcheck->dobj.catId, 0, tyinfo->dobj.dumpId);
    }

    destroyPQExpBuffer(conprefix);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qtypname);
  free(qualtypname);
}

   
                     
                                                                           
                    
   
static void
dumpCompositeType(Archive *fout, TypeInfo *tyinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer dropped = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  PQExpBuffer query = createPQExpBuffer();
  PGresult *res;
  char *qtypname;
  char *qualtypname;
  int ntups;
  int i_attname;
  int i_atttypdefn;
  int i_attlen;
  int i_attalign;
  int i_attisdropped;
  int i_attcollation;
  int i;
  int actual_atts;

                                   
  if (fout->remoteVersion >= 90100)
  {
       
                                                                       
                                                                      
                                                                    
                                                                       
                                            
       
    appendPQExpBuffer(query,
        "SELECT a.attname, "
        "pg_catalog.format_type(a.atttypid, a.atttypmod) AS atttypdefn, "
        "a.attlen, a.attalign, a.attisdropped, "
        "CASE WHEN a.attcollation <> at.typcollation "
        "THEN a.attcollation ELSE 0 END AS attcollation "
        "FROM pg_catalog.pg_type ct "
        "JOIN pg_catalog.pg_attribute a ON a.attrelid = ct.typrelid "
        "LEFT JOIN pg_catalog.pg_type at ON at.oid = a.atttypid "
        "WHERE ct.oid = '%u'::pg_catalog.oid "
        "ORDER BY a.attnum ",
        tyinfo->dobj.catId.oid);
  }
  else
  {
       
                                                                       
                               
       
    appendPQExpBuffer(query,
        "SELECT a.attname, "
        "pg_catalog.format_type(a.atttypid, a.atttypmod) AS atttypdefn, "
        "a.attlen, a.attalign, a.attisdropped, "
        "0 AS attcollation "
        "FROM pg_catalog.pg_type ct, pg_catalog.pg_attribute a "
        "WHERE ct.oid = '%u'::pg_catalog.oid "
        "AND a.attrelid = ct.typrelid "
        "ORDER BY a.attnum ",
        tyinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_attname = PQfnumber(res, "attname");
  i_atttypdefn = PQfnumber(res, "atttypdefn");
  i_attlen = PQfnumber(res, "attlen");
  i_attalign = PQfnumber(res, "attalign");
  i_attisdropped = PQfnumber(res, "attisdropped");
  i_attcollation = PQfnumber(res, "attcollation");

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo->dobj.catId.oid, false);
    binary_upgrade_set_pg_class_oids(fout, q, tyinfo->typrelid, false);
  }

  qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
  qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

  appendPQExpBuffer(q, "CREATE TYPE %s AS (", qualtypname);

  actual_atts = 0;
  for (i = 0; i < ntups; i++)
  {
    char *attname;
    char *atttypdefn;
    char *attlen;
    char *attalign;
    bool attisdropped;
    Oid attcollation;

    attname = PQgetvalue(res, i, i_attname);
    atttypdefn = PQgetvalue(res, i, i_atttypdefn);
    attlen = PQgetvalue(res, i, i_attlen);
    attalign = PQgetvalue(res, i, i_attalign);
    attisdropped = (PQgetvalue(res, i, i_attisdropped)[0] == 't');
    attcollation = atooid(PQgetvalue(res, i, i_attcollation));

    if (attisdropped && !dopt->binary_upgrade)
    {
      continue;
    }

                                           
    if (actual_atts++ > 0)
    {
      appendPQExpBufferChar(q, ',');
    }
    appendPQExpBufferStr(q, "\n\t");

    if (!attisdropped)
    {
      appendPQExpBuffer(q, "%s %s", fmtId(attname), atttypdefn);

                                                            
      if (OidIsValid(attcollation))
      {
        CollInfo *coll;

        coll = findCollationByOid(attcollation);
        if (coll)
        {
          appendPQExpBuffer(q, " COLLATE %s", fmtQualifiedDumpable(coll));
        }
      }
    }
    else
    {
         
                                                                       
                                                                         
                                                                 
                                                            
         
      appendPQExpBuffer(q, "%s INTEGER            ", fmtId(attname));

                                                                
      appendPQExpBufferStr(dropped, "\n-- For binary upgrade, recreate dropped column.\n");
      appendPQExpBuffer(dropped,
          "UPDATE pg_catalog.pg_attribute\n"
          "SET attlen = %s, "
          "attalign = '%s', attbyval = false\n"
          "WHERE attname = ",
          attlen, attalign);
      appendStringLiteralAH(dropped, attname, fout);
      appendPQExpBufferStr(dropped, "\n  AND attrelid = ");
      appendStringLiteralAH(dropped, qualtypname, fout);
      appendPQExpBufferStr(dropped, "::pg_catalog.regclass;\n");

      appendPQExpBuffer(dropped, "ALTER TYPE %s ", qualtypname);
      appendPQExpBuffer(dropped, "DROP ATTRIBUTE %s;\n", fmtId(attname));
    }
  }
  appendPQExpBufferStr(q, "\n);\n");
  appendPQExpBufferStr(q, dropped->data);

  appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tyinfo->dobj, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tyinfo->dobj.name, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "TYPE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                              
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "TYPE", qtypname, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
  }

  if (tyinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, tyinfo->dobj.dumpId, InvalidDumpId, "TYPE", qtypname, NULL, tyinfo->dobj.namespace->dobj.name, tyinfo->rolname, tyinfo->typacl, tyinfo->rtypacl, tyinfo->inittypacl, tyinfo->initrtypacl);
  }

  PQclear(res);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(dropped);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qtypname);
  free(qualtypname);

                                    
  if (tyinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpCompositeTypeColComments(fout, tyinfo);
  }
}

   
                                
                                                                           
                                               
   
static void
dumpCompositeTypeColComments(Archive *fout, TypeInfo *tyinfo)
{
  CommentItem *comments;
  int ncomments;
  PGresult *res;
  PQExpBuffer query;
  PQExpBuffer target;
  Oid pgClassOid;
  int i;
  int ntups;
  int i_attname;
  int i_attnum;

                                                
  if (fout->dopt->no_comments)
  {
    return;
  }

  query = createPQExpBuffer();

  appendPQExpBuffer(query,
      "SELECT c.tableoid, a.attname, a.attnum "
      "FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a "
      "WHERE c.oid = '%u' AND c.oid = a.attrelid "
      "  AND NOT a.attisdropped "
      "ORDER BY a.attnum ",
      tyinfo->typrelid);

                             
  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  if (ntups < 1)
  {
    PQclear(res);
    destroyPQExpBuffer(query);
    return;
  }

  pgClassOid = atooid(PQgetvalue(res, 0, PQfnumber(res, "tableoid")));

                                                               
  ncomments = findComments(fout, pgClassOid, tyinfo->typrelid, &comments);

                                        
  if (ncomments <= 0)
  {
    PQclear(res);
    destroyPQExpBuffer(query);
    return;
  }

                                   
  target = createPQExpBuffer();

  i_attnum = PQfnumber(res, "attnum");
  i_attname = PQfnumber(res, "attname");
  while (ncomments > 0)
  {
    const char *attname;

    attname = NULL;
    for (i = 0; i < ntups; i++)
    {
      if (atoi(PQgetvalue(res, i, i_attnum)) == comments->objsubid)
      {
        attname = PQgetvalue(res, i, i_attname);
        break;
      }
    }
    if (attname)                                    
    {
      const char *descr = comments->descr;

      resetPQExpBuffer(target);
      appendPQExpBuffer(target, "COLUMN %s.", fmtId(tyinfo->dobj.name));
      appendPQExpBufferStr(target, fmtId(attname));

      resetPQExpBuffer(query);
      appendPQExpBuffer(query, "COMMENT ON COLUMN %s.", fmtQualifiedDumpable(tyinfo));
      appendPQExpBuffer(query, "%s IS ", fmtId(attname));
      appendStringLiteralAH(query, descr, fout);
      appendPQExpBufferStr(query, ";\n");

      ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = target->data, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "COMMENT", .section = SECTION_NONE, .createStmt = query->data, .deps = &(tyinfo->dobj.dumpId), .nDeps = 1));
    }

    comments++;
    ncomments--;
  }

  PQclear(res);
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(target);
}

   
                 
                                                           
   
                                                                            
   
static void
dumpShellType(Archive *fout, ShellTypeInfo *stinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;

                                
  if (!stinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();

     
                                                                           
                                                                      
                                                                        
                                                                            
                                                                             
                                                            
     

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_type_oid(fout, q, stinfo->baseType->dobj.catId.oid, false);
  }

  appendPQExpBuffer(q, "CREATE TYPE %s;\n", fmtQualifiedDumpable(stinfo));

  if (stinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, stinfo->dobj.catId, stinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = stinfo->dobj.name, .namespace = stinfo->dobj.namespace->dobj.name, .owner = stinfo->baseType->rolname, .description = "SHELL TYPE", .section = SECTION_PRE_DATA, .createStmt = q->data));
  }

  destroyPQExpBuffer(q);
}

   
                
                                                                
                          
   
static void
dumpProcLang(Archive *fout, ProcLangInfo *plang)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer defqry;
  PQExpBuffer delqry;
  bool useParams;
  char *qlanname;
  FuncInfo *funcInfo;
  FuncInfo *inlineInfo = NULL;
  FuncInfo *validatorInfo = NULL;

                                
  if (!plang->dobj.dump || dopt->dataOnly)
  {
    return;
  }

     
                                                                          
                                                                        
                                                                           
                                                                      
                                                              
     

  funcInfo = findFuncByOid(plang->lanplcallfoid);
  if (funcInfo != NULL && !funcInfo->dobj.dump)
  {
    funcInfo = NULL;                                         
  }

  if (OidIsValid(plang->laninline))
  {
    inlineInfo = findFuncByOid(plang->laninline);
    if (inlineInfo != NULL && !inlineInfo->dobj.dump)
    {
      inlineInfo = NULL;
    }
  }

  if (OidIsValid(plang->lanvalidator))
  {
    validatorInfo = findFuncByOid(plang->lanvalidator);
    if (validatorInfo != NULL && !validatorInfo->dobj.dump)
    {
      validatorInfo = NULL;
    }
  }

     
                                                                           
                                                                             
                                           
     
  useParams = (funcInfo != NULL && (inlineInfo != NULL || !OidIsValid(plang->laninline)) && (validatorInfo != NULL || !OidIsValid(plang->lanvalidator)));

  defqry = createPQExpBuffer();
  delqry = createPQExpBuffer();

  qlanname = pg_strdup(fmtId(plang->dobj.name));

  appendPQExpBuffer(delqry, "DROP PROCEDURAL LANGUAGE %s;\n", qlanname);

  if (useParams)
  {
    appendPQExpBuffer(defqry, "CREATE %sPROCEDURAL LANGUAGE %s", plang->lanpltrusted ? "TRUSTED " : "", qlanname);
    appendPQExpBuffer(defqry, " HANDLER %s", fmtQualifiedDumpable(funcInfo));
    if (OidIsValid(plang->laninline))
    {
      appendPQExpBuffer(defqry, " INLINE %s", fmtQualifiedDumpable(inlineInfo));
    }
    if (OidIsValid(plang->lanvalidator))
    {
      appendPQExpBuffer(defqry, " VALIDATOR %s", fmtQualifiedDumpable(validatorInfo));
    }
  }
  else
  {
       
                                                                         
                                                                           
                                                                       
                                                                    
                                                                          
                                                                         
                          
       
    appendPQExpBuffer(defqry, "CREATE OR REPLACE PROCEDURAL LANGUAGE %s", qlanname);
  }
  appendPQExpBufferStr(defqry, ";\n");

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(defqry, &plang->dobj, "LANGUAGE", qlanname, NULL);
  }

  if (plang->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, plang->dobj.catId, plang->dobj.dumpId, ARCHIVE_OPTS(.tag = plang->dobj.name, .owner = plang->lanowner, .description = "PROCEDURAL LANGUAGE", .section = SECTION_PRE_DATA, .createStmt = defqry->data, .dropStmt = delqry->data, ));
  }

                                                   
  if (plang->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "LANGUAGE", qlanname, NULL, plang->lanowner, plang->dobj.catId, 0, plang->dobj.dumpId);
  }

  if (plang->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "LANGUAGE", qlanname, NULL, plang->lanowner, plang->dobj.catId, 0, plang->dobj.dumpId);
  }

  if (plang->lanpltrusted && plang->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, plang->dobj.dumpId, InvalidDumpId, "LANGUAGE", qlanname, NULL, NULL, plang->lanowner, plang->lanacl, plang->rlanacl, plang->initlanacl, plang->initrlanacl);
  }

  free(qlanname);

  destroyPQExpBuffer(defqry);
  destroyPQExpBuffer(delqry);
}

   
                                                                       
   
                                                                        
                                                                     
                                                   
   
static char *
format_function_arguments(FuncInfo *finfo, char *funcargs, bool is_agg)
{
  PQExpBufferData fn;

  initPQExpBuffer(&fn);
  appendPQExpBufferStr(&fn, fmtId(finfo->dobj.name));
  if (is_agg && finfo->nargs == 0)
  {
    appendPQExpBufferStr(&fn, "(*)");
  }
  else
  {
    appendPQExpBuffer(&fn, "(%s)", funcargs);
  }
  return fn.data;
}

   
                                                                           
   
                                                                       
                       
   
                                                                         
                                                                          
   
                                                              
   
static char *
format_function_arguments_old(Archive *fout, FuncInfo *finfo, int nallargs, char **allargtypes, char **argmodes, char **argnames)
{
  PQExpBufferData fn;
  int j;

  initPQExpBuffer(&fn);
  appendPQExpBuffer(&fn, "%s(", fmtId(finfo->dobj.name));
  for (j = 0; j < nallargs; j++)
  {
    Oid typid;
    const char *typname;
    const char *argmode;
    const char *argname;

    typid = allargtypes ? atooid(allargtypes[j]) : finfo->argtypes[j];
    typname = getFormattedTypeName(fout, typid, zeroAsOpaque);

    if (argmodes)
    {
      switch (argmodes[j][0])
      {
      case PROARGMODE_IN:
        argmode = "";
        break;
      case PROARGMODE_OUT:
        argmode = "OUT ";
        break;
      case PROARGMODE_INOUT:
        argmode = "INOUT ";
        break;
      default:
        pg_log_warning("bogus value in proargmodes array");
        argmode = "";
        break;
      }
    }
    else
    {
      argmode = "";
    }

    argname = argnames ? argnames[j] : (char *)NULL;
    if (argname && argname[0] == '\0')
    {
      argname = NULL;
    }

    appendPQExpBuffer(&fn, "%s%s%s%s%s", (j > 0) ? ", " : "", argmode, argname ? fmtId(argname) : "", argname ? " " : "", typname);
  }
  appendPQExpBufferChar(&fn, ')');
  return fn.data;
}

   
                                                                       
   
                                                                         
                                                                    
                                                 
   
                                                                    
                                                                     
   
static char *
format_function_signature(Archive *fout, FuncInfo *finfo, bool honor_quotes)
{
  PQExpBufferData fn;
  int j;

  initPQExpBuffer(&fn);
  if (honor_quotes)
  {
    appendPQExpBuffer(&fn, "%s(", fmtId(finfo->dobj.name));
  }
  else
  {
    appendPQExpBuffer(&fn, "%s(", finfo->dobj.name);
  }
  for (j = 0; j < finfo->nargs; j++)
  {
    if (j > 0)
    {
      appendPQExpBufferStr(&fn, ", ");
    }

    appendPQExpBufferStr(&fn, getFormattedTypeName(fout, finfo->argtypes[j], zeroAsOpaque));
  }
  appendPQExpBufferChar(&fn, ')');
  return fn.data;
}

   
             
                           
   
static void
dumpFunc(Archive *fout, FuncInfo *finfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delqry;
  PQExpBuffer asPart;
  PGresult *res;
  char *funcsig;                                    
  char *funcfullsig = NULL;                     
  char *funcsig_tag;
  char *proretset;
  char *prosrc;
  char *probin;
  char *funcargs;
  char *funciargs;
  char *funcresult;
  char *proallargtypes;
  char *proargmodes;
  char *proargnames;
  char *protrftypes;
  char *prokind;
  char *provolatile;
  char *proisstrict;
  char *prosecdef;
  char *proleakproof;
  char *proconfig;
  char *procost;
  char *prorows;
  char *prosupport;
  char *proparallel;
  char *lanname;
  int nallargs;
  char **allargtypes = NULL;
  char **argmodes = NULL;
  char **argnames = NULL;
  char **configitems = NULL;
  int nconfigitems = 0;
  const char *keyword;
  int i;

                                
  if (!finfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delqry = createPQExpBuffer();
  asPart = createPQExpBuffer();

                                       
  if (fout->remoteVersion >= 120000)
  {
       
                                  
       
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
        "pg_catalog.pg_get_function_result(oid) AS funcresult, "
        "array_to_string(protrftypes, ' ') AS protrftypes, "
        "prokind, provolatile, proisstrict, prosecdef, "
        "proleakproof, proconfig, procost, prorows, "
        "prosupport, proparallel, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 110000)
  {
       
                               
       
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
        "pg_catalog.pg_get_function_result(oid) AS funcresult, "
        "array_to_string(protrftypes, ' ') AS protrftypes, "
        "prokind, provolatile, proisstrict, prosecdef, "
        "proleakproof, proconfig, procost, prorows, "
        "'-' AS prosupport, proparallel, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 90600)
  {
       
                                    
       
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
        "pg_catalog.pg_get_function_result(oid) AS funcresult, "
        "array_to_string(protrftypes, ' ') AS protrftypes, "
        "CASE WHEN proiswindow THEN 'w' ELSE 'f' END AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "proleakproof, proconfig, procost, prorows, "
        "'-' AS prosupport, proparallel, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 90500)
  {
       
                                    
       
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
        "pg_catalog.pg_get_function_result(oid) AS funcresult, "
        "array_to_string(protrftypes, ' ') AS protrftypes, "
        "CASE WHEN proiswindow THEN 'w' ELSE 'f' END AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "proleakproof, proconfig, procost, prorows, "
        "'-' AS prosupport, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 90200)
  {
       
                                     
       
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
        "pg_catalog.pg_get_function_result(oid) AS funcresult, "
        "CASE WHEN proiswindow THEN 'w' ELSE 'f' END AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "proleakproof, proconfig, procost, prorows, "
        "'-' AS prosupport, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80400)
  {
       
                                                              
                                                                       
       
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
        "pg_catalog.pg_get_function_result(oid) AS funcresult, "
        "CASE WHEN proiswindow THEN 'w' ELSE 'f' END AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "false AS proleakproof, "
        " proconfig, procost, prorows, "
        "'-' AS prosupport, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80300)
  {
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "proallargtypes, proargmodes, proargnames, "
        "'f' AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "false AS proleakproof, "
        "proconfig, procost, prorows, "
        "'-' AS prosupport, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80100)
  {
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "proallargtypes, proargmodes, proargnames, "
        "'f' AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "false AS proleakproof, "
        "null AS proconfig, 0 AS procost, 0 AS prorows, "
        "'-' AS prosupport, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT proretset, prosrc, probin, "
        "null AS proallargtypes, "
        "null AS proargmodes, "
        "proargnames, "
        "'f' AS prokind, "
        "provolatile, proisstrict, prosecdef, "
        "false AS proleakproof, "
        "null AS proconfig, 0 AS procost, 0 AS prorows, "
        "'-' AS prosupport, "
        "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
        "FROM pg_catalog.pg_proc "
        "WHERE oid = '%u'::pg_catalog.oid",
        finfo->dobj.catId.oid);
  }

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  proretset = PQgetvalue(res, 0, PQfnumber(res, "proretset"));
  prosrc = PQgetvalue(res, 0, PQfnumber(res, "prosrc"));
  probin = PQgetvalue(res, 0, PQfnumber(res, "probin"));
  if (fout->remoteVersion >= 80400)
  {
    funcargs = PQgetvalue(res, 0, PQfnumber(res, "funcargs"));
    funciargs = PQgetvalue(res, 0, PQfnumber(res, "funciargs"));
    funcresult = PQgetvalue(res, 0, PQfnumber(res, "funcresult"));
    proallargtypes = proargmodes = proargnames = NULL;
  }
  else
  {
    proallargtypes = PQgetvalue(res, 0, PQfnumber(res, "proallargtypes"));
    proargmodes = PQgetvalue(res, 0, PQfnumber(res, "proargmodes"));
    proargnames = PQgetvalue(res, 0, PQfnumber(res, "proargnames"));
    funcargs = funciargs = funcresult = NULL;
  }
  if (PQfnumber(res, "protrftypes") != -1)
  {
    protrftypes = PQgetvalue(res, 0, PQfnumber(res, "protrftypes"));
  }
  else
  {
    protrftypes = NULL;
  }
  prokind = PQgetvalue(res, 0, PQfnumber(res, "prokind"));
  provolatile = PQgetvalue(res, 0, PQfnumber(res, "provolatile"));
  proisstrict = PQgetvalue(res, 0, PQfnumber(res, "proisstrict"));
  prosecdef = PQgetvalue(res, 0, PQfnumber(res, "prosecdef"));
  proleakproof = PQgetvalue(res, 0, PQfnumber(res, "proleakproof"));
  proconfig = PQgetvalue(res, 0, PQfnumber(res, "proconfig"));
  procost = PQgetvalue(res, 0, PQfnumber(res, "procost"));
  prorows = PQgetvalue(res, 0, PQfnumber(res, "prorows"));
  prosupport = PQgetvalue(res, 0, PQfnumber(res, "prosupport"));

  if (PQfnumber(res, "proparallel") != -1)
  {
    proparallel = PQgetvalue(res, 0, PQfnumber(res, "proparallel"));
  }
  else
  {
    proparallel = NULL;
  }

  lanname = PQgetvalue(res, 0, PQfnumber(res, "lanname"));

     
                                                                            
                                                                           
                                                                             
                                                                 
     
  if (probin[0] != '\0' && strcmp(probin, "-") != 0)
  {
    appendPQExpBufferStr(asPart, "AS ");
    appendStringLiteralAH(asPart, probin, fout);
    if (strcmp(prosrc, "-") != 0)
    {
      appendPQExpBufferStr(asPart, ", ");

         
                                                                  
                                                                
         
      if (dopt->disable_dollar_quoting || (strchr(prosrc, '\'') == NULL && strchr(prosrc, '\\') == NULL))
      {
        appendStringLiteralAH(asPart, prosrc, fout);
      }
      else
      {
        appendStringLiteralDQ(asPart, prosrc, NULL);
      }
    }
  }
  else
  {
    if (strcmp(prosrc, "-") != 0)
    {
      appendPQExpBufferStr(asPart, "AS ");
                                                                    
      if (dopt->disable_dollar_quoting)
      {
        appendStringLiteralAH(asPart, prosrc, fout);
      }
      else
      {
        appendStringLiteralDQ(asPart, prosrc, NULL);
      }
    }
  }

  nallargs = finfo->nargs;                                             

  if (proallargtypes && *proallargtypes)
  {
    int nitems = 0;

    if (!parsePGArray(proallargtypes, &allargtypes, &nitems) || nitems < finfo->nargs)
    {
      pg_log_warning("could not parse proallargtypes array");
      if (allargtypes)
      {
        free(allargtypes);
      }
      allargtypes = NULL;
    }
    else
    {
      nallargs = nitems;
    }
  }

  if (proargmodes && *proargmodes)
  {
    int nitems = 0;

    if (!parsePGArray(proargmodes, &argmodes, &nitems) || nitems != nallargs)
    {
      pg_log_warning("could not parse proargmodes array");
      if (argmodes)
      {
        free(argmodes);
      }
      argmodes = NULL;
    }
  }

  if (proargnames && *proargnames)
  {
    int nitems = 0;

    if (!parsePGArray(proargnames, &argnames, &nitems) || nitems != nallargs)
    {
      pg_log_warning("could not parse proargnames array");
      if (argnames)
      {
        free(argnames);
      }
      argnames = NULL;
    }
  }

  if (proconfig && *proconfig)
  {
    if (!parsePGArray(proconfig, &configitems, &nconfigitems))
    {
      pg_log_warning("could not parse proconfig array");
      if (configitems)
      {
        free(configitems);
      }
      configitems = NULL;
      nconfigitems = 0;
    }
  }

  if (funcargs)
  {
                                                                        
    funcfullsig = format_function_arguments(finfo, funcargs, false);
    funcsig = format_function_arguments(finfo, funciargs, false);
  }
  else
  {
                                  
    funcsig = format_function_arguments_old(fout, finfo, nallargs, allargtypes, argmodes, argnames);
  }

  funcsig_tag = format_function_signature(fout, finfo, false);

  if (prokind[0] == PROKIND_PROCEDURE)
  {
    keyword = "PROCEDURE";
  }
  else
  {
    keyword = "FUNCTION";                                     
  }

  appendPQExpBuffer(delqry, "DROP %s %s.%s;\n", keyword, fmtId(finfo->dobj.namespace->dobj.name), funcsig);

  appendPQExpBuffer(q, "CREATE %s %s.%s", keyword, fmtId(finfo->dobj.namespace->dobj.name), funcfullsig ? funcfullsig : funcsig);

  if (prokind[0] == PROKIND_PROCEDURE)
                                  ;
  else if (funcresult)
  {
    appendPQExpBuffer(q, " RETURNS %s", funcresult);
  }
  else
  {
    appendPQExpBuffer(q, " RETURNS %s%s", (proretset[0] == 't') ? "SETOF " : "", getFormattedTypeName(fout, finfo->prorettype, zeroAsOpaque));
  }

  appendPQExpBuffer(q, "\n    LANGUAGE %s", fmtId(lanname));

  if (protrftypes != NULL && strcmp(protrftypes, "") != 0)
  {
    Oid *typeids = palloc(FUNC_MAX_ARGS * sizeof(Oid));
    int i;

    appendPQExpBufferStr(q, " TRANSFORM ");
    parseOidArray(protrftypes, typeids, FUNC_MAX_ARGS);
    for (i = 0; typeids[i]; i++)
    {
      if (i != 0)
      {
        appendPQExpBufferStr(q, ", ");
      }
      appendPQExpBuffer(q, "FOR TYPE %s", getFormattedTypeName(fout, typeids[i], zeroAsNone));
    }
  }

  if (prokind[0] == PROKIND_WINDOW)
  {
    appendPQExpBufferStr(q, " WINDOW");
  }

  if (provolatile[0] != PROVOLATILE_VOLATILE)
  {
    if (provolatile[0] == PROVOLATILE_IMMUTABLE)
    {
      appendPQExpBufferStr(q, " IMMUTABLE");
    }
    else if (provolatile[0] == PROVOLATILE_STABLE)
    {
      appendPQExpBufferStr(q, " STABLE");
    }
    else if (provolatile[0] != PROVOLATILE_VOLATILE)
    {
      fatal("unrecognized provolatile value for function \"%s\"", finfo->dobj.name);
    }
  }

  if (proisstrict[0] == 't')
  {
    appendPQExpBufferStr(q, " STRICT");
  }

  if (prosecdef[0] == 't')
  {
    appendPQExpBufferStr(q, " SECURITY DEFINER");
  }

  if (proleakproof[0] == 't')
  {
    appendPQExpBufferStr(q, " LEAKPROOF");
  }

     
                                                                             
                                                                             
                                                  
     
  if (strcmp(procost, "0") != 0)
  {
    if (strcmp(lanname, "internal") == 0 || strcmp(lanname, "c") == 0)
    {
                             
      if (strcmp(procost, "1") != 0)
      {
        appendPQExpBuffer(q, " COST %s", procost);
      }
    }
    else
    {
                               
      if (strcmp(procost, "100") != 0)
      {
        appendPQExpBuffer(q, " COST %s", procost);
      }
    }
  }
  if (proretset[0] == 't' && strcmp(prorows, "0") != 0 && strcmp(prorows, "1000") != 0)
  {
    appendPQExpBuffer(q, " ROWS %s", prorows);
  }

  if (strcmp(prosupport, "-") != 0)
  {
                                                                    
    appendPQExpBuffer(q, " SUPPORT %s", prosupport);
  }

  if (proparallel != NULL && proparallel[0] != PROPARALLEL_UNSAFE)
  {
    if (proparallel[0] == PROPARALLEL_SAFE)
    {
      appendPQExpBufferStr(q, " PARALLEL SAFE");
    }
    else if (proparallel[0] == PROPARALLEL_RESTRICTED)
    {
      appendPQExpBufferStr(q, " PARALLEL RESTRICTED");
    }
    else if (proparallel[0] != PROPARALLEL_UNSAFE)
    {
      fatal("unrecognized proparallel value for function \"%s\"", finfo->dobj.name);
    }
  }

  for (i = 0; i < nconfigitems; i++)
  {
                                                        
    char *configitem = configitems[i];
    char *pos;

    pos = strchr(configitem, '=');
    if (pos == NULL)
    {
      continue;
    }
    *pos++ = '\0';
    appendPQExpBuffer(q, "\n    SET %s TO ", fmtId(configitem));

       
                                                                          
                                                                    
                                                                       
                                                                        
                                                                          
                                                                      
                                                                  
                                                
       
                                                                         
                                                         
                                                                         
                                                      
       
    if (variable_is_guc_list_quote(configitem))
    {
      char **namelist;
      char **nameptr;

                                                 
                                      
      if (SplitGUCList(pos, ',', &namelist))
      {
        for (nameptr = namelist; *nameptr; nameptr++)
        {
          if (nameptr != namelist)
          {
            appendPQExpBufferStr(q, ", ");
          }
          appendStringLiteralAH(q, *nameptr, fout);
        }
      }
      pg_free(namelist);
    }
    else
    {
      appendStringLiteralAH(q, pos, fout);
    }
  }

  appendPQExpBuffer(q, "\n    %s;\n", asPart->data);

  append_depends_on_extension(fout, q, &finfo->dobj, "pg_catalog.pg_proc", keyword, psprintf("%s.%s", fmtId(finfo->dobj.namespace->dobj.name), funcsig));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &finfo->dobj, keyword, funcsig, finfo->dobj.namespace->dobj.name);
  }

  if (finfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, finfo->dobj.catId, finfo->dobj.dumpId, ARCHIVE_OPTS(.tag = funcsig_tag, .namespace = finfo->dobj.namespace->dobj.name, .owner = finfo->rolname, .description = keyword, .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delqry->data));
  }

                                                  
  if (finfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, keyword, funcsig, finfo->dobj.namespace->dobj.name, finfo->rolname, finfo->dobj.catId, 0, finfo->dobj.dumpId);
  }

  if (finfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, keyword, funcsig, finfo->dobj.namespace->dobj.name, finfo->rolname, finfo->dobj.catId, 0, finfo->dobj.dumpId);
  }

  if (finfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, finfo->dobj.dumpId, InvalidDumpId, keyword, funcsig, NULL, finfo->dobj.namespace->dobj.name, finfo->rolname, finfo->proacl, finfo->rproacl, finfo->initproacl, finfo->initrproacl);
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delqry);
  destroyPQExpBuffer(asPart);
  free(funcsig);
  if (funcfullsig)
  {
    free(funcfullsig);
  }
  free(funcsig_tag);
  if (allargtypes)
  {
    free(allargtypes);
  }
  if (argmodes)
  {
    free(argmodes);
  }
  if (argnames)
  {
    free(argnames);
  }
  if (configitems)
  {
    free(configitems);
  }
}

   
                            
   
static void
dumpCast(Archive *fout, CastInfo *cast)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer defqry;
  PQExpBuffer delqry;
  PQExpBuffer labelq;
  PQExpBuffer castargs;
  FuncInfo *funcInfo = NULL;
  const char *sourceType;
  const char *targetType;

                                
  if (!cast->dobj.dump || dopt->dataOnly)
  {
    return;
  }

                                                             
  if (OidIsValid(cast->castfunc))
  {
    funcInfo = findFuncByOid(cast->castfunc);
    if (funcInfo == NULL)
    {
      fatal("could not find function definition for function with OID %u", cast->castfunc);
    }
  }

  defqry = createPQExpBuffer();
  delqry = createPQExpBuffer();
  labelq = createPQExpBuffer();
  castargs = createPQExpBuffer();

  sourceType = getFormattedTypeName(fout, cast->castsource, zeroAsNone);
  targetType = getFormattedTypeName(fout, cast->casttarget, zeroAsNone);
  appendPQExpBuffer(delqry, "DROP CAST (%s AS %s);\n", sourceType, targetType);

  appendPQExpBuffer(defqry, "CREATE CAST (%s AS %s) ", sourceType, targetType);

  switch (cast->castmethod)
  {
  case COERCION_METHOD_BINARY:
    appendPQExpBufferStr(defqry, "WITHOUT FUNCTION");
    break;
  case COERCION_METHOD_INOUT:
    appendPQExpBufferStr(defqry, "WITH INOUT");
    break;
  case COERCION_METHOD_FUNCTION:
    if (funcInfo)
    {
      char *fsig = format_function_signature(fout, funcInfo, true);

         
                                                                     
                            
         
      appendPQExpBuffer(defqry, "WITH FUNCTION %s.%s", fmtId(funcInfo->dobj.namespace->dobj.name), fsig);
      free(fsig);
    }
    else
    {
      pg_log_warning("bogus value in pg_cast.castfunc or pg_cast.castmethod field");
    }
    break;
  default:
    pg_log_warning("bogus value in pg_cast.castmethod field");
  }

  if (cast->castcontext == 'a')
  {
    appendPQExpBufferStr(defqry, " AS ASSIGNMENT");
  }
  else if (cast->castcontext == 'i')
  {
    appendPQExpBufferStr(defqry, " AS IMPLICIT");
  }
  appendPQExpBufferStr(defqry, ";\n");

  appendPQExpBuffer(labelq, "CAST (%s AS %s)", sourceType, targetType);

  appendPQExpBuffer(castargs, "(%s AS %s)", sourceType, targetType);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(defqry, &cast->dobj, "CAST", castargs->data, NULL);
  }

  if (cast->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, cast->dobj.catId, cast->dobj.dumpId, ARCHIVE_OPTS(.tag = labelq->data, .description = "CAST", .section = SECTION_PRE_DATA, .createStmt = defqry->data, .dropStmt = delqry->data));
  }

                          
  if (cast->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "CAST", castargs->data, NULL, "", cast->dobj.catId, 0, cast->dobj.dumpId);
  }

  destroyPQExpBuffer(defqry);
  destroyPQExpBuffer(delqry);
  destroyPQExpBuffer(labelq);
  destroyPQExpBuffer(castargs);
}

   
                    
   
static void
dumpTransform(Archive *fout, TransformInfo *transform)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer defqry;
  PQExpBuffer delqry;
  PQExpBuffer labelq;
  PQExpBuffer transformargs;
  FuncInfo *fromsqlFuncInfo = NULL;
  FuncInfo *tosqlFuncInfo = NULL;
  char *lanname;
  const char *transformType;

                                
  if (!transform->dobj.dump || dopt->dataOnly)
  {
    return;
  }

                                                                  
  if (OidIsValid(transform->trffromsql))
  {
    fromsqlFuncInfo = findFuncByOid(transform->trffromsql);
    if (fromsqlFuncInfo == NULL)
    {
      fatal("could not find function definition for function with OID %u", transform->trffromsql);
    }
  }
  if (OidIsValid(transform->trftosql))
  {
    tosqlFuncInfo = findFuncByOid(transform->trftosql);
    if (tosqlFuncInfo == NULL)
    {
      fatal("could not find function definition for function with OID %u", transform->trftosql);
    }
  }

  defqry = createPQExpBuffer();
  delqry = createPQExpBuffer();
  labelq = createPQExpBuffer();
  transformargs = createPQExpBuffer();

  lanname = get_language_name(fout, transform->trflang);
  transformType = getFormattedTypeName(fout, transform->trftype, zeroAsNone);

  appendPQExpBuffer(delqry, "DROP TRANSFORM FOR %s LANGUAGE %s;\n", transformType, lanname);

  appendPQExpBuffer(defqry, "CREATE TRANSFORM FOR %s LANGUAGE %s (", transformType, lanname);

  if (!transform->trffromsql && !transform->trftosql)
  {
    pg_log_warning("bogus transform definition, at least one of trffromsql and trftosql should be nonzero");
  }

  if (transform->trffromsql)
  {
    if (fromsqlFuncInfo)
    {
      char *fsig = format_function_signature(fout, fromsqlFuncInfo, true);

         
                                                                     
                            
         
      appendPQExpBuffer(defqry, "FROM SQL WITH FUNCTION %s.%s", fmtId(fromsqlFuncInfo->dobj.namespace->dobj.name), fsig);
      free(fsig);
    }
    else
    {
      pg_log_warning("bogus value in pg_transform.trffromsql field");
    }
  }

  if (transform->trftosql)
  {
    if (transform->trffromsql)
    {
      appendPQExpBuffer(defqry, ", ");
    }

    if (tosqlFuncInfo)
    {
      char *fsig = format_function_signature(fout, tosqlFuncInfo, true);

         
                                                                     
                            
         
      appendPQExpBuffer(defqry, "TO SQL WITH FUNCTION %s.%s", fmtId(tosqlFuncInfo->dobj.namespace->dobj.name), fsig);
      free(fsig);
    }
    else
    {
      pg_log_warning("bogus value in pg_transform.trftosql field");
    }
  }

  appendPQExpBuffer(defqry, ");\n");

  appendPQExpBuffer(labelq, "TRANSFORM FOR %s LANGUAGE %s", transformType, lanname);

  appendPQExpBuffer(transformargs, "FOR %s LANGUAGE %s", transformType, lanname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(defqry, &transform->dobj, "TRANSFORM", transformargs->data, NULL);
  }

  if (transform->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, transform->dobj.catId, transform->dobj.dumpId, ARCHIVE_OPTS(.tag = labelq->data, .description = "TRANSFORM", .section = SECTION_PRE_DATA, .createStmt = defqry->data, .dropStmt = delqry->data, .deps = transform->dobj.dependencies, .nDeps = transform->dobj.nDeps));
  }

                               
  if (transform->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TRANSFORM", transformargs->data, NULL, "", transform->dobj.catId, 0, transform->dobj.dumpId);
  }

  free(lanname);
  destroyPQExpBuffer(defqry);
  destroyPQExpBuffer(delqry);
  destroyPQExpBuffer(labelq);
  destroyPQExpBuffer(transformargs);
}

   
           
                                            
   
static void
dumpOpr(Archive *fout, OprInfo *oprinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer oprid;
  PQExpBuffer details;
  PGresult *res;
  int i_oprkind;
  int i_oprcode;
  int i_oprleft;
  int i_oprright;
  int i_oprcom;
  int i_oprnegate;
  int i_oprrest;
  int i_oprjoin;
  int i_oprcanmerge;
  int i_oprcanhash;
  char *oprkind;
  char *oprcode;
  char *oprleft;
  char *oprright;
  char *oprcom;
  char *oprnegate;
  char *oprrest;
  char *oprjoin;
  char *oprcanmerge;
  char *oprcanhash;
  char *oprregproc;
  char *oprref;

                                
  if (!oprinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

     
                                                                     
                                                 
     
  if (!OidIsValid(oprinfo->oprcode))
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  oprid = createPQExpBuffer();
  details = createPQExpBuffer();

  if (fout->remoteVersion >= 80300)
  {
    appendPQExpBuffer(query,
        "SELECT oprkind, "
        "oprcode::pg_catalog.regprocedure, "
        "oprleft::pg_catalog.regtype, "
        "oprright::pg_catalog.regtype, "
        "oprcom, "
        "oprnegate, "
        "oprrest::pg_catalog.regprocedure, "
        "oprjoin::pg_catalog.regprocedure, "
        "oprcanmerge, oprcanhash "
        "FROM pg_catalog.pg_operator "
        "WHERE oid = '%u'::pg_catalog.oid",
        oprinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT oprkind, "
        "oprcode::pg_catalog.regprocedure, "
        "oprleft::pg_catalog.regtype, "
        "oprright::pg_catalog.regtype, "
        "oprcom, "
        "oprnegate, "
        "oprrest::pg_catalog.regprocedure, "
        "oprjoin::pg_catalog.regprocedure, "
        "(oprlsortop != 0) AS oprcanmerge, "
        "oprcanhash "
        "FROM pg_catalog.pg_operator "
        "WHERE oid = '%u'::pg_catalog.oid",
        oprinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  i_oprkind = PQfnumber(res, "oprkind");
  i_oprcode = PQfnumber(res, "oprcode");
  i_oprleft = PQfnumber(res, "oprleft");
  i_oprright = PQfnumber(res, "oprright");
  i_oprcom = PQfnumber(res, "oprcom");
  i_oprnegate = PQfnumber(res, "oprnegate");
  i_oprrest = PQfnumber(res, "oprrest");
  i_oprjoin = PQfnumber(res, "oprjoin");
  i_oprcanmerge = PQfnumber(res, "oprcanmerge");
  i_oprcanhash = PQfnumber(res, "oprcanhash");

  oprkind = PQgetvalue(res, 0, i_oprkind);
  oprcode = PQgetvalue(res, 0, i_oprcode);
  oprleft = PQgetvalue(res, 0, i_oprleft);
  oprright = PQgetvalue(res, 0, i_oprright);
  oprcom = PQgetvalue(res, 0, i_oprcom);
  oprnegate = PQgetvalue(res, 0, i_oprnegate);
  oprrest = PQgetvalue(res, 0, i_oprrest);
  oprjoin = PQgetvalue(res, 0, i_oprjoin);
  oprcanmerge = PQgetvalue(res, 0, i_oprcanmerge);
  oprcanhash = PQgetvalue(res, 0, i_oprcanhash);

  oprregproc = convertRegProcReference(fout, oprcode);
  if (oprregproc)
  {
    appendPQExpBuffer(details, "    FUNCTION = %s", oprregproc);
    free(oprregproc);
  }

  appendPQExpBuffer(oprid, "%s (", oprinfo->dobj.name);

     
                                                                         
               
     
  if (strcmp(oprkind, "r") == 0 || strcmp(oprkind, "b") == 0)
  {
    appendPQExpBuffer(details, ",\n    LEFTARG = %s", oprleft);
    appendPQExpBufferStr(oprid, oprleft);
  }
  else
  {
    appendPQExpBufferStr(oprid, "NONE");
  }

  if (strcmp(oprkind, "l") == 0 || strcmp(oprkind, "b") == 0)
  {
    appendPQExpBuffer(details, ",\n    RIGHTARG = %s", oprright);
    appendPQExpBuffer(oprid, ", %s)", oprright);
  }
  else
  {
    appendPQExpBufferStr(oprid, ", NONE)");
  }

  oprref = getFormattedOperatorName(fout, oprcom);
  if (oprref)
  {
    appendPQExpBuffer(details, ",\n    COMMUTATOR = %s", oprref);
    free(oprref);
  }

  oprref = getFormattedOperatorName(fout, oprnegate);
  if (oprref)
  {
    appendPQExpBuffer(details, ",\n    NEGATOR = %s", oprref);
    free(oprref);
  }

  if (strcmp(oprcanmerge, "t") == 0)
  {
    appendPQExpBufferStr(details, ",\n    MERGES");
  }

  if (strcmp(oprcanhash, "t") == 0)
  {
    appendPQExpBufferStr(details, ",\n    HASHES");
  }

  oprregproc = convertRegProcReference(fout, oprrest);
  if (oprregproc)
  {
    appendPQExpBuffer(details, ",\n    RESTRICT = %s", oprregproc);
    free(oprregproc);
  }

  oprregproc = convertRegProcReference(fout, oprjoin);
  if (oprregproc)
  {
    appendPQExpBuffer(details, ",\n    JOIN = %s", oprregproc);
    free(oprregproc);
  }

  appendPQExpBuffer(delq, "DROP OPERATOR %s.%s;\n", fmtId(oprinfo->dobj.namespace->dobj.name), oprid->data);

  appendPQExpBuffer(q, "CREATE OPERATOR %s.%s (\n%s\n);\n", fmtId(oprinfo->dobj.namespace->dobj.name), oprinfo->dobj.name, details->data);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &oprinfo->dobj, "OPERATOR", oprid->data, oprinfo->dobj.namespace->dobj.name);
  }

  if (oprinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, oprinfo->dobj.catId, oprinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = oprinfo->dobj.name, .namespace = oprinfo->dobj.namespace->dobj.name, .owner = oprinfo->rolname, .description = "OPERATOR", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                              
  if (oprinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "OPERATOR", oprid->data, oprinfo->dobj.namespace->dobj.name, oprinfo->rolname, oprinfo->dobj.catId, 0, oprinfo->dobj.dumpId);
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(oprid);
  destroyPQExpBuffer(details);
}

   
                                                          
   
                                                                             
                                                                          
   
                                                                            
         
   
static char *
convertRegProcReference(Archive *fout, const char *proc)
{
  char *name;
  char *paren;
  bool inquote;

                                               
  if (strcmp(proc, "-") == 0)
  {
    return NULL;
  }

  name = pg_strdup(proc);
                                         
  inquote = false;
  for (paren = name; *paren; paren++)
  {
    if (*paren == '(' && !inquote)
    {
      *paren = '\0';
      break;
    }
    if (*paren == '"')
    {
      inquote = !inquote;
    }
  }
  return name;
}

   
                                                                 
                                                  
   
                                                                     
                                                     
   
                                                                            
                                                                               
                                                                            
                                                                              
                                                                            
                             
   
static char *
getFormattedOperatorName(Archive *fout, const char *oproid)
{
  OprInfo *oprInfo;

                                               
  if (strcmp(oproid, "0") == 0)
  {
    return NULL;
  }

  oprInfo = findOprByOid(atooid(oproid));
  if (oprInfo == NULL)
  {
    pg_log_warning("could not find operator with OID %s", oproid);
    return NULL;
  }

  return psprintf("OPERATOR(%s.%s)", fmtId(oprInfo->dobj.namespace->dobj.name), oprInfo->dobj.name);
}

   
                                                                       
   
                                                                       
                                                                       
                                                                         
                              
   
static char *
convertTSFunction(Archive *fout, Oid funcOid)
{
  char *result;
  char query[128];
  PGresult *res;

  snprintf(query, sizeof(query), "SELECT '%u'::pg_catalog.regproc", funcOid);
  res = ExecuteSqlQueryForSingleRow(fout, query);

  result = pg_strdup(PQgetvalue(res, 0, 0));

  PQclear(res);

  return result;
}

   
                    
                                                 
   
static void
dumpAccessMethod(Archive *fout, AccessMethodInfo *aminfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qamname;

                                
  if (!aminfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qamname = pg_strdup(fmtId(aminfo->dobj.name));

  appendPQExpBuffer(q, "CREATE ACCESS METHOD %s ", qamname);

  switch (aminfo->amtype)
  {
  case AMTYPE_INDEX:
    appendPQExpBuffer(q, "TYPE INDEX ");
    break;
  case AMTYPE_TABLE:
    appendPQExpBuffer(q, "TYPE TABLE ");
    break;
  default:
    pg_log_warning("invalid type \"%c\" of access method \"%s\"", aminfo->amtype, qamname);
    destroyPQExpBuffer(q);
    destroyPQExpBuffer(delq);
    free(qamname);
    return;
  }

  appendPQExpBuffer(q, "HANDLER %s;\n", aminfo->amhandler);

  appendPQExpBuffer(delq, "DROP ACCESS METHOD %s;\n", qamname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &aminfo->dobj, "ACCESS METHOD", qamname, NULL);
  }

  if (aminfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, aminfo->dobj.catId, aminfo->dobj.dumpId, ARCHIVE_OPTS(.tag = aminfo->dobj.name, .description = "ACCESS METHOD", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                   
  if (aminfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "ACCESS METHOD", qamname, NULL, "", aminfo->dobj.catId, 0, aminfo->dobj.dumpId);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qamname);
}

   
               
                                                  
   
static void
dumpOpclass(Archive *fout, OpclassInfo *opcinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer nameusing;
  PGresult *res;
  int ntups;
  int i_opcintype;
  int i_opckeytype;
  int i_opcdefault;
  int i_opcfamily;
  int i_opcfamilyname;
  int i_opcfamilynsp;
  int i_amname;
  int i_amopstrategy;
  int i_amopreqcheck;
  int i_amopopr;
  int i_sortfamily;
  int i_sortfamilynsp;
  int i_amprocnum;
  int i_amproc;
  int i_amproclefttype;
  int i_amprocrighttype;
  char *opcintype;
  char *opckeytype;
  char *opcdefault;
  char *opcfamily;
  char *opcfamilyname;
  char *opcfamilynsp;
  char *amname;
  char *amopstrategy;
  char *amopreqcheck;
  char *amopopr;
  char *sortfamily;
  char *sortfamilynsp;
  char *amprocnum;
  char *amproc;
  char *amproclefttype;
  char *amprocrighttype;
  bool needComma;
  int i;

                                
  if (!opcinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  nameusing = createPQExpBuffer();

                                                     
  if (fout->remoteVersion >= 80300)
  {
    appendPQExpBuffer(query,
        "SELECT opcintype::pg_catalog.regtype, "
        "opckeytype::pg_catalog.regtype, "
        "opcdefault, opcfamily, "
        "opfname AS opcfamilyname, "
        "nspname AS opcfamilynsp, "
        "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcmethod) AS amname "
        "FROM pg_catalog.pg_opclass c "
        "LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = opcfamily "
        "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
        "WHERE c.oid = '%u'::pg_catalog.oid",
        opcinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT opcintype::pg_catalog.regtype, "
        "opckeytype::pg_catalog.regtype, "
        "opcdefault, NULL AS opcfamily, "
        "NULL AS opcfamilyname, "
        "NULL AS opcfamilynsp, "
        "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcamid) AS amname "
        "FROM pg_catalog.pg_opclass "
        "WHERE oid = '%u'::pg_catalog.oid",
        opcinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  i_opcintype = PQfnumber(res, "opcintype");
  i_opckeytype = PQfnumber(res, "opckeytype");
  i_opcdefault = PQfnumber(res, "opcdefault");
  i_opcfamily = PQfnumber(res, "opcfamily");
  i_opcfamilyname = PQfnumber(res, "opcfamilyname");
  i_opcfamilynsp = PQfnumber(res, "opcfamilynsp");
  i_amname = PQfnumber(res, "amname");

                                                          
  opcintype = pg_strdup(PQgetvalue(res, 0, i_opcintype));
  opckeytype = PQgetvalue(res, 0, i_opckeytype);
  opcdefault = PQgetvalue(res, 0, i_opcdefault);
                                                           
  opcfamily = pg_strdup(PQgetvalue(res, 0, i_opcfamily));
  opcfamilyname = PQgetvalue(res, 0, i_opcfamilyname);
  opcfamilynsp = PQgetvalue(res, 0, i_opcfamilynsp);
                                                        
  amname = pg_strdup(PQgetvalue(res, 0, i_amname));

  appendPQExpBuffer(delq, "DROP OPERATOR CLASS %s", fmtQualifiedDumpable(opcinfo));
  appendPQExpBuffer(delq, " USING %s;\n", fmtId(amname));

                                                     
  appendPQExpBuffer(q, "CREATE OPERATOR CLASS %s\n    ", fmtQualifiedDumpable(opcinfo));
  if (strcmp(opcdefault, "t") == 0)
  {
    appendPQExpBufferStr(q, "DEFAULT ");
  }
  appendPQExpBuffer(q, "FOR TYPE %s USING %s", opcintype, fmtId(amname));
  if (strlen(opcfamilyname) > 0)
  {
    appendPQExpBufferStr(q, " FAMILY ");
    appendPQExpBuffer(q, "%s.", fmtId(opcfamilynsp));
    appendPQExpBufferStr(q, fmtId(opcfamilyname));
  }
  appendPQExpBufferStr(q, " AS\n    ");

  needComma = false;

  if (strcmp(opckeytype, "-") != 0)
  {
    appendPQExpBuffer(q, "STORAGE %s", opckeytype);
    needComma = true;
  }

  PQclear(res);

     
                                                              
     
                                                                       
                        
     
                                                                           
                                                                   
                                                                        
                                                                           
                                                            
     
  resetPQExpBuffer(query);

  if (fout->remoteVersion >= 90100)
  {
    appendPQExpBuffer(query,
        "SELECT amopstrategy, false AS amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "opfname AS sortfamily, "
        "nspname AS sortfamilynsp "
        "FROM pg_catalog.pg_amop ao JOIN pg_catalog.pg_depend ON "
        "(classid = 'pg_catalog.pg_amop'::pg_catalog.regclass AND objid = ao.oid) "
        "LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = amopsortfamily "
        "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
        "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND amopfamily = '%s'::pg_catalog.oid "
        "ORDER BY amopstrategy",
        opcinfo->dobj.catId.oid, opcfamily);
  }
  else if (fout->remoteVersion >= 80400)
  {
    appendPQExpBuffer(query,
        "SELECT amopstrategy, false AS amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "NULL AS sortfamily, "
        "NULL AS sortfamilynsp "
        "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
        "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
        "AND objid = ao.oid "
        "ORDER BY amopstrategy",
        opcinfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80300)
  {
    appendPQExpBuffer(query,
        "SELECT amopstrategy, amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "NULL AS sortfamily, "
        "NULL AS sortfamilynsp "
        "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
        "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
        "AND objid = ao.oid "
        "ORDER BY amopstrategy",
        opcinfo->dobj.catId.oid);
  }
  else
  {
       
                                                                          
                                          
       
    appendPQExpBuffer(query,
        "SELECT amopstrategy, amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "NULL AS sortfamily, "
        "NULL AS sortfamilynsp "
        "FROM pg_catalog.pg_amop "
        "WHERE amopclaid = '%u'::pg_catalog.oid "
        "ORDER BY amopstrategy",
        opcinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_amopstrategy = PQfnumber(res, "amopstrategy");
  i_amopreqcheck = PQfnumber(res, "amopreqcheck");
  i_amopopr = PQfnumber(res, "amopopr");
  i_sortfamily = PQfnumber(res, "sortfamily");
  i_sortfamilynsp = PQfnumber(res, "sortfamilynsp");

  for (i = 0; i < ntups; i++)
  {
    amopstrategy = PQgetvalue(res, i, i_amopstrategy);
    amopreqcheck = PQgetvalue(res, i, i_amopreqcheck);
    amopopr = PQgetvalue(res, i, i_amopopr);
    sortfamily = PQgetvalue(res, i, i_sortfamily);
    sortfamilynsp = PQgetvalue(res, i, i_sortfamilynsp);

    if (needComma)
    {
      appendPQExpBufferStr(q, " ,\n    ");
    }

    appendPQExpBuffer(q, "OPERATOR %s %s", amopstrategy, amopopr);

    if (strlen(sortfamily) > 0)
    {
      appendPQExpBufferStr(q, " FOR ORDER BY ");
      appendPQExpBuffer(q, "%s.", fmtId(sortfamilynsp));
      appendPQExpBufferStr(q, fmtId(sortfamily));
    }

    if (strcmp(amopreqcheck, "t") == 0)
    {
      appendPQExpBufferStr(q, " RECHECK");
    }

    needComma = true;
  }

  PQclear(res);

     
                                                                
     
                                                                       
                        
     
                                                                           
                                                                           
                                                                           
                                                                         
                                     
     
  resetPQExpBuffer(query);

  if (fout->remoteVersion >= 80300)
  {
    appendPQExpBuffer(query,
        "SELECT amprocnum, "
        "amproc::pg_catalog.regprocedure, "
        "amproclefttype::pg_catalog.regtype, "
        "amprocrighttype::pg_catalog.regtype "
        "FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
        "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND classid = 'pg_catalog.pg_amproc'::pg_catalog.regclass "
        "AND objid = ap.oid "
        "ORDER BY amprocnum",
        opcinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT amprocnum, "
        "amproc::pg_catalog.regprocedure, "
        "'' AS amproclefttype, "
        "'' AS amprocrighttype "
        "FROM pg_catalog.pg_amproc "
        "WHERE amopclaid = '%u'::pg_catalog.oid "
        "ORDER BY amprocnum",
        opcinfo->dobj.catId.oid);
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_amprocnum = PQfnumber(res, "amprocnum");
  i_amproc = PQfnumber(res, "amproc");
  i_amproclefttype = PQfnumber(res, "amproclefttype");
  i_amprocrighttype = PQfnumber(res, "amprocrighttype");

  for (i = 0; i < ntups; i++)
  {
    amprocnum = PQgetvalue(res, i, i_amprocnum);
    amproc = PQgetvalue(res, i, i_amproc);
    amproclefttype = PQgetvalue(res, i, i_amproclefttype);
    amprocrighttype = PQgetvalue(res, i, i_amprocrighttype);

    if (needComma)
    {
      appendPQExpBufferStr(q, " ,\n    ");
    }

    appendPQExpBuffer(q, "FUNCTION %s", amprocnum);

    if (*amproclefttype && *amprocrighttype)
    {
      appendPQExpBuffer(q, " (%s, %s)", amproclefttype, amprocrighttype);
    }

    appendPQExpBuffer(q, " %s", amproc);

    needComma = true;
  }

  PQclear(res);

     
                                                                          
                                                                           
                                                                  
                                                                         
     
  if (!needComma)
  {
    appendPQExpBuffer(q, "STORAGE %s", opcintype);
  }

  appendPQExpBufferStr(q, ";\n");

  appendPQExpBufferStr(nameusing, fmtId(opcinfo->dobj.name));
  appendPQExpBuffer(nameusing, " USING %s", fmtId(amname));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &opcinfo->dobj, "OPERATOR CLASS", nameusing->data, opcinfo->dobj.namespace->dobj.name);
  }

  if (opcinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, opcinfo->dobj.catId, opcinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = opcinfo->dobj.name, .namespace = opcinfo->dobj.namespace->dobj.name, .owner = opcinfo->rolname, .description = "OPERATOR CLASS", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                    
  if (opcinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "OPERATOR CLASS", nameusing->data, opcinfo->dobj.namespace->dobj.name, opcinfo->rolname, opcinfo->dobj.catId, 0, opcinfo->dobj.dumpId);
  }

  free(opcintype);
  free(opcfamily);
  free(amname);
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(nameusing);
}

   
                
                                                   
   
                                                                             
                                         
   
static void
dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer nameusing;
  PGresult *res;
  PGresult *res_ops;
  PGresult *res_procs;
  int ntups;
  int i_amname;
  int i_amopstrategy;
  int i_amopreqcheck;
  int i_amopopr;
  int i_sortfamily;
  int i_sortfamilynsp;
  int i_amprocnum;
  int i_amproc;
  int i_amproclefttype;
  int i_amprocrighttype;
  char *amname;
  char *amopstrategy;
  char *amopreqcheck;
  char *amopopr;
  char *sortfamily;
  char *sortfamilynsp;
  char *amprocnum;
  char *amproc;
  char *amproclefttype;
  char *amprocrighttype;
  bool needComma;
  int i;

                                
  if (!opfinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  nameusing = createPQExpBuffer();

     
                                                                     
                                    
     
                                                                           
                                                                   
                                                                        
                                                                           
                                                            
     
  if (fout->remoteVersion >= 90100)
  {
    appendPQExpBuffer(query,
        "SELECT amopstrategy, false AS amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "opfname AS sortfamily, "
        "nspname AS sortfamilynsp "
        "FROM pg_catalog.pg_amop ao JOIN pg_catalog.pg_depend ON "
        "(classid = 'pg_catalog.pg_amop'::pg_catalog.regclass AND objid = ao.oid) "
        "LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = amopsortfamily "
        "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
        "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND amopfamily = '%u'::pg_catalog.oid "
        "ORDER BY amopstrategy",
        opfinfo->dobj.catId.oid, opfinfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80400)
  {
    appendPQExpBuffer(query,
        "SELECT amopstrategy, false AS amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "NULL AS sortfamily, "
        "NULL AS sortfamilynsp "
        "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
        "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
        "AND objid = ao.oid "
        "ORDER BY amopstrategy",
        opfinfo->dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT amopstrategy, amopreqcheck, "
        "amopopr::pg_catalog.regoperator, "
        "NULL AS sortfamily, "
        "NULL AS sortfamilynsp "
        "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
        "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
        "AND refobjid = '%u'::pg_catalog.oid "
        "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
        "AND objid = ao.oid "
        "ORDER BY amopstrategy",
        opfinfo->dobj.catId.oid);
  }

  res_ops = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  resetPQExpBuffer(query);

  appendPQExpBuffer(query,
      "SELECT amprocnum, "
      "amproc::pg_catalog.regprocedure, "
      "amproclefttype::pg_catalog.regtype, "
      "amprocrighttype::pg_catalog.regtype "
      "FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
      "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
      "AND refobjid = '%u'::pg_catalog.oid "
      "AND classid = 'pg_catalog.pg_amproc'::pg_catalog.regclass "
      "AND objid = ap.oid "
      "ORDER BY amprocnum",
      opfinfo->dobj.catId.oid);

  res_procs = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

                                                      
  resetPQExpBuffer(query);

  appendPQExpBuffer(query,
      "SELECT "
      "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opfmethod) AS amname "
      "FROM pg_catalog.pg_opfamily "
      "WHERE oid = '%u'::pg_catalog.oid",
      opfinfo->dobj.catId.oid);

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  i_amname = PQfnumber(res, "amname");

                                                        
  amname = pg_strdup(PQgetvalue(res, 0, i_amname));

  appendPQExpBuffer(delq, "DROP OPERATOR FAMILY %s", fmtQualifiedDumpable(opfinfo));
  appendPQExpBuffer(delq, " USING %s;\n", fmtId(amname));

                                                     
  appendPQExpBuffer(q, "CREATE OPERATOR FAMILY %s", fmtQualifiedDumpable(opfinfo));
  appendPQExpBuffer(q, " USING %s;\n", fmtId(amname));

  PQclear(res);

                                                 
  if (PQntuples(res_ops) > 0 || PQntuples(res_procs) > 0)
  {
    appendPQExpBuffer(q, "ALTER OPERATOR FAMILY %s", fmtQualifiedDumpable(opfinfo));
    appendPQExpBuffer(q, " USING %s ADD\n    ", fmtId(amname));

    needComma = false;

       
                                                                
       
    ntups = PQntuples(res_ops);

    i_amopstrategy = PQfnumber(res_ops, "amopstrategy");
    i_amopreqcheck = PQfnumber(res_ops, "amopreqcheck");
    i_amopopr = PQfnumber(res_ops, "amopopr");
    i_sortfamily = PQfnumber(res_ops, "sortfamily");
    i_sortfamilynsp = PQfnumber(res_ops, "sortfamilynsp");

    for (i = 0; i < ntups; i++)
    {
      amopstrategy = PQgetvalue(res_ops, i, i_amopstrategy);
      amopreqcheck = PQgetvalue(res_ops, i, i_amopreqcheck);
      amopopr = PQgetvalue(res_ops, i, i_amopopr);
      sortfamily = PQgetvalue(res_ops, i, i_sortfamily);
      sortfamilynsp = PQgetvalue(res_ops, i, i_sortfamilynsp);

      if (needComma)
      {
        appendPQExpBufferStr(q, " ,\n    ");
      }

      appendPQExpBuffer(q, "OPERATOR %s %s", amopstrategy, amopopr);

      if (strlen(sortfamily) > 0)
      {
        appendPQExpBufferStr(q, " FOR ORDER BY ");
        appendPQExpBuffer(q, "%s.", fmtId(sortfamilynsp));
        appendPQExpBufferStr(q, fmtId(sortfamily));
      }

      if (strcmp(amopreqcheck, "t") == 0)
      {
        appendPQExpBufferStr(q, " RECHECK");
      }

      needComma = true;
    }

       
                                                                  
       
    ntups = PQntuples(res_procs);

    i_amprocnum = PQfnumber(res_procs, "amprocnum");
    i_amproc = PQfnumber(res_procs, "amproc");
    i_amproclefttype = PQfnumber(res_procs, "amproclefttype");
    i_amprocrighttype = PQfnumber(res_procs, "amprocrighttype");

    for (i = 0; i < ntups; i++)
    {
      amprocnum = PQgetvalue(res_procs, i, i_amprocnum);
      amproc = PQgetvalue(res_procs, i, i_amproc);
      amproclefttype = PQgetvalue(res_procs, i, i_amproclefttype);
      amprocrighttype = PQgetvalue(res_procs, i, i_amprocrighttype);

      if (needComma)
      {
        appendPQExpBufferStr(q, " ,\n    ");
      }

      appendPQExpBuffer(q, "FUNCTION %s (%s, %s) %s", amprocnum, amproclefttype, amprocrighttype, amproc);

      needComma = true;
    }

    appendPQExpBufferStr(q, ";\n");
  }

  appendPQExpBufferStr(nameusing, fmtId(opfinfo->dobj.name));
  appendPQExpBuffer(nameusing, " USING %s", fmtId(amname));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &opfinfo->dobj, "OPERATOR FAMILY", nameusing->data, opfinfo->dobj.namespace->dobj.name);
  }

  if (opfinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, opfinfo->dobj.catId, opfinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = opfinfo->dobj.name, .namespace = opfinfo->dobj.namespace->dobj.name, .owner = opfinfo->rolname, .description = "OPERATOR FAMILY", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                     
  if (opfinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "OPERATOR FAMILY", nameusing->data, opfinfo->dobj.namespace->dobj.name, opfinfo->rolname, opfinfo->dobj.catId, 0, opfinfo->dobj.dumpId);
  }

  free(amname);
  PQclear(res_ops);
  PQclear(res_procs);
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(nameusing);
}

   
                 
                                             
   
static void
dumpCollation(Archive *fout, CollInfo *collinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qcollname;
  PGresult *res;
  int i_collprovider;
  int i_collisdeterministic;
  int i_collcollate;
  int i_collctype;
  const char *collprovider;
  const char *collcollate;
  const char *collctype;

                                
  if (!collinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qcollname = pg_strdup(fmtId(collinfo->dobj.name));

                                      
  appendPQExpBuffer(query, "SELECT ");

  if (fout->remoteVersion >= 100000)
  {
    appendPQExpBuffer(query, "collprovider, "
                             "collversion, ");
  }
  else
  {
    appendPQExpBuffer(query, "'c' AS collprovider, "
                             "NULL AS collversion, ");
  }

  if (fout->remoteVersion >= 120000)
  {
    appendPQExpBuffer(query, "collisdeterministic, ");
  }
  else
  {
    appendPQExpBuffer(query, "true AS collisdeterministic, ");
  }

  appendPQExpBuffer(query,
      "collcollate, "
      "collctype "
      "FROM pg_catalog.pg_collation c "
      "WHERE c.oid = '%u'::pg_catalog.oid",
      collinfo->dobj.catId.oid);

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  i_collprovider = PQfnumber(res, "collprovider");
  i_collisdeterministic = PQfnumber(res, "collisdeterministic");
  i_collcollate = PQfnumber(res, "collcollate");
  i_collctype = PQfnumber(res, "collctype");

  collprovider = PQgetvalue(res, 0, i_collprovider);
  collcollate = PQgetvalue(res, 0, i_collcollate);
  collctype = PQgetvalue(res, 0, i_collctype);

  appendPQExpBuffer(delq, "DROP COLLATION %s;\n", fmtQualifiedDumpable(collinfo));

  appendPQExpBuffer(q, "CREATE COLLATION %s (", fmtQualifiedDumpable(collinfo));

  appendPQExpBufferStr(q, "provider = ");
  if (collprovider[0] == 'c')
  {
    appendPQExpBufferStr(q, "libc");
  }
  else if (collprovider[0] == 'i')
  {
    appendPQExpBufferStr(q, "icu");
  }
  else if (collprovider[0] == 'd')
  {
                                                            
    appendPQExpBufferStr(q, "default");
  }
  else
  {
    fatal("unrecognized collation provider: %s", collprovider);
  }

  if (strcmp(PQgetvalue(res, 0, i_collisdeterministic), "f") == 0)
  {
    appendPQExpBufferStr(q, ", deterministic = false");
  }

  if (strcmp(collcollate, collctype) == 0)
  {
    appendPQExpBufferStr(q, ", locale = ");
    appendStringLiteralAH(q, collcollate, fout);
  }
  else
  {
    appendPQExpBufferStr(q, ", lc_collate = ");
    appendStringLiteralAH(q, collcollate, fout);
    appendPQExpBufferStr(q, ", lc_ctype = ");
    appendStringLiteralAH(q, collctype, fout);
  }

     
                                                                       
                                                                          
     
  if (dopt->binary_upgrade)
  {
    int i_collversion;

    i_collversion = PQfnumber(res, "collversion");
    if (!PQgetisnull(res, 0, i_collversion))
    {
      appendPQExpBufferStr(q, ", version = ");
      appendStringLiteralAH(q, PQgetvalue(res, 0, i_collversion), fout);
    }
  }

  appendPQExpBufferStr(q, ");\n");

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &collinfo->dobj, "COLLATION", qcollname, collinfo->dobj.namespace->dobj.name);
  }

  if (collinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, collinfo->dobj.catId, collinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = collinfo->dobj.name, .namespace = collinfo->dobj.namespace->dobj.name, .owner = collinfo->rolname, .description = "COLLATION", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                               
  if (collinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "COLLATION", qcollname, collinfo->dobj.namespace->dobj.name, collinfo->rolname, collinfo->dobj.catId, 0, collinfo->dobj.dumpId);
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qcollname);
}

   
                  
                                              
   
static void
dumpConversion(Archive *fout, ConvInfo *convinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qconvname;
  PGresult *res;
  int i_conforencoding;
  int i_contoencoding;
  int i_conproc;
  int i_condefault;
  const char *conforencoding;
  const char *contoencoding;
  const char *conproc;
  bool condefault;

                                
  if (!convinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qconvname = pg_strdup(fmtId(convinfo->dobj.name));

                                       
  appendPQExpBuffer(query,
      "SELECT "
      "pg_catalog.pg_encoding_to_char(conforencoding) AS conforencoding, "
      "pg_catalog.pg_encoding_to_char(contoencoding) AS contoencoding, "
      "conproc, condefault "
      "FROM pg_catalog.pg_conversion c "
      "WHERE c.oid = '%u'::pg_catalog.oid",
      convinfo->dobj.catId.oid);

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  i_conforencoding = PQfnumber(res, "conforencoding");
  i_contoencoding = PQfnumber(res, "contoencoding");
  i_conproc = PQfnumber(res, "conproc");
  i_condefault = PQfnumber(res, "condefault");

  conforencoding = PQgetvalue(res, 0, i_conforencoding);
  contoencoding = PQgetvalue(res, 0, i_contoencoding);
  conproc = PQgetvalue(res, 0, i_conproc);
  condefault = (PQgetvalue(res, 0, i_condefault)[0] == 't');

  appendPQExpBuffer(delq, "DROP CONVERSION %s;\n", fmtQualifiedDumpable(convinfo));

  appendPQExpBuffer(q, "CREATE %sCONVERSION %s FOR ", (condefault) ? "DEFAULT " : "", fmtQualifiedDumpable(convinfo));
  appendStringLiteralAH(q, conforencoding, fout);
  appendPQExpBufferStr(q, " TO ");
  appendStringLiteralAH(q, contoencoding, fout);
                                                     
  appendPQExpBuffer(q, " FROM %s;\n", conproc);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &convinfo->dobj, "CONVERSION", qconvname, convinfo->dobj.namespace->dobj.name);
  }

  if (convinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, convinfo->dobj.catId, convinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = convinfo->dobj.name, .namespace = convinfo->dobj.namespace->dobj.name, .owner = convinfo->rolname, .description = "CONVERSION", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                
  if (convinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "CONVERSION", qconvname, convinfo->dobj.namespace->dobj.name, convinfo->rolname, convinfo->dobj.catId, 0, convinfo->dobj.dumpId);
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qconvname);
}

   
                                                                         
   
                                                                        
                       
   
static char *
format_aggregate_signature(AggInfo *agginfo, Archive *fout, bool honor_quotes)
{
  PQExpBufferData buf;
  int j;

  initPQExpBuffer(&buf);
  if (honor_quotes)
  {
    appendPQExpBufferStr(&buf, fmtId(agginfo->aggfn.dobj.name));
  }
  else
  {
    appendPQExpBufferStr(&buf, agginfo->aggfn.dobj.name);
  }

  if (agginfo->aggfn.nargs == 0)
  {
    appendPQExpBuffer(&buf, "(*)");
  }
  else
  {
    appendPQExpBufferChar(&buf, '(');
    for (j = 0; j < agginfo->aggfn.nargs; j++)
    {
      appendPQExpBuffer(&buf, "%s%s", (j > 0) ? ", " : "", getFormattedTypeName(fout, agginfo->aggfn.argtypes[j], zeroAsOpaque));
    }
    appendPQExpBufferChar(&buf, ')');
  }
  return buf.data;
}

   
           
                                             
   
static void
dumpAgg(Archive *fout, AggInfo *agginfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer details;
  char *aggsig;                                    
  char *aggfullsig = NULL;                     
  char *aggsig_tag;
  PGresult *res;
  int i_aggtransfn;
  int i_aggfinalfn;
  int i_aggcombinefn;
  int i_aggserialfn;
  int i_aggdeserialfn;
  int i_aggmtransfn;
  int i_aggminvtransfn;
  int i_aggmfinalfn;
  int i_aggfinalextra;
  int i_aggmfinalextra;
  int i_aggfinalmodify;
  int i_aggmfinalmodify;
  int i_aggsortop;
  int i_aggkind;
  int i_aggtranstype;
  int i_aggtransspace;
  int i_aggmtranstype;
  int i_aggmtransspace;
  int i_agginitval;
  int i_aggminitval;
  int i_convertok;
  int i_proparallel;
  const char *aggtransfn;
  const char *aggfinalfn;
  const char *aggcombinefn;
  const char *aggserialfn;
  const char *aggdeserialfn;
  const char *aggmtransfn;
  const char *aggminvtransfn;
  const char *aggmfinalfn;
  bool aggfinalextra;
  bool aggmfinalextra;
  char aggfinalmodify;
  char aggmfinalmodify;
  const char *aggsortop;
  char *aggsortconvop;
  char aggkind;
  const char *aggtranstype;
  const char *aggtransspace;
  const char *aggmtranstype;
  const char *aggmtransspace;
  const char *agginitval;
  const char *aggminitval;
  bool convertok;
  const char *proparallel;
  char defaultfinalmodify;

                                
  if (!agginfo->aggfn.dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  details = createPQExpBuffer();

                                      
  if (fout->remoteVersion >= 110000)
  {
    appendPQExpBuffer(query,
        "SELECT aggtransfn, "
        "aggfinalfn, aggtranstype::pg_catalog.regtype, "
        "aggcombinefn, aggserialfn, aggdeserialfn, aggmtransfn, "
        "aggminvtransfn, aggmfinalfn, aggmtranstype::pg_catalog.regtype, "
        "aggfinalextra, aggmfinalextra, "
        "aggfinalmodify, aggmfinalmodify, "
        "aggsortop, "
        "aggkind, "
        "aggtransspace, agginitval, "
        "aggmtransspace, aggminitval, "
        "true AS convertok, "
        "pg_catalog.pg_get_function_arguments(p.oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(p.oid) AS funciargs, "
        "p.proparallel "
        "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
        "WHERE a.aggfnoid = p.oid "
        "AND p.oid = '%u'::pg_catalog.oid",
        agginfo->aggfn.dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 90600)
  {
    appendPQExpBuffer(query,
        "SELECT aggtransfn, "
        "aggfinalfn, aggtranstype::pg_catalog.regtype, "
        "aggcombinefn, aggserialfn, aggdeserialfn, aggmtransfn, "
        "aggminvtransfn, aggmfinalfn, aggmtranstype::pg_catalog.regtype, "
        "aggfinalextra, aggmfinalextra, "
        "'0' AS aggfinalmodify, '0' AS aggmfinalmodify, "
        "aggsortop, "
        "aggkind, "
        "aggtransspace, agginitval, "
        "aggmtransspace, aggminitval, "
        "true AS convertok, "
        "pg_catalog.pg_get_function_arguments(p.oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(p.oid) AS funciargs, "
        "p.proparallel "
        "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
        "WHERE a.aggfnoid = p.oid "
        "AND p.oid = '%u'::pg_catalog.oid",
        agginfo->aggfn.dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 90400)
  {
    appendPQExpBuffer(query,
        "SELECT aggtransfn, "
        "aggfinalfn, aggtranstype::pg_catalog.regtype, "
        "'-' AS aggcombinefn, '-' AS aggserialfn, "
        "'-' AS aggdeserialfn, aggmtransfn, aggminvtransfn, "
        "aggmfinalfn, aggmtranstype::pg_catalog.regtype, "
        "aggfinalextra, aggmfinalextra, "
        "'0' AS aggfinalmodify, '0' AS aggmfinalmodify, "
        "aggsortop, "
        "aggkind, "
        "aggtransspace, agginitval, "
        "aggmtransspace, aggminitval, "
        "true AS convertok, "
        "pg_catalog.pg_get_function_arguments(p.oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(p.oid) AS funciargs "
        "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
        "WHERE a.aggfnoid = p.oid "
        "AND p.oid = '%u'::pg_catalog.oid",
        agginfo->aggfn.dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80400)
  {
    appendPQExpBuffer(query,
        "SELECT aggtransfn, "
        "aggfinalfn, aggtranstype::pg_catalog.regtype, "
        "'-' AS aggcombinefn, '-' AS aggserialfn, "
        "'-' AS aggdeserialfn, '-' AS aggmtransfn, "
        "'-' AS aggminvtransfn, '-' AS aggmfinalfn, "
        "0 AS aggmtranstype, false AS aggfinalextra, "
        "false AS aggmfinalextra, "
        "'0' AS aggfinalmodify, '0' AS aggmfinalmodify, "
        "aggsortop, "
        "'n' AS aggkind, "
        "0 AS aggtransspace, agginitval, "
        "0 AS aggmtransspace, NULL AS aggminitval, "
        "true AS convertok, "
        "pg_catalog.pg_get_function_arguments(p.oid) AS funcargs, "
        "pg_catalog.pg_get_function_identity_arguments(p.oid) AS funciargs "
        "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
        "WHERE a.aggfnoid = p.oid "
        "AND p.oid = '%u'::pg_catalog.oid",
        agginfo->aggfn.dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80100)
  {
    appendPQExpBuffer(query,
        "SELECT aggtransfn, "
        "aggfinalfn, aggtranstype::pg_catalog.regtype, "
        "'-' AS aggcombinefn, '-' AS aggserialfn, "
        "'-' AS aggdeserialfn, '-' AS aggmtransfn, "
        "'-' AS aggminvtransfn, '-' AS aggmfinalfn, "
        "0 AS aggmtranstype, false AS aggfinalextra, "
        "false AS aggmfinalextra, "
        "'0' AS aggfinalmodify, '0' AS aggmfinalmodify, "
        "aggsortop, "
        "'n' AS aggkind, "
        "0 AS aggtransspace, agginitval, "
        "0 AS aggmtransspace, NULL AS aggminitval, "
        "true AS convertok "
        "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
        "WHERE a.aggfnoid = p.oid "
        "AND p.oid = '%u'::pg_catalog.oid",
        agginfo->aggfn.dobj.catId.oid);
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT aggtransfn, "
        "aggfinalfn, aggtranstype::pg_catalog.regtype, "
        "'-' AS aggcombinefn, '-' AS aggserialfn, "
        "'-' AS aggdeserialfn, '-' AS aggmtransfn, "
        "'-' AS aggminvtransfn, '-' AS aggmfinalfn, "
        "0 AS aggmtranstype, false AS aggfinalextra, "
        "false AS aggmfinalextra, "
        "'0' AS aggfinalmodify, '0' AS aggmfinalmodify, "
        "0 AS aggsortop, "
        "'n' AS aggkind, "
        "0 AS aggtransspace, agginitval, "
        "0 AS aggmtransspace, NULL AS aggminitval, "
        "true AS convertok "
        "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
        "WHERE a.aggfnoid = p.oid "
        "AND p.oid = '%u'::pg_catalog.oid",
        agginfo->aggfn.dobj.catId.oid);
  }

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  i_aggtransfn = PQfnumber(res, "aggtransfn");
  i_aggfinalfn = PQfnumber(res, "aggfinalfn");
  i_aggcombinefn = PQfnumber(res, "aggcombinefn");
  i_aggserialfn = PQfnumber(res, "aggserialfn");
  i_aggdeserialfn = PQfnumber(res, "aggdeserialfn");
  i_aggmtransfn = PQfnumber(res, "aggmtransfn");
  i_aggminvtransfn = PQfnumber(res, "aggminvtransfn");
  i_aggmfinalfn = PQfnumber(res, "aggmfinalfn");
  i_aggfinalextra = PQfnumber(res, "aggfinalextra");
  i_aggmfinalextra = PQfnumber(res, "aggmfinalextra");
  i_aggfinalmodify = PQfnumber(res, "aggfinalmodify");
  i_aggmfinalmodify = PQfnumber(res, "aggmfinalmodify");
  i_aggsortop = PQfnumber(res, "aggsortop");
  i_aggkind = PQfnumber(res, "aggkind");
  i_aggtranstype = PQfnumber(res, "aggtranstype");
  i_aggtransspace = PQfnumber(res, "aggtransspace");
  i_aggmtranstype = PQfnumber(res, "aggmtranstype");
  i_aggmtransspace = PQfnumber(res, "aggmtransspace");
  i_agginitval = PQfnumber(res, "agginitval");
  i_aggminitval = PQfnumber(res, "aggminitval");
  i_convertok = PQfnumber(res, "convertok");
  i_proparallel = PQfnumber(res, "proparallel");

  aggtransfn = PQgetvalue(res, 0, i_aggtransfn);
  aggfinalfn = PQgetvalue(res, 0, i_aggfinalfn);
  aggcombinefn = PQgetvalue(res, 0, i_aggcombinefn);
  aggserialfn = PQgetvalue(res, 0, i_aggserialfn);
  aggdeserialfn = PQgetvalue(res, 0, i_aggdeserialfn);
  aggmtransfn = PQgetvalue(res, 0, i_aggmtransfn);
  aggminvtransfn = PQgetvalue(res, 0, i_aggminvtransfn);
  aggmfinalfn = PQgetvalue(res, 0, i_aggmfinalfn);
  aggfinalextra = (PQgetvalue(res, 0, i_aggfinalextra)[0] == 't');
  aggmfinalextra = (PQgetvalue(res, 0, i_aggmfinalextra)[0] == 't');
  aggfinalmodify = PQgetvalue(res, 0, i_aggfinalmodify)[0];
  aggmfinalmodify = PQgetvalue(res, 0, i_aggmfinalmodify)[0];
  aggsortop = PQgetvalue(res, 0, i_aggsortop);
  aggkind = PQgetvalue(res, 0, i_aggkind)[0];
  aggtranstype = PQgetvalue(res, 0, i_aggtranstype);
  aggtransspace = PQgetvalue(res, 0, i_aggtransspace);
  aggmtranstype = PQgetvalue(res, 0, i_aggmtranstype);
  aggmtransspace = PQgetvalue(res, 0, i_aggmtransspace);
  agginitval = PQgetvalue(res, 0, i_agginitval);
  aggminitval = PQgetvalue(res, 0, i_aggminitval);
  convertok = (PQgetvalue(res, 0, i_convertok)[0] == 't');

  if (fout->remoteVersion >= 80400)
  {
                                                                        
    char *funcargs;
    char *funciargs;

    funcargs = PQgetvalue(res, 0, PQfnumber(res, "funcargs"));
    funciargs = PQgetvalue(res, 0, PQfnumber(res, "funciargs"));
    aggfullsig = format_function_arguments(&agginfo->aggfn, funcargs, true);
    aggsig = format_function_arguments(&agginfo->aggfn, funciargs, true);
  }
  else
  {
                                  
    aggsig = format_aggregate_signature(agginfo, fout, true);
  }

  aggsig_tag = format_aggregate_signature(agginfo, fout, false);

  if (i_proparallel != -1)
  {
    proparallel = PQgetvalue(res, 0, PQfnumber(res, "proparallel"));
  }
  else
  {
    proparallel = NULL;
  }

  if (!convertok)
  {
    pg_log_warning("aggregate function %s could not be dumped correctly for this database version; ignored", aggsig);

    if (aggfullsig)
    {
      free(aggfullsig);
    }

    free(aggsig);

    return;
  }

                                                                             
  defaultfinalmodify = (aggkind == AGGKIND_NORMAL) ? AGGMODIFY_READ_ONLY : AGGMODIFY_READ_WRITE;
                                              
  if (aggfinalmodify == '0')
  {
    aggfinalmodify = defaultfinalmodify;
  }
  if (aggmfinalmodify == '0')
  {
    aggmfinalmodify = defaultfinalmodify;
  }

                                                                 
  appendPQExpBuffer(details, "    SFUNC = %s,\n    STYPE = %s", aggtransfn, aggtranstype);

  if (strcmp(aggtransspace, "0") != 0)
  {
    appendPQExpBuffer(details, ",\n    SSPACE = %s", aggtransspace);
  }

  if (!PQgetisnull(res, 0, i_agginitval))
  {
    appendPQExpBufferStr(details, ",\n    INITCOND = ");
    appendStringLiteralAH(details, agginitval, fout);
  }

  if (strcmp(aggfinalfn, "-") != 0)
  {
    appendPQExpBuffer(details, ",\n    FINALFUNC = %s", aggfinalfn);
    if (aggfinalextra)
    {
      appendPQExpBufferStr(details, ",\n    FINALFUNC_EXTRA");
    }
    if (aggfinalmodify != defaultfinalmodify)
    {
      switch (aggfinalmodify)
      {
      case AGGMODIFY_READ_ONLY:
        appendPQExpBufferStr(details, ",\n    FINALFUNC_MODIFY = READ_ONLY");
        break;
      case AGGMODIFY_SHAREABLE:
        appendPQExpBufferStr(details, ",\n    FINALFUNC_MODIFY = SHAREABLE");
        break;
      case AGGMODIFY_READ_WRITE:
        appendPQExpBufferStr(details, ",\n    FINALFUNC_MODIFY = READ_WRITE");
        break;
      default:
        fatal("unrecognized aggfinalmodify value for aggregate \"%s\"", agginfo->aggfn.dobj.name);
        break;
      }
    }
  }

  if (strcmp(aggcombinefn, "-") != 0)
  {
    appendPQExpBuffer(details, ",\n    COMBINEFUNC = %s", aggcombinefn);
  }

  if (strcmp(aggserialfn, "-") != 0)
  {
    appendPQExpBuffer(details, ",\n    SERIALFUNC = %s", aggserialfn);
  }

  if (strcmp(aggdeserialfn, "-") != 0)
  {
    appendPQExpBuffer(details, ",\n    DESERIALFUNC = %s", aggdeserialfn);
  }

  if (strcmp(aggmtransfn, "-") != 0)
  {
    appendPQExpBuffer(details, ",\n    MSFUNC = %s,\n    MINVFUNC = %s,\n    MSTYPE = %s", aggmtransfn, aggminvtransfn, aggmtranstype);
  }

  if (strcmp(aggmtransspace, "0") != 0)
  {
    appendPQExpBuffer(details, ",\n    MSSPACE = %s", aggmtransspace);
  }

  if (!PQgetisnull(res, 0, i_aggminitval))
  {
    appendPQExpBufferStr(details, ",\n    MINITCOND = ");
    appendStringLiteralAH(details, aggminitval, fout);
  }

  if (strcmp(aggmfinalfn, "-") != 0)
  {
    appendPQExpBuffer(details, ",\n    MFINALFUNC = %s", aggmfinalfn);
    if (aggmfinalextra)
    {
      appendPQExpBufferStr(details, ",\n    MFINALFUNC_EXTRA");
    }
    if (aggmfinalmodify != defaultfinalmodify)
    {
      switch (aggmfinalmodify)
      {
      case AGGMODIFY_READ_ONLY:
        appendPQExpBufferStr(details, ",\n    MFINALFUNC_MODIFY = READ_ONLY");
        break;
      case AGGMODIFY_SHAREABLE:
        appendPQExpBufferStr(details, ",\n    MFINALFUNC_MODIFY = SHAREABLE");
        break;
      case AGGMODIFY_READ_WRITE:
        appendPQExpBufferStr(details, ",\n    MFINALFUNC_MODIFY = READ_WRITE");
        break;
      default:
        fatal("unrecognized aggmfinalmodify value for aggregate \"%s\"", agginfo->aggfn.dobj.name);
        break;
      }
    }
  }

  aggsortconvop = getFormattedOperatorName(fout, aggsortop);
  if (aggsortconvop)
  {
    appendPQExpBuffer(details, ",\n    SORTOP = %s", aggsortconvop);
    free(aggsortconvop);
  }

  if (aggkind == AGGKIND_HYPOTHETICAL)
  {
    appendPQExpBufferStr(details, ",\n    HYPOTHETICAL");
  }

  if (proparallel != NULL && proparallel[0] != PROPARALLEL_UNSAFE)
  {
    if (proparallel[0] == PROPARALLEL_SAFE)
    {
      appendPQExpBufferStr(details, ",\n    PARALLEL = safe");
    }
    else if (proparallel[0] == PROPARALLEL_RESTRICTED)
    {
      appendPQExpBufferStr(details, ",\n    PARALLEL = restricted");
    }
    else if (proparallel[0] != PROPARALLEL_UNSAFE)
    {
      fatal("unrecognized proparallel value for function \"%s\"", agginfo->aggfn.dobj.name);
    }
  }

  appendPQExpBuffer(delq, "DROP AGGREGATE %s.%s;\n", fmtId(agginfo->aggfn.dobj.namespace->dobj.name), aggsig);

  appendPQExpBuffer(q, "CREATE AGGREGATE %s.%s (\n%s\n);\n", fmtId(agginfo->aggfn.dobj.namespace->dobj.name), aggfullsig ? aggfullsig : aggsig, details->data);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &agginfo->aggfn.dobj, "AGGREGATE", aggsig, agginfo->aggfn.dobj.namespace->dobj.name);
  }

  if (agginfo->aggfn.dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId, ARCHIVE_OPTS(.tag = aggsig_tag, .namespace = agginfo->aggfn.dobj.namespace->dobj.name, .owner = agginfo->aggfn.rolname, .description = "AGGREGATE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                               
  if (agginfo->aggfn.dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "AGGREGATE", aggsig, agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname, agginfo->aggfn.dobj.catId, 0, agginfo->aggfn.dobj.dumpId);
  }

  if (agginfo->aggfn.dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "AGGREGATE", aggsig, agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname, agginfo->aggfn.dobj.catId, 0, agginfo->aggfn.dobj.dumpId);
  }

     
                                                                          
                                                                          
                                                                     
     
  free(aggsig);

  aggsig = format_function_signature(fout, &agginfo->aggfn, true);

  if (agginfo->aggfn.dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, agginfo->aggfn.dobj.dumpId, InvalidDumpId, "FUNCTION", aggsig, NULL, agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname, agginfo->aggfn.proacl, agginfo->aggfn.rproacl, agginfo->aggfn.initproacl, agginfo->aggfn.initrproacl);
  }

  free(aggsig);
  if (aggfullsig)
  {
    free(aggfullsig);
  }
  free(aggsig_tag);

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(details);
}

   
                
                                           
   
static void
dumpTSParser(Archive *fout, TSParserInfo *prsinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qprsname;

                                
  if (!prsinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qprsname = pg_strdup(fmtId(prsinfo->dobj.name));

  appendPQExpBuffer(q, "CREATE TEXT SEARCH PARSER %s (\n", fmtQualifiedDumpable(prsinfo));

  appendPQExpBuffer(q, "    START = %s,\n", convertTSFunction(fout, prsinfo->prsstart));
  appendPQExpBuffer(q, "    GETTOKEN = %s,\n", convertTSFunction(fout, prsinfo->prstoken));
  appendPQExpBuffer(q, "    END = %s,\n", convertTSFunction(fout, prsinfo->prsend));
  if (prsinfo->prsheadline != InvalidOid)
  {
    appendPQExpBuffer(q, "    HEADLINE = %s,\n", convertTSFunction(fout, prsinfo->prsheadline));
  }
  appendPQExpBuffer(q, "    LEXTYPES = %s );\n", convertTSFunction(fout, prsinfo->prslextype));

  appendPQExpBuffer(delq, "DROP TEXT SEARCH PARSER %s;\n", fmtQualifiedDumpable(prsinfo));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &prsinfo->dobj, "TEXT SEARCH PARSER", qprsname, prsinfo->dobj.namespace->dobj.name);
  }

  if (prsinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, prsinfo->dobj.catId, prsinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = prsinfo->dobj.name, .namespace = prsinfo->dobj.namespace->dobj.name, .description = "TEXT SEARCH PARSER", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                            
  if (prsinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TEXT SEARCH PARSER", qprsname, prsinfo->dobj.namespace->dobj.name, "", prsinfo->dobj.catId, 0, prsinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qprsname);
}

   
                    
                                               
   
static void
dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer query;
  char *qdictname;
  PGresult *res;
  char *nspname;
  char *tmplname;

                                
  if (!dictinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  query = createPQExpBuffer();

  qdictname = pg_strdup(fmtId(dictinfo->dobj.name));

                                                             
  appendPQExpBuffer(query,
      "SELECT nspname, tmplname "
      "FROM pg_ts_template p, pg_namespace n "
      "WHERE p.oid = '%u' AND n.oid = tmplnamespace",
      dictinfo->dicttemplate);
  res = ExecuteSqlQueryForSingleRow(fout, query->data);
  nspname = PQgetvalue(res, 0, 0);
  tmplname = PQgetvalue(res, 0, 1);

  appendPQExpBuffer(q, "CREATE TEXT SEARCH DICTIONARY %s (\n", fmtQualifiedDumpable(dictinfo));

  appendPQExpBufferStr(q, "    TEMPLATE = ");
  appendPQExpBuffer(q, "%s.", fmtId(nspname));
  appendPQExpBufferStr(q, fmtId(tmplname));

  PQclear(res);

                                                                  
  if (dictinfo->dictinitoption)
  {
    appendPQExpBuffer(q, ",\n    %s", dictinfo->dictinitoption);
  }

  appendPQExpBufferStr(q, " );\n");

  appendPQExpBuffer(delq, "DROP TEXT SEARCH DICTIONARY %s;\n", fmtQualifiedDumpable(dictinfo));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &dictinfo->dobj, "TEXT SEARCH DICTIONARY", qdictname, dictinfo->dobj.namespace->dobj.name);
  }

  if (dictinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, dictinfo->dobj.catId, dictinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = dictinfo->dobj.name, .namespace = dictinfo->dobj.namespace->dobj.name, .owner = dictinfo->rolname, .description = "TEXT SEARCH DICTIONARY", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                
  if (dictinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TEXT SEARCH DICTIONARY", qdictname, dictinfo->dobj.namespace->dobj.name, dictinfo->rolname, dictinfo->dobj.catId, 0, dictinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qdictname);
}

   
                  
                                             
   
static void
dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qtmplname;

                                
  if (!tmplinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qtmplname = pg_strdup(fmtId(tmplinfo->dobj.name));

  appendPQExpBuffer(q, "CREATE TEXT SEARCH TEMPLATE %s (\n", fmtQualifiedDumpable(tmplinfo));

  if (tmplinfo->tmplinit != InvalidOid)
  {
    appendPQExpBuffer(q, "    INIT = %s,\n", convertTSFunction(fout, tmplinfo->tmplinit));
  }
  appendPQExpBuffer(q, "    LEXIZE = %s );\n", convertTSFunction(fout, tmplinfo->tmpllexize));

  appendPQExpBuffer(delq, "DROP TEXT SEARCH TEMPLATE %s;\n", fmtQualifiedDumpable(tmplinfo));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tmplinfo->dobj, "TEXT SEARCH TEMPLATE", qtmplname, tmplinfo->dobj.namespace->dobj.name);
  }

  if (tmplinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tmplinfo->dobj.catId, tmplinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tmplinfo->dobj.name, .namespace = tmplinfo->dobj.namespace->dobj.name, .description = "TEXT SEARCH TEMPLATE", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                              
  if (tmplinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TEXT SEARCH TEMPLATE", qtmplname, tmplinfo->dobj.namespace->dobj.name, "", tmplinfo->dobj.catId, 0, tmplinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qtmplname);
}

   
                
                                                  
   
static void
dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer query;
  char *qcfgname;
  PGresult *res;
  char *nspname;
  char *prsname;
  int ntups, i;
  int i_tokenname;
  int i_dictname;

                                
  if (!cfginfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  query = createPQExpBuffer();

  qcfgname = pg_strdup(fmtId(cfginfo->dobj.name));

                                                       
  appendPQExpBuffer(query,
      "SELECT nspname, prsname "
      "FROM pg_ts_parser p, pg_namespace n "
      "WHERE p.oid = '%u' AND n.oid = prsnamespace",
      cfginfo->cfgparser);
  res = ExecuteSqlQueryForSingleRow(fout, query->data);
  nspname = PQgetvalue(res, 0, 0);
  prsname = PQgetvalue(res, 0, 1);

  appendPQExpBuffer(q, "CREATE TEXT SEARCH CONFIGURATION %s (\n", fmtQualifiedDumpable(cfginfo));

  appendPQExpBuffer(q, "    PARSER = %s.", fmtId(nspname));
  appendPQExpBuffer(q, "%s );\n", fmtId(prsname));

  PQclear(res);

  resetPQExpBuffer(query);
  appendPQExpBuffer(query,
      "SELECT\n"
      "  ( SELECT alias FROM pg_catalog.ts_token_type('%u'::pg_catalog.oid) AS t\n"
      "    WHERE t.tokid = m.maptokentype ) AS tokenname,\n"
      "  m.mapdict::pg_catalog.regdictionary AS dictname\n"
      "FROM pg_catalog.pg_ts_config_map AS m\n"
      "WHERE m.mapcfg = '%u'\n"
      "ORDER BY m.mapcfg, m.maptokentype, m.mapseqno",
      cfginfo->cfgparser, cfginfo->dobj.catId.oid);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
  ntups = PQntuples(res);

  i_tokenname = PQfnumber(res, "tokenname");
  i_dictname = PQfnumber(res, "dictname");

  for (i = 0; i < ntups; i++)
  {
    char *tokenname = PQgetvalue(res, i, i_tokenname);
    char *dictname = PQgetvalue(res, i, i_dictname);

    if (i == 0 || strcmp(tokenname, PQgetvalue(res, i - 1, i_tokenname)) != 0)
    {
                                                             
      if (i > 0)
      {
        appendPQExpBufferStr(q, ";\n");
      }
      appendPQExpBuffer(q, "\nALTER TEXT SEARCH CONFIGURATION %s\n", fmtQualifiedDumpable(cfginfo));
                                                      
      appendPQExpBuffer(q, "    ADD MAPPING FOR %s WITH %s", fmtId(tokenname), dictname);
    }
    else
    {
      appendPQExpBuffer(q, ", %s", dictname);
    }
  }

  if (ntups > 0)
  {
    appendPQExpBufferStr(q, ";\n");
  }

  PQclear(res);

  appendPQExpBuffer(delq, "DROP TEXT SEARCH CONFIGURATION %s;\n", fmtQualifiedDumpable(cfginfo));

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &cfginfo->dobj, "TEXT SEARCH CONFIGURATION", qcfgname, cfginfo->dobj.namespace->dobj.name);
  }

  if (cfginfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, cfginfo->dobj.catId, cfginfo->dobj.dumpId, ARCHIVE_OPTS(.tag = cfginfo->dobj.name, .namespace = cfginfo->dobj.namespace->dobj.name, .owner = cfginfo->rolname, .description = "TEXT SEARCH CONFIGURATION", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                   
  if (cfginfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "TEXT SEARCH CONFIGURATION", qcfgname, cfginfo->dobj.namespace->dobj.name, cfginfo->rolname, cfginfo->dobj.catId, 0, cfginfo->dobj.dumpId);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qcfgname);
}

   
                          
                                                        
   
static void
dumpForeignDataWrapper(Archive *fout, FdwInfo *fdwinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qfdwname;

                                
  if (!fdwinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qfdwname = pg_strdup(fmtId(fdwinfo->dobj.name));

  appendPQExpBuffer(q, "CREATE FOREIGN DATA WRAPPER %s", qfdwname);

  if (strcmp(fdwinfo->fdwhandler, "-") != 0)
  {
    appendPQExpBuffer(q, " HANDLER %s", fdwinfo->fdwhandler);
  }

  if (strcmp(fdwinfo->fdwvalidator, "-") != 0)
  {
    appendPQExpBuffer(q, " VALIDATOR %s", fdwinfo->fdwvalidator);
  }

  if (strlen(fdwinfo->fdwoptions) > 0)
  {
    appendPQExpBuffer(q, " OPTIONS (\n    %s\n)", fdwinfo->fdwoptions);
  }

  appendPQExpBufferStr(q, ";\n");

  appendPQExpBuffer(delq, "DROP FOREIGN DATA WRAPPER %s;\n", qfdwname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &fdwinfo->dobj, "FOREIGN DATA WRAPPER", qfdwname, NULL);
  }

  if (fdwinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, fdwinfo->dobj.catId, fdwinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = fdwinfo->dobj.name, .owner = fdwinfo->rolname, .description = "FOREIGN DATA WRAPPER", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                          
  if (fdwinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "FOREIGN DATA WRAPPER", qfdwname, NULL, fdwinfo->rolname, fdwinfo->dobj.catId, 0, fdwinfo->dobj.dumpId);
  }

                      
  if (fdwinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, fdwinfo->dobj.dumpId, InvalidDumpId, "FOREIGN DATA WRAPPER", qfdwname, NULL, NULL, fdwinfo->rolname, fdwinfo->fdwacl, fdwinfo->rfdwacl, fdwinfo->initfdwacl, fdwinfo->initrfdwacl);
  }

  free(qfdwname);

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
}

   
                     
                                           
   
static void
dumpForeignServer(Archive *fout, ForeignServerInfo *srvinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer query;
  PGresult *res;
  char *qsrvname;
  char *fdwname;

                                
  if (!srvinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  query = createPQExpBuffer();

  qsrvname = pg_strdup(fmtId(srvinfo->dobj.name));

                                        
  appendPQExpBuffer(query,
      "SELECT fdwname "
      "FROM pg_foreign_data_wrapper w "
      "WHERE w.oid = '%u'",
      srvinfo->srvfdw);
  res = ExecuteSqlQueryForSingleRow(fout, query->data);
  fdwname = PQgetvalue(res, 0, 0);

  appendPQExpBuffer(q, "CREATE SERVER %s", qsrvname);
  if (srvinfo->srvtype && strlen(srvinfo->srvtype) > 0)
  {
    appendPQExpBufferStr(q, " TYPE ");
    appendStringLiteralAH(q, srvinfo->srvtype, fout);
  }
  if (srvinfo->srvversion && strlen(srvinfo->srvversion) > 0)
  {
    appendPQExpBufferStr(q, " VERSION ");
    appendStringLiteralAH(q, srvinfo->srvversion, fout);
  }

  appendPQExpBufferStr(q, " FOREIGN DATA WRAPPER ");
  appendPQExpBufferStr(q, fmtId(fdwname));

  if (srvinfo->srvoptions && strlen(srvinfo->srvoptions) > 0)
  {
    appendPQExpBuffer(q, " OPTIONS (\n    %s\n)", srvinfo->srvoptions);
  }

  appendPQExpBufferStr(q, ";\n");

  appendPQExpBuffer(delq, "DROP SERVER %s;\n", qsrvname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &srvinfo->dobj, "SERVER", qsrvname, NULL);
  }

  if (srvinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, srvinfo->dobj.catId, srvinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = srvinfo->dobj.name, .owner = srvinfo->rolname, .description = "SERVER", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                    
  if (srvinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "SERVER", qsrvname, NULL, srvinfo->rolname, srvinfo->dobj.catId, 0, srvinfo->dobj.dumpId);
  }

                      
  if (srvinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    dumpACL(fout, srvinfo->dobj.dumpId, InvalidDumpId, "FOREIGN SERVER", qsrvname, NULL, NULL, srvinfo->rolname, srvinfo->srvacl, srvinfo->rsrvacl, srvinfo->initsrvacl, srvinfo->initrsrvacl);
  }

                          
  if (srvinfo->dobj.dump & DUMP_COMPONENT_USERMAP)
  {
    dumpUserMappings(fout, srvinfo->dobj.name, NULL, srvinfo->rolname, srvinfo->dobj.catId, srvinfo->dobj.dumpId);
  }

  free(qsrvname);

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
}

   
                    
   
                                                                      
                                                                        
                   
   
static void
dumpUserMappings(Archive *fout, const char *servername, const char *namespace, const char *owner, CatalogId catalogId, DumpId dumpId)
{
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer query;
  PQExpBuffer tag;
  PGresult *res;
  int ntups;
  int i_usename;
  int i_umoptions;
  int i;

  q = createPQExpBuffer();
  tag = createPQExpBuffer();
  delq = createPQExpBuffer();
  query = createPQExpBuffer();

     
                                                                           
                                                                      
                                                                            
                                                                           
                                                                      
                                                                
     
  appendPQExpBuffer(query,
      "SELECT usename, "
      "array_to_string(ARRAY("
      "SELECT quote_ident(option_name) || ' ' || "
      "quote_literal(option_value) "
      "FROM pg_options_to_table(umoptions) "
      "ORDER BY option_name"
      "), E',\n    ') AS umoptions "
      "FROM pg_user_mappings "
      "WHERE srvid = '%u' "
      "ORDER BY usename",
      catalogId.oid);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);
  i_usename = PQfnumber(res, "usename");
  i_umoptions = PQfnumber(res, "umoptions");

  for (i = 0; i < ntups; i++)
  {
    char *usename;
    char *umoptions;

    usename = PQgetvalue(res, i, i_usename);
    umoptions = PQgetvalue(res, i, i_umoptions);

    resetPQExpBuffer(q);
    appendPQExpBuffer(q, "CREATE USER MAPPING FOR %s", fmtId(usename));
    appendPQExpBuffer(q, " SERVER %s", fmtId(servername));

    if (umoptions && strlen(umoptions) > 0)
    {
      appendPQExpBuffer(q, " OPTIONS (\n    %s\n)", umoptions);
    }

    appendPQExpBufferStr(q, ";\n");

    resetPQExpBuffer(delq);
    appendPQExpBuffer(delq, "DROP USER MAPPING FOR %s", fmtId(usename));
    appendPQExpBuffer(delq, " SERVER %s;\n", fmtId(servername));

    resetPQExpBuffer(tag);
    appendPQExpBuffer(tag, "USER MAPPING %s SERVER %s", usename, servername);

    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tag->data, .namespace = namespace, .owner = owner, .description = "USER MAPPING", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(tag);
  destroyPQExpBuffer(q);
}

   
                                            
   
static void
dumpDefaultACL(Archive *fout, DefaultACLInfo *daclinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer tag;
  const char *type;

                                
  if (!daclinfo->dobj.dump || dopt->dataOnly || dopt->aclsSkip)
  {
    return;
  }

  q = createPQExpBuffer();
  tag = createPQExpBuffer();

  switch (daclinfo->defaclobjtype)
  {
  case DEFACLOBJ_RELATION:
    type = "TABLES";
    break;
  case DEFACLOBJ_SEQUENCE:
    type = "SEQUENCES";
    break;
  case DEFACLOBJ_FUNCTION:
    type = "FUNCTIONS";
    break;
  case DEFACLOBJ_TYPE:
    type = "TYPES";
    break;
  case DEFACLOBJ_NAMESPACE:
    type = "SCHEMAS";
    break;
  default:
                            
    fatal("unrecognized object type in default privileges: %d", (int)daclinfo->defaclobjtype);
    type = "";                          
  }

  appendPQExpBuffer(tag, "DEFAULT PRIVILEGES FOR %s", type);

                                                  
  if (!buildDefaultACLCommands(type, daclinfo->dobj.namespace != NULL ? daclinfo->dobj.namespace->dobj.name : NULL, daclinfo->defaclacl, daclinfo->rdefaclacl, daclinfo->initdefaclacl, daclinfo->initrdefaclacl, daclinfo->defaclrole, fout->remoteVersion, q))
  {
    fatal("could not parse default ACL list (%s)", daclinfo->defaclacl);
  }

  if (daclinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    ArchiveEntry(fout, daclinfo->dobj.catId, daclinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag->data, .namespace = daclinfo->dobj.namespace ? daclinfo->dobj.namespace->dobj.name : NULL, .owner = daclinfo->defaclrole, .description = "DEFAULT ACL", .section = SECTION_POST_DATA, .createStmt = q->data));
  }

  destroyPQExpBuffer(tag);
  destroyPQExpBuffer(q);
}

             
                                      
   
                                                        
                                                                              
                                                                  
                         
                                                                       
                                                   
                                                                             
                                                                               
                                                                           
                                                               
                                                                    
                                                                            
                                                                           
                                 
                                                                              
                                                                    
                                  
                                                                         
                                                   
                                                                  
                                                                        
   
                                                                              
                                                                         
                                                      
   
                                                                         
                             
             
   
static DumpId
dumpACL(Archive *fout, DumpId objDumpId, DumpId altDumpId, const char *type, const char *name, const char *subname, const char *nspname, const char *owner, const char *acls, const char *racls, const char *initacls, const char *initracls)
{
  DumpId aclDumpId = InvalidDumpId;
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer sql;

                                             
  if (dopt->aclsSkip)
  {
    return InvalidDumpId;
  }

                                                 
  if (dopt->dataOnly && strcmp(type, "LARGE OBJECT") != 0)
  {
    return InvalidDumpId;
  }

  sql = createPQExpBuffer();

     
                                                                           
                                                                         
                                                                    
                                                                          
                                                                          
                       
     
  if (strlen(initacls) != 0 || strlen(initracls) != 0)
  {
    appendPQExpBuffer(sql, "SELECT pg_catalog.binary_upgrade_set_record_init_privs(true);\n");
    if (!buildACLCommands(name, subname, nspname, type, initacls, initracls, owner, "", fout->remoteVersion, sql))
    {
      fatal("could not parse initial GRANT ACL list (%s) or initial REVOKE ACL list (%s) for object \"%s\" (%s)", initacls, initracls, name, type);
    }
    appendPQExpBuffer(sql, "SELECT pg_catalog.binary_upgrade_set_record_init_privs(false);\n");
  }

  if (!buildACLCommands(name, subname, nspname, type, acls, racls, owner, "", fout->remoteVersion, sql))
  {
    fatal("could not parse GRANT ACL list (%s) or REVOKE ACL list (%s) for object \"%s\" (%s)", acls, racls, name, type);
  }

  if (sql->len > 0)
  {
    PQExpBuffer tag = createPQExpBuffer();
    DumpId aclDeps[2];
    int nDeps = 0;

    if (subname)
    {
      appendPQExpBuffer(tag, "COLUMN %s.%s", name, subname);
    }
    else
    {
      appendPQExpBuffer(tag, "%s %s", type, name);
    }

    aclDeps[nDeps++] = objDumpId;
    if (altDumpId != InvalidDumpId)
    {
      aclDeps[nDeps++] = altDumpId;
    }

    aclDumpId = createDumpId();

    ArchiveEntry(fout, nilCatalogId, aclDumpId, ARCHIVE_OPTS(.tag = tag->data, .namespace = nspname, .owner = owner, .description = "ACL", .section = SECTION_NONE, .createStmt = sql->data, .deps = aclDeps, .nDeps = nDeps));

    destroyPQExpBuffer(tag);
  }

  destroyPQExpBuffer(sql);

  return aclDumpId;
}

   
                
   
                                                                        
                                                                    
                                                                        
                                                                          
                                                                       
                                                               
                                                           
   
                                                                       
                                                                        
                                                                            
                                                                          
                                                                              
                                                    
   
static void
dumpSecLabel(Archive *fout, const char *type, const char *name, const char *namespace, const char *owner, CatalogId catalogId, int subid, DumpId dumpId)
{
  DumpOptions *dopt = fout->dopt;
  SecLabelItem *labels;
  int nlabels;
  int i;
  PQExpBuffer query;

                                                       
  if (dopt->no_security_labels)
  {
    return;
  }

                                                                           
  if (strcmp(type, "LARGE OBJECT") != 0)
  {
    if (dopt->dataOnly)
    {
      return;
    }
  }
  else
  {
                                                                
    if (dopt->schemaOnly && !dopt->binary_upgrade)
    {
      return;
    }
  }

                                                                         
  nlabels = findSecLabels(fout, catalogId.tableoid, catalogId.oid, &labels);

  query = createPQExpBuffer();

  for (i = 0; i < nlabels; i++)
  {
       
                                                               
       
    if (labels[i].objsubid != subid)
    {
      continue;
    }

    appendPQExpBuffer(query, "SECURITY LABEL FOR %s ON %s ", fmtId(labels[i].provider), type);
    if (namespace && *namespace)
    {
      appendPQExpBuffer(query, "%s.", fmtId(namespace));
    }
    appendPQExpBuffer(query, "%s IS ", name);
    appendStringLiteralAH(query, labels[i].label, fout);
    appendPQExpBufferStr(query, ";\n");
  }

  if (query->len > 0)
  {
    PQExpBuffer tag = createPQExpBuffer();

    appendPQExpBuffer(tag, "%s %s", type, name);
    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tag->data, .namespace = namespace, .owner = owner, .description = "SECURITY LABEL", .section = SECTION_NONE, .createStmt = query->data, .deps = &dumpId, .nDeps = 1));
    destroyPQExpBuffer(tag);
  }

  destroyPQExpBuffer(query);
}

   
                     
   
                                                                            
                    
   
static void
dumpTableSecLabel(Archive *fout, TableInfo *tbinfo, const char *reltypename)
{
  DumpOptions *dopt = fout->dopt;
  SecLabelItem *labels;
  int nlabels;
  int i;
  PQExpBuffer query;
  PQExpBuffer target;

                                                       
  if (dopt->no_security_labels)
  {
    return;
  }

                                    
  if (dopt->dataOnly)
  {
    return;
  }

                                                                 
  nlabels = findSecLabels(fout, tbinfo->dobj.catId.tableoid, tbinfo->dobj.catId.oid, &labels);

                                                                 
  if (nlabels <= 0)
  {
    return;
  }

  query = createPQExpBuffer();
  target = createPQExpBuffer();

  for (i = 0; i < nlabels; i++)
  {
    const char *colname;
    const char *provider = labels[i].provider;
    const char *label = labels[i].label;
    int objsubid = labels[i].objsubid;

    resetPQExpBuffer(target);
    if (objsubid == 0)
    {
      appendPQExpBuffer(target, "%s %s", reltypename, fmtQualifiedDumpable(tbinfo));
    }
    else
    {
      colname = getAttrName(objsubid, tbinfo);
                                                                     
      appendPQExpBuffer(target, "COLUMN %s", fmtQualifiedDumpable(tbinfo));
      appendPQExpBuffer(target, ".%s", fmtId(colname));
    }
    appendPQExpBuffer(query, "SECURITY LABEL FOR %s ON %s IS ", fmtId(provider), target->data);
    appendStringLiteralAH(query, label, fout);
    appendPQExpBufferStr(query, ";\n");
  }
  if (query->len > 0)
  {
    resetPQExpBuffer(target);
    appendPQExpBuffer(target, "%s %s", reltypename, fmtId(tbinfo->dobj.name));
    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = target->data, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "SECURITY LABEL", .section = SECTION_NONE, .createStmt = query->data, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1));
  }
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(target);
}

   
                 
   
                                                                         
                                                                         
                          
   
static int
findSecLabels(Archive *fout, Oid classoid, Oid objoid, SecLabelItem **items)
{
                                                   
  static SecLabelItem *labels = NULL;
  static int nlabels = -1;

  SecLabelItem *middle = NULL;
  SecLabelItem *low;
  SecLabelItem *high;
  int nmatch;

                                                
  if (nlabels < 0)
  {
    nlabels = collectSecLabels(fout, &labels);
  }

  if (nlabels <= 0)                                         
  {
    *items = NULL;
    return 0;
  }

     
                                                             
     
  low = &labels[0];
  high = &labels[nlabels - 1];
  while (low <= high)
  {
    middle = low + (high - low) / 2;

    if (classoid < middle->classoid)
    {
      high = middle - 1;
    }
    else if (classoid > middle->classoid)
    {
      low = middle + 1;
    }
    else if (objoid < middle->objoid)
    {
      high = middle - 1;
    }
    else if (objoid > middle->objoid)
    {
      low = middle + 1;
    }
    else
    {
      break;                    
    }
  }

  if (low > high)                 
  {
    *items = NULL;
    return 0;
  }

     
                                                                     
                                                                            
            
     
  nmatch = 1;
  while (middle > low)
  {
    if (classoid != middle[-1].classoid || objoid != middle[-1].objoid)
    {
      break;
    }
    middle--;
    nmatch++;
  }

  *items = middle;

  middle += nmatch;
  while (middle <= high)
  {
    if (classoid != middle->classoid || objoid != middle->objoid)
    {
      break;
    }
    middle++;
    nmatch++;
  }

  return nmatch;
}

   
                    
   
                                                                            
                                              
   
                                                                       
   
static int
collectSecLabels(Archive *fout, SecLabelItem **items)
{
  PGresult *res;
  PQExpBuffer query;
  int i_label;
  int i_provider;
  int i_classoid;
  int i_objoid;
  int i_objsubid;
  int ntups;
  int i;
  SecLabelItem *labels;

  query = createPQExpBuffer();

  appendPQExpBufferStr(query, "SELECT label, provider, classoid, objoid, objsubid "
                              "FROM pg_catalog.pg_seclabel "
                              "ORDER BY classoid, objoid, objsubid");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

                                                              
  i_label = PQfnumber(res, "label");
  i_provider = PQfnumber(res, "provider");
  i_classoid = PQfnumber(res, "classoid");
  i_objoid = PQfnumber(res, "objoid");
  i_objsubid = PQfnumber(res, "objsubid");

  ntups = PQntuples(res);

  labels = (SecLabelItem *)pg_malloc(ntups * sizeof(SecLabelItem));

  for (i = 0; i < ntups; i++)
  {
    labels[i].label = PQgetvalue(res, i, i_label);
    labels[i].provider = PQgetvalue(res, i, i_provider);
    labels[i].classoid = atooid(PQgetvalue(res, i, i_classoid));
    labels[i].objoid = atooid(PQgetvalue(res, i, i_objoid));
    labels[i].objsubid = atoi(PQgetvalue(res, i, i_objsubid));
  }

                                                                      
  destroyPQExpBuffer(query);

  *items = labels;
  return ntups;
}

   
             
                                                                           
   
static void
dumpTable(Archive *fout, TableInfo *tbinfo)
{
  DumpOptions *dopt = fout->dopt;
  DumpId tableAclDumpId = InvalidDumpId;
  char *namecopy;

     
                                                                        
                            
     
  if (!tbinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  if (tbinfo->relkind == RELKIND_SEQUENCE)
  {
    dumpSequence(fout, tbinfo);
  }
  else
  {
    dumpTableSchema(fout, tbinfo);
  }

                           
  namecopy = pg_strdup(fmtId(tbinfo->dobj.name));
  if (tbinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    const char *objtype = (tbinfo->relkind == RELKIND_SEQUENCE) ? "SEQUENCE" : "TABLE";

    tableAclDumpId = dumpACL(fout, tbinfo->dobj.dumpId, InvalidDumpId, objtype, namecopy, NULL, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, tbinfo->relacl, tbinfo->rrelacl, tbinfo->initrelacl, tbinfo->initrrelacl);
  }

     
                                                                            
                                                                             
                                  
     
  if (fout->remoteVersion >= 80400 && tbinfo->dobj.dump & DUMP_COMPONENT_ACL)
  {
    PQExpBuffer query = createPQExpBuffer();
    PGresult *res;
    int i;

    if (fout->remoteVersion >= 90600)
    {
      PQExpBuffer acl_subquery = createPQExpBuffer();
      PQExpBuffer racl_subquery = createPQExpBuffer();
      PQExpBuffer initacl_subquery = createPQExpBuffer();
      PQExpBuffer initracl_subquery = createPQExpBuffer();

      buildACLQueries(acl_subquery, racl_subquery, initacl_subquery, initracl_subquery, "at.attacl", "c.relowner", "'c'", dopt->binary_upgrade);

      appendPQExpBuffer(query,
          "SELECT at.attname, "
          "%s AS attacl, "
          "%s AS rattacl, "
          "%s AS initattacl, "
          "%s AS initrattacl "
          "FROM pg_catalog.pg_attribute at "
          "JOIN pg_catalog.pg_class c ON (at.attrelid = c.oid) "
          "LEFT JOIN pg_catalog.pg_init_privs pip ON "
          "(at.attrelid = pip.objoid "
          "AND pip.classoid = 'pg_catalog.pg_class'::pg_catalog.regclass "
          "AND at.attnum = pip.objsubid) "
          "WHERE at.attrelid = '%u'::pg_catalog.oid AND "
          "NOT at.attisdropped "
          "AND ("
          "%s IS NOT NULL OR "
          "%s IS NOT NULL OR "
          "%s IS NOT NULL OR "
          "%s IS NOT NULL)"
          "ORDER BY at.attnum",
          acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data, tbinfo->dobj.catId.oid, acl_subquery->data, racl_subquery->data, initacl_subquery->data, initracl_subquery->data);

      destroyPQExpBuffer(acl_subquery);
      destroyPQExpBuffer(racl_subquery);
      destroyPQExpBuffer(initacl_subquery);
      destroyPQExpBuffer(initracl_subquery);
    }
    else
    {
      appendPQExpBuffer(query,
          "SELECT attname, attacl, NULL as rattacl, "
          "NULL AS initattacl, NULL AS initrattacl "
          "FROM pg_catalog.pg_attribute "
          "WHERE attrelid = '%u'::pg_catalog.oid AND NOT attisdropped "
          "AND attacl IS NOT NULL "
          "ORDER BY attnum",
          tbinfo->dobj.catId.oid);
    }

    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

    for (i = 0; i < PQntuples(res); i++)
    {
      char *attname = PQgetvalue(res, i, 0);
      char *attacl = PQgetvalue(res, i, 1);
      char *rattacl = PQgetvalue(res, i, 2);
      char *initattacl = PQgetvalue(res, i, 3);
      char *initrattacl = PQgetvalue(res, i, 4);
      char *attnamecopy;

      attnamecopy = pg_strdup(fmtId(attname));

         
                                                                       
                                                                     
                                                                
         
      dumpACL(fout, tbinfo->dobj.dumpId, tableAclDumpId, "TABLE", namecopy, attnamecopy, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, attacl, rattacl, initattacl, initrattacl);
      free(attnamecopy);
    }
    PQclear(res);
    destroyPQExpBuffer(query);
  }

  free(namecopy);

  return;
}

   
                                                                          
                                                                        
   
                                                                
   
static PQExpBuffer
createViewAsClause(Archive *fout, TableInfo *tbinfo)
{
  PQExpBuffer query = createPQExpBuffer();
  PQExpBuffer result = createPQExpBuffer();
  PGresult *res;
  int len;

                                 
  appendPQExpBuffer(query, "SELECT pg_catalog.pg_get_viewdef('%u'::pg_catalog.oid) AS viewdef", tbinfo->dobj.catId.oid);

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  if (PQntuples(res) != 1)
  {
    if (PQntuples(res) < 1)
    {
      fatal("query to obtain definition of view \"%s\" returned no data", tbinfo->dobj.name);
    }
    else
    {
      fatal("query to obtain definition of view \"%s\" returned more than one definition", tbinfo->dobj.name);
    }
  }

  len = PQgetlength(res, 0, 0);

  if (len == 0)
  {
    fatal("definition of view \"%s\" appears to be empty (length zero)", tbinfo->dobj.name);
  }

                                                                         
  Assert(PQgetvalue(res, 0, 0)[len - 1] == ';');
  appendBinaryPQExpBuffer(result, PQgetvalue(res, 0, 0), len - 1);

  PQclear(res);
  destroyPQExpBuffer(query);

  return result;
}

   
                                                                         
                                                                    
                                                                              
                                                                        
   
                                                                
   
static PQExpBuffer
createDummyViewAsClause(Archive *fout, TableInfo *tbinfo)
{
  PQExpBuffer result = createPQExpBuffer();
  int j;

  appendPQExpBufferStr(result, "SELECT");

  for (j = 0; j < tbinfo->numatts; j++)
  {
    if (j > 0)
    {
      appendPQExpBufferChar(result, ',');
    }
    appendPQExpBufferStr(result, "\n    ");

    appendPQExpBuffer(result, "NULL::%s", tbinfo->atttypnames[j]);

       
                                                                         
                                    
       
    if (OidIsValid(tbinfo->attcollation[j]))
    {
      CollInfo *coll;

      coll = findCollationByOid(tbinfo->attcollation[j]);
      if (coll)
      {
        appendPQExpBuffer(result, " COLLATE %s", fmtQualifiedDumpable(coll));
      }
    }

    appendPQExpBuffer(result, " AS %s", fmtId(tbinfo->attnames[j]));
  }

  return result;
}

   
                   
                                                                        
   
static void
dumpTableSchema(Archive *fout, TableInfo *tbinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q = createPQExpBuffer();
  PQExpBuffer delq = createPQExpBuffer();
  char *qrelname;
  char *qualrelname;
  int numParents;
  TableInfo **parents;
  int actual_atts;                                               
  const char *reltypename;
  char *storage;
  int j, k;

                                                                     
  Assert(tbinfo->interesting);

  qrelname = pg_strdup(fmtId(tbinfo->dobj.name));
  qualrelname = pg_strdup(fmtQualifiedDumpable(tbinfo));

  if (tbinfo->hasoids)
  {
    pg_log_warning("WITH OIDS is not supported anymore (table \"%s\")", qrelname);
  }

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_type_oids_by_rel_oid(fout, q, tbinfo->dobj.catId.oid);
  }

                                
  if (tbinfo->relkind == RELKIND_VIEW)
  {
    PQExpBuffer result;

       
                                                                        
       

    reltypename = "VIEW";

    appendPQExpBuffer(delq, "DROP VIEW %s;\n", qualrelname);

    if (dopt->binary_upgrade)
    {
      binary_upgrade_set_pg_class_oids(fout, q, tbinfo->dobj.catId.oid, false);
    }

    appendPQExpBuffer(q, "CREATE VIEW %s", qualrelname);

    if (tbinfo->dummy_view)
    {
      result = createDummyViewAsClause(fout, tbinfo);
    }
    else
    {
      if (nonemptyReloptions(tbinfo->reloptions))
      {
        appendPQExpBufferStr(q, " WITH (");
        appendReloptionsArrayAH(q, tbinfo->reloptions, "", fout);
        appendPQExpBufferChar(q, ')');
      }
      result = createViewAsClause(fout, tbinfo);
    }
    appendPQExpBuffer(q, " AS\n%s", result->data);
    destroyPQExpBuffer(result);

    if (tbinfo->checkoption != NULL && !tbinfo->dummy_view)
    {
      appendPQExpBuffer(q, "\n  WITH %s CHECK OPTION", tbinfo->checkoption);
    }
    appendPQExpBufferStr(q, ";\n");
  }
  else
  {
    char *partkeydef = NULL;
    char *ftoptions = NULL;
    char *srvname = NULL;

       
                                                                      
                                        
       
    switch (tbinfo->relkind)
    {
    case RELKIND_PARTITIONED_TABLE:
    {
      PQExpBuffer query = createPQExpBuffer();
      PGresult *res;

      reltypename = "TABLE";

                                             
      appendPQExpBuffer(query, "SELECT pg_get_partkeydef('%u')", tbinfo->dobj.catId.oid);
      res = ExecuteSqlQueryForSingleRow(fout, query->data);
      partkeydef = pg_strdup(PQgetvalue(res, 0, 0));
      PQclear(res);
      destroyPQExpBuffer(query);
      break;
    }
    case RELKIND_FOREIGN_TABLE:
    {
      PQExpBuffer query = createPQExpBuffer();
      PGresult *res;
      int i_srvname;
      int i_ftoptions;

      reltypename = "FOREIGN TABLE";

                                                               
      appendPQExpBuffer(query,
          "SELECT fs.srvname, "
          "pg_catalog.array_to_string(ARRAY("
          "SELECT pg_catalog.quote_ident(option_name) || "
          "' ' || pg_catalog.quote_literal(option_value) "
          "FROM pg_catalog.pg_options_to_table(ftoptions) "
          "ORDER BY option_name"
          "), E',\n    ') AS ftoptions "
          "FROM pg_catalog.pg_foreign_table ft "
          "JOIN pg_catalog.pg_foreign_server fs "
          "ON (fs.oid = ft.ftserver) "
          "WHERE ft.ftrelid = '%u'",
          tbinfo->dobj.catId.oid);
      res = ExecuteSqlQueryForSingleRow(fout, query->data);
      i_srvname = PQfnumber(res, "srvname");
      i_ftoptions = PQfnumber(res, "ftoptions");
      srvname = pg_strdup(PQgetvalue(res, 0, i_srvname));
      ftoptions = pg_strdup(PQgetvalue(res, 0, i_ftoptions));
      PQclear(res);
      destroyPQExpBuffer(query);
      break;
    }
    case RELKIND_MATVIEW:
      reltypename = "MATERIALIZED VIEW";
      break;
    default:
      reltypename = "TABLE";
      break;
    }

    numParents = tbinfo->numParents;
    parents = tbinfo->parents;

    appendPQExpBuffer(delq, "DROP %s %s;\n", reltypename, qualrelname);

    if (dopt->binary_upgrade)
    {
      binary_upgrade_set_pg_class_oids(fout, q, tbinfo->dobj.catId.oid, false);
    }

    appendPQExpBuffer(q, "CREATE %s%s %s", tbinfo->relpersistence == RELPERSISTENCE_UNLOGGED ? "UNLOGGED " : "", reltypename, qualrelname);

       
                                                                         
                                                                       
       
    if (OidIsValid(tbinfo->reloftype) && !dopt->binary_upgrade)
    {
      appendPQExpBuffer(q, " OF %s", getFormattedTypeName(fout, tbinfo->reloftype, zeroAsOpaque));
    }

    if (tbinfo->relkind != RELKIND_MATVIEW)
    {
                               
      actual_atts = 0;
      for (j = 0; j < tbinfo->numatts; j++)
      {
           
                                                                     
                                                                    
                                                                   
                  
           
        if (shouldPrintColumn(dopt, tbinfo, j))
        {
          bool print_default;
          bool print_notnull;

             
                                                                     
             
          print_default = (tbinfo->attrdefs[j] != NULL && !tbinfo->attrdefs[j]->separate);

             
                                                                   
                                                                
                         
             
          print_notnull = (tbinfo->notnull[j] && (!tbinfo->inhNotNull[j] || tbinfo->ispartition || dopt->binary_upgrade));

             
                                                                  
                            
             
          if (OidIsValid(tbinfo->reloftype) && !print_default && !print_notnull && !dopt->binary_upgrade)
          {
            continue;
          }

                                                 
          if (actual_atts == 0)
          {
            appendPQExpBufferStr(q, " (");
          }
          else
          {
            appendPQExpBufferChar(q, ',');
          }
          appendPQExpBufferStr(q, "\n    ");
          actual_atts++;

                              
          appendPQExpBufferStr(q, fmtId(tbinfo->attnames[j]));

          if (tbinfo->attisdropped[j])
          {
               
                                              
                                                                   
                                                                   
                                      
               
            appendPQExpBufferStr(q, " INTEGER            ");
                                             
            continue;
          }

             
                                                                   
                                                                 
                                        
             
          if (dopt->binary_upgrade || !OidIsValid(tbinfo->reloftype))
          {
            appendPQExpBuffer(q, " %s", tbinfo->atttypnames[j]);
          }

          if (print_default)
          {
            if (tbinfo->attgenerated[j] == ATTRIBUTE_GENERATED_STORED)
            {
              appendPQExpBuffer(q, " GENERATED ALWAYS AS (%s) STORED", tbinfo->attrdefs[j]->adef_expr);
            }
            else
            {
              appendPQExpBuffer(q, " DEFAULT %s", tbinfo->attrdefs[j]->adef_expr);
            }
          }

          if (print_notnull)
          {
            appendPQExpBufferStr(q, " NOT NULL");
          }

                                                         
          if (OidIsValid(tbinfo->attcollation[j]))
          {
            CollInfo *coll;

            coll = findCollationByOid(tbinfo->attcollation[j]);
            if (coll)
            {
              appendPQExpBuffer(q, " COLLATE %s", fmtQualifiedDumpable(coll));
            }
          }
        }
      }

         
                                                      
         
                                                                      
                                                                     
                                                                      
                                                                         
         
      for (j = 0; j < tbinfo->ncheck; j++)
      {
        ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

        if (constr->separate || (!constr->conislocal && !tbinfo->ispartition))
        {
          continue;
        }

        if (actual_atts == 0)
        {
          appendPQExpBufferStr(q, " (\n    ");
        }
        else
        {
          appendPQExpBufferStr(q, ",\n    ");
        }

        appendPQExpBuffer(q, "CONSTRAINT %s ", fmtId(constr->dobj.name));
        appendPQExpBufferStr(q, constr->condef);

        actual_atts++;
      }

      if (actual_atts)
      {
        appendPQExpBufferStr(q, "\n)");
      }
      else if (!(OidIsValid(tbinfo->reloftype) && !dopt->binary_upgrade))
      {
           
                                                                       
                                                                 
           
        appendPQExpBufferStr(q, " (\n)");
      }

         
                                                                  
                              
         
      if (numParents > 0 && !tbinfo->ispartition && !dopt->binary_upgrade)
      {
        appendPQExpBufferStr(q, "\nINHERITS (");
        for (k = 0; k < numParents; k++)
        {
          TableInfo *parentRel = parents[k];

          if (k > 0)
          {
            appendPQExpBufferStr(q, ", ");
          }
          appendPQExpBufferStr(q, fmtQualifiedDumpable(parentRel));
        }
        appendPQExpBufferChar(q, ')');
      }

      if (tbinfo->relkind == RELKIND_PARTITIONED_TABLE)
      {
        appendPQExpBuffer(q, "\nPARTITION BY %s", partkeydef);
      }

      if (tbinfo->relkind == RELKIND_FOREIGN_TABLE)
      {
        appendPQExpBuffer(q, "\nSERVER %s", fmtId(srvname));
      }
    }

    if (nonemptyReloptions(tbinfo->reloptions) || nonemptyReloptions(tbinfo->toast_reloptions))
    {
      bool addcomma = false;

      appendPQExpBufferStr(q, "\nWITH (");
      if (nonemptyReloptions(tbinfo->reloptions))
      {
        addcomma = true;
        appendReloptionsArrayAH(q, tbinfo->reloptions, "", fout);
      }
      if (nonemptyReloptions(tbinfo->toast_reloptions))
      {
        if (addcomma)
        {
          appendPQExpBufferStr(q, ", ");
        }
        appendReloptionsArrayAH(q, tbinfo->toast_reloptions, "toast.", fout);
      }
      appendPQExpBufferChar(q, ')');
    }

                                     
    if (ftoptions && ftoptions[0])
    {
      appendPQExpBuffer(q, "\nOPTIONS (\n    %s\n)", ftoptions);
    }

       
                                                                         
                                                             
       
    if (tbinfo->relkind == RELKIND_MATVIEW)
    {
      PQExpBuffer result;

      result = createViewAsClause(fout, tbinfo);
      appendPQExpBuffer(q, " AS\n%s\n  WITH NO DATA;\n", result->data);
      destroyPQExpBuffer(result);
    }
    else
    {
      appendPQExpBufferStr(q, ";\n");
    }

                                                     
    if (tbinfo->relkind == RELKIND_MATVIEW)
    {
      append_depends_on_extension(fout, q, &tbinfo->dobj, "pg_catalog.pg_class", tbinfo->relkind == RELKIND_MATVIEW ? "MATERIALIZED VIEW" : "INDEX", qualrelname);
    }

       
                                                                          
                              
       
    if (dopt->binary_upgrade)
    {
      for (j = 0; j < tbinfo->numatts; j++)
      {
        if (tbinfo->attmissingval[j][0] != '\0')
        {
          appendPQExpBufferStr(q, "\n-- set missing value.\n");
          appendPQExpBufferStr(q, "SELECT pg_catalog.binary_upgrade_set_missing_value(");
          appendStringLiteralAH(q, qualrelname, fout);
          appendPQExpBufferStr(q, "::pg_catalog.regclass,");
          appendStringLiteralAH(q, tbinfo->attnames[j], fout);
          appendPQExpBufferStr(q, ",");
          appendStringLiteralAH(q, tbinfo->attmissingval[j], fout);
          appendPQExpBufferStr(q, ");\n\n");
        }
      }
    }

       
                                                                          
                                                                   
                                                                           
                                                                    
                                                                       
                                                                    
                                                                         
                                                                          
                                                                         
                                                                          
                                                                        
       
                                                                        
                                                                      
                                                                        
                                                                          
                                                                     
                                                                          
                             
       
    if (dopt->binary_upgrade && (tbinfo->relkind == RELKIND_RELATION || tbinfo->relkind == RELKIND_FOREIGN_TABLE || tbinfo->relkind == RELKIND_PARTITIONED_TABLE))
    {
      for (j = 0; j < tbinfo->numatts; j++)
      {
        if (tbinfo->attisdropped[j])
        {
          appendPQExpBufferStr(q, "\n-- For binary upgrade, recreate dropped column.\n");
          appendPQExpBuffer(q,
              "UPDATE pg_catalog.pg_attribute\n"
              "SET attlen = %d, "
              "attalign = '%c', attbyval = false\n"
              "WHERE attname = ",
              tbinfo->attlen[j], tbinfo->attalign[j]);
          appendStringLiteralAH(q, tbinfo->attnames[j], fout);
          appendPQExpBufferStr(q, "\n  AND attrelid = ");
          appendStringLiteralAH(q, qualrelname, fout);
          appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");

          if (tbinfo->relkind == RELKIND_RELATION || tbinfo->relkind == RELKIND_PARTITIONED_TABLE)
          {
            appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
          }
          else
          {
            appendPQExpBuffer(q, "ALTER FOREIGN TABLE ONLY %s ", qualrelname);
          }
          appendPQExpBuffer(q, "DROP COLUMN %s;\n", fmtId(tbinfo->attnames[j]));
        }
        else if (!tbinfo->attislocal[j])
        {
          appendPQExpBufferStr(q, "\n-- For binary upgrade, recreate inherited column.\n");
          appendPQExpBufferStr(q, "UPDATE pg_catalog.pg_attribute\n"
                                  "SET attislocal = false\n"
                                  "WHERE attname = ");
          appendStringLiteralAH(q, tbinfo->attnames[j], fout);
          appendPQExpBufferStr(q, "\n  AND attrelid = ");
          appendStringLiteralAH(q, qualrelname, fout);
          appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");
        }
      }

         
                                                  
         
                                                                  
                              
         
      for (k = 0; k < tbinfo->ncheck; k++)
      {
        ConstraintInfo *constr = &(tbinfo->checkexprs[k]);

        if (constr->separate || constr->conislocal || tbinfo->ispartition)
        {
          continue;
        }

        appendPQExpBufferStr(q, "\n-- For binary upgrade, set up inherited constraint.\n");
        appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
        appendPQExpBuffer(q, " ADD CONSTRAINT %s ", fmtId(constr->dobj.name));
        appendPQExpBuffer(q, "%s;\n", constr->condef);
        appendPQExpBufferStr(q, "UPDATE pg_catalog.pg_constraint\n"
                                "SET conislocal = false\n"
                                "WHERE contype = 'c' AND conname = ");
        appendStringLiteralAH(q, constr->dobj.name, fout);
        appendPQExpBufferStr(q, "\n  AND conrelid = ");
        appendStringLiteralAH(q, qualrelname, fout);
        appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");
      }

      if (numParents > 0 && !tbinfo->ispartition)
      {
        appendPQExpBufferStr(q, "\n-- For binary upgrade, set up inheritance this way.\n");
        for (k = 0; k < numParents; k++)
        {
          TableInfo *parentRel = parents[k];

          appendPQExpBuffer(q, "ALTER TABLE ONLY %s INHERIT %s;\n", qualrelname, fmtQualifiedDumpable(parentRel));
        }
      }

      if (OidIsValid(tbinfo->reloftype))
      {
        appendPQExpBufferStr(q, "\n-- For binary upgrade, set up typed tables this way.\n");
        appendPQExpBuffer(q, "ALTER TABLE ONLY %s OF %s;\n", qualrelname, getFormattedTypeName(fout, tbinfo->reloftype, zeroAsOpaque));
      }
    }

       
                                                                       
                                                                          
                                                                          
                                                                          
                                                                   
       
    if (tbinfo->ispartition)
    {
      PGresult *ares;
      char *partbound;
      PQExpBuffer q2;

                                                        
      if (tbinfo->numParents != 1)
      {
        fatal("invalid number of parents %d for table \"%s\"", tbinfo->numParents, tbinfo->dobj.name);
      }

      q2 = createPQExpBuffer();

                                           
      appendPQExpBuffer(q2,
          "SELECT pg_get_expr(c.relpartbound, c.oid) "
          "FROM pg_class c "
          "WHERE c.oid = '%u'",
          tbinfo->dobj.catId.oid);
      ares = ExecuteSqlQueryForSingleRow(fout, q2->data);
      partbound = PQgetvalue(ares, 0, 0);

                                             
      appendPQExpBuffer(q, "ALTER TABLE ONLY %s ATTACH PARTITION %s %s;\n", fmtQualifiedDumpable(parents[0]), qualrelname, partbound);

      PQclear(ares);
      destroyPQExpBuffer(q2);
    }

       
                                                                           
                                                                          
                                                                          
                                                                          
                                            
       
    if (dopt->binary_upgrade && (tbinfo->relkind == RELKIND_RELATION || tbinfo->relkind == RELKIND_MATVIEW))
    {
      appendPQExpBufferStr(q, "\n-- For binary upgrade, set heap's relfrozenxid and relminmxid\n");
      appendPQExpBuffer(q,
          "UPDATE pg_catalog.pg_class\n"
          "SET relfrozenxid = '%u', relminmxid = '%u'\n"
          "WHERE oid = ",
          tbinfo->frozenxid, tbinfo->minmxid);
      appendStringLiteralAH(q, qualrelname, fout);
      appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");

      if (tbinfo->toast_oid)
      {
           
                                                                    
                                        
           
        appendPQExpBufferStr(q, "\n-- For binary upgrade, set toast's relfrozenxid and relminmxid\n");
        appendPQExpBuffer(q,
            "UPDATE pg_catalog.pg_class\n"
            "SET relfrozenxid = '%u', relminmxid = '%u'\n"
            "WHERE oid = '%u';\n",
            tbinfo->toast_frozenxid, tbinfo->toast_minmxid, tbinfo->toast_oid);
      }
    }

       
                                                                     
                                                                        
                                                                          
                                                                          
                                                                        
       
    if (dopt->binary_upgrade && tbinfo->relkind == RELKIND_MATVIEW && tbinfo->relispopulated)
    {
      appendPQExpBufferStr(q, "\n-- For binary upgrade, mark materialized view as populated\n");
      appendPQExpBufferStr(q, "UPDATE pg_catalog.pg_class\n"
                              "SET relispopulated = 't'\n"
                              "WHERE oid = ");
      appendStringLiteralAH(q, qualrelname, fout);
      appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");
    }

       
                                                                         
                                  
       
    for (j = 0; j < tbinfo->numatts; j++)
    {
                                                   
      if (tbinfo->attisdropped[j])
      {
        continue;
      }

         
                                                                       
                                                                         
                                        
         
      if (!shouldPrintColumn(dopt, tbinfo, j) && tbinfo->notnull[j] && !tbinfo->inhNotNull[j])
      {
        appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
        appendPQExpBuffer(q, "ALTER COLUMN %s SET NOT NULL;\n", fmtId(tbinfo->attnames[j]));
      }

         
                                                                        
                                                                       
                                                        
         
      if (tbinfo->attstattarget[j] >= 0)
      {
        appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
        appendPQExpBuffer(q, "ALTER COLUMN %s ", fmtId(tbinfo->attnames[j]));
        appendPQExpBuffer(q, "SET STATISTICS %d;\n", tbinfo->attstattarget[j]);
      }

         
                                                                     
                                                                         
         
      if (tbinfo->attstorage[j] != tbinfo->typstorage[j])
      {
        switch (tbinfo->attstorage[j])
        {
        case 'p':
          storage = "PLAIN";
          break;
        case 'e':
          storage = "EXTERNAL";
          break;
        case 'm':
          storage = "MAIN";
          break;
        case 'x':
          storage = "EXTENDED";
          break;
        default:
          storage = NULL;
        }

           
                                                                       
           
        if (storage != NULL)
        {
          appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
          appendPQExpBuffer(q, "ALTER COLUMN %s ", fmtId(tbinfo->attnames[j]));
          appendPQExpBuffer(q, "SET STORAGE %s;\n", storage);
        }
      }

         
                                     
         
      if (tbinfo->attoptions[j][0] != '\0')
      {
        appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
        appendPQExpBuffer(q, "ALTER COLUMN %s ", fmtId(tbinfo->attnames[j]));
        appendPQExpBuffer(q, "SET (%s);\n", tbinfo->attoptions[j]);
      }

         
                                      
         
      if (tbinfo->relkind == RELKIND_FOREIGN_TABLE && tbinfo->attfdwoptions[j][0] != '\0')
      {
        appendPQExpBuffer(q, "ALTER FOREIGN TABLE %s ", qualrelname);
        appendPQExpBuffer(q, "ALTER COLUMN %s ", fmtId(tbinfo->attnames[j]));
        appendPQExpBuffer(q, "OPTIONS (\n    %s\n);\n", tbinfo->attfdwoptions[j]);
      }
    }

    if (partkeydef)
    {
      free(partkeydef);
    }
    if (ftoptions)
    {
      free(ftoptions);
    }
    if (srvname)
    {
      free(srvname);
    }
  }

     
                                                         
     
  if ((tbinfo->relkind == RELKIND_RELATION || tbinfo->relkind == RELKIND_PARTITIONED_TABLE || tbinfo->relkind == RELKIND_MATVIEW) && tbinfo->relreplident != REPLICA_IDENTITY_DEFAULT)
  {
    if (tbinfo->relreplident == REPLICA_IDENTITY_INDEX)
    {
                                                               
    }
    else if (tbinfo->relreplident == REPLICA_IDENTITY_NOTHING)
    {
      appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY NOTHING;\n", qualrelname);
    }
    else if (tbinfo->relreplident == REPLICA_IDENTITY_FULL)
    {
      appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY FULL;\n", qualrelname);
    }
  }

  if (tbinfo->forcerowsec)
  {
    appendPQExpBuffer(q, "\nALTER TABLE ONLY %s FORCE ROW LEVEL SECURITY;\n", qualrelname);
  }

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(q, &tbinfo->dobj, reltypename, qrelname, tbinfo->dobj.namespace->dobj.name);
  }

  if (tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    char *tableam = NULL;

    if (tbinfo->relkind == RELKIND_RELATION || tbinfo->relkind == RELKIND_MATVIEW)
    {
      tableam = tbinfo->amname;
    }

    ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tbinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .tablespace = (tbinfo->relkind == RELKIND_VIEW) ? NULL : tbinfo->reltablespace, .tableam = tableam, .owner = tbinfo->rolname, .description = reltypename, .section = tbinfo->postponed_def ? SECTION_POST_DATA : SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                           
  if (tbinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpTableComment(fout, tbinfo, reltypename);
  }

                                  
  if (tbinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpTableSecLabel(fout, tbinfo, reltypename);
  }

                                                  
  for (j = 0; j < tbinfo->ncheck; j++)
  {
    ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

    if (constr->separate || !constr->conislocal)
    {
      continue;
    }

    if (tbinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
    {
      dumpTableConstraintComment(fout, constr);
    }
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qrelname);
  free(qualrelname);
}

   
                                                                 
   
static void
dumpAttrDef(Archive *fout, AttrDefInfo *adinfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = adinfo->adtable;
  int adnum = adinfo->adnum;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qualrelname;
  char *tag;

                                                 
  if (!tbinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

                                                                       
  if (!adinfo->separate)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qualrelname = pg_strdup(fmtQualifiedDumpable(tbinfo));

  appendPQExpBuffer(q, "ALTER TABLE ONLY %s ", qualrelname);
  appendPQExpBuffer(q, "ALTER COLUMN %s SET DEFAULT %s;\n", fmtId(tbinfo->attnames[adnum - 1]), adinfo->adef_expr);

  appendPQExpBuffer(delq, "ALTER TABLE %s ", qualrelname);
  appendPQExpBuffer(delq, "ALTER COLUMN %s DROP DEFAULT;\n", fmtId(tbinfo->attnames[adnum - 1]));

  tag = psprintf("%s %s", tbinfo->dobj.name, tbinfo->attnames[adnum - 1]);

  if (adinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, adinfo->dobj.catId, adinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "DEFAULT", .section = SECTION_PRE_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

  free(tag);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qualrelname);
}

   
                                                          
   
                                                                         
                                                                 
                                                                 
   
static const char *
getAttrName(int attrnum, TableInfo *tblInfo)
{
  if (attrnum > 0 && attrnum <= tblInfo->numatts)
  {
    return tblInfo->attnames[attrnum - 1];
  }
  switch (attrnum)
  {
  case SelfItemPointerAttributeNumber:
    return "ctid";
  case MinTransactionIdAttributeNumber:
    return "xmin";
  case MinCommandIdAttributeNumber:
    return "cmin";
  case MaxTransactionIdAttributeNumber:
    return "xmax";
  case MaxCommandIdAttributeNumber:
    return "cmax";
  case TableOidAttributeNumber:
    return "tableoid";
  }
  fatal("invalid column number %d for table \"%s\"", attrnum, tblInfo->dobj.name);
  return NULL;                          
}

   
             
                                            
   
static void
dumpIndex(Archive *fout, IndxInfo *indxinfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = indxinfo->indextable;
  bool is_constraint = (indxinfo->indexconstraint != 0);
  PQExpBuffer q;
  PQExpBuffer delq;
  char *qindxname;
  char *qqindxname;

  if (dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  qindxname = pg_strdup(fmtId(indxinfo->dobj.name));
  qqindxname = pg_strdup(fmtQualifiedDumpable(indxinfo));

     
                                                                           
                                                                            
                                                                       
                                                                             
                           
     
  if (!is_constraint)
  {
    char *indstatcols = indxinfo->indstatcols;
    char *indstatvals = indxinfo->indstatvals;
    char **indstatcolsarray = NULL;
    char **indstatvalsarray = NULL;
    int nstatcols;
    int nstatvals;

    if (dopt->binary_upgrade)
    {
      binary_upgrade_set_pg_class_oids(fout, q, indxinfo->dobj.catId.oid, true);
    }

                               
    appendPQExpBuffer(q, "%s;\n", indxinfo->indexdef);

       
                                                                       
                                                                     
                                       
       

                                                            
    if (indxinfo->indisclustered)
    {
      appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER", fmtQualifiedDumpable(tbinfo));
                                                      
      appendPQExpBuffer(q, " ON %s;\n", qindxname);
    }

       
                                                                        
                                           
       
    if (parsePGArray(indstatcols, &indstatcolsarray, &nstatcols) && parsePGArray(indstatvals, &indstatvalsarray, &nstatvals) && nstatcols == nstatvals)
    {
      int j;

      for (j = 0; j < nstatcols; j++)
      {
        appendPQExpBuffer(q, "ALTER INDEX %s ", qqindxname);

           
                                                                     
                 
           
        appendPQExpBuffer(q, "ALTER COLUMN %s ", indstatcolsarray[j]);
        appendPQExpBuffer(q, "SET STATISTICS %s;\n", indstatvalsarray[j]);
      }
    }

                                          
    append_depends_on_extension(fout, q, &indxinfo->dobj, "pg_catalog.pg_class", "INDEX", qqindxname);

                                                                
    if (indxinfo->indisreplident)
    {
      appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY USING", fmtQualifiedDumpable(tbinfo));
                                                      
      appendPQExpBuffer(q, " INDEX %s;\n", qindxname);
    }

    appendPQExpBuffer(delq, "DROP INDEX %s;\n", qqindxname);

    if (indxinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
    {
      ArchiveEntry(fout, indxinfo->dobj.catId, indxinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = indxinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .tablespace = indxinfo->tablespace, .owner = tbinfo->rolname, .description = "INDEX", .section = SECTION_POST_DATA, .createStmt = q->data, .dropStmt = delq->data));
    }

    if (indstatcolsarray)
    {
      free(indstatcolsarray);
    }
    if (indstatvalsarray)
    {
      free(indstatvalsarray);
    }
  }

                           
  if (indxinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "INDEX", qindxname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, indxinfo->dobj.catId, 0, is_constraint ? indxinfo->indexconstraint : indxinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  free(qindxname);
  free(qqindxname);
}

   
                   
                                                             
   
static void
dumpIndexAttach(Archive *fout, IndexAttachInfo *attachinfo)
{
  if (fout->dopt->dataOnly)
  {
    return;
  }

  if (attachinfo->partitionIdx->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    PQExpBuffer q = createPQExpBuffer();

    appendPQExpBuffer(q, "ALTER INDEX %s ", fmtQualifiedDumpable(attachinfo->parentIdx));
    appendPQExpBuffer(q, "ATTACH PARTITION %s;\n", fmtQualifiedDumpable(attachinfo->partitionIdx));

       
                                                                         
                                                           
                                                                    
                                                                        
                                                           
       
    ArchiveEntry(fout, attachinfo->dobj.catId, attachinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = attachinfo->dobj.name, .namespace = attachinfo->dobj.namespace->dobj.name, .owner = attachinfo->parentIdx->indextable->rolname, .description = "INDEX ATTACH", .section = SECTION_POST_DATA, .createStmt = q->data));

    destroyPQExpBuffer(q);
  }
}

   
                     
                                                     
   
static void
dumpStatisticsExt(Archive *fout, StatsExtInfo *statsextinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer q;
  PQExpBuffer delq;
  PQExpBuffer query;
  char *qstatsextname;
  PGresult *res;
  char *stxdef;

                                
  if (!statsextinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();
  query = createPQExpBuffer();

  qstatsextname = pg_strdup(fmtId(statsextinfo->dobj.name));

  appendPQExpBuffer(query,
      "SELECT "
      "pg_catalog.pg_get_statisticsobjdef('%u'::pg_catalog.oid)",
      statsextinfo->dobj.catId.oid);

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

  stxdef = PQgetvalue(res, 0, 0);

                                                                          
  appendPQExpBuffer(q, "%s;\n", stxdef);

  appendPQExpBuffer(delq, "DROP STATISTICS %s;\n", fmtQualifiedDumpable(statsextinfo));

  if (statsextinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, statsextinfo->dobj.catId, statsextinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = statsextinfo->dobj.name, .namespace = statsextinfo->dobj.namespace->dobj.name, .owner = statsextinfo->rolname, .description = "STATISTICS", .section = SECTION_POST_DATA, .createStmt = q->data, .dropStmt = delq->data));
  }

                                
  if (statsextinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "STATISTICS", qstatsextname, statsextinfo->dobj.namespace->dobj.name, statsextinfo->rolname, statsextinfo->dobj.catId, 0, statsextinfo->dobj.dumpId);
  }

  PQclear(res);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
  destroyPQExpBuffer(query);
  free(qstatsextname);
}

   
                  
                                                 
   
static void
dumpConstraint(Archive *fout, ConstraintInfo *coninfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = coninfo->contable;
  PQExpBuffer q;
  PQExpBuffer delq;
  char *tag = NULL;

                                
  if (!coninfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  q = createPQExpBuffer();
  delq = createPQExpBuffer();

  if (coninfo->contype == 'p' || coninfo->contype == 'u' || coninfo->contype == 'x')
  {
                                  
    IndxInfo *indxinfo;
    int k;

    indxinfo = (IndxInfo *)findObjectByDumpId(coninfo->conindex);

    if (indxinfo == NULL)
    {
      fatal("missing index for constraint \"%s\"", coninfo->dobj.name);
    }

    if (dopt->binary_upgrade)
    {
      binary_upgrade_set_pg_class_oids(fout, q, indxinfo->dobj.catId.oid, true);
    }

    appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n", fmtQualifiedDumpable(tbinfo));
    appendPQExpBuffer(q, "    ADD CONSTRAINT %s ", fmtId(coninfo->dobj.name));

    if (coninfo->condef)
    {
                                                                
      appendPQExpBuffer(q, "%s;\n", coninfo->condef);
    }
    else
    {
      appendPQExpBuffer(q, "%s (", coninfo->contype == 'p' ? "PRIMARY KEY" : "UNIQUE");
      for (k = 0; k < indxinfo->indnkeyattrs; k++)
      {
        int indkey = (int)indxinfo->indkeys[k];
        const char *attname;

        if (indkey == InvalidAttrNumber)
        {
          break;
        }
        attname = getAttrName(indkey, tbinfo);

        appendPQExpBuffer(q, "%s%s", (k == 0) ? "" : ", ", fmtId(attname));
      }

      if (indxinfo->indnkeyattrs < indxinfo->indnattrs)
      {
        appendPQExpBuffer(q, ") INCLUDE (");
      }

      for (k = indxinfo->indnkeyattrs; k < indxinfo->indnattrs; k++)
      {
        int indkey = (int)indxinfo->indkeys[k];
        const char *attname;

        if (indkey == InvalidAttrNumber)
        {
          break;
        }
        attname = getAttrName(indkey, tbinfo);

        appendPQExpBuffer(q, "%s%s", (k == indxinfo->indnkeyattrs) ? "" : ", ", fmtId(attname));
      }

      appendPQExpBufferChar(q, ')');

      if (nonemptyReloptions(indxinfo->indreloptions))
      {
        appendPQExpBufferStr(q, " WITH (");
        appendReloptionsArrayAH(q, indxinfo->indreloptions, "", fout);
        appendPQExpBufferChar(q, ')');
      }

      if (coninfo->condeferrable)
      {
        appendPQExpBufferStr(q, " DEFERRABLE");
        if (coninfo->condeferred)
        {
          appendPQExpBufferStr(q, " INITIALLY DEFERRED");
        }
      }

      appendPQExpBufferStr(q, ";\n");
    }

       
                                                                       
                                                                     
                                  
       

                                                            
    if (indxinfo->indisclustered)
    {
      appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER", fmtQualifiedDumpable(tbinfo));
                                                      
      appendPQExpBuffer(q, " ON %s;\n", fmtId(indxinfo->dobj.name));
    }

                                                                
    if (indxinfo->indisreplident)
    {
      appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY USING", fmtQualifiedDumpable(tbinfo));
                                                      
      appendPQExpBuffer(q, " INDEX %s;\n", fmtId(indxinfo->dobj.name));
    }

                                          
    append_depends_on_extension(fout, q, &indxinfo->dobj, "pg_catalog.pg_class", "INDEX", fmtQualifiedDumpable(indxinfo));

    appendPQExpBuffer(delq, "ALTER TABLE ONLY %s ", fmtQualifiedDumpable(tbinfo));
    appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n", fmtId(coninfo->dobj.name));

    tag = psprintf("%s %s", tbinfo->dobj.name, coninfo->dobj.name);

    if (coninfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
    {
      ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .tablespace = indxinfo->tablespace, .owner = tbinfo->rolname, .description = "CONSTRAINT", .section = SECTION_POST_DATA, .createStmt = q->data, .dropStmt = delq->data));
    }
  }
  else if (coninfo->contype == 'f')
  {
    char *only;

       
                                                                 
                                                                   
                                                                          
                                          
       
    only = tbinfo->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";

       
                                                                         
                                           
       
    appendPQExpBuffer(q, "ALTER TABLE %s%s\n", only, fmtQualifiedDumpable(tbinfo));
    appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n", fmtId(coninfo->dobj.name), coninfo->condef);

    appendPQExpBuffer(delq, "ALTER TABLE %s%s ", only, fmtQualifiedDumpable(tbinfo));
    appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n", fmtId(coninfo->dobj.name));

    tag = psprintf("%s %s", tbinfo->dobj.name, coninfo->dobj.name);

    if (coninfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
    {
      ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "FK CONSTRAINT", .section = SECTION_POST_DATA, .createStmt = q->data, .dropStmt = delq->data));
    }
  }
  else if (coninfo->contype == 'c' && tbinfo)
  {
                                     

                                                                       
    if (coninfo->separate && coninfo->conislocal)
    {
                                                              
      appendPQExpBuffer(q, "ALTER TABLE %s\n", fmtQualifiedDumpable(tbinfo));
      appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n", fmtId(coninfo->dobj.name), coninfo->condef);

      appendPQExpBuffer(delq, "ALTER TABLE %s ", fmtQualifiedDumpable(tbinfo));
      appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n", fmtId(coninfo->dobj.name));

      tag = psprintf("%s %s", tbinfo->dobj.name, coninfo->dobj.name);

      if (coninfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
      {
        ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "CHECK CONSTRAINT", .section = SECTION_POST_DATA, .createStmt = q->data, .dropStmt = delq->data));
      }
    }
  }
  else if (coninfo->contype == 'c' && tbinfo == NULL)
  {
                                      
    TypeInfo *tyinfo = coninfo->condomain;

                                               
    if (coninfo->separate)
    {
      appendPQExpBuffer(q, "ALTER DOMAIN %s\n", fmtQualifiedDumpable(tyinfo));
      appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n", fmtId(coninfo->dobj.name), coninfo->condef);

      appendPQExpBuffer(delq, "ALTER DOMAIN %s ", fmtQualifiedDumpable(tyinfo));
      appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n", fmtId(coninfo->dobj.name));

      tag = psprintf("%s %s", tyinfo->dobj.name, coninfo->dobj.name);

      if (coninfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
      {
        ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tyinfo->dobj.namespace->dobj.name, .owner = tyinfo->rolname, .description = "CHECK CONSTRAINT", .section = SECTION_POST_DATA, .createStmt = q->data, .dropStmt = delq->data));
      }
    }
  }
  else
  {
    fatal("unrecognized constraint type: %c", coninfo->contype);
  }

                                                                     
  if (tbinfo && coninfo->separate && coninfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpTableConstraintComment(fout, coninfo);
  }

  free(tag);
  destroyPQExpBuffer(q);
  destroyPQExpBuffer(delq);
}

   
                                                                     
   
                                                                          
                                                                         
                                   
   
static void
dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo)
{
  TableInfo *tbinfo = coninfo->contable;
  PQExpBuffer conprefix = createPQExpBuffer();
  char *qtabname;

  qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

  appendPQExpBuffer(conprefix, "CONSTRAINT %s ON", fmtId(coninfo->dobj.name));

  if (coninfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, conprefix->data, qtabname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, coninfo->dobj.catId, 0, coninfo->separate ? coninfo->dobj.dumpId : tbinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(conprefix);
  free(qtabname);
}

   
                            
   
                              
   
                                                                        
                                                                          
                                            
   
static Oid
findLastBuiltinOid_V71(Archive *fout)
{
  PGresult *res;
  Oid last_oid;

  res = ExecuteSqlQueryForSingleRow(fout, "SELECT datlastsysoid FROM pg_database WHERE datname = current_database()");
  last_oid = atooid(PQgetvalue(res, 0, PQfnumber(res, "datlastsysoid")));
  PQclear(res);

  return last_oid;
}

   
                
                                                                   
   
static void
dumpSequence(Archive *fout, TableInfo *tbinfo)
{
  DumpOptions *dopt = fout->dopt;
  PGresult *res;
  char *startv, *incby, *maxv, *minv, *cache, *seqtype;
  bool cycled;
  bool is_ascending;
  int64 default_minv, default_maxv;
  char bufm[32], bufx[32];
  PQExpBuffer query = createPQExpBuffer();
  PQExpBuffer delqry = createPQExpBuffer();
  char *qseqname;

  qseqname = pg_strdup(fmtId(tbinfo->dobj.name));

  if (fout->remoteVersion >= 100000)
  {
    appendPQExpBuffer(query,
        "SELECT format_type(seqtypid, NULL), "
        "seqstart, seqincrement, "
        "seqmax, seqmin, "
        "seqcache, seqcycle "
        "FROM pg_catalog.pg_sequence "
        "WHERE seqrelid = '%u'::oid",
        tbinfo->dobj.catId.oid);
  }
  else if (fout->remoteVersion >= 80400)
  {
       
                                                                          
       
                                                                 
                                                        
       
    appendPQExpBuffer(query,
        "SELECT 'bigint' AS sequence_type, "
        "start_value, increment_by, max_value, min_value, "
        "cache_value, is_cycled FROM %s",
        fmtQualifiedDumpable(tbinfo));
  }
  else
  {
    appendPQExpBuffer(query,
        "SELECT 'bigint' AS sequence_type, "
        "0 AS start_value, increment_by, max_value, min_value, "
        "cache_value, is_cycled FROM %s",
        fmtQualifiedDumpable(tbinfo));
  }

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  if (PQntuples(res) != 1)
  {
    pg_log_error(ngettext("query to get data of sequence \"%s\" returned %d row (expected 1)", "query to get data of sequence \"%s\" returned %d rows (expected 1)", PQntuples(res)), tbinfo->dobj.name, PQntuples(res));
    exit_nicely(1);
  }

  seqtype = PQgetvalue(res, 0, 0);
  startv = PQgetvalue(res, 0, 1);
  incby = PQgetvalue(res, 0, 2);
  maxv = PQgetvalue(res, 0, 3);
  minv = PQgetvalue(res, 0, 4);
  cache = PQgetvalue(res, 0, 5);
  cycled = (strcmp(PQgetvalue(res, 0, 6), "t") == 0);

                                                            
  is_ascending = (incby[0] != '-');
  if (strcmp(seqtype, "smallint") == 0)
  {
    default_minv = is_ascending ? 1 : PG_INT16_MIN;
    default_maxv = is_ascending ? PG_INT16_MAX : -1;
  }
  else if (strcmp(seqtype, "integer") == 0)
  {
    default_minv = is_ascending ? 1 : PG_INT32_MIN;
    default_maxv = is_ascending ? PG_INT32_MAX : -1;
  }
  else if (strcmp(seqtype, "bigint") == 0)
  {
    default_minv = is_ascending ? 1 : PG_INT64_MIN;
    default_maxv = is_ascending ? PG_INT64_MAX : -1;
  }
  else
  {
    fatal("unrecognized sequence type: %s", seqtype);
    default_minv = default_maxv = 0;                          
  }

     
                                                                           
                           
     
  snprintf(bufm, sizeof(bufm), INT64_FORMAT, default_minv);
  snprintf(bufx, sizeof(bufx), INT64_FORMAT, default_maxv);

                                                                        
  if (strcmp(minv, bufm) == 0)
  {
    minv = NULL;
  }
  if (strcmp(maxv, bufx) == 0)
  {
    maxv = NULL;
  }

     
                                                          
     
  if (!tbinfo->is_identity_sequence)
  {
    appendPQExpBuffer(delqry, "DROP SEQUENCE %s;\n", fmtQualifiedDumpable(tbinfo));
  }

  resetPQExpBuffer(query);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_set_pg_class_oids(fout, query, tbinfo->dobj.catId.oid, false);
    binary_upgrade_set_type_oids_by_rel_oid(fout, query, tbinfo->dobj.catId.oid);
  }

  if (tbinfo->is_identity_sequence)
  {
    TableInfo *owning_tab = findTableByOid(tbinfo->owning_tab);

    appendPQExpBuffer(query, "ALTER TABLE %s ", fmtQualifiedDumpable(owning_tab));
    appendPQExpBuffer(query, "ALTER COLUMN %s ADD GENERATED ", fmtId(owning_tab->attnames[tbinfo->owning_col - 1]));
    if (owning_tab->attidentity[tbinfo->owning_col - 1] == ATTRIBUTE_IDENTITY_ALWAYS)
    {
      appendPQExpBuffer(query, "ALWAYS");
    }
    else if (owning_tab->attidentity[tbinfo->owning_col - 1] == ATTRIBUTE_IDENTITY_BY_DEFAULT)
    {
      appendPQExpBuffer(query, "BY DEFAULT");
    }
    appendPQExpBuffer(query, " AS IDENTITY (\n    SEQUENCE NAME %s\n", fmtQualifiedDumpable(tbinfo));
  }
  else
  {
    appendPQExpBuffer(query, "CREATE SEQUENCE %s\n", fmtQualifiedDumpable(tbinfo));

    if (strcmp(seqtype, "bigint") != 0)
    {
      appendPQExpBuffer(query, "    AS %s\n", seqtype);
    }
  }

  if (fout->remoteVersion >= 80400)
  {
    appendPQExpBuffer(query, "    START WITH %s\n", startv);
  }

  appendPQExpBuffer(query, "    INCREMENT BY %s\n", incby);

  if (minv)
  {
    appendPQExpBuffer(query, "    MINVALUE %s\n", minv);
  }
  else
  {
    appendPQExpBufferStr(query, "    NO MINVALUE\n");
  }

  if (maxv)
  {
    appendPQExpBuffer(query, "    MAXVALUE %s\n", maxv);
  }
  else
  {
    appendPQExpBufferStr(query, "    NO MAXVALUE\n");
  }

  appendPQExpBuffer(query, "    CACHE %s%s", cache, (cycled ? "\n    CYCLE" : ""));

  if (tbinfo->is_identity_sequence)
  {
    appendPQExpBufferStr(query, "\n);\n");
  }
  else
  {
    appendPQExpBufferStr(query, ";\n");
  }

                                                        

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(query, &tbinfo->dobj, "SEQUENCE", qseqname, tbinfo->dobj.namespace->dobj.name);
  }

  if (tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tbinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "SEQUENCE", .section = SECTION_PRE_DATA, .createStmt = query->data, .dropStmt = delqry->data));
  }

     
                                                                            
                                                                             
                                                                     
                                                                          
                                                                      
                                                                         
                              
     
                                                                          
                                           
     
  if (OidIsValid(tbinfo->owning_tab) && !tbinfo->is_identity_sequence)
  {
    TableInfo *owning_tab = findTableByOid(tbinfo->owning_tab);

    if (owning_tab == NULL)
    {
      fatal("failed sanity check, parent table with OID %u of sequence with OID %u not found", tbinfo->owning_tab, tbinfo->dobj.catId.oid);
    }

    if (owning_tab->dobj.dump & DUMP_COMPONENT_DEFINITION)
    {
      resetPQExpBuffer(query);
      appendPQExpBuffer(query, "ALTER SEQUENCE %s", fmtQualifiedDumpable(tbinfo));
      appendPQExpBuffer(query, " OWNED BY %s", fmtQualifiedDumpable(owning_tab));
      appendPQExpBuffer(query, ".%s;\n", fmtId(owning_tab->attnames[tbinfo->owning_col - 1]));

      if (tbinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
      {
        ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tbinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "SEQUENCE OWNED BY", .section = SECTION_PRE_DATA, .createStmt = query->data, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1));
      }
    }
  }

                                                  
  if (tbinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "SEQUENCE", qseqname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, tbinfo->dobj.catId, 0, tbinfo->dobj.dumpId);
  }

  if (tbinfo->dobj.dump & DUMP_COMPONENT_SECLABEL)
  {
    dumpSecLabel(fout, "SEQUENCE", qseqname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, tbinfo->dobj.catId, 0, tbinfo->dobj.dumpId);
  }

  PQclear(res);

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(delqry);
  free(qseqname);
}

   
                    
                                                 
   
static void
dumpSequenceData(Archive *fout, TableDataInfo *tdinfo)
{
  TableInfo *tbinfo = tdinfo->tdtable;
  PGresult *res;
  char *last;
  bool called;
  PQExpBuffer query = createPQExpBuffer();

  appendPQExpBuffer(query, "SELECT last_value, is_called FROM %s", fmtQualifiedDumpable(tbinfo));

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  if (PQntuples(res) != 1)
  {
    pg_log_error(ngettext("query to get data of sequence \"%s\" returned %d row (expected 1)", "query to get data of sequence \"%s\" returned %d rows (expected 1)", PQntuples(res)), tbinfo->dobj.name, PQntuples(res));
    exit_nicely(1);
  }

  last = PQgetvalue(res, 0, 0);
  called = (strcmp(PQgetvalue(res, 0, 1), "t") == 0);

  resetPQExpBuffer(query);
  appendPQExpBufferStr(query, "SELECT pg_catalog.setval(");
  appendStringLiteralAH(query, fmtQualifiedDumpable(tbinfo), fout);
  appendPQExpBuffer(query, ", %s, %s);\n", last, (called ? "true" : "false"));

  if (tdinfo->dobj.dump & DUMP_COMPONENT_DATA)
  {
    ArchiveEntry(fout, nilCatalogId, createDumpId(), ARCHIVE_OPTS(.tag = tbinfo->dobj.name, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "SEQUENCE SET", .section = SECTION_DATA, .createStmt = query->data, .deps = &(tbinfo->dobj.dumpId), .nDeps = 1));
  }

  PQclear(res);

  destroyPQExpBuffer(query);
}

   
               
                                                             
   
static void
dumpTrigger(Archive *fout, TriggerInfo *tginfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = tginfo->tgtable;
  PQExpBuffer query;
  PQExpBuffer delqry;
  PQExpBuffer trigprefix;
  PQExpBuffer trigidentity;
  char *qtabname;
  char *tgargs;
  size_t lentgargs;
  const char *p;
  int findx;
  char *tag;

     
                                                                       
                                                          
     
  if (dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  delqry = createPQExpBuffer();
  trigprefix = createPQExpBuffer();
  trigidentity = createPQExpBuffer();

  qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

  appendPQExpBuffer(trigidentity, "%s ", fmtId(tginfo->dobj.name));
  appendPQExpBuffer(trigidentity, "ON %s", fmtQualifiedDumpable(tbinfo));

  appendPQExpBuffer(delqry, "DROP TRIGGER %s;\n", trigidentity->data);

  if (tginfo->tgdef)
  {
    appendPQExpBuffer(query, "%s;\n", tginfo->tgdef);
  }
  else
  {
    if (tginfo->tgisconstraint)
    {
      appendPQExpBufferStr(query, "CREATE CONSTRAINT TRIGGER ");
      appendPQExpBufferStr(query, fmtId(tginfo->tgconstrname));
    }
    else
    {
      appendPQExpBufferStr(query, "CREATE TRIGGER ");
      appendPQExpBufferStr(query, fmtId(tginfo->dobj.name));
    }
    appendPQExpBufferStr(query, "\n    ");

                      
    if (TRIGGER_FOR_BEFORE(tginfo->tgtype))
    {
      appendPQExpBufferStr(query, "BEFORE");
    }
    else if (TRIGGER_FOR_AFTER(tginfo->tgtype))
    {
      appendPQExpBufferStr(query, "AFTER");
    }
    else if (TRIGGER_FOR_INSTEAD(tginfo->tgtype))
    {
      appendPQExpBufferStr(query, "INSTEAD OF");
    }
    else
    {
      pg_log_error("unexpected tgtype value: %d", tginfo->tgtype);
      exit_nicely(1);
    }

    findx = 0;
    if (TRIGGER_FOR_INSERT(tginfo->tgtype))
    {
      appendPQExpBufferStr(query, " INSERT");
      findx++;
    }
    if (TRIGGER_FOR_DELETE(tginfo->tgtype))
    {
      if (findx > 0)
      {
        appendPQExpBufferStr(query, " OR DELETE");
      }
      else
      {
        appendPQExpBufferStr(query, " DELETE");
      }
      findx++;
    }
    if (TRIGGER_FOR_UPDATE(tginfo->tgtype))
    {
      if (findx > 0)
      {
        appendPQExpBufferStr(query, " OR UPDATE");
      }
      else
      {
        appendPQExpBufferStr(query, " UPDATE");
      }
      findx++;
    }
    if (TRIGGER_FOR_TRUNCATE(tginfo->tgtype))
    {
      if (findx > 0)
      {
        appendPQExpBufferStr(query, " OR TRUNCATE");
      }
      else
      {
        appendPQExpBufferStr(query, " TRUNCATE");
      }
      findx++;
    }
    appendPQExpBuffer(query, " ON %s\n", fmtQualifiedDumpable(tbinfo));

    if (tginfo->tgisconstraint)
    {
      if (OidIsValid(tginfo->tgconstrrelid))
      {
                                               
        appendPQExpBuffer(query, "    FROM %s\n    ", tginfo->tgconstrrelname);
      }
      if (!tginfo->tgdeferrable)
      {
        appendPQExpBufferStr(query, "NOT ");
      }
      appendPQExpBufferStr(query, "DEFERRABLE INITIALLY ");
      if (tginfo->tginitdeferred)
      {
        appendPQExpBufferStr(query, "DEFERRED\n");
      }
      else
      {
        appendPQExpBufferStr(query, "IMMEDIATE\n");
      }
    }

    if (TRIGGER_FOR_ROW(tginfo->tgtype))
    {
      appendPQExpBufferStr(query, "    FOR EACH ROW\n    ");
    }
    else
    {
      appendPQExpBufferStr(query, "    FOR EACH STATEMENT\n    ");
    }

                                                       
    appendPQExpBuffer(query, "EXECUTE FUNCTION %s(", tginfo->tgfname);

    tgargs = (char *)PQunescapeBytea((unsigned char *)tginfo->tgargs, &lentgargs);
    p = tgargs;
    for (findx = 0; findx < tginfo->tgnargs; findx++)
    {
                                                                        
      size_t tlen = strlen(p);

      if (p + tlen >= tgargs + lentgargs)
      {
                                                        
        pg_log_error("invalid argument string (%s) for trigger \"%s\" on table \"%s\"", tginfo->tgargs, tginfo->dobj.name, tbinfo->dobj.name);
        exit_nicely(1);
      }

      if (findx > 0)
      {
        appendPQExpBufferStr(query, ", ");
      }
      appendStringLiteralAH(query, p, fout);
      p += tlen + 1;
    }
    free(tgargs);
    appendPQExpBufferStr(query, ");\n");
  }

                                         
  append_depends_on_extension(fout, query, &tginfo->dobj, "pg_catalog.pg_trigger", "TRIGGER", trigidentity->data);

  if (tginfo->tgisinternal)
  {
       
                                                                           
                                                                           
                                                                       
                                                                        
       
    resetPQExpBuffer(query);
    resetPQExpBuffer(delqry);
    appendPQExpBuffer(query, "\nALTER %sTABLE %s ", tbinfo->relkind == RELKIND_FOREIGN_TABLE ? "FOREIGN " : "", fmtQualifiedDumpable(tbinfo));
    switch (tginfo->tgenabled)
    {
    case 'f':
    case 'D':
      appendPQExpBufferStr(query, "DISABLE");
      break;
    case 't':
    case 'O':
      appendPQExpBufferStr(query, "ENABLE");
      break;
    case 'R':
      appendPQExpBufferStr(query, "ENABLE REPLICA");
      break;
    case 'A':
      appendPQExpBufferStr(query, "ENABLE ALWAYS");
      break;
    }
    appendPQExpBuffer(query, " TRIGGER %s;\n", fmtId(tginfo->dobj.name));
  }
  else if (tginfo->tgenabled != 't' && tginfo->tgenabled != 'O')
  {
    appendPQExpBuffer(query, "\nALTER TABLE %s ", fmtQualifiedDumpable(tbinfo));
    switch (tginfo->tgenabled)
    {
    case 'D':
    case 'f':
      appendPQExpBufferStr(query, "DISABLE");
      break;
    case 'A':
      appendPQExpBufferStr(query, "ENABLE ALWAYS");
      break;
    case 'R':
      appendPQExpBufferStr(query, "ENABLE REPLICA");
      break;
    default:
      appendPQExpBufferStr(query, "ENABLE");
      break;
    }
    appendPQExpBuffer(query, " TRIGGER %s;\n", fmtId(tginfo->dobj.name));
  }

  appendPQExpBuffer(trigprefix, "TRIGGER %s ON", fmtId(tginfo->dobj.name));

  tag = psprintf("%s %s", tbinfo->dobj.name, tginfo->dobj.name);

  if (tginfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, tginfo->dobj.catId, tginfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "TRIGGER", .section = SECTION_POST_DATA, .createStmt = query->data, .dropStmt = delqry->data));
  }

  if (tginfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, trigprefix->data, qtabname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, tginfo->dobj.catId, 0, tginfo->dobj.dumpId);
  }

  free(tag);
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(delqry);
  destroyPQExpBuffer(trigprefix);
  destroyPQExpBuffer(trigidentity);
  free(qtabname);
}

   
                    
                                                             
   
static void
dumpEventTrigger(Archive *fout, EventTriggerInfo *evtinfo)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PQExpBuffer delqry;
  char *qevtname;

                                
  if (!evtinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

  query = createPQExpBuffer();
  delqry = createPQExpBuffer();

  qevtname = pg_strdup(fmtId(evtinfo->dobj.name));

  appendPQExpBufferStr(query, "CREATE EVENT TRIGGER ");
  appendPQExpBufferStr(query, qevtname);
  appendPQExpBufferStr(query, " ON ");
  appendPQExpBufferStr(query, fmtId(evtinfo->evtevent));

  if (strcmp("", evtinfo->evttags) != 0)
  {
    appendPQExpBufferStr(query, "\n         WHEN TAG IN (");
    appendPQExpBufferStr(query, evtinfo->evttags);
    appendPQExpBufferChar(query, ')');
  }

  appendPQExpBufferStr(query, "\n   EXECUTE FUNCTION ");
  appendPQExpBufferStr(query, evtinfo->evtfname);
  appendPQExpBufferStr(query, "();\n");

  if (evtinfo->evtenabled != 'O')
  {
    appendPQExpBuffer(query, "\nALTER EVENT TRIGGER %s ", qevtname);
    switch (evtinfo->evtenabled)
    {
    case 'D':
      appendPQExpBufferStr(query, "DISABLE");
      break;
    case 'A':
      appendPQExpBufferStr(query, "ENABLE ALWAYS");
      break;
    case 'R':
      appendPQExpBufferStr(query, "ENABLE REPLICA");
      break;
    default:
      appendPQExpBufferStr(query, "ENABLE");
      break;
    }
    appendPQExpBufferStr(query, ";\n");
  }

  appendPQExpBuffer(delqry, "DROP EVENT TRIGGER %s;\n", qevtname);

  if (dopt->binary_upgrade)
  {
    binary_upgrade_extension_member(query, &evtinfo->dobj, "EVENT TRIGGER", qevtname, NULL);
  }

  if (evtinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, evtinfo->dobj.catId, evtinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = evtinfo->dobj.name, .owner = evtinfo->evtowner, .description = "EVENT TRIGGER", .section = SECTION_POST_DATA, .createStmt = query->data, .dropStmt = delqry->data));
  }

  if (evtinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, "EVENT TRIGGER", qevtname, NULL, evtinfo->evtowner, evtinfo->dobj.catId, 0, evtinfo->dobj.dumpId);
  }

  destroyPQExpBuffer(query);
  destroyPQExpBuffer(delqry);
  free(qevtname);
}

   
            
                
   
static void
dumpRule(Archive *fout, RuleInfo *rinfo)
{
  DumpOptions *dopt = fout->dopt;
  TableInfo *tbinfo = rinfo->ruletable;
  bool is_view;
  PQExpBuffer query;
  PQExpBuffer cmd;
  PQExpBuffer delcmd;
  PQExpBuffer ruleprefix;
  char *qtabname;
  PGresult *res;
  char *tag;

                                
  if (!rinfo->dobj.dump || dopt->dataOnly)
  {
    return;
  }

     
                                                                           
                                                     
     
  if (!rinfo->separate)
  {
    return;
  }

     
                                                                          
                        
     
  is_view = (rinfo->ev_type == '1' && rinfo->is_instead);

  query = createPQExpBuffer();
  cmd = createPQExpBuffer();
  delcmd = createPQExpBuffer();
  ruleprefix = createPQExpBuffer();

  qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

  if (is_view)
  {
    PQExpBuffer result;

       
                                                                        
                                                                           
       
    appendPQExpBuffer(cmd, "CREATE OR REPLACE VIEW %s", fmtQualifiedDumpable(tbinfo));
    if (nonemptyReloptions(tbinfo->reloptions))
    {
      appendPQExpBufferStr(cmd, " WITH (");
      appendReloptionsArrayAH(cmd, tbinfo->reloptions, "", fout);
      appendPQExpBufferChar(cmd, ')');
    }
    result = createViewAsClause(fout, tbinfo);
    appendPQExpBuffer(cmd, " AS\n%s", result->data);
    destroyPQExpBuffer(result);
    if (tbinfo->checkoption != NULL)
    {
      appendPQExpBuffer(cmd, "\n  WITH %s CHECK OPTION", tbinfo->checkoption);
    }
    appendPQExpBufferStr(cmd, ";\n");
  }
  else
  {
                                                                       
    appendPQExpBuffer(query, "SELECT pg_catalog.pg_get_ruledef('%u'::pg_catalog.oid)", rinfo->dobj.catId.oid);

    res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

    if (PQntuples(res) != 1)
    {
      pg_log_error("query to get rule \"%s\" for table \"%s\" failed: wrong number of rows returned", rinfo->dobj.name, tbinfo->dobj.name);
      exit_nicely(1);
    }

    printfPQExpBuffer(cmd, "%s\n", PQgetvalue(res, 0, 0));

    PQclear(res);
  }

     
                                                                           
                               
     
  if (rinfo->ev_enabled != 'O')
  {
    appendPQExpBuffer(cmd, "ALTER TABLE %s ", fmtQualifiedDumpable(tbinfo));
    switch (rinfo->ev_enabled)
    {
    case 'A':
      appendPQExpBuffer(cmd, "ENABLE ALWAYS RULE %s;\n", fmtId(rinfo->dobj.name));
      break;
    case 'R':
      appendPQExpBuffer(cmd, "ENABLE REPLICA RULE %s;\n", fmtId(rinfo->dobj.name));
      break;
    case 'D':
      appendPQExpBuffer(cmd, "DISABLE RULE %s;\n", fmtId(rinfo->dobj.name));
      break;
    }
  }

  if (is_view)
  {
       
                                                                      
                                                                    
                     
       
    PQExpBuffer result;

    appendPQExpBuffer(delcmd, "CREATE OR REPLACE VIEW %s", fmtQualifiedDumpable(tbinfo));
    result = createDummyViewAsClause(fout, tbinfo);
    appendPQExpBuffer(delcmd, " AS\n%s;\n", result->data);
    destroyPQExpBuffer(result);
  }
  else
  {
    appendPQExpBuffer(delcmd, "DROP RULE %s ", fmtId(rinfo->dobj.name));
    appendPQExpBuffer(delcmd, "ON %s;\n", fmtQualifiedDumpable(tbinfo));
  }

  appendPQExpBuffer(ruleprefix, "RULE %s ON", fmtId(rinfo->dobj.name));

  tag = psprintf("%s %s", tbinfo->dobj.name, rinfo->dobj.name);

  if (rinfo->dobj.dump & DUMP_COMPONENT_DEFINITION)
  {
    ArchiveEntry(fout, rinfo->dobj.catId, rinfo->dobj.dumpId, ARCHIVE_OPTS(.tag = tag, .namespace = tbinfo->dobj.namespace->dobj.name, .owner = tbinfo->rolname, .description = "RULE", .section = SECTION_POST_DATA, .createStmt = cmd->data, .dropStmt = delcmd->data));
  }

                          
  if (rinfo->dobj.dump & DUMP_COMPONENT_COMMENT)
  {
    dumpComment(fout, ruleprefix->data, qtabname, tbinfo->dobj.namespace->dobj.name, tbinfo->rolname, rinfo->dobj.catId, 0, rinfo->dobj.dumpId);
  }

  free(tag);
  destroyPQExpBuffer(query);
  destroyPQExpBuffer(cmd);
  destroyPQExpBuffer(delcmd);
  destroyPQExpBuffer(ruleprefix);
  free(qtabname);
}

   
                                                               
   
                                                                             
                                                                              
                                                                            
                                                                       
                                                                               
                 
   
void
getExtensionMembership(Archive *fout, ExtensionInfo extinfo[], int numExtensions)
{
  PQExpBuffer query;
  PGresult *res;
  int ntups, nextmembers, i;
  int i_classid, i_objid, i_refobjid;
  ExtensionMemberId *extmembers;
  ExtensionInfo *ext;

                                      
  if (numExtensions == 0)
  {
    return;
  }

  query = createPQExpBuffer();

                                                                   
  appendPQExpBufferStr(query, "SELECT "
                              "classid, objid, refobjid "
                              "FROM pg_depend "
                              "WHERE refclassid = 'pg_extension'::regclass "
                              "AND deptype = 'e' "
                              "ORDER BY 3");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_classid = PQfnumber(res, "classid");
  i_objid = PQfnumber(res, "objid");
  i_refobjid = PQfnumber(res, "refobjid");

  extmembers = (ExtensionMemberId *)pg_malloc(ntups * sizeof(ExtensionMemberId));
  nextmembers = 0;

     
                                        
     
                                                                      
                                                                        
                        
     
  ext = NULL;

  for (i = 0; i < ntups; i++)
  {
    CatalogId objId;
    Oid extId;

    objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
    objId.oid = atooid(PQgetvalue(res, i, i_objid));
    extId = atooid(PQgetvalue(res, i, i_refobjid));

    if (ext == NULL || ext->dobj.catId.oid != extId)
    {
      ext = findExtensionByOid(extId);
    }

    if (ext == NULL)
    {
                            
      pg_log_warning("could not find referenced extension %u", extId);
      continue;
    }

    extmembers[nextmembers].catId = objId;
    extmembers[nextmembers].ext = ext;
    nextmembers++;
  }

  PQclear(res);

                                       
  setExtensionMembership(extmembers, nextmembers);

  destroyPQExpBuffer(query);
}

   
                                                                       
   
                                        
   
                                                                           
   
                                                                              
                                                                              
                                                                            
                                                                               
                                       
   
                                                           
   
                                                                                
                                                                               
                                                                                
                                                                              
                                                                                
                                                                                
                                                                 
   
void
processExtensionTables(Archive *fout, ExtensionInfo extinfo[], int numExtensions)
{
  DumpOptions *dopt = fout->dopt;
  PQExpBuffer query;
  PGresult *res;
  int ntups, i;
  int i_conrelid, i_confrelid;

                                      
  if (numExtensions == 0)
  {
    return;
  }

     
                                                                      
                                                                          
                                 
     
                                                                            
                                                                          
                                                                     
                                   
     
  for (i = 0; i < numExtensions; i++)
  {
    ExtensionInfo *curext = &(extinfo[i]);
    char *extconfig = curext->extconfig;
    char *extcondition = curext->extcondition;
    char **extconfigarray = NULL;
    char **extconditionarray = NULL;
    int nconfigitems;
    int nconditionitems;

    if (parsePGArray(extconfig, &extconfigarray, &nconfigitems) && parsePGArray(extcondition, &extconditionarray, &nconditionitems) && nconfigitems == nconditionitems)
    {
      int j;

      for (j = 0; j < nconfigitems; j++)
      {
        TableInfo *configtbl;
        Oid configtbloid = atooid(extconfigarray[j]);
        bool dumpobj = curext->dobj.dump & DUMP_COMPONENT_DEFINITION;

        configtbl = findTableByOid(configtbloid);
        if (configtbl == NULL)
        {
          continue;
        }

           
                                                                     
                                                                 
           
        if (!(curext->dobj.dump & DUMP_COMPONENT_DEFINITION))
        {
                                                
          if (table_include_oids.head != NULL && simple_oid_list_member(&table_include_oids, configtbloid))
          {
            dumpobj = true;
          }

                                                         
          if (configtbl->dobj.namespace->dobj.dump & DUMP_COMPONENT_DATA)
          {
            dumpobj = true;
          }
        }

                                                         
        if (table_exclude_oids.head != NULL && simple_oid_list_member(&table_exclude_oids, configtbloid))
        {
          dumpobj = false;
        }

                                                          
        if (simple_oid_list_member(&schema_exclude_oids, configtbl->dobj.namespace->dobj.catId.oid))
        {
          dumpobj = false;
        }

        if (dumpobj)
        {
          makeTableDataInfo(dopt, configtbl);
          if (configtbl->dataObj != NULL)
          {
            if (strlen(extconditionarray[j]) > 0)
            {
              configtbl->dataObj->filtercond = pg_strdup(extconditionarray[j]);
            }
          }
        }
      }
    }
    if (extconfigarray)
    {
      free(extconfigarray);
    }
    if (extconditionarray)
    {
      free(extconditionarray);
    }
  }

     
                                                                          
                                                                          
                                                                 
     
                                                                      
                                               
     

  query = createPQExpBuffer();

  printfPQExpBuffer(query, "SELECT conrelid, confrelid "
                           "FROM pg_constraint "
                           "JOIN pg_depend ON (objid = confrelid) "
                           "WHERE contype = 'f' "
                           "AND refclassid = 'pg_extension'::regclass "
                           "AND classid = 'pg_class'::regclass;");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
  ntups = PQntuples(res);

  i_conrelid = PQfnumber(res, "conrelid");
  i_confrelid = PQfnumber(res, "confrelid");

                                                  
  for (i = 0; i < ntups; i++)
  {
    Oid conrelid, confrelid;
    TableInfo *reftable, *contable;

    conrelid = atooid(PQgetvalue(res, i, i_conrelid));
    confrelid = atooid(PQgetvalue(res, i, i_confrelid));
    contable = findTableByOid(conrelid);
    reftable = findTableByOid(confrelid);

    if (reftable == NULL || reftable->dataObj == NULL || contable == NULL || contable->dataObj == NULL)
    {
      continue;
    }

       
                                                                           
                          
       
    addObjectDependency(&contable->dataObj->dobj, reftable->dataObj->dobj.dumpId);
  }
  PQclear(res);
  destroyPQExpBuffer(query);
}

   
                                                        
   
static void
getDependencies(Archive *fout)
{
  PQExpBuffer query;
  PGresult *res;
  int ntups, i;
  int i_classid, i_objid, i_refclassid, i_refobjid, i_deptype;
  DumpableObject *dobj, *refdobj;

  pg_log_info("reading dependency data");

  query = createPQExpBuffer();

     
                                                                       
                                                                          
                                                           
     
                                                                          
                                                  
     
  appendPQExpBufferStr(query, "SELECT "
                              "classid, objid, refclassid, refobjid, deptype "
                              "FROM pg_depend "
                              "WHERE deptype != 'p' AND deptype != 'e'\n");

     
                                                                          
                                                                            
                                                                            
                                                                          
                                                                         
                                                                       
                                                                         
                                                                           
     
                                                                            
                                                                            
                         
     
  if (fout->remoteVersion >= 80300)
  {
    appendPQExpBufferStr(query, "UNION ALL\n"
                                "SELECT 'pg_opfamily'::regclass AS classid, amopfamily AS objid, refclassid, refobjid, deptype "
                                "FROM pg_depend d, pg_amop o "
                                "WHERE deptype NOT IN ('p', 'e', 'i') AND "
                                "classid = 'pg_amop'::regclass AND objid = o.oid "
                                "AND NOT (refclassid = 'pg_opfamily'::regclass AND amopfamily = refobjid)\n");

                                        
    appendPQExpBufferStr(query, "UNION ALL\n"
                                "SELECT 'pg_opfamily'::regclass AS classid, amprocfamily AS objid, refclassid, refobjid, deptype "
                                "FROM pg_depend d, pg_amproc p "
                                "WHERE deptype NOT IN ('p', 'e', 'i') AND "
                                "classid = 'pg_amproc'::regclass AND objid = p.oid "
                                "AND NOT (refclassid = 'pg_opfamily'::regclass AND amprocfamily = refobjid)\n");
  }

                                            
  appendPQExpBufferStr(query, "ORDER BY 1,2");

  res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

  ntups = PQntuples(res);

  i_classid = PQfnumber(res, "classid");
  i_objid = PQfnumber(res, "objid");
  i_refclassid = PQfnumber(res, "refclassid");
  i_refobjid = PQfnumber(res, "refobjid");
  i_deptype = PQfnumber(res, "deptype");

     
                                                                       
                                                                           
                  
     
  dobj = NULL;

  for (i = 0; i < ntups; i++)
  {
    CatalogId objId;
    CatalogId refobjId;
    char deptype;

    objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
    objId.oid = atooid(PQgetvalue(res, i, i_objid));
    refobjId.tableoid = atooid(PQgetvalue(res, i, i_refclassid));
    refobjId.oid = atooid(PQgetvalue(res, i, i_refobjid));
    deptype = *(PQgetvalue(res, i, i_deptype));

    if (dobj == NULL || dobj->catId.tableoid != objId.tableoid || dobj->catId.oid != objId.oid)
    {
      dobj = findObjectByCatalogId(objId);
    }

       
                                                                         
                                                                   
       
    if (dobj == NULL)
    {
#ifdef NOT_USED
      pg_log_warning("no referencing object %u %u", objId.tableoid, objId.oid);
#endif
      continue;
    }

    refdobj = findObjectByCatalogId(refobjId);

    if (refdobj == NULL)
    {
#ifdef NOT_USED
      pg_log_warning("no referenced object %u %u", refobjId.tableoid, refobjId.oid);
#endif
      continue;
    }

       
                                                                         
                                                                     
                                                                        
                                                              
       
    if (deptype == 'x')
    {
      dobj->depends_on_ext = true;
    }

       
                                                                      
                                                                           
                                                                         
                                                                          
                                                         
       
    if (deptype == 'i' && dobj->objType == DO_TABLE && refdobj->objType == DO_TYPE)
    {
      addObjectDependency(refdobj, dobj->dumpId);
    }
    else
    {
                       
      addObjectDependency(dobj, refdobj->dumpId);
    }
  }

  PQclear(res);

  destroyPQExpBuffer(query);
}

   
                                                                     
                            
   
static DumpableObject *
createBoundaryObjects(void)
{
  DumpableObject *dobjs;

  dobjs = (DumpableObject *)pg_malloc(2 * sizeof(DumpableObject));

  dobjs[0].objType = DO_PRE_DATA_BOUNDARY;
  dobjs[0].catId = nilCatalogId;
  AssignDumpId(dobjs + 0);
  dobjs[0].name = pg_strdup("PRE-DATA BOUNDARY");

  dobjs[1].objType = DO_POST_DATA_BOUNDARY;
  dobjs[1].catId = nilCatalogId;
  AssignDumpId(dobjs + 1);
  dobjs[1].name = pg_strdup("POST-DATA BOUNDARY");

  return dobjs;
}

   
                                                                            
                       
   
static void
addBoundaryDependencies(DumpableObject **dobjs, int numObjs, DumpableObject *boundaryObjs)
{
  DumpableObject *preDataBound = boundaryObjs + 0;
  DumpableObject *postDataBound = boundaryObjs + 1;
  int i;

  for (i = 0; i < numObjs; i++)
  {
    DumpableObject *dobj = dobjs[i];

       
                                                                          
                                                             
       
    switch (dobj->objType)
    {
    case DO_NAMESPACE:
    case DO_EXTENSION:
    case DO_TYPE:
    case DO_SHELL_TYPE:
    case DO_FUNC:
    case DO_AGG:
    case DO_OPERATOR:
    case DO_ACCESS_METHOD:
    case DO_OPCLASS:
    case DO_OPFAMILY:
    case DO_COLLATION:
    case DO_CONVERSION:
    case DO_TABLE:
    case DO_ATTRDEF:
    case DO_PROCLANG:
    case DO_CAST:
    case DO_DUMMY_TYPE:
    case DO_TSPARSER:
    case DO_TSDICT:
    case DO_TSTEMPLATE:
    case DO_TSCONFIG:
    case DO_FDW:
    case DO_FOREIGN_SERVER:
    case DO_TRANSFORM:
    case DO_BLOB:
                                                                    
      addObjectDependency(preDataBound, dobj->dumpId);
      break;
    case DO_TABLE_DATA:
    case DO_SEQUENCE_SET:
    case DO_BLOB_DATA:
                                                          
      addObjectDependency(dobj, preDataBound->dumpId);
      addObjectDependency(postDataBound, dobj->dumpId);
      break;
    case DO_INDEX:
    case DO_INDEX_ATTACH:
    case DO_STATSEXT:
    case DO_REFRESH_MATVIEW:
    case DO_TRIGGER:
    case DO_EVENT_TRIGGER:
    case DO_DEFAULT_ACL:
    case DO_POLICY:
    case DO_PUBLICATION:
    case DO_PUBLICATION_REL:
    case DO_SUBSCRIPTION:
                                                                     
      addObjectDependency(dobj, postDataBound->dumpId);
      break;
    case DO_RULE:
                                                              
      if (((RuleInfo *)dobj)->separate)
      {
        addObjectDependency(dobj, postDataBound->dumpId);
      }
      break;
    case DO_CONSTRAINT:
    case DO_FK_CONSTRAINT:
                                                                    
      if (((ConstraintInfo *)dobj)->separate)
      {
        addObjectDependency(dobj, postDataBound->dumpId);
      }
      break;
    case DO_PRE_DATA_BOUNDARY:
                         
      break;
    case DO_POST_DATA_BOUNDARY:
                                                 
      addObjectDependency(dobj, preDataBound->dumpId);
      break;
    }
  }
}

   
                                                                             
   
                                                                         
                                                                         
                                                                            
                                                                              
                                                                               
                                                                             
                                                              
   
                                                                               
                                                                           
                                                                          
                                                                            
                                                   
   
                                                                             
                                                                        
                                                                           
                                                                             
                     
   
static void
BuildArchiveDependencies(Archive *fout)
{
  ArchiveHandle *AH = (ArchiveHandle *)fout;
  TocEntry *te;

                                           
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    DumpableObject *dobj;
    DumpId *dependencies;
    int nDeps;
    int allocDeps;

                                                            
    if (te->reqs == 0)
    {
      continue;
    }
                                                                 
    if (te->nDeps > 0)
    {
      continue;
    }
                                                                       
    dobj = findObjectByDumpId(te->dumpId);
    if (dobj == NULL)
    {
      continue;
    }
                                           
    if (dobj->nDeps <= 0)
    {
      continue;
    }
                           
    allocDeps = 64;
    dependencies = (DumpId *)pg_malloc(allocDeps * sizeof(DumpId));
    nDeps = 0;
                                                    
    findDumpableDependencies(AH, dobj, &dependencies, &nDeps, &allocDeps);
                          
    if (nDeps > 0)
    {
      dependencies = (DumpId *)pg_realloc(dependencies, nDeps * sizeof(DumpId));
      te->dependencies = dependencies;
      te->nDeps = nDeps;
    }
    else
    {
      free(dependencies);
    }
  }
}

                                                              
static void
findDumpableDependencies(ArchiveHandle *AH, DumpableObject *dobj, DumpId **dependencies, int *nDeps, int *allocDeps)
{
  int i;

     
                                                                       
                                        
     
  if (dobj->objType == DO_PRE_DATA_BOUNDARY || dobj->objType == DO_POST_DATA_BOUNDARY)
  {
    return;
  }

  for (i = 0; i < dobj->nDeps; i++)
  {
    DumpId depid = dobj->dependencies[i];

    if (TocIDRequired(AH, depid) != 0)
    {
                                                                       
      if (*nDeps >= *allocDeps)
      {
        *allocDeps *= 2;
        *dependencies = (DumpId *)pg_realloc(*dependencies, *allocDeps * sizeof(DumpId));
      }
      (*dependencies)[*nDeps] = depid;
      (*nDeps)++;
    }
    else
    {
         
                                                                         
                                                                       
                                                                 
         
      DumpableObject *otherdobj = findObjectByDumpId(depid);

      if (otherdobj)
      {
        findDumpableDependencies(AH, otherdobj, dependencies, nDeps, allocDeps);
      }
    }
  }
}

   
                                                                        
                   
   
                                                                          
                                                                          
   
                                                                       
   
static const char *
getFormattedTypeName(Archive *fout, Oid oid, OidOptions opts)
{
  TypeInfo *typeInfo;
  char *result;
  PQExpBuffer query;
  PGresult *res;

  if (oid == 0)
  {
    if ((opts & zeroAsOpaque) != 0)
    {
      return g_opaque_type;
    }
    else if ((opts & zeroAsAny) != 0)
    {
      return "'any'";
    }
    else if ((opts & zeroAsStar) != 0)
    {
      return "*";
    }
    else if ((opts & zeroAsNone) != 0)
    {
      return "NONE";
    }
  }

                                                                      
  typeInfo = findTypeByOid(oid);
  if (typeInfo && typeInfo->ftypname)
  {
    return typeInfo->ftypname;
  }

  query = createPQExpBuffer();
  appendPQExpBuffer(query, "SELECT pg_catalog.format_type('%u'::pg_catalog.oid, NULL)", oid);

  res = ExecuteSqlQueryForSingleRow(fout, query->data);

                                               
  result = pg_strdup(PQgetvalue(res, 0, 0));

  PQclear(res);
  destroyPQExpBuffer(query);

     
                                                                        
                                                                            
                                                                           
                                          
     
  if (typeInfo)
  {
    typeInfo->ftypname = result;
  }

  return result;
}

   
                                                       
   
                                                                           
                                        
   
static const char *
fmtCopyColumnList(const TableInfo *ti, PQExpBuffer buffer)
{
  int numatts = ti->numatts;
  char **attnames = ti->attnames;
  bool *attisdropped = ti->attisdropped;
  char *attgenerated = ti->attgenerated;
  bool needComma;
  int i;

  appendPQExpBufferChar(buffer, '(');
  needComma = false;
  for (i = 0; i < numatts; i++)
  {
    if (attisdropped[i])
    {
      continue;
    }
    if (attgenerated[i])
    {
      continue;
    }
    if (needComma)
    {
      appendPQExpBufferStr(buffer, ", ");
    }
    appendPQExpBufferStr(buffer, fmtId(attnames[i]));
    needComma = true;
  }

  if (!needComma)
  {
    return "";                           
  }

  appendPQExpBufferChar(buffer, ')');
  return buffer->data;
}

   
                                            
   
static bool
nonemptyReloptions(const char *reloptions)
{
                                                
  return (reloptions != NULL && strlen(reloptions) > 2);
}

   
                                                                
   
                                                                             
   
static void
appendReloptionsArrayAH(PQExpBuffer buffer, const char *reloptions, const char *prefix, Archive *fout)
{
  bool res;

  res = appendReloptionsArray(buffer, reloptions, prefix, fout->encoding, fout->std_strings);
  if (!res)
  {
    pg_log_warning("could not parse reloptions array");
  }
}
