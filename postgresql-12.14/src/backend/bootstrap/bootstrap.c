                                                                            
   
               
                                                              
                                                                  
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>
#include <signal.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "bootstrap/bootstrap.h"
#include "catalog/index.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "common/link-canary.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "pg_getopt.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "postmaster/startup.h"
#include "postmaster/walwriter.h"
#include "replication/walreceiver.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/condition_variable.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/rel.h"
#include "utils/relmapper.h"

uint32 bootstrap_data_checksum_version = 0;                  

#define ALLOC(t, c) ((t *)MemoryContextAllocZero(TopMemoryContext, (unsigned)(c) * sizeof(t)))

static void
CheckerModeMain(void);
static void
BootstrapModeMain(void);
static void
bootstrap_signals(void);
static void
ShutdownAuxiliaryProcess(int code, Datum arg);
static Form_pg_attribute
AllocateAttribute(void);
static Oid
gettype(char *type);
static void
cleanup(void);

                    
                     
                    
   

AuxProcType MyAuxProcType = NotAnAuxProcess;                              

Relation boot_reldesc;                                  

Form_pg_attribute attrtypes[MAXATTR];                               
int numattr;                                                                 

   
                                                                     
                                                                            
                                        
   
                                                                 
                                                                     
                                                         
   
struct typinfo
{
  char name[NAMEDATALEN];
  Oid oid;
  Oid elem;
  int16 len;
  bool byval;
  char align;
  char storage;
  Oid collation;
  Oid inproc;
  Oid outproc;
};

static const struct typinfo TypInfo[] = {{"bool", BOOLOID, 0, 1, true, 'c', 'p', InvalidOid, F_BOOLIN, F_BOOLOUT}, {"bytea", BYTEAOID, 0, -1, false, 'i', 'x', InvalidOid, F_BYTEAIN, F_BYTEAOUT}, {"char", CHAROID, 0, 1, true, 'c', 'p', InvalidOid, F_CHARIN, F_CHAROUT}, {"int2", INT2OID, 0, 2, true, 's', 'p', InvalidOid, F_INT2IN, F_INT2OUT}, {"int4", INT4OID, 0, 4, true, 'i', 'p', InvalidOid, F_INT4IN, F_INT4OUT}, {"float4", FLOAT4OID, 0, 4, FLOAT4PASSBYVAL, 'i', 'p', InvalidOid, F_FLOAT4IN, F_FLOAT4OUT}, {"name", NAMEOID, CHAROID, NAMEDATALEN, false, 'c', 'p', C_COLLATION_OID, F_NAMEIN, F_NAMEOUT}, {"regclass", REGCLASSOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGCLASSIN, F_REGCLASSOUT}, {"regproc", REGPROCOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGPROCIN, F_REGPROCOUT}, {"regtype", REGTYPEOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGTYPEIN, F_REGTYPEOUT}, {"regrole", REGROLEOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGROLEIN, F_REGROLEOUT},
    {"regnamespace", REGNAMESPACEOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGNAMESPACEIN, F_REGNAMESPACEOUT}, {"text", TEXTOID, 0, -1, false, 'i', 'x', DEFAULT_COLLATION_OID, F_TEXTIN, F_TEXTOUT}, {"oid", OIDOID, 0, 4, true, 'i', 'p', InvalidOid, F_OIDIN, F_OIDOUT}, {"tid", TIDOID, 0, 6, false, 's', 'p', InvalidOid, F_TIDIN, F_TIDOUT}, {"xid", XIDOID, 0, 4, true, 'i', 'p', InvalidOid, F_XIDIN, F_XIDOUT}, {"cid", CIDOID, 0, 4, true, 'i', 'p', InvalidOid, F_CIDIN, F_CIDOUT}, {"pg_node_tree", PGNODETREEOID, 0, -1, false, 'i', 'x', DEFAULT_COLLATION_OID, F_PG_NODE_TREE_IN, F_PG_NODE_TREE_OUT}, {"int2vector", INT2VECTOROID, INT2OID, -1, false, 'i', 'p', InvalidOid, F_INT2VECTORIN, F_INT2VECTOROUT}, {"oidvector", OIDVECTOROID, OIDOID, -1, false, 'i', 'p', InvalidOid, F_OIDVECTORIN, F_OIDVECTOROUT}, {"_int4", INT4ARRAYOID, INT4OID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}, {"_text", 1009, TEXTOID, -1, false, 'i', 'x', DEFAULT_COLLATION_OID, F_ARRAY_IN, F_ARRAY_OUT},
    {"_oid", 1028, OIDOID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}, {"_char", 1002, CHAROID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}, {"_aclitem", 1034, ACLITEMOID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}};

static const int n_types = sizeof(TypInfo) / sizeof(struct typinfo);

struct typmap
{             
  Oid am_oid;
  FormData_pg_type am_typ;
};

static struct typmap **Typ = NULL;
static struct typmap *Ap = NULL;

static Datum values[MAXATTR];                                     
static bool Nulls[MAXATTR];

static MemoryContext nogc = NULL;                                

   
                                                                        
                                                                       
                                                                 
   

typedef struct _IndexList
{
  Oid il_heap;
  Oid il_ind;
  IndexInfo *il_info;
  struct _IndexList *il_next;
} IndexList;

static IndexList *ILHead = NULL;

   
                         
   
                                                                        
                                                                             
   
                                                          
   
void
AuxiliaryProcessMain(int argc, char *argv[])
{
  char *progname = argv[0];
  int flag;
  char *userDoption = NULL;

     
                                                                           
                         
     
  if (!IsUnderPostmaster)
  {
    InitStandaloneProcess(argv[0]);
  }

     
                               
     

                                                                
  if (!IsUnderPostmaster)
  {
    InitializeGUCOptions();
  }

                                                      
  if (argc > 1 && strcmp(argv[1], "--boot") == 0)
  {
    argv++;
    argc--;
  }

                                                  
  MyAuxProcType = CheckerProcess;

  while ((flag = getopt(argc, argv, "B:c:d:D:Fkr:x:X:-:")) != -1)
  {
    switch (flag)
    {
    case 'B':
      SetConfigOption("shared_buffers", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;
    case 'D':
      userDoption = pstrdup(optarg);
      break;
    case 'd':
    {
                                                        
      char *debugstr;

      debugstr = psprintf("debug%s", optarg);
      SetConfigOption("log_min_messages", debugstr, PGC_POSTMASTER, PGC_S_ARGV);
      SetConfigOption("client_min_messages", debugstr, PGC_POSTMASTER, PGC_S_ARGV);
      pfree(debugstr);
    }
    break;
    case 'F':
      SetConfigOption("fsync", "false", PGC_POSTMASTER, PGC_S_ARGV);
      break;
    case 'k':
      bootstrap_data_checksum_version = PG_DATA_CHECKSUM_VERSION;
      break;
    case 'r':
      strlcpy(OutputFileName, optarg, MAXPGPATH);
      break;
    case 'x':
      MyAuxProcType = atoi(optarg);
      break;
    case 'X':
    {
      int WalSegSz = strtoul(optarg, NULL, 0);

      if (!IsValidWalSegSize(WalSegSz))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("-X requires a power of two value between 1 MB and 1 GB")));
      }
      SetConfigOption("wal_segment_size", optarg, PGC_INTERNAL, PGC_S_OVERRIDE);
    }
    break;
    case 'c':
    case '-':
    {
      char *name, *value;

      ParseLongOption(optarg, &name, &value);
      if (!value)
      {
        if (flag == '-')
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("--%s requires a value", optarg)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("-c %s requires a value", optarg)));
        }
      }

      SetConfigOption(name, value, PGC_POSTMASTER, PGC_S_ARGV);
      free(name);
      if (value)
      {
        free(value);
      }
      break;
    }
    default:
      write_stderr("Try \"%s --help\" for more information.\n", progname);
      proc_exit(1);
      break;
    }
  }

  if (argc != optind)
  {
    write_stderr("%s: invalid command-line arguments\n", progname);
    proc_exit(1);
  }

     
                            
     
  if (IsUnderPostmaster)
  {
    const char *statmsg;

    switch (MyAuxProcType)
    {
    case StartupProcess:
      statmsg = pgstat_get_backend_desc(B_STARTUP);
      break;
    case BgWriterProcess:
      statmsg = pgstat_get_backend_desc(B_BG_WRITER);
      break;
    case CheckpointerProcess:
      statmsg = pgstat_get_backend_desc(B_CHECKPOINTER);
      break;
    case WalWriterProcess:
      statmsg = pgstat_get_backend_desc(B_WAL_WRITER);
      break;
    case WalReceiverProcess:
      statmsg = pgstat_get_backend_desc(B_WAL_RECEIVER);
      break;
    default:
      statmsg = "??? process";
      break;
    }
    init_ps_display(statmsg, "", "", "");
  }

                                                                          
  if (!IsUnderPostmaster)
  {
    if (!SelectConfigFiles(userDoption, progname))
    {
      proc_exit(1);
    }
  }

     
                                                                         
                                                            
     
  if (!IsUnderPostmaster)
  {
    checkDataDir();
    ChangeToDataDir();
  }

                                                         
  if (!IsUnderPostmaster)
  {
    CreateDataDirLockFile(false);
  }

  SetProcessingMode(BootstrapProcessing);
  IgnoreSystemIndexes = true;

                                                                      
  if (!IsUnderPostmaster)
  {
    InitializeMaxBackends();
  }

  BaseInit();

     
                                                                      
                                                                             
                                          
     
  if (IsUnderPostmaster)
  {
       
                                                                         
                                                     
       
#ifndef EXEC_BACKEND
    InitAuxiliaryProcess();
#endif

       
                                                                     
                                                                           
                                                                       
                                                                         
                                                                     
                          
       
                                                                    
                                          
       
    ProcSignalInit(MaxBackends + MyAuxProcType + 1);

                                    
    InitBufferPoolBackend();

       
                                                                       
                                                                    
                                                            
       
    CreateAuxProcessResourceOwner();

                                               
    pgstat_initialize();
    pgstat_bestart();

                                                                
    before_shmem_exit(ShutdownAuxiliaryProcess, 0);
  }

     
                     
     
  SetProcessingMode(NormalProcessing);

  switch (MyAuxProcType)
  {
  case CheckerProcess:
                                                 
    CheckerModeMain();
    proc_exit(1);                          

  case BootstrapProcess:

       
                                                                       
                                                                       
                                             
       
    SetProcessingMode(BootstrapProcessing);
    bootstrap_signals();
    BootStrapXLOG();
    BootstrapModeMain();
    proc_exit(1);                          

  case StartupProcess:
                                                               
    StartupProcessMain();
    proc_exit(1);                          

  case BgWriterProcess:
                                                        
    BackgroundWriterMain();
    proc_exit(1);                          

  case CheckpointerProcess:
                                                            
    CheckpointerMain();
    proc_exit(1);                          

  case WalWriterProcess:
                                                         
    InitXLOGAccess();
    WalWriterMain();
    proc_exit(1);                          

  case WalReceiverProcess:
                                                           
    WalReceiverMain();
    proc_exit(1);                          

  default:
    elog(PANIC, "unrecognized process type: %d", (int)MyAuxProcType);
    proc_exit(1);
  }
}

   
                                                                            
                                                                          
                                                                    
                                    
   
static void
CheckerModeMain(void)
{
  proc_exit(0);
}

   
                                                                   
   
                                                                    
                                                                 
                                              
   
static void
BootstrapModeMain(void)
{
  int i;

  Assert(!IsUnderPostmaster);
  Assert(IsBootstrapProcessingMode());

     
                                                                            
                                                                
     
  if (pg_link_canary_is_frontend())
  {
    elog(ERROR, "backend is incorrectly linked to frontend functions");
  }

     
                                                       
     
  InitProcess();

  InitPostgres(NULL, InvalidOid, NULL, InvalidOid, NULL, false);

                                                      
  for (i = 0; i < MAXATTR; i++)
  {
    attrtypes[i] = NULL;
    Nulls[i] = false;
  }

     
                              
     
  StartTransactionCommand();
  boot_yyparse();
  CommitTransactionCommand();

     
                                                                          
                                             
     
  RelationMapFinishBootstrap();

                         
  cleanup();
  proc_exit(0);
}

                                                                    
                       
                                                                    
   

   
                                                  
   
static void
bootstrap_signals(void)
{
  Assert(!IsUnderPostmaster);

     
                                                                         
                                                                           
                                                                           
     
  pqsignal(SIGHUP, SIG_DFL);
  pqsignal(SIGINT, SIG_DFL);
  pqsignal(SIGTERM, SIG_DFL);
  pqsignal(SIGQUIT, SIG_DFL);
}

   
                                                                                 
                                                                         
                                                                               
                                                                          
                                                 
   
static void
ShutdownAuxiliaryProcess(int code, Datum arg)
{
  LWLockReleaseAll();
  ConditionVariableCancelSleep();
  pgstat_report_wait_end();
}

                                                                    
                                                    
                                                                    
   

                    
                 
                    
   
void
boot_openrel(char *relname)
{
  int i;
  struct typmap **app;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tup;

  if (strlen(relname) >= NAMEDATALEN)
  {
    relname[NAMEDATALEN - 1] = '\0';
  }

  if (Typ == NULL)
  {
                                          
    rel = table_open(TypeRelationId, NoLock);
    scan = table_beginscan_catalog(rel, 0, NULL);
    i = 0;
    while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
      ++i;
    }
    table_endscan(scan);
    app = Typ = ALLOC(struct typmap *, i + 1);
    while (i-- > 0)
    {
      *app++ = ALLOC(struct typmap, 1);
    }
    *app = NULL;
    scan = table_beginscan_catalog(rel, 0, NULL);
    app = Typ;
    while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
      (*app)->am_oid = ((Form_pg_type)GETSTRUCT(tup))->oid;
      memcpy((char *)&(*app)->am_typ, (char *)GETSTRUCT(tup), sizeof((*app)->am_typ));
      app++;
    }
    table_endscan(scan);
    table_close(rel, NoLock);
  }

  if (boot_reldesc != NULL)
  {
    closerel(NULL);
  }

  elog(DEBUG4, "open relation %s, attrsize %d", relname, (int)ATTRIBUTE_FIXED_PART_SIZE);

  boot_reldesc = table_openrv(makeRangeVar(NULL, relname, -1), NoLock);
  numattr = RelationGetNumberOfAttributes(boot_reldesc);
  for (i = 0; i < numattr; i++)
  {
    if (attrtypes[i] == NULL)
    {
      attrtypes[i] = AllocateAttribute();
    }
    memmove((char *)attrtypes[i], (char *)TupleDescAttr(boot_reldesc->rd_att, i), ATTRIBUTE_FIXED_PART_SIZE);

    {
      Form_pg_attribute at = attrtypes[i];

      elog(DEBUG4, "create attribute %d name %s len %d num %d type %u", i, NameStr(at->attname), at->attlen, at->attnum, at->atttypid);
    }
  }
}

                    
             
                    
   
void
closerel(char *name)
{
  if (name)
  {
    if (boot_reldesc)
    {
      if (strcmp(RelationGetRelationName(boot_reldesc), name) != 0)
      {
        elog(ERROR, "close of %s when %s was expected", name, RelationGetRelationName(boot_reldesc));
      }
    }
    else
    {
      elog(ERROR, "close of %s before any relation was opened", name);
    }
  }

  if (boot_reldesc == NULL)
  {
    elog(ERROR, "no open relation to close");
  }
  else
  {
    elog(DEBUG4, "close relation %s", RelationGetRelationName(boot_reldesc));
    table_close(boot_reldesc, NoLock);
    boot_reldesc = NULL;
  }
}

                    
                
   
                              
                                                                   
                          
                    
   
void
DefineAttr(char *name, char *type, int attnum, int nullness)
{
  Oid typeoid;

  if (boot_reldesc != NULL)
  {
    elog(WARNING, "no open relations allowed with CREATE command");
    closerel(NULL);
  }

  if (attrtypes[attnum] == NULL)
  {
    attrtypes[attnum] = AllocateAttribute();
  }
  MemSet(attrtypes[attnum], 0, ATTRIBUTE_FIXED_PART_SIZE);

  namestrcpy(&attrtypes[attnum]->attname, name);
  elog(DEBUG4, "column %s %s", NameStr(attrtypes[attnum]->attname), type);
  attrtypes[attnum]->attnum = attnum + 1;              

  typeoid = gettype(type);

  if (Typ != NULL)
  {
    attrtypes[attnum]->atttypid = Ap->am_oid;
    attrtypes[attnum]->attlen = Ap->am_typ.typlen;
    attrtypes[attnum]->attbyval = Ap->am_typ.typbyval;
    attrtypes[attnum]->attstorage = Ap->am_typ.typstorage;
    attrtypes[attnum]->attalign = Ap->am_typ.typalign;
    attrtypes[attnum]->attcollation = Ap->am_typ.typcollation;
                                                          
    if (Ap->am_typ.typelem != InvalidOid && Ap->am_typ.typlen < 0)
    {
      attrtypes[attnum]->attndims = 1;
    }
    else
    {
      attrtypes[attnum]->attndims = 0;
    }
  }
  else
  {
    attrtypes[attnum]->atttypid = TypInfo[typeoid].oid;
    attrtypes[attnum]->attlen = TypInfo[typeoid].len;
    attrtypes[attnum]->attbyval = TypInfo[typeoid].byval;
    attrtypes[attnum]->attstorage = TypInfo[typeoid].storage;
    attrtypes[attnum]->attalign = TypInfo[typeoid].align;
    attrtypes[attnum]->attcollation = TypInfo[typeoid].collation;
                                                          
    if (TypInfo[typeoid].elem != InvalidOid && attrtypes[attnum]->attlen < 0)
    {
      attrtypes[attnum]->attndims = 1;
    }
    else
    {
      attrtypes[attnum]->attndims = 0;
    }
  }

     
                                                                      
                                                                      
                                                                          
                                   
     
  if (OidIsValid(attrtypes[attnum]->attcollation))
  {
    attrtypes[attnum]->attcollation = C_COLLATION_OID;
  }

  attrtypes[attnum]->attstattarget = -1;
  attrtypes[attnum]->attcacheoff = -1;
  attrtypes[attnum]->atttypmod = -1;
  attrtypes[attnum]->attislocal = true;

  if (nullness == BOOTCOL_NULL_FORCE_NOT_NULL)
  {
    attrtypes[attnum]->attnotnull = true;
  }
  else if (nullness == BOOTCOL_NULL_FORCE_NULL)
  {
    attrtypes[attnum]->attnotnull = false;
  }
  else
  {
    Assert(nullness == BOOTCOL_NULL_AUTO);

       
                                                                       
                                                                   
                                          
       
                                                                       
                                              
       
#define MARKNOTNULL(att) ((att)->attlen > 0 || (att)->atttypid == OIDVECTOROID || (att)->atttypid == INT2VECTOROID)

    if (MARKNOTNULL(attrtypes[attnum]))
    {
      int i;

                                    
      for (i = 0; i < attnum; i++)
      {
        if (!attrtypes[i]->attnotnull)
        {
          break;
        }
      }
      if (i == attnum)
      {
        attrtypes[attnum]->attnotnull = true;
      }
    }
  }
}

                    
                   
   
                                                                         
                                                                     
                    
   
void
InsertOneTuple(void)
{
  HeapTuple tuple;
  TupleDesc tupDesc;
  int i;

  elog(DEBUG4, "inserting row with %d columns", numattr);

  tupDesc = CreateTupleDesc(numattr, attrtypes);
  tuple = heap_form_tuple(tupDesc, values, Nulls);
  pfree(tupDesc);                                             

  simple_heap_insert(boot_reldesc, tuple);
  heap_freetuple(tuple);
  elog(DEBUG4, "row inserted");

     
                                       
     
  for (i = 0; i < numattr; i++)
  {
    Nulls[i] = false;
  }
}

                    
                   
                    
   
void
InsertOneValue(char *value, int i)
{
  Oid typoid;
  int16 typlen;
  bool typbyval;
  char typalign;
  char typdelim;
  Oid typioparam;
  Oid typinput;
  Oid typoutput;

  AssertArg(i >= 0 && i < MAXATTR);

  elog(DEBUG4, "inserting column %d value \"%s\"", i, value);

  typoid = TupleDescAttr(boot_reldesc->rd_att, i)->atttypid;

  boot_get_type_io_data(typoid, &typlen, &typbyval, &typalign, &typdelim, &typioparam, &typinput, &typoutput);

  values[i] = OidInputFunctionCall(typinput, value, typioparam, -1);

     
                                                                             
                                                                  
     
  ereport(DEBUG4, (errmsg_internal("inserted -> %s", OidOutputFunctionCall(typoutput, values[i]))));
}

                    
                  
                    
   
void
InsertOneNull(int i)
{
  elog(DEBUG4, "inserting column %d NULL", i);
  Assert(i >= 0 && i < MAXATTR);
  if (TupleDescAttr(boot_reldesc->rd_att, i)->attnotnull)
  {
    elog(ERROR, "NULL value specified for not-null column \"%s\" of relation \"%s\"", NameStr(TupleDescAttr(boot_reldesc->rd_att, i)->attname), RelationGetRelationName(boot_reldesc));
  }
  values[i] = PointerGetDatum(NULL);
  Nulls[i] = true;
}

                    
            
                    
   
static void
cleanup(void)
{
  if (boot_reldesc != NULL)
  {
    closerel(NULL);
  }
}

                    
            
   
                                                                            
                                                                           
                                                                              
                                                                        
                                                                        
                                                     
                    
   
static Oid
gettype(char *type)
{
  int i;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tup;
  struct typmap **app;

  if (Typ != NULL)
  {
    for (app = Typ; *app != NULL; app++)
    {
      if (strncmp(NameStr((*app)->am_typ.typname), type, NAMEDATALEN) == 0)
      {
        Ap = *app;
        return (*app)->am_oid;
      }
    }
  }
  else
  {
    for (i = 0; i < n_types; i++)
    {
      if (strncmp(type, TypInfo[i].name, NAMEDATALEN) == 0)
      {
        return i;
      }
    }
    elog(DEBUG4, "external type: %s", type);
    rel = table_open(TypeRelationId, NoLock);
    scan = table_beginscan_catalog(rel, 0, NULL);
    i = 0;
    while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
      ++i;
    }
    table_endscan(scan);
    app = Typ = ALLOC(struct typmap *, i + 1);
    while (i-- > 0)
    {
      *app++ = ALLOC(struct typmap, 1);
    }
    *app = NULL;
    scan = table_beginscan_catalog(rel, 0, NULL);
    app = Typ;
    while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
      (*app)->am_oid = ((Form_pg_type)GETSTRUCT(tup))->oid;
      memmove((char *)&(*app++)->am_typ, (char *)GETSTRUCT(tup), sizeof((*app)->am_typ));
    }
    table_endscan(scan);
    table_close(rel, NoLock);
    return gettype(type);
  }
  elog(ERROR, "unrecognized type \"%s\"", type);
                                                
  return 0;
}

                    
                          
   
                                                                          
                                                                      
                                                                      
                                                                           
                                               
                    
   
void
boot_get_type_io_data(Oid typid, int16 *typlen, bool *typbyval, char *typalign, char *typdelim, Oid *typioparam, Oid *typinput, Oid *typoutput)
{
  if (Typ != NULL)
  {
                                                              
    struct typmap **app;
    struct typmap *ap;

    app = Typ;
    while (*app && (*app)->am_oid != typid)
    {
      ++app;
    }
    ap = *app;
    if (ap == NULL)
    {
      elog(ERROR, "type OID %u not found in Typ list", typid);
    }

    *typlen = ap->am_typ.typlen;
    *typbyval = ap->am_typ.typbyval;
    *typalign = ap->am_typ.typalign;
    *typdelim = ap->am_typ.typdelim;

                                                    
    if (OidIsValid(ap->am_typ.typelem))
    {
      *typioparam = ap->am_typ.typelem;
    }
    else
    {
      *typioparam = typid;
    }

    *typinput = ap->am_typ.typinput;
    *typoutput = ap->am_typ.typoutput;
  }
  else
  {
                                                                        
    int typeindex;

    for (typeindex = 0; typeindex < n_types; typeindex++)
    {
      if (TypInfo[typeindex].oid == typid)
      {
        break;
      }
    }
    if (typeindex >= n_types)
    {
      elog(ERROR, "type OID %u not found in TypInfo", typid);
    }

    *typlen = TypInfo[typeindex].len;
    *typbyval = TypInfo[typeindex].byval;
    *typalign = TypInfo[typeindex].align;
                                                           
    *typdelim = ',';

                                                    
    if (OidIsValid(TypInfo[typeindex].elem))
    {
      *typioparam = TypInfo[typeindex].elem;
    }
    else
    {
      *typioparam = typid;
    }

    *typinput = TypInfo[typeindex].inproc;
    *typoutput = TypInfo[typeindex].outproc;
  }
}

                    
                      
   
                                                                   
                                                  
                    
   
static Form_pg_attribute
AllocateAttribute(void)
{
  return (Form_pg_attribute)MemoryContextAllocZero(TopMemoryContext, ATTRIBUTE_FIXED_PART_SIZE);
}

   
                                                                         
               
   
                                                                        
                                                                      
                                                                        
                                                                          
                                                                       
                                                                       
   
void
index_register(Oid heap, Oid ind, IndexInfo *indexInfo)
{
  IndexList *newind;
  MemoryContext oldcxt;

     
                                                                     
                                                                             
            
     

  if (nogc == NULL)
  {
    nogc = AllocSetContextCreate(NULL, "BootstrapNoGC", ALLOCSET_DEFAULT_SIZES);
  }

  oldcxt = MemoryContextSwitchTo(nogc);

  newind = (IndexList *)palloc(sizeof(IndexList));
  newind->il_heap = heap;
  newind->il_ind = ind;
  newind->il_info = (IndexInfo *)palloc(sizeof(IndexInfo));

  memcpy(newind->il_info, indexInfo, sizeof(IndexInfo));
                                                                
  newind->il_info->ii_Expressions = copyObject(indexInfo->ii_Expressions);
  newind->il_info->ii_ExpressionsState = NIL;
                                                              
  newind->il_info->ii_Predicate = copyObject(indexInfo->ii_Predicate);
  newind->il_info->ii_PredicateState = NULL;
                                                                      
  Assert(indexInfo->ii_ExclusionOps == NULL);
  Assert(indexInfo->ii_ExclusionProcs == NULL);
  Assert(indexInfo->ii_ExclusionStrats == NULL);

  newind->il_next = ILHead;
  ILHead = newind;

  MemoryContextSwitchTo(oldcxt);
}

   
                                                               
   
void
build_indices(void)
{
  for (; ILHead != NULL; ILHead = ILHead->il_next)
  {
    Relation heap;
    Relation ind;

                                                     
    heap = table_open(ILHead->il_heap, NoLock);
    ind = index_open(ILHead->il_ind, NoLock);

    index_build(heap, ind, ILHead->il_info, false, false);

    index_close(ind, NoLock);
    table_close(heap, NoLock);
  }
}
