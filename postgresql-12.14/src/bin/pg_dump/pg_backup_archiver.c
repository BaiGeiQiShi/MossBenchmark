                                                                            
   
                        
   
                                                    
   
                                                   
   
                                     
                                                              
                                  
   
                                                              
                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef WIN32
#include <io.h>
#endif

#include "parallel.h"
#include "pg_backup_archiver.h"
#include "pg_backup_db.h"
#include "pg_backup_utils.h"
#include "dumputils.h"
#include "fe_utils/string_utils.h"

#include "libpq/libpq-fs.h"

#define TEXT_DUMP_HEADER "--\n-- PostgreSQL database dump\n--\n\n"
#define TEXT_DUMPALL_HEADER "--\n-- PostgreSQL database cluster dump\n--\n\n"

                                                             
typedef struct _outputContext
{
  void *OF;
  int gzOut;
} OutputContext;

   
                                                                            
                                                                            
                                                           
   
                                                         
                                                                
                                                                           
                                                                      
   
typedef struct _parallelReadyList
{
  TocEntry **tes;                              
  int first_te;                                            
  int last_te;                                            
  bool sorted;                                             
} ParallelReadyList;

static ArchiveHandle *
_allocAH(const char *FileSpec, const ArchiveFormat fmt, const int compression, bool dosync, ArchiveMode mode, SetupWorkerPtrType setupWorkerPtr);
static void
_getObjectDescription(PQExpBuffer buf, TocEntry *te, ArchiveHandle *AH);
static void
_printTocEntry(ArchiveHandle *AH, TocEntry *te, bool isData);
static char *
sanitize_line(const char *str, bool want_hyphen);
static void
_doSetFixedOutputState(ArchiveHandle *AH);
static void
_doSetSessionAuth(ArchiveHandle *AH, const char *user);
static void
_reconnectToDB(ArchiveHandle *AH, const char *dbname);
static void
_becomeUser(ArchiveHandle *AH, const char *user);
static void
_becomeOwner(ArchiveHandle *AH, TocEntry *te);
static void
_selectOutputSchema(ArchiveHandle *AH, const char *schemaName);
static void
_selectTablespace(ArchiveHandle *AH, const char *tablespace);
static void
_selectTableAccessMethod(ArchiveHandle *AH, const char *tableam);
static void
processEncodingEntry(ArchiveHandle *AH, TocEntry *te);
static void
processStdStringsEntry(ArchiveHandle *AH, TocEntry *te);
static void
processSearchPathEntry(ArchiveHandle *AH, TocEntry *te);
static teReqs
_tocEntryRequired(TocEntry *te, teSection curSection, ArchiveHandle *AH);
static RestorePass
_tocEntryRestorePass(TocEntry *te);
static bool
_tocEntryIsACL(TocEntry *te);
static void
_disableTriggersIfNecessary(ArchiveHandle *AH, TocEntry *te);
static void
_enableTriggersIfNecessary(ArchiveHandle *AH, TocEntry *te);
static void
buildTocEntryArrays(ArchiveHandle *AH);
static void
_moveBefore(ArchiveHandle *AH, TocEntry *pos, TocEntry *te);
static int
_discoverArchiveFormat(ArchiveHandle *AH);

static int
RestoringToDB(ArchiveHandle *AH);
static void
dump_lo_buf(ArchiveHandle *AH);
static void
dumpTimestamp(ArchiveHandle *AH, const char *msg, time_t tim);
static void
SetOutput(ArchiveHandle *AH, const char *filename, int compression);
static OutputContext
SaveOutput(ArchiveHandle *AH);
static void
RestoreOutput(ArchiveHandle *AH, OutputContext savedContext);

static int
restore_toc_entry(ArchiveHandle *AH, TocEntry *te, bool is_parallel);
static void
restore_toc_entries_prefork(ArchiveHandle *AH, TocEntry *pending_list);
static void
restore_toc_entries_parallel(ArchiveHandle *AH, ParallelState *pstate, TocEntry *pending_list);
static void
restore_toc_entries_postfork(ArchiveHandle *AH, TocEntry *pending_list);
static void
pending_list_header_init(TocEntry *l);
static void
pending_list_append(TocEntry *l, TocEntry *te);
static void
pending_list_remove(TocEntry *te);
static void
ready_list_init(ParallelReadyList *ready_list, int tocCount);
static void
ready_list_free(ParallelReadyList *ready_list);
static void
ready_list_insert(ParallelReadyList *ready_list, TocEntry *te);
static void
ready_list_remove(ParallelReadyList *ready_list, int i);
static void
ready_list_sort(ParallelReadyList *ready_list);
static int
TocEntrySizeCompare(const void *p1, const void *p2);
static void
move_to_ready_list(TocEntry *pending_list, ParallelReadyList *ready_list, RestorePass pass);
static TocEntry *
pop_next_work_item(ArchiveHandle *AH, ParallelReadyList *ready_list, ParallelState *pstate);
static void
mark_dump_job_done(ArchiveHandle *AH, TocEntry *te, int status, void *callback_data);
static void
mark_restore_job_done(ArchiveHandle *AH, TocEntry *te, int status, void *callback_data);
static void
fix_dependencies(ArchiveHandle *AH);
static bool
has_lock_conflicts(TocEntry *te1, TocEntry *te2);
static void
repoint_table_dependencies(ArchiveHandle *AH);
static void
identify_locking_dependencies(ArchiveHandle *AH, TocEntry *te);
static void
reduce_dependencies(ArchiveHandle *AH, TocEntry *te, ParallelReadyList *ready_list);
static void
mark_create_done(ArchiveHandle *AH, TocEntry *te);
static void
inhibit_data_for_failed_table(ArchiveHandle *AH, TocEntry *te);

static void
StrictNamesCheck(RestoreOptions *ropt);

   
                                                                   
   
DumpOptions *
NewDumpOptions(void)
{
  DumpOptions *opts = (DumpOptions *)pg_malloc(sizeof(DumpOptions));

  InitDumpOptions(opts);
  return opts;
}

   
                                                         
   
void
InitDumpOptions(DumpOptions *opts)
{
  memset(opts, 0, sizeof(DumpOptions));
                                                       
  opts->include_everything = true;
  opts->cparams.promptPassword = TRI_DEFAULT;
  opts->dumpSections = DUMP_UNSECTIONED;
}

   
                                                                           
                                      
   
DumpOptions *
dumpOptionsFromRestoreOptions(RestoreOptions *ropt)
{
  DumpOptions *dopt = NewDumpOptions();

                                                                      
  dopt->cparams.dbname = ropt->cparams.dbname ? pg_strdup(ropt->cparams.dbname) : NULL;
  dopt->cparams.pgport = ropt->cparams.pgport ? pg_strdup(ropt->cparams.pgport) : NULL;
  dopt->cparams.pghost = ropt->cparams.pghost ? pg_strdup(ropt->cparams.pghost) : NULL;
  dopt->cparams.username = ropt->cparams.username ? pg_strdup(ropt->cparams.username) : NULL;
  dopt->cparams.promptPassword = ropt->cparams.promptPassword;
  dopt->outputClean = ropt->dropSchema;
  dopt->dataOnly = ropt->dataOnly;
  dopt->schemaOnly = ropt->schemaOnly;
  dopt->if_exists = ropt->if_exists;
  dopt->column_inserts = ropt->column_inserts;
  dopt->dumpSections = ropt->dumpSections;
  dopt->aclsSkip = ropt->aclsSkip;
  dopt->outputSuperuser = ropt->superuser;
  dopt->outputCreateDB = ropt->createDB;
  dopt->outputNoOwner = ropt->noOwner;
  dopt->outputNoTablespaces = ropt->noTablespace;
  dopt->disable_triggers = ropt->disable_triggers;
  dopt->use_setsessauth = ropt->use_setsessauth;
  dopt->disable_dollar_quoting = ropt->disable_dollar_quoting;
  dopt->dump_inserts = ropt->dump_inserts;
  dopt->no_comments = ropt->no_comments;
  dopt->no_publications = ropt->no_publications;
  dopt->no_security_labels = ropt->no_security_labels;
  dopt->no_subscriptions = ropt->no_subscriptions;
  dopt->lockWaitTimeout = ropt->lockWaitTimeout;
  dopt->include_everything = ropt->include_everything;
  dopt->enable_row_security = ropt->enable_row_security;
  dopt->sequence_data = ropt->sequence_data;

  return dopt;
}

   
                      
   
                                                                      
                                                                         
   
   

   
                                                                              
                                                                                
                                                                   
   
static void
setupRestoreWorker(Archive *AHX)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;

  AH->ReopenPtr(AH);
}

                          
            
Archive *
CreateArchive(const char *FileSpec, const ArchiveFormat fmt, const int compression, bool dosync, ArchiveMode mode, SetupWorkerPtrType setupDumpWorker)

{
  ArchiveHandle *AH = _allocAH(FileSpec, fmt, compression, dosync, mode, setupDumpWorker);

  return (Archive *)AH;
}

                              
            
Archive *
OpenArchive(const char *FileSpec, const ArchiveFormat fmt)
{
  ArchiveHandle *AH = _allocAH(FileSpec, fmt, 0, true, archModeRead, setupRestoreWorker);

  return (Archive *)AH;
}

            
void
CloseArchive(Archive *AHX)
{
  int res = 0;
  ArchiveHandle *AH = (ArchiveHandle *)AHX;

  AH->ClosePtr(AH);

                        
  errno = 0;                                       
  if (AH->gzOut)
  {
    res = GZCLOSE(AH->OF);
  }
  else if (AH->OF != stdout)
  {
    res = fclose(AH->OF);
  }

  if (res != 0)
  {
    fatal("could not close output file: %m");
  }
}

            
void
SetArchiveOptions(Archive *AH, DumpOptions *dopt, RestoreOptions *ropt)
{
                                                                      
  if (dopt == NULL && ropt != NULL)
  {
    dopt = dumpOptionsFromRestoreOptions(ropt);
  }

                                     
  AH->dopt = dopt;
  AH->ropt = ropt;
}

            
void
ProcessArchiveRestoreOptions(Archive *AHX)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;
  RestoreOptions *ropt = AH->public.ropt;
  TocEntry *te;
  teSection curSection;

                                                                       
  curSection = SECTION_PRE_DATA;
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
       
                                                                       
                                                                        
                                                                         
                                                                     
       
    if (AH->mode != archModeRead)
    {
      switch (te->section)
      {
      case SECTION_NONE:
                               
        break;
      case SECTION_PRE_DATA:
        if (curSection != SECTION_PRE_DATA)
        {
          pg_log_warning("archive items not in correct section order");
        }
        break;
      case SECTION_DATA:
        if (curSection == SECTION_POST_DATA)
        {
          pg_log_warning("archive items not in correct section order");
        }
        break;
      case SECTION_POST_DATA:
                                                   
        break;
      default:
        fatal("unexpected section code %d", (int)te->section);
        break;
      }
    }

    if (te->section != SECTION_NONE)
    {
      curSection = te->section;
    }

    te->reqs = _tocEntryRequired(te, curSection, AH);
  }

                                     
  if (ropt->strict_names)
  {
    StrictNamesCheck(ropt);
  }
}

            
void
RestoreArchive(Archive *AHX)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;
  RestoreOptions *ropt = AH->public.ropt;
  bool parallel_mode;
  TocEntry *te;
  OutputContext sav;

  AH->stage = STAGE_INITIALIZING;

     
                                                                         
     
  parallel_mode = (AH->public.numWorkers > 1 && ropt->useDB);
  if (parallel_mode)
  {
                                                                          
    if (AH->ClonePtr == NULL || AH->ReopenPtr == NULL)
    {
      fatal("parallel restore is not supported with this archive file format");
    }

                                                                     
    if (AH->version < K_VERS_1_8)
    {
      fatal("parallel restore is not supported with archives made by pre-8.0 pg_dump");
    }

       
                                                                      
                                   
       
    AH->ReopenPtr(AH);
  }

     
                                                            
     
#ifndef HAVE_LIBZ
  if (AH->compression != 0 && AH->PrintTocDataPtr != NULL)
  {
    for (te = AH->toc->next; te != AH->toc; te = te->next)
    {
      if (te->hadDumper && (te->reqs & REQ_DATA) != 0)
      {
        fatal("cannot restore from compressed archive (compression not supported in this installation)");
      }
    }
  }
#endif

     
                                                                             
                                                
     
  if (AH->tocsByDumpId == NULL)
  {
    buildTocEntryArrays(AH);
  }

     
                                                      
     
  if (ropt->useDB)
  {
    pg_log_info("connecting to database for restore");
    if (AH->version < K_VERS_1_3)
    {
      fatal("direct database connections are not supported in pre-1.3 archives");
    }

       
                                                                    
                                                                           
               
       
    AHX->minRemoteVersion = 0;
    AHX->maxRemoteVersion = 9999999;

    ConnectDatabase(AHX, &ropt->cparams, false);

       
                                                                           
                                          
       
    AH->noTocComments = 1;
  }

     
                                                                          
                                                                          
                                                                            
                                              
     
                                                                         
                                                                 
     
  if (!ropt->dataOnly)
  {
    int impliedDataOnly = 1;

    for (te = AH->toc->next; te != AH->toc; te = te->next)
    {
      if ((te->reqs & REQ_SCHEMA) != 0)
      {                                   
        impliedDataOnly = 0;
        break;
      }
    }
    if (impliedDataOnly)
    {
      ropt->dataOnly = impliedDataOnly;
      pg_log_info("implied data-only restore");
    }
  }

     
                                         
     
  sav = SaveOutput(AH);
  if (ropt->filename || ropt->compression)
  {
    SetOutput(AH, ropt->filename, ropt->compression);
  }

  ahprintf(AH, "--\n-- PostgreSQL database dump\n--\n\n");

  if (AH->archiveRemoteVersion)
  {
    ahprintf(AH, "-- Dumped from database version %s\n", AH->archiveRemoteVersion);
  }
  if (AH->archiveDumpVersion)
  {
    ahprintf(AH, "-- Dumped by pg_dump version %s\n", AH->archiveDumpVersion);
  }

  ahprintf(AH, "\n");

  if (AH->public.verbose)
  {
    dumpTimestamp(AH, "Started on", AH->createDate);
  }

  if (ropt->single_txn)
  {
    if (AH->connection)
    {
      StartTransaction(AHX);
    }
    else
    {
      ahprintf(AH, "BEGIN;\n\n");
    }
  }

     
                                                      
     
  _doSetFixedOutputState(AH);

  AH->stage = STAGE_PROCESSING;

     
                                                   
     
  if (ropt->dropSchema)
  {
    for (te = AH->toc->prev; te != AH->toc; te = te->prev)
    {
      AH->currentTE = te;

         
                                                                     
                                                                     
                                                                      
                                                                        
                              
         
      if (ropt->createDB)
      {
        if (strcmp(te->desc, "DATABASE") != 0 && strcmp(te->desc, "DATABASE PROPERTIES") != 0)
        {
          continue;
        }
      }

                                                                       
      if (((te->reqs & (REQ_SCHEMA | REQ_DATA)) != 0) && te->dropStmt)
      {
        pg_log_info("dropping %s %s", te->desc, te->tag);
                                                  
        _becomeOwner(AH, te);
        _selectOutputSchema(AH, te->namespace);

           
                                                                      
                                                                       
                                                                   
           
        if (*te->dropStmt != '\0')
        {
          if (!ropt->if_exists)
          {
                                                            
            ahprintf(AH, "%s", te->dropStmt);
          }
          else
          {
               
                                                                   
                                                              
                                                        
                                                                  
                                     
               
            if (strncmp(te->desc, "BLOB", 4) == 0)
            {
              DropBlobIfExists(AH, te->catalogId.oid);
            }
            else
            {
              char *dropStmt = pg_strdup(te->dropStmt);
              char *dropStmtOrig = dropStmt;
              PQExpBuffer ftStmt = createPQExpBuffer();

                 
                                                             
                                                             
                 
              if (strncmp(dropStmt, "ALTER TABLE", 11) == 0)
              {
                appendPQExpBuffer(ftStmt, "ALTER TABLE IF EXISTS");
                dropStmt = dropStmt + 11;
              }

                 
                                                              
                                                                 
                                                                 
                                                             
                 
                                                                
                 
                                                                 
                                                               
                                               
                 
                                                                
                                                           
                                                             
                                                               
                                                            
                                                                 
                 
              if (strcmp(te->desc, "DEFAULT") == 0 || strcmp(te->desc, "DATABASE PROPERTIES") == 0 || strncmp(dropStmt, "CREATE OR REPLACE VIEW", 22) == 0)
              {
                appendPQExpBufferStr(ftStmt, dropStmt);
              }
              else
              {
                char buffer[40];
                char *mark;

                if (strcmp(te->desc, "CONSTRAINT") == 0 || strcmp(te->desc, "CHECK CONSTRAINT") == 0 || strcmp(te->desc, "FK CONSTRAINT") == 0)
                {
                  strcpy(buffer, "DROP CONSTRAINT");
                }
                else
                {
                  snprintf(buffer, sizeof(buffer), "DROP %s", te->desc);
                }

                mark = strstr(dropStmt, buffer);

                if (mark)
                {
                  *mark = '\0';
                  appendPQExpBuffer(ftStmt, "%s%s IF EXISTS%s", dropStmt, buffer, mark + strlen(buffer));
                }
                else
                {
                                                            
                  pg_log_warning("could not find where to insert IF EXISTS in statement \"%s\"", dropStmtOrig);
                  appendPQExpBufferStr(ftStmt, dropStmt);
                }
              }

              ahprintf(AH, "%s", ftStmt->data);

              destroyPQExpBuffer(ftStmt);
              pg_free(dropStmtOrig);
            }
          }
        }
      }
    }

       
                                                                         
                                                                          
                                                                           
                                                                          
                                                                    
                                                                        
                    
       
                                                                           
                          
       
    if (AH->currSchema)
    {
      free(AH->currSchema);
    }
    AH->currSchema = NULL;
  }

  if (parallel_mode)
  {
       
                                                                          
       
    ParallelState *pstate;
    TocEntry pending_list;

                                                                
    if (AH->PrepParallelRestorePtr)
    {
      AH->PrepParallelRestorePtr(AH);
    }

    pending_list_header_init(&pending_list);

                                                                         
    restore_toc_entries_prefork(AH, &pending_list);
    Assert(AH->connection == NULL);

                                                                
    pstate = ParallelBackupStart(AH);
    restore_toc_entries_parallel(AH, pstate, &pending_list);
    ParallelBackupEnd(AH, pstate);

                                                             
    restore_toc_entries_postfork(AH, &pending_list);
    Assert(AH->connection != NULL);
  }
  else
  {
       
                                                                         
                                                                        
                                                               
       
    bool haveACL = false;
    bool havePostACL = false;

    for (te = AH->toc->next; te != AH->toc; te = te->next)
    {
      if ((te->reqs & (REQ_SCHEMA | REQ_DATA)) == 0)
      {
        continue;                                        
      }

      switch (_tocEntryRestorePass(te))
      {
      case RESTORE_PASS_MAIN:
        (void)restore_toc_entry(AH, te, false);
        break;
      case RESTORE_PASS_ACL:
        haveACL = true;
        break;
      case RESTORE_PASS_POST_ACL:
        havePostACL = true;
        break;
      }
    }

    if (haveACL)
    {
      for (te = AH->toc->next; te != AH->toc; te = te->next)
      {
        if ((te->reqs & (REQ_SCHEMA | REQ_DATA)) != 0 && _tocEntryRestorePass(te) == RESTORE_PASS_ACL)
        {
          (void)restore_toc_entry(AH, te, false);
        }
      }
    }

    if (havePostACL)
    {
      for (te = AH->toc->next; te != AH->toc; te = te->next)
      {
        if ((te->reqs & (REQ_SCHEMA | REQ_DATA)) != 0 && _tocEntryRestorePass(te) == RESTORE_PASS_POST_ACL)
        {
          (void)restore_toc_entry(AH, te, false);
        }
      }
    }
  }

  if (ropt->single_txn)
  {
    if (AH->connection)
    {
      CommitTransaction(AHX);
    }
    else
    {
      ahprintf(AH, "COMMIT;\n\n");
    }
  }

  if (AH->public.verbose)
  {
    dumpTimestamp(AH, "Completed on", time(NULL));
  }

  ahprintf(AH, "--\n-- PostgreSQL database dump complete\n--\n\n");

     
                            
     
  AH->stage = STAGE_FINALIZING;

  if (ropt->filename || ropt->compression)
  {
    RestoreOutput(AH, sav);
  }

  if (ropt->useDB)
  {
    DisconnectDatabase(&AH->public);
  }
}

   
                                                                               
                                                            
   
                                                                        
                                                                    
   
static int
restore_toc_entry(ArchiveHandle *AH, TocEntry *te, bool is_parallel)
{
  RestoreOptions *ropt = AH->public.ropt;
  int status = WORKER_OK;
  teReqs reqs;
  bool defnDumped;

  AH->currentTE = te;

                                                 
  if (!ropt->suppressDumpWarnings && strcmp(te->desc, "WARNING") == 0)
  {
    if (!ropt->dataOnly && te->defn != NULL && strlen(te->defn) != 0)
    {
      pg_log_warning("warning from original dump file: %s", te->defn);
    }
    else if (te->copyStmt != NULL && strlen(te->copyStmt) != 0)
    {
      pg_log_warning("warning from original dump file: %s", te->copyStmt);
    }
  }

                                                           
  reqs = te->reqs;

  defnDumped = false;

     
                                                                  
     
  if ((reqs & REQ_SCHEMA) != 0)
  {
                                                    
    if (te->namespace)
    {
      pg_log_info("creating %s \"%s.%s\"", te->desc, te->namespace, te->tag);
    }
    else
    {
      pg_log_info("creating %s \"%s\"", te->desc, te->tag);
    }

    _printTocEntry(AH, te, false);
    defnDumped = true;

    if (strcmp(te->desc, "TABLE") == 0)
    {
      if (AH->lastErrorTE == te)
      {
           
                                             
                                                           
                                                   
           
                                                                       
                                      
           
        if (ropt->noDataForFailedTables)
        {
          if (is_parallel)
          {
            status = WORKER_INHIBIT_DATA;
          }
          else
          {
            inhibit_data_for_failed_table(AH, te);
          }
        }
      }
      else
      {
           
                                                                      
                                               
           
                                                                       
                                      
           
        if (is_parallel)
        {
          status = WORKER_CREATE_DONE;
        }
        else
        {
          mark_create_done(AH, te);
        }
      }
    }

       
                                                                  
                                                                      
                               
       
    if (strcmp(te->desc, "DATABASE") == 0 || strcmp(te->desc, "DATABASE PROPERTIES") == 0)
    {
      pg_log_info("connecting to new database \"%s\"", te->tag);
      _reconnectToDB(AH, te->tag);
    }
  }

     
                                                                
     
  if ((reqs & REQ_DATA) != 0)
  {
       
                                                                         
                                                                       
                                                       
       
    if (te->hadDumper)
    {
         
                                                     
         
      if (AH->PrintTocDataPtr != NULL)
      {
        _printTocEntry(AH, te, true);

        if (strcmp(te->desc, "BLOBS") == 0 || strcmp(te->desc, "BLOB COMMENTS") == 0)
        {
          pg_log_info("processing %s", te->desc);

          _selectOutputSchema(AH, "pg_catalog");

                                                                  
          if (strcmp(te->desc, "BLOB COMMENTS") == 0)
          {
            AH->outputKind = OUTPUT_OTHERDATA;
          }

          AH->PrintTocDataPtr(AH, te);

          AH->outputKind = OUTPUT_SQLCMDS;
        }
        else
        {
          _disableTriggersIfNecessary(AH, te);

                                                    
          _becomeOwner(AH, te);
          _selectOutputSchema(AH, te->namespace);

          pg_log_info("processing data for table \"%s.%s\"", te->namespace, te->tag);

             
                                                                     
                                                                
                                                                 
                                                                 
                                                                   
                                    
             
          if (is_parallel && te->created)
          {
               
                                                                
                                                                   
               
            StartTransaction(&AH->public);

               
                                                                   
                                                               
                      
               
            ahprintf(AH, "TRUNCATE TABLE %s%s;\n\n", (PQserverVersion(AH->connection) >= 80400 ? "ONLY " : ""), fmtQualifiedId(te->namespace, te->tag));
          }

             
                                                  
             
          if (te->copyStmt && strlen(te->copyStmt) > 0)
          {
            ahprintf(AH, "%s", te->copyStmt);
            AH->outputKind = OUTPUT_COPYDATA;
          }
          else
          {
            AH->outputKind = OUTPUT_OTHERDATA;
          }

          AH->PrintTocDataPtr(AH, te);

             
                                       
             
          if (AH->outputKind == OUTPUT_COPYDATA && RestoringToDB(AH))
          {
            EndDBCopyMode(&AH->public, te->tag);
          }
          AH->outputKind = OUTPUT_SQLCMDS;

                                                       
          if (is_parallel && te->created)
          {
            CommitTransaction(&AH->public);
          }

          _enableTriggersIfNecessary(AH, te);
        }
      }
    }
    else if (!defnDumped)
    {
                                                                 
      pg_log_info("executing %s %s", te->desc, te->tag);
      _printTocEntry(AH, te, false);
    }
  }

  if (AH->public.n_errors > 0 && status == WORKER_OK)
  {
    status = WORKER_IGNORED_ERRORS;
  }

  return status;
}

   
                                        
                                                                          
   
RestoreOptions *
NewRestoreOptions(void)
{
  RestoreOptions *opts;

  opts = (RestoreOptions *)pg_malloc0(sizeof(RestoreOptions));

                                                       
  opts->format = archUnknown;
  opts->cparams.promptPassword = TRI_DEFAULT;
  opts->dumpSections = DUMP_UNSECTIONED;

  return opts;
}

static void
_disableTriggersIfNecessary(ArchiveHandle *AH, TocEntry *te)
{
  RestoreOptions *ropt = AH->public.ropt;

                                                       
  if (!ropt->dataOnly || !ropt->disable_triggers)
  {
    return;
  }

  pg_log_info("disabling triggers for %s", te->tag);

     
                                                                        
                                                                           
                                                                          
                   
     
  _becomeUser(AH, ropt->superuser);

     
                   
     
  ahprintf(AH, "ALTER TABLE %s DISABLE TRIGGER ALL;\n\n", fmtQualifiedId(te->namespace, te->tag));
}

static void
_enableTriggersIfNecessary(ArchiveHandle *AH, TocEntry *te)
{
  RestoreOptions *ropt = AH->public.ropt;

                                                       
  if (!ropt->dataOnly || !ropt->disable_triggers)
  {
    return;
  }

  pg_log_info("enabling triggers for %s", te->tag);

     
                                                                        
                                                                           
                                                                          
                   
     
  _becomeUser(AH, ropt->superuser);

     
                  
     
  ahprintf(AH, "ALTER TABLE %s ENABLE TRIGGER ALL;\n\n", fmtQualifiedId(te->namespace, te->tag));
}

   
                                                                                           
   

            
void
WriteData(Archive *AHX, const void *data, size_t dLen)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;

  if (!AH->currToc)
  {
    fatal("internal error -- WriteData cannot be called outside the context of a DataDumper routine");
  }

  AH->WriteDataPtr(AH, data, dLen);

  return;
}

   
                                                                         
                                                        
   
                                                                              
                                                                         
                                     
   

            
TocEntry *
ArchiveEntry(Archive *AHX, CatalogId catalogId, DumpId dumpId, ArchiveOpts *opts)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;
  TocEntry *newToc;

  newToc = (TocEntry *)pg_malloc0(sizeof(TocEntry));

  AH->tocCount++;
  if (dumpId > AH->maxDumpId)
  {
    AH->maxDumpId = dumpId;
  }

  newToc->prev = AH->toc->prev;
  newToc->next = AH->toc;
  AH->toc->prev->next = newToc;
  AH->toc->prev = newToc;

  newToc->catalogId = catalogId;
  newToc->dumpId = dumpId;
  newToc->section = opts->section;

  newToc->tag = pg_strdup(opts->tag);
  newToc->namespace = opts->namespace ? pg_strdup(opts->namespace) : NULL;
  newToc->tablespace = opts->tablespace ? pg_strdup(opts->tablespace) : NULL;
  newToc->tableam = opts->tableam ? pg_strdup(opts->tableam) : NULL;
  newToc->owner = opts->owner ? pg_strdup(opts->owner) : NULL;
  newToc->desc = pg_strdup(opts->description);
  newToc->defn = opts->createStmt ? pg_strdup(opts->createStmt) : NULL;
  newToc->dropStmt = opts->dropStmt ? pg_strdup(opts->dropStmt) : NULL;
  newToc->copyStmt = opts->copyStmt ? pg_strdup(opts->copyStmt) : NULL;

  if (opts->nDeps > 0)
  {
    newToc->dependencies = (DumpId *)pg_malloc(opts->nDeps * sizeof(DumpId));
    memcpy(newToc->dependencies, opts->deps, opts->nDeps * sizeof(DumpId));
    newToc->nDeps = opts->nDeps;
  }
  else
  {
    newToc->dependencies = NULL;
    newToc->nDeps = 0;
  }

  newToc->dataDumper = opts->dumpFn;
  newToc->dataDumperArg = opts->dumpArg;
  newToc->hadDumper = opts->dumpFn ? true : false;

  newToc->formatData = NULL;
  newToc->dataLength = 0;

  if (AH->ArchiveEntryPtr != NULL)
  {
    AH->ArchiveEntryPtr(AH, newToc);
  }

  return newToc;
}

            
void
PrintTOCSummary(Archive *AHX)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;
  RestoreOptions *ropt = AH->public.ropt;
  TocEntry *te;
  teSection curSection;
  OutputContext sav;
  const char *fmtName;
  char stamp_str[64];

  sav = SaveOutput(AH);
  if (ropt->filename)
  {
    SetOutput(AH, ropt->filename, 0                     );
  }

  if (strftime(stamp_str, sizeof(stamp_str), PGDUMP_STRFTIME_FMT, localtime(&AH->createDate)) == 0)
  {
    strcpy(stamp_str, "[unknown]");
  }

  ahprintf(AH, ";\n; Archive created at %s\n", stamp_str);
  ahprintf(AH, ";     dbname: %s\n;     TOC Entries: %d\n;     Compression: %d\n", sanitize_line(AH->archdbname, false), AH->tocCount, AH->compression);

  switch (AH->format)
  {
  case archCustom:
    fmtName = "CUSTOM";
    break;
  case archDirectory:
    fmtName = "DIRECTORY";
    break;
  case archTar:
    fmtName = "TAR";
    break;
  default:
    fmtName = "UNKNOWN";
  }

  ahprintf(AH, ";     Dump Version: %d.%d-%d\n", ARCHIVE_MAJOR(AH->version), ARCHIVE_MINOR(AH->version), ARCHIVE_REV(AH->version));
  ahprintf(AH, ";     Format: %s\n", fmtName);
  ahprintf(AH, ";     Integer: %d bytes\n", (int)AH->intSize);
  ahprintf(AH, ";     Offset: %d bytes\n", (int)AH->offSize);
  if (AH->archiveRemoteVersion)
  {
    ahprintf(AH, ";     Dumped from database version: %s\n", AH->archiveRemoteVersion);
  }
  if (AH->archiveDumpVersion)
  {
    ahprintf(AH, ";     Dumped by pg_dump version: %s\n", AH->archiveDumpVersion);
  }

  ahprintf(AH, ";\n;\n; Selected TOC Entries:\n;\n");

  curSection = SECTION_PRE_DATA;
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    if (te->section != SECTION_NONE)
    {
      curSection = te->section;
    }
    if (ropt->verbose || (_tocEntryRequired(te, curSection, AH) & (REQ_SCHEMA | REQ_DATA)) != 0)
    {
      char *sanitized_name;
      char *sanitized_schema;
      char *sanitized_owner;

         
         
      sanitized_name = sanitize_line(te->tag, false);
      sanitized_schema = sanitize_line(te->namespace, true);
      sanitized_owner = sanitize_line(te->owner, false);

      ahprintf(AH, "%d; %u %u %s %s %s %s\n", te->dumpId, te->catalogId.tableoid, te->catalogId.oid, te->desc, sanitized_schema, sanitized_name, sanitized_owner);

      free(sanitized_name);
      free(sanitized_schema);
      free(sanitized_owner);
    }
    if (ropt->verbose && te->nDeps > 0)
    {
      int i;

      ahprintf(AH, ";\tdepends on:");
      for (i = 0; i < te->nDeps; i++)
      {
        ahprintf(AH, " %d", te->dependencies[i]);
      }
      ahprintf(AH, "\n");
    }
  }

                                     
  if (ropt->strict_names)
  {
    StrictNamesCheck(ropt);
  }

  if (ropt->filename)
  {
    RestoreOutput(AH, sav);
  }
}

             
                 
             

                                                  
int
StartBlob(Archive *AHX, Oid oid)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;

  if (!AH->StartBlobPtr)
  {
    fatal("large-object output not supported in chosen format");
  }

  AH->StartBlobPtr(AH, AH->currToc, oid);

  return 1;
}

                                                
int
EndBlob(Archive *AHX, Oid oid)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;

  if (AH->EndBlobPtr)
  {
    AH->EndBlobPtr(AH, AH->currToc, oid);
  }

  return 1;
}

            
                    
            

   
                                                            
   
void
StartRestoreBlobs(ArchiveHandle *AH)
{
  RestoreOptions *ropt = AH->public.ropt;

  if (!ropt->single_txn)
  {
    if (AH->connection)
    {
      StartTransaction(&AH->public);
    }
    else
    {
      ahprintf(AH, "BEGIN;\n\n");
    }
  }

  AH->blobCount = 0;
}

   
                                                           
   
void
EndRestoreBlobs(ArchiveHandle *AH)
{
  RestoreOptions *ropt = AH->public.ropt;

  if (!ropt->single_txn)
  {
    if (AH->connection)
    {
      CommitTransaction(&AH->public);
    }
    else
    {
      ahprintf(AH, "COMMIT;\n\n");
    }
  }

  pg_log_info(ngettext("restored %d large object", "restored %d large objects", AH->blobCount), AH->blobCount);
}

   
                                                                
   
void
StartRestoreBlob(ArchiveHandle *AH, Oid oid, bool drop)
{
  bool old_blob_style = (AH->version < K_VERS_1_12);
  Oid loOid;

  AH->blobCount++;

                                
  AH->lo_buf_used = 0;

  pg_log_info("restoring large object with OID %u", oid);

                                                                 
  if (old_blob_style && drop)
  {
    DropBlobIfExists(AH, oid);
  }

  if (AH->connection)
  {
    if (old_blob_style)
    {
      loOid = lo_create(AH->connection, oid);
      if (loOid == 0 || loOid != oid)
      {
        fatal("could not create large object %u: %s", oid, PQerrorMessage(AH->connection));
      }
    }
    AH->loFd = lo_open(AH->connection, oid, INV_WRITE);
    if (AH->loFd == -1)
    {
      fatal("could not open large object %u: %s", oid, PQerrorMessage(AH->connection));
    }
  }
  else
  {
    if (old_blob_style)
    {
      ahprintf(AH, "SELECT pg_catalog.lo_open(pg_catalog.lo_create('%u'), %d);\n", oid, INV_WRITE);
    }
    else
    {
      ahprintf(AH, "SELECT pg_catalog.lo_open('%u', %d);\n", oid, INV_WRITE);
    }
  }

  AH->writingBlob = 1;
}

void
EndRestoreBlob(ArchiveHandle *AH, Oid oid)
{
  if (AH->lo_buf_used > 0)
  {
                                                  
    dump_lo_buf(AH);
  }

  AH->writingBlob = 0;

  if (AH->connection)
  {
    lo_close(AH->connection, AH->loFd);
    AH->loFd = -1;
  }
  else
  {
    ahprintf(AH, "SELECT pg_catalog.lo_close(0);\n\n");
  }
}

             
                          
             

void
SortTocFromFile(Archive *AHX)
{
  ArchiveHandle *AH = (ArchiveHandle *)AHX;
  RestoreOptions *ropt = AH->public.ropt;
  FILE *fh;
  char buf[100];
  bool incomplete_line;

                                                          
  ropt->idWanted = (bool *)pg_malloc0(sizeof(bool) * AH->maxDumpId);

                      
  fh = fopen(ropt->tocFile, PG_BINARY_R);
  if (!fh)
  {
    fatal("could not open TOC file \"%s\": %m", ropt->tocFile);
  }

  incomplete_line = false;
  while (fgets(buf, sizeof(buf), fh) != NULL)
  {
    bool prev_incomplete_line = incomplete_line;
    int buflen;
    char *cmnt;
    char *endptr;
    DumpId id;
    TocEntry *te;

       
                                                                         
                                                                         
                                                                         
                                                
       
    buflen = strlen(buf);
    if (buflen > 0 && buf[buflen - 1] == '\n')
    {
      incomplete_line = false;
    }
    else
    {
      incomplete_line = true;
    }
    if (prev_incomplete_line)
    {
      continue;
    }

                                          
    cmnt = strchr(buf, ';');
    if (cmnt != NULL)
    {
      cmnt[0] = '\0';
    }

                             
    if (strspn(buf, " \t\r\n") == strlen(buf))
    {
      continue;
    }

                                                          
    id = strtol(buf, &endptr, 10);
    if (endptr == buf || id <= 0 || id > AH->maxDumpId || ropt->idWanted[id - 1])
    {
      pg_log_warning("line ignored: %s", buf);
      continue;
    }

                        
    te = getTocEntryByDumpId(AH, id);
    if (!te)
    {
      fatal("could not find entry for ID %d", id);
    }

                        
    ropt->idWanted[id - 1] = true;

       
                                                                        
                                                                          
                                                                        
                                                                  
                                                                        
                                                                  
                                                               
                                                                        
                 
       
    _moveBefore(AH, AH->toc, te);
  }

  if (fclose(fh) != 0)
  {
    fatal("could not close TOC file: %m");
  }
}

                        
                                                               
                                       
                        

            
void
archputs(const char *s, Archive *AH)
{
  WriteData(AH, s, strlen(s));
  return;
}

            
int
archprintf(Archive *AH, const char *fmt, ...)
{
  int save_errno = errno;
  char *p;
  size_t len = 128;                                           
  size_t cnt;

  for (;;)
  {
    va_list args;

                               
    p = (char *)pg_malloc(len);

                                 
    errno = save_errno;
    va_start(args, fmt);
    cnt = pvsnprintf(p, len, fmt, args);
    va_end(args);

    if (cnt < len)
    {
      break;              
    }

                                                                      
    free(p);
    len = cnt;
  }

  WriteData(AH, p, cnt);
  free(p);
  return (int)cnt;
}

                                 
                                                                 
                                 

static void
SetOutput(ArchiveHandle *AH, const char *filename, int compression)
{
  int fn;

  if (filename)
  {
    if (strcmp(filename, "-") == 0)
    {
      fn = fileno(stdout);
    }
    else
    {
      fn = -1;
    }
  }
  else if (AH->FH)
  {
    fn = fileno(AH->FH);
  }
  else if (AH->fSpec)
  {
    fn = -1;
    filename = AH->fSpec;
  }
  else
  {
    fn = fileno(stdout);
  }

                                                       
#ifdef HAVE_LIBZ
  if (compression != 0)
  {
    char fmode[14];

                                                  
    sprintf(fmode, "wb%d", compression);
    if (fn >= 0)
    {
      AH->OF = gzdopen(dup(fn), fmode);
    }
    else
    {
      AH->OF = gzopen(filename, fmode);
    }
    AH->gzOut = 1;
  }
  else
#endif
  {                
    if (AH->mode == archModeAppend)
    {
      if (fn >= 0)
      {
        AH->OF = fdopen(dup(fn), PG_BINARY_A);
      }
      else
      {
        AH->OF = fopen(filename, PG_BINARY_A);
      }
    }
    else
    {
      if (fn >= 0)
      {
        AH->OF = fdopen(dup(fn), PG_BINARY_W);
      }
      else
      {
        AH->OF = fopen(filename, PG_BINARY_W);
      }
    }
    AH->gzOut = 0;
  }

  if (!AH->OF)
  {
    if (filename)
    {
      fatal("could not open output file \"%s\": %m", filename);
    }
    else
    {
      fatal("could not open output file: %m");
    }
  }
}

static OutputContext
SaveOutput(ArchiveHandle *AH)
{
  OutputContext sav;

  sav.OF = AH->OF;
  sav.gzOut = AH->gzOut;

  return sav;
}

static void
RestoreOutput(ArchiveHandle *AH, OutputContext savedContext)
{
  int res;

  errno = 0;                                       
  if (AH->gzOut)
  {
    res = GZCLOSE(AH->OF);
  }
  else
  {
    res = fclose(AH->OF);
  }

  if (res != 0)
  {
    fatal("could not close output file: %m");
  }

  AH->gzOut = savedContext.gzOut;
  AH->OF = savedContext.OF;
}

   
                                                             
   
int
ahprintf(ArchiveHandle *AH, const char *fmt, ...)
{
  int save_errno = errno;
  char *p;
  size_t len = 128;                                           
  size_t cnt;

  for (;;)
  {
    va_list args;

                               
    p = (char *)pg_malloc(len);

                                 
    errno = save_errno;
    va_start(args, fmt);
    cnt = pvsnprintf(p, len, fmt, args);
    va_end(args);

    if (cnt < len)
    {
      break;              
    }

                                                                      
    free(p);
    len = cnt;
  }

  ahwrite(p, 1, cnt, AH);
  free(p);
  return (int)cnt;
}

   
                                                                                   
   
static int
RestoringToDB(ArchiveHandle *AH)
{
  RestoreOptions *ropt = AH->public.ropt;

  return (ropt && ropt->useDB && AH->connection);
}

   
                                                                        
   
static void
dump_lo_buf(ArchiveHandle *AH)
{
  if (AH->connection)
  {
    size_t res;

    res = lo_write(AH->connection, AH->loFd, AH->lo_buf, AH->lo_buf_used);
    pg_log_debug(ngettext("wrote %lu byte of large object data (result = %lu)", "wrote %lu bytes of large object data (result = %lu)", AH->lo_buf_used), (unsigned long)AH->lo_buf_used, (unsigned long)res);
    if (res != AH->lo_buf_used)
    {
      fatal("could not write to large object (result: %lu, expected: %lu)", (unsigned long)res, (unsigned long)AH->lo_buf_used);
    }
  }
  else
  {
    PQExpBuffer buf = createPQExpBuffer();

    appendByteaLiteralAHX(buf, (const unsigned char *)AH->lo_buf, AH->lo_buf_used, AH);

                                                                       
    AH->writingBlob = 0;
    ahprintf(AH, "SELECT pg_catalog.lowrite(0, %s);\n", buf->data);
    AH->writingBlob = 1;

    destroyPQExpBuffer(buf);
  }
  AH->lo_buf_used = 0;
}

   
                                                                      
                                                                        
                                                                      
                                                
   
void
ahwrite(const void *ptr, size_t size, size_t nmemb, ArchiveHandle *AH)
{
  int bytes_written = 0;

  if (AH->writingBlob)
  {
    size_t remaining = size * nmemb;

    while (AH->lo_buf_used + remaining > AH->lo_buf_size)
    {
      size_t avail = AH->lo_buf_size - AH->lo_buf_used;

      memcpy((char *)AH->lo_buf + AH->lo_buf_used, ptr, avail);
      ptr = (const void *)((const char *)ptr + avail);
      remaining -= avail;
      AH->lo_buf_used += avail;
      dump_lo_buf(AH);
    }

    memcpy((char *)AH->lo_buf + AH->lo_buf_used, ptr, remaining);
    AH->lo_buf_used += remaining;

    bytes_written = size * nmemb;
  }
  else if (AH->gzOut)
  {
    bytes_written = GZWRITE(ptr, size, nmemb, AH->OF);
  }
  else if (AH->CustomOutPtr)
  {
    bytes_written = AH->CustomOutPtr(AH, ptr, size * nmemb);
  }

  else
  {
       
                                                                  
                                         
       
    if (RestoringToDB(AH))
    {
      bytes_written = ExecuteSqlCommandBuf(&AH->public, (const char *)ptr, size * nmemb);
    }
    else
    {
      bytes_written = fwrite(ptr, size, nmemb, AH->OF) * size;
    }
  }

  if (bytes_written != size * nmemb)
  {
    WRITE_ERROR_EXIT;
  }

  return;
}

                                              
void
warn_or_exit_horribly(ArchiveHandle *AH, const char *fmt, ...)
{
  va_list ap;

  switch (AH->stage)
  {

  case STAGE_NONE:
                            
    break;

  case STAGE_INITIALIZING:
    if (AH->stage != AH->lastErrorStage)
    {
      pg_log_generic(PG_LOG_INFO, "while INITIALIZING:");
    }
    break;

  case STAGE_PROCESSING:
    if (AH->stage != AH->lastErrorStage)
    {
      pg_log_generic(PG_LOG_INFO, "while PROCESSING TOC:");
    }
    break;

  case STAGE_FINALIZING:
    if (AH->stage != AH->lastErrorStage)
    {
      pg_log_generic(PG_LOG_INFO, "while FINALIZING:");
    }
    break;
  }
  if (AH->currentTE != NULL && AH->currentTE != AH->lastErrorTE)
  {
    pg_log_generic(PG_LOG_INFO, "from TOC entry %d; %u %u %s %s %s", AH->currentTE->dumpId, AH->currentTE->catalogId.tableoid, AH->currentTE->catalogId.oid, AH->currentTE->desc ? AH->currentTE->desc : "(no desc)", AH->currentTE->tag ? AH->currentTE->tag : "(no tag)", AH->currentTE->owner ? AH->currentTE->owner : "(no owner)");
  }
  AH->lastErrorStage = AH->stage;
  AH->lastErrorTE = AH->currentTE;

  va_start(ap, fmt);
  pg_log_generic_v(PG_LOG_ERROR, fmt, ap);
  va_end(ap);

  if (AH->public.exit_on_error)
  {
    exit_nicely(1);
  }
  else
  {
    AH->public.n_errors++;
  }
}

#ifdef NOT_USED

static void
_moveAfter(ArchiveHandle *AH, TocEntry *pos, TocEntry *te)
{
                           
  te->prev->next = te->next;
  te->next->prev = te->prev;

                                 
  te->prev = pos;
  te->next = pos->next;
  pos->next->prev = te;
  pos->next = te;
}
#endif

static void
_moveBefore(ArchiveHandle *AH, TocEntry *pos, TocEntry *te)
{
                           
  te->prev->next = te->next;
  te->next->prev = te->prev;

                                  
  te->prev = pos->prev;
  te->next = pos;
  pos->prev->next = te;
  pos->prev = te;
}

   
                                       
   
                                                                            
          
   
                                                                               
                                                                             
                                                                          
                                          
   
static void
buildTocEntryArrays(ArchiveHandle *AH)
{
  DumpId maxDumpId = AH->maxDumpId;
  TocEntry *te;

  AH->tocsByDumpId = (TocEntry **)pg_malloc0((maxDumpId + 1) * sizeof(TocEntry *));
  AH->tableDataId = (DumpId *)pg_malloc0((maxDumpId + 1) * sizeof(DumpId));

  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
                                                                    
    if (te->dumpId <= 0 || te->dumpId > maxDumpId)
    {
      fatal("bad dumpId");
    }

                                                        
    AH->tocsByDumpId[te->dumpId] = te;

       
                                                                         
                                                                         
                                                                        
                                                     
       
    if (strcmp(te->desc, "TABLE DATA") == 0 && te->nDeps > 0)
    {
      DumpId tableId = te->dependencies[0];

         
                                                                        
                                                                        
                                                                         
         
      if (tableId <= 0 || tableId > maxDumpId)
      {
        fatal("bad table dumpId for TABLE DATA item");
      }

      AH->tableDataId[tableId] = te->dumpId;
    }
  }
}

TocEntry *
getTocEntryByDumpId(ArchiveHandle *AH, DumpId id)
{
                                               
  if (AH->tocsByDumpId == NULL)
  {
    buildTocEntryArrays(AH);
  }

  if (id > 0 && id <= AH->maxDumpId)
  {
    return AH->tocsByDumpId[id];
  }

  return NULL;
}

teReqs
TocIDRequired(ArchiveHandle *AH, DumpId id)
{
  TocEntry *te = getTocEntryByDumpId(AH, id);

  if (!te)
  {
    return 0;
  }

  return te->reqs;
}

size_t
WriteOffset(ArchiveHandle *AH, pgoff_t o, int wasSet)
{
  int off;

                     
  AH->WriteBytePtr(AH, wasSet);

                                                                       
  for (off = 0; off < sizeof(pgoff_t); off++)
  {
    AH->WriteBytePtr(AH, o & 0xFF);
    o >>= 8;
  }
  return sizeof(pgoff_t) + 1;
}

int
ReadOffset(ArchiveHandle *AH, pgoff_t *o)
{
  int i;
  int off;
  int offsetFlg;

                          
  *o = 0;

                             
  if (AH->version < K_VERS_1_7)
  {
                                                     
    i = ReadInt(AH);
                          
    if (i < 0)
    {
      return K_OFFSET_POS_NOT_SET;
    }
    else if (i == 0)
    {
      return K_OFFSET_NO_DATA;
    }

                                                           
    *o = (pgoff_t)i;
    return K_OFFSET_POS_SET;
  }

     
                                                                            
                     
     
                                                                          
                                            
     
  offsetFlg = AH->ReadBytePtr(AH) & 0xFF;

  switch (offsetFlg)
  {
  case K_OFFSET_POS_NOT_SET:
  case K_OFFSET_NO_DATA:
  case K_OFFSET_POS_SET:

    break;

  default:
    fatal("unexpected data offset flag %d", offsetFlg);
  }

     
                    
     
  for (off = 0; off < AH->offSize; off++)
  {
    if (off < sizeof(pgoff_t))
    {
      *o |= ((pgoff_t)(AH->ReadBytePtr(AH))) << (off * 8);
    }
    else
    {
      if (AH->ReadBytePtr(AH) != 0)
      {
        fatal("file offset in dump file is too large");
      }
    }
  }

  return offsetFlg;
}

size_t
WriteInt(ArchiveHandle *AH, int i)
{
  int b;

     
                                                                          
                                                                             
                                                                      
                                                                             
              
     

                 
  if (i < 0)
  {
    AH->WriteBytePtr(AH, 1);
    i = -i;
  }
  else
  {
    AH->WriteBytePtr(AH, 0);
  }

  for (b = 0; b < AH->intSize; b++)
  {
    AH->WriteBytePtr(AH, i & 0xFF);
    i >>= 8;
  }

  return AH->intSize + 1;
}

int
ReadInt(ArchiveHandle *AH)
{
  int res = 0;
  int bv, b;
  int sign = 0;                       
  int bitShift = 0;

  if (AH->version > K_VERS_1_0)
  {
                          
    sign = AH->ReadBytePtr(AH);
  }

  for (b = 0; b < AH->intSize; b++)
  {
    bv = AH->ReadBytePtr(AH) & 0xFF;
    if (bv != 0)
    {
      res = res + (bv << bitShift);
    }
    bitShift += 8;
  }

  if (sign)
  {
    res = -res;
  }

  return res;
}

size_t
WriteStr(ArchiveHandle *AH, const char *c)
{
  size_t res;

  if (c)
  {
    int len = strlen(c);

    res = WriteInt(AH, len);
    AH->WriteBufPtr(AH, c, len);
    res += len;
  }
  else
  {
    res = WriteInt(AH, -1);
  }

  return res;
}

char *
ReadStr(ArchiveHandle *AH)
{
  char *buf;
  int l;

  l = ReadInt(AH);
  if (l < 0)
  {
    buf = NULL;
  }
  else
  {
    buf = (char *)pg_malloc(l + 1);
    AH->ReadBufPtr(AH, (void *)buf, l);

    buf[l] = '\0';
  }

  return buf;
}

static int
_discoverArchiveFormat(ArchiveHandle *AH)
{
  FILE *fh;
  char sig[6];                       
  size_t cnt;
  int wantClose = 0;

  pg_log_debug("attempting to ascertain archive format");

  if (AH->lookahead)
  {
    free(AH->lookahead);
  }

  AH->readHeader = 0;
  AH->lookaheadSize = 512;
  AH->lookahead = pg_malloc0(512);
  AH->lookaheadLen = 0;
  AH->lookaheadPos = 0;

  if (AH->fSpec)
  {
    struct stat st;

    wantClose = 1;

       
                                                                      
                                                         
       
    if (stat(AH->fSpec, &st) == 0 && S_ISDIR(st.st_mode))
    {
      char buf[MAXPGPATH];

      if (snprintf(buf, MAXPGPATH, "%s/toc.dat", AH->fSpec) >= MAXPGPATH)
      {
        fatal("directory name too long: \"%s\"", AH->fSpec);
      }
      if (stat(buf, &st) == 0 && S_ISREG(st.st_mode))
      {
        AH->format = archDirectory;
        return AH->format;
      }

#ifdef HAVE_LIBZ
      if (snprintf(buf, MAXPGPATH, "%s/toc.dat.gz", AH->fSpec) >= MAXPGPATH)
      {
        fatal("directory name too long: \"%s\"", AH->fSpec);
      }
      if (stat(buf, &st) == 0 && S_ISREG(st.st_mode))
      {
        AH->format = archDirectory;
        return AH->format;
      }
#endif
      fatal("directory \"%s\" does not appear to be a valid archive (\"toc.dat\" does not exist)", AH->fSpec);
      fh = NULL;                          
    }
    else
    {
      fh = fopen(AH->fSpec, PG_BINARY_R);
      if (!fh)
      {
        fatal("could not open input file \"%s\": %m", AH->fSpec);
      }
    }
  }
  else
  {
    fh = stdin;
    if (!fh)
    {
      fatal("could not open input file: %m");
    }
  }

  if ((cnt = fread(sig, 1, 5, fh)) != 5)
  {
    if (ferror(fh))
    {
      fatal("could not read input file: %m");
    }
    else
    {
      fatal("input file is too short (read %lu, expected 5)", (unsigned long)cnt);
    }
  }

                                              
  memcpy(&AH->lookahead[0], sig, 5);
  AH->lookaheadLen = 5;

  if (strncmp(sig, "PGDMP", 5) == 0)
  {
                                       
    AH->format = archCustom;
    AH->readHeader = 1;
  }
  else
  {
       
                                                                        
                                     
       
    cnt = fread(&AH->lookahead[AH->lookaheadLen], 1, 512 - AH->lookaheadLen, fh);
                                       
    AH->lookaheadLen += cnt;

    if (AH->lookaheadLen >= strlen(TEXT_DUMPALL_HEADER) && (strncmp(AH->lookahead, TEXT_DUMP_HEADER, strlen(TEXT_DUMP_HEADER)) == 0 || strncmp(AH->lookahead, TEXT_DUMPALL_HEADER, strlen(TEXT_DUMPALL_HEADER)) == 0))
    {
         
                                                                      
                  
         
      fatal("input file appears to be a text format dump. Please use psql.");
    }

    if (AH->lookaheadLen != 512)
    {
      if (feof(fh))
      {
        fatal("input file does not appear to be a valid archive (too short?)");
      }
      else
      {
        READ_ERROR_EXIT(fh);
      }
    }

    if (!isValidTarHeader(AH->lookahead))
    {
      fatal("input file does not appear to be a valid archive");
    }

    AH->format = archTar;
  }

                                      
  if (wantClose)
  {
    if (fclose(fh) != 0)
    {
      fatal("could not close input file: %m");
    }
                                                                       
    AH->readHeader = 0;
    AH->lookaheadLen = 0;
  }

  return AH->format;
}

   
                              
   
static ArchiveHandle *
_allocAH(const char *FileSpec, const ArchiveFormat fmt, const int compression, bool dosync, ArchiveMode mode, SetupWorkerPtrType setupWorkerPtr)
{
  ArchiveHandle *AH;

  pg_log_debug("allocating AH for %s, format %d", FileSpec, fmt);

  AH = (ArchiveHandle *)pg_malloc0(sizeof(ArchiveHandle));

  AH->version = K_VERS_SELF;

                                                             
  AH->public.encoding = 0;                   
  AH->public.std_strings = false;

                          
  AH->public.exit_on_error = true;
  AH->public.n_errors = 0;

  AH->archiveDumpVersion = PG_VERSION;

  AH->createDate = time(NULL);

  AH->intSize = sizeof(int);
  AH->offSize = sizeof(pgoff_t);
  if (FileSpec)
  {
    AH->fSpec = pg_strdup(FileSpec);

       
                                 
       
                                                                           
                                         
       
  }
  else
  {
    AH->fSpec = NULL;
  }

  AH->currUser = NULL;                    
  AH->currSchema = NULL;                
  AH->currTablespace = NULL;            
  AH->currTableAm = NULL;               

  AH->toc = (TocEntry *)pg_malloc0(sizeof(TocEntry));

  AH->toc->next = AH->toc;
  AH->toc->prev = AH->toc;

  AH->mode = mode;
  AH->compression = compression;
  AH->dosync = dosync;

  memset(&(AH->sqlparse), 0, sizeof(AH->sqlparse));

                                                            
  AH->gzOut = 0;
  AH->OF = stdout;

     
                                                                          
                                                                         
                                                                       
     
#ifdef WIN32
  if ((fmt != archNull || compression != 0) && (AH->fSpec == NULL || strcmp(AH->fSpec, "") == 0))
  {
    if (mode == archModeWrite)
    {
      _setmode(fileno(stdout), O_BINARY);
    }
    else
    {
      _setmode(fileno(stdin), O_BINARY);
    }
  }
#endif

  AH->SetupWorkerPtr = setupWorkerPtr;

  if (fmt == archUnknown)
  {
    AH->format = _discoverArchiveFormat(AH);
  }
  else
  {
    AH->format = fmt;
  }

  switch (AH->format)
  {
  case archCustom:
    InitArchiveFmt_Custom(AH);
    break;

  case archNull:
    InitArchiveFmt_Null(AH);
    break;

  case archDirectory:
    InitArchiveFmt_Directory(AH);
    break;

  case archTar:
    InitArchiveFmt_Tar(AH);
    break;

  default:
    fatal("unrecognized file format \"%d\"", fmt);
  }

  return AH;
}

   
                                       
   
void
WriteDataChunks(ArchiveHandle *AH, ParallelState *pstate)
{
  TocEntry *te;

  if (pstate && pstate->numWorkers > 1)
  {
       
                                                                   
                                                                         
                                                                        
                                                                      
                                                                     
       
    TocEntry **tes;
    int ntes;

    tes = (TocEntry **)pg_malloc(AH->tocCount * sizeof(TocEntry *));
    ntes = 0;
    for (te = AH->toc->next; te != AH->toc; te = te->next)
    {
                                                           
      if (!te->dataDumper)
      {
        continue;
      }
                                                    
      if ((te->reqs & REQ_DATA) == 0)
      {
        continue;
      }

      tes[ntes++] = te;
    }

    if (ntes > 1)
    {
      qsort((void *)tes, ntes, sizeof(TocEntry *), TocEntrySizeCompare);
    }

    for (int i = 0; i < ntes; i++)
    {
      DispatchJobForTocEntry(AH, pstate, tes[i], ACT_DUMP, mark_dump_job_done, NULL);
    }

    pg_free(tes);

                                         
    WaitForWorkers(AH, pstate, WFW_ALL_IDLE);
  }
  else
  {
                                                                      
    for (te = AH->toc->next; te != AH->toc; te = te->next)
    {
                                                     
      if (!te->dataDumper)
      {
        continue;
      }
      if ((te->reqs & REQ_DATA) == 0)
      {
        continue;
      }

      WriteDataChunksForTocEntry(AH, te);
    }
  }
}

   
                                                                           
                         
   
                                                                 
   
static void
mark_dump_job_done(ArchiveHandle *AH, TocEntry *te, int status, void *callback_data)
{
  pg_log_info("finished item %d %s %s", te->dumpId, te->desc, te->tag);

  if (status != 0)
  {
    fatal("worker process failed: exit code %d", status);
  }
}

void
WriteDataChunksForTocEntry(ArchiveHandle *AH, TocEntry *te)
{
  StartDataPtrType startPtr;
  EndDataPtrType endPtr;

  AH->currToc = te;

  if (strcmp(te->desc, "BLOBS") == 0)
  {
    startPtr = AH->StartBlobsPtr;
    endPtr = AH->EndBlobsPtr;
  }
  else
  {
    startPtr = AH->StartDataPtr;
    endPtr = AH->EndDataPtr;
  }

  if (startPtr != NULL)
  {
    (*startPtr)(AH, te);
  }

     
                                                                      
     
  te->dataDumper((Archive *)AH, te->dataDumperArg);

  if (endPtr != NULL)
  {
    (*endPtr)(AH, te);
  }

  AH->currToc = NULL;
}

void
WriteToc(ArchiveHandle *AH)
{
  TocEntry *te;
  char workbuf[32];
  int tocCount;
  int i;

                                                  
  tocCount = 0;
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    if ((te->reqs & (REQ_SCHEMA | REQ_DATA | REQ_SPECIAL)) != 0)
    {
      tocCount++;
    }
  }

                                                     

  WriteInt(AH, tocCount);

  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    if ((te->reqs & (REQ_SCHEMA | REQ_DATA | REQ_SPECIAL)) == 0)
    {
      continue;
    }

    WriteInt(AH, te->dumpId);
    WriteInt(AH, te->dataDumper ? 1 : 0);

                                                            
    sprintf(workbuf, "%u", te->catalogId.tableoid);
    WriteStr(AH, workbuf);
    sprintf(workbuf, "%u", te->catalogId.oid);
    WriteStr(AH, workbuf);

    WriteStr(AH, te->tag);
    WriteStr(AH, te->desc);
    WriteInt(AH, te->section);
    WriteStr(AH, te->defn);
    WriteStr(AH, te->dropStmt);
    WriteStr(AH, te->copyStmt);
    WriteStr(AH, te->namespace);
    WriteStr(AH, te->tablespace);
    WriteStr(AH, te->tableam);
    WriteStr(AH, te->owner);
    WriteStr(AH, "false");

                                   
    for (i = 0; i < te->nDeps; i++)
    {
      sprintf(workbuf, "%d", te->dependencies[i]);
      WriteStr(AH, workbuf);
    }
    WriteStr(AH, NULL);                     

    if (AH->WriteExtraTocPtr)
    {
      AH->WriteExtraTocPtr(AH, te);
    }
  }
}

void
ReadToc(ArchiveHandle *AH)
{
  int i;
  char *tmp;
  DumpId *deps;
  int depIdx;
  int depSize;
  TocEntry *te;

  AH->tocCount = ReadInt(AH);
  AH->maxDumpId = 0;

  for (i = 0; i < AH->tocCount; i++)
  {
    te = (TocEntry *)pg_malloc0(sizeof(TocEntry));
    te->dumpId = ReadInt(AH);

    if (te->dumpId > AH->maxDumpId)
    {
      AH->maxDumpId = te->dumpId;
    }

                      
    if (te->dumpId <= 0)
    {
      fatal("entry ID %d out of range -- perhaps a corrupt TOC", te->dumpId);
    }

    te->hadDumper = ReadInt(AH);

    if (AH->version >= K_VERS_1_8)
    {
      tmp = ReadStr(AH);
      sscanf(tmp, "%u", &te->catalogId.tableoid);
      free(tmp);
    }
    else
    {
      te->catalogId.tableoid = InvalidOid;
    }
    tmp = ReadStr(AH);
    sscanf(tmp, "%u", &te->catalogId.oid);
    free(tmp);

    te->tag = ReadStr(AH);
    te->desc = ReadStr(AH);

    if (AH->version >= K_VERS_1_11)
    {
      te->section = ReadInt(AH);
    }
    else
    {
         
                                                                      
                                                                    
                                     
         
      if (strcmp(te->desc, "COMMENT") == 0 || strcmp(te->desc, "ACL") == 0 || strcmp(te->desc, "ACL LANGUAGE") == 0)
      {
        te->section = SECTION_NONE;
      }
      else if (strcmp(te->desc, "TABLE DATA") == 0 || strcmp(te->desc, "BLOBS") == 0 || strcmp(te->desc, "BLOB COMMENTS") == 0)
      {
        te->section = SECTION_DATA;
      }
      else if (strcmp(te->desc, "CONSTRAINT") == 0 || strcmp(te->desc, "CHECK CONSTRAINT") == 0 || strcmp(te->desc, "FK CONSTRAINT") == 0 || strcmp(te->desc, "INDEX") == 0 || strcmp(te->desc, "RULE") == 0 || strcmp(te->desc, "TRIGGER") == 0)
      {
        te->section = SECTION_POST_DATA;
      }
      else
      {
        te->section = SECTION_PRE_DATA;
      }
    }

    te->defn = ReadStr(AH);
    te->dropStmt = ReadStr(AH);

    if (AH->version >= K_VERS_1_3)
    {
      te->copyStmt = ReadStr(AH);
    }

    if (AH->version >= K_VERS_1_6)
    {
      te->namespace = ReadStr(AH);
    }

    if (AH->version >= K_VERS_1_10)
    {
      te->tablespace = ReadStr(AH);
    }

    if (AH->version >= K_VERS_1_14)
    {
      te->tableam = ReadStr(AH);
    }

    te->owner = ReadStr(AH);
    if (AH->version < K_VERS_1_9 || strcmp(ReadStr(AH), "true") == 0)
    {
      pg_log_warning("restoring tables WITH OIDS is not supported anymore");
    }

                                     
    if (AH->version >= K_VERS_1_5)
    {
      depSize = 100;
      deps = (DumpId *)pg_malloc(sizeof(DumpId) * depSize);
      depIdx = 0;
      for (;;)
      {
        tmp = ReadStr(AH);
        if (!tmp)
        {
          break;                  
        }
        if (depIdx >= depSize)
        {
          depSize *= 2;
          deps = (DumpId *)pg_realloc(deps, sizeof(DumpId) * depSize);
        }
        sscanf(tmp, "%d", &deps[depIdx]);
        free(tmp);
        depIdx++;
      }

      if (depIdx > 0)                               
      {
        deps = (DumpId *)pg_realloc(deps, sizeof(DumpId) * depIdx);
        te->dependencies = deps;
        te->nDeps = depIdx;
      }
      else
      {
        free(deps);
        te->dependencies = NULL;
        te->nDeps = 0;
      }
    }
    else
    {
      te->dependencies = NULL;
      te->nDeps = 0;
    }
    te->dataLength = 0;

    if (AH->ReadExtraTocPtr)
    {
      AH->ReadExtraTocPtr(AH, te);
    }

    pg_log_debug("read TOC entry %d (ID %d) for %s %s", i, te->dumpId, te->desc, te->tag);

                                                     
    te->prev = AH->toc->prev;
    AH->toc->prev->next = te;
    AH->toc->prev = te;
    te->next = AH->toc;

                                                                 
    if (strcmp(te->desc, "ENCODING") == 0)
    {
      processEncodingEntry(AH, te);
    }
    else if (strcmp(te->desc, "STDSTRINGS") == 0)
    {
      processStdStringsEntry(AH, te);
    }
    else if (strcmp(te->desc, "SEARCHPATH") == 0)
    {
      processSearchPathEntry(AH, te);
    }
  }
}

static void
processEncodingEntry(ArchiveHandle *AH, TocEntry *te)
{
                                                                  
  char *defn = pg_strdup(te->defn);
  char *ptr1;
  char *ptr2 = NULL;
  int encoding;

  ptr1 = strchr(defn, '\'');
  if (ptr1)
  {
    ptr2 = strchr(++ptr1, '\'');
  }
  if (ptr2)
  {
    *ptr2 = '\0';
    encoding = pg_char_to_encoding(ptr1);
    if (encoding < 0)
    {
      fatal("unrecognized encoding \"%s\"", ptr1);
    }
    AH->public.encoding = encoding;
  }
  else
  {
    fatal("invalid ENCODING item: %s", te->defn);
  }

  free(defn);
}

static void
processStdStringsEntry(ArchiveHandle *AH, TocEntry *te)
{
                                                                            
  char *ptr1;

  ptr1 = strchr(te->defn, '\'');
  if (ptr1 && strncmp(ptr1, "'on'", 4) == 0)
  {
    AH->public.std_strings = true;
  }
  else if (ptr1 && strncmp(ptr1, "'off'", 5) == 0)
  {
    AH->public.std_strings = false;
  }
  else
  {
    fatal("invalid STDSTRINGS item: %s", te->defn);
  }
}

static void
processSearchPathEntry(ArchiveHandle *AH, TocEntry *te)
{
     
                                                                            
                             
     
  AH->public.searchpath = pg_strdup(te->defn);
}

static void
StrictNamesCheck(RestoreOptions *ropt)
{
  const char *missing_name;

  Assert(ropt->strict_names);

  if (ropt->schemaNames.head != NULL)
  {
    missing_name = simple_string_list_not_touched(&ropt->schemaNames);
    if (missing_name != NULL)
    {
      fatal("schema \"%s\" not found", missing_name);
    }
  }

  if (ropt->tableNames.head != NULL)
  {
    missing_name = simple_string_list_not_touched(&ropt->tableNames);
    if (missing_name != NULL)
    {
      fatal("table \"%s\" not found", missing_name);
    }
  }

  if (ropt->indexNames.head != NULL)
  {
    missing_name = simple_string_list_not_touched(&ropt->indexNames);
    if (missing_name != NULL)
    {
      fatal("index \"%s\" not found", missing_name);
    }
  }

  if (ropt->functionNames.head != NULL)
  {
    missing_name = simple_string_list_not_touched(&ropt->functionNames);
    if (missing_name != NULL)
    {
      fatal("function \"%s\" not found", missing_name);
    }
  }

  if (ropt->triggerNames.head != NULL)
  {
    missing_name = simple_string_list_not_touched(&ropt->triggerNames);
    if (missing_name != NULL)
    {
      fatal("trigger \"%s\" not found", missing_name);
    }
  }
}

   
                                                        
   
                                                                    
                                                                         
                                                                       
   
static teReqs
_tocEntryRequired(TocEntry *te, teSection curSection, ArchiveHandle *AH)
{
  teReqs res = REQ_SCHEMA | REQ_DATA;
  RestoreOptions *ropt = AH->public.ropt;

                                         
  if (strcmp(te->desc, "ENCODING") == 0 || strcmp(te->desc, "STDSTRINGS") == 0 || strcmp(te->desc, "SEARCHPATH") == 0)
  {
    return REQ_SPECIAL;
  }

     
                                                                         
                                                                             
               
     
  if (strcmp(te->desc, "DATABASE") == 0 || strcmp(te->desc, "DATABASE PROPERTIES") == 0)
  {
    if (ropt->createDB)
    {
      return REQ_SCHEMA;
    }
    else
    {
      return 0;
    }
  }

     
                                                                    
     

                                       
  if (ropt->aclsSkip && _tocEntryIsACL(te))
  {
    return 0;
  }

                                          
  if (ropt->no_comments && strcmp(te->desc, "COMMENT") == 0)
  {
    return 0;
  }

     
                                                                          
         
     
  if (ropt->no_publications && (strcmp(te->desc, "PUBLICATION") == 0 || strcmp(te->desc, "PUBLICATION TABLE") == 0))
  {
    return 0;
  }

                                                 
  if (ropt->no_security_labels && strcmp(te->desc, "SECURITY LABEL") == 0)
  {
    return 0;
  }

                                               
  if (ropt->no_subscriptions && strcmp(te->desc, "SUBSCRIPTION") == 0)
  {
    return 0;
  }

                                                         
  switch (curSection)
  {
  case SECTION_PRE_DATA:
    if (!(ropt->dumpSections & DUMP_PRE_DATA))
    {
      return 0;
    }
    break;
  case SECTION_DATA:
    if (!(ropt->dumpSections & DUMP_DATA))
    {
      return 0;
    }
    break;
  case SECTION_POST_DATA:
    if (!(ropt->dumpSections & DUMP_POST_DATA))
    {
      return 0;
    }
    break;
  default:
                                                   
    return 0;
  }

                                                                 
  if (ropt->idWanted && !ropt->idWanted[te->dumpId - 1])
  {
    return 0;
  }

     
                                               
     
  if (strcmp(te->desc, "ACL") == 0 || strcmp(te->desc, "COMMENT") == 0 || strcmp(te->desc, "SECURITY LABEL") == 0)
  {
                                                                         
    if (strncmp(te->tag, "DATABASE ", 9) == 0)
    {
      if (!ropt->createDB)
      {
        return 0;
      }
    }
    else if (ropt->schemaNames.head != NULL || ropt->schemaExcludeNames.head != NULL || ropt->selTypes)
    {
         
                                                                         
                                                                        
                                                                       
                                                                     
                                                
         
                                                                     
                                                               
                                                                     
                               
         
                                                                        
                                                                         
                  
         
      if (te->nDeps != 1 || TocIDRequired(AH, te->dependencies[0]) == 0)
      {
        return 0;
      }
    }
  }
  else
  {
                                                                   
    if (ropt->schemaNames.head != NULL)
    {
                                                       
      if (!te->namespace)
      {
        return 0;
      }
      if (!simple_string_list_member(&ropt->schemaNames, te->namespace))
      {
        return 0;
      }
    }

    if (ropt->schemaExcludeNames.head != NULL && te->namespace && simple_string_list_member(&ropt->schemaExcludeNames, te->namespace))
    {
      return 0;
    }

    if (ropt->selTypes)
    {
      if (strcmp(te->desc, "TABLE") == 0 || strcmp(te->desc, "TABLE DATA") == 0 || strcmp(te->desc, "VIEW") == 0 || strcmp(te->desc, "FOREIGN TABLE") == 0 || strcmp(te->desc, "MATERIALIZED VIEW") == 0 || strcmp(te->desc, "MATERIALIZED VIEW DATA") == 0 || strcmp(te->desc, "SEQUENCE") == 0 || strcmp(te->desc, "SEQUENCE SET") == 0)
      {
        if (!ropt->selTable)
        {
          return 0;
        }
        if (ropt->tableNames.head != NULL && !simple_string_list_member(&ropt->tableNames, te->tag))
        {
          return 0;
        }
      }
      else if (strcmp(te->desc, "INDEX") == 0)
      {
        if (!ropt->selIndex)
        {
          return 0;
        }
        if (ropt->indexNames.head != NULL && !simple_string_list_member(&ropt->indexNames, te->tag))
        {
          return 0;
        }
      }
      else if (strcmp(te->desc, "FUNCTION") == 0 || strcmp(te->desc, "AGGREGATE") == 0 || strcmp(te->desc, "PROCEDURE") == 0)
      {
        if (!ropt->selFunction)
        {
          return 0;
        }
        if (ropt->functionNames.head != NULL && !simple_string_list_member(&ropt->functionNames, te->tag))
        {
          return 0;
        }
      }
      else if (strcmp(te->desc, "TRIGGER") == 0)
      {
        if (!ropt->selTrigger)
        {
          return 0;
        }
        if (ropt->triggerNames.head != NULL && !simple_string_list_member(&ropt->triggerNames, te->tag))
        {
          return 0;
        }
      }
      else
      {
        return 0;
      }
    }
  }

     
                                                                             
                                                                         
                                                                          
                           
     
  if (!te->hadDumper)
  {
       
                                                                          
                                                                      
                                                                      
                                                                         
                      
       
    if (strcmp(te->desc, "SEQUENCE SET") == 0 || strcmp(te->desc, "BLOB") == 0 || (strcmp(te->desc, "ACL") == 0 && strncmp(te->tag, "LARGE OBJECT ", 13) == 0) || (strcmp(te->desc, "COMMENT") == 0 && strncmp(te->tag, "LARGE OBJECT ", 13) == 0) || (strcmp(te->desc, "SECURITY LABEL") == 0 && strncmp(te->tag, "LARGE OBJECT ", 13) == 0))
    {
      res = res & REQ_DATA;
    }
    else
    {
      res = res & ~REQ_DATA;
    }
  }

                                                                     
  if (!te->defn || !te->defn[0])
  {
    res = res & ~REQ_SCHEMA;
  }

     
                                                                           
                       
     
  if ((strcmp(te->desc, "<Init>") == 0) && (strcmp(te->tag, "Max OID") == 0))
  {
    return 0;
  }

                                      
  if (ropt->schemaOnly)
  {
       
                                                                       
       
                                                                        
                                                                        
                                                                          
                                                        
       
    if (!(ropt->sequence_data && strcmp(te->desc, "SEQUENCE SET") == 0) && !(ropt->binary_upgrade && (strcmp(te->desc, "BLOB") == 0 || (strcmp(te->desc, "ACL") == 0 && strncmp(te->tag, "LARGE OBJECT ", 13) == 0) || (strcmp(te->desc, "COMMENT") == 0 && strncmp(te->tag, "LARGE OBJECT ", 13) == 0) || (strcmp(te->desc, "SECURITY LABEL") == 0 && strncmp(te->tag, "LARGE OBJECT ", 13) == 0))))
    {
      res = res & REQ_SCHEMA;
    }
  }

                                    
  if (ropt->dataOnly)
  {
    res = res & REQ_DATA;
  }

  return res;
}

   
                                                            
   
                                                                   
   
static RestorePass
_tocEntryRestorePass(TocEntry *te)
{
                                                         
  if (strcmp(te->desc, "ACL") == 0 || strcmp(te->desc, "ACL LANGUAGE") == 0 || strcmp(te->desc, "DEFAULT ACL") == 0)
  {
    return RESTORE_PASS_ACL;
  }
  if (strcmp(te->desc, "EVENT TRIGGER") == 0 || strcmp(te->desc, "MATERIALIZED VIEW DATA") == 0)
  {
    return RESTORE_PASS_POST_ACL;
  }

     
                                                                           
                                                                         
                                                                           
                                                
     
  if (strcmp(te->desc, "COMMENT") == 0 && strncmp(te->tag, "EVENT TRIGGER ", 14) == 0)
  {
    return RESTORE_PASS_POST_ACL;
  }

                                                 
  return RESTORE_PASS_MAIN;
}

   
                                       
   
                                                                         
                                                                             
                               
   
static bool
_tocEntryIsACL(TocEntry *te)
{
                                                         
  if (strcmp(te->desc, "ACL") == 0 || strcmp(te->desc, "ACL LANGUAGE") == 0 || strcmp(te->desc, "DEFAULT ACL") == 0)
  {
    return true;
  }
  return false;
}

   
                                                                           
                                                      
   
static void
_doSetFixedOutputState(ArchiveHandle *AH)
{
  RestoreOptions *ropt = AH->public.ropt;

     
                                                                             
     
  ahprintf(AH, "SET statement_timeout = 0;\n");
  ahprintf(AH, "SET lock_timeout = 0;\n");
  ahprintf(AH, "SET idle_in_transaction_session_timeout = 0;\n");

                                                 
  ahprintf(AH, "SET client_encoding = '%s';\n", pg_encoding_to_char(AH->public.encoding));

                                                
  ahprintf(AH, "SET standard_conforming_strings = %s;\n", AH->public.std_strings ? "on" : "off");

                                                 
  if (ropt && ropt->use_role)
  {
    ahprintf(AH, "SET ROLE %s;\n", fmtId(ropt->use_role));
  }

                                        
  if (AH->public.searchpath)
  {
    ahprintf(AH, "%s", AH->public.searchpath);
  }

                                               
  ahprintf(AH, "SET check_function_bodies = false;\n");

                                                       
  ahprintf(AH, "SET xmloption = content;\n");

                                  
  ahprintf(AH, "SET client_min_messages = warning;\n");
  if (!AH->public.std_strings)
  {
    ahprintf(AH, "SET escape_string_warning = off;\n");
  }

                                 
  if (ropt && ropt->enable_row_security)
  {
    ahprintf(AH, "SET row_security = on;\n");
  }
  else
  {
    ahprintf(AH, "SET row_security = off;\n");
  }

  ahprintf(AH, "\n");
}

   
                                                                     
                                                                           
                                           
   
static void
_doSetSessionAuth(ArchiveHandle *AH, const char *user)
{
  PQExpBuffer cmd = createPQExpBuffer();

  appendPQExpBufferStr(cmd, "SET SESSION AUTHORIZATION ");

     
                                                                    
     
  if (user && *user)
  {
    appendStringLiteralAHX(cmd, user, AH);
  }
  else
  {
    appendPQExpBufferStr(cmd, "DEFAULT");
  }
  appendPQExpBufferChar(cmd, ';');

  if (RestoringToDB(AH))
  {
    PGresult *res;

    res = PQexec(AH->connection, cmd->data);

    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
    {
                                                                     
      fatal("could not set session user to \"%s\": %s", user, PQerrorMessage(AH->connection));
    }

    PQclear(res);
  }
  else
  {
    ahprintf(AH, "%s\n\n", cmd->data);
  }

  destroyPQExpBuffer(cmd);
}

   
                                                            
   
                                                                 
                                                                      
                      
   
static void
_reconnectToDB(ArchiveHandle *AH, const char *dbname)
{
  if (RestoringToDB(AH))
  {
    ReconnectToServer(AH, dbname);
  }
  else
  {
    PQExpBufferData connectbuf;

    initPQExpBuffer(&connectbuf);
    appendPsqlMetaConnect(&connectbuf, dbname);
    ahprintf(AH, "%s\n", connectbuf.data);
    termPQExpBuffer(&connectbuf);
  }

     
                                                                          
                                                                    
     
  if (AH->currUser)
  {
    free(AH->currUser);
  }
  AH->currUser = NULL;

                                                                            
  if (AH->currSchema)
  {
    free(AH->currSchema);
  }
  AH->currSchema = NULL;
  if (AH->currTablespace)
  {
    free(AH->currTablespace);
  }
  AH->currTablespace = NULL;

                                
  _doSetFixedOutputState(AH);
}

   
                                                                           
   
                                                                         
   
static void
_becomeUser(ArchiveHandle *AH, const char *user)
{
  if (!user)
  {
    user = "";                          
  }

  if (AH->currUser && strcmp(AH->currUser, user) == 0)
  {
    return;                             
  }

  _doSetSessionAuth(AH, user);

     
                                                                          
               
     
  if (AH->currUser)
  {
    free(AH->currUser);
  }
  AH->currUser = pg_strdup(user);
}

   
                                                       
                                                                   
   
static void
_becomeOwner(ArchiveHandle *AH, TocEntry *te)
{
  RestoreOptions *ropt = AH->public.ropt;

  if (ropt && (ropt->noOwner || !ropt->use_setsessauth))
  {
    return;
  }

  _becomeUser(AH, te->owner);
}

   
                                                                           
                           
   
static void
_selectOutputSchema(ArchiveHandle *AH, const char *schemaName)
{
  PQExpBuffer qry;

     
                                                                           
                                                                     
                                                                           
                                               
     
  if (AH->public.searchpath)
  {
    return;
  }

  if (!schemaName || *schemaName == '\0' || (AH->currSchema && strcmp(AH->currSchema, schemaName) == 0))
  {
    return;                             
  }

  qry = createPQExpBuffer();

  appendPQExpBuffer(qry, "SET search_path = %s", fmtId(schemaName));
  if (strcmp(schemaName, "pg_catalog") != 0)
  {
    appendPQExpBufferStr(qry, ", pg_catalog");
  }

  if (RestoringToDB(AH))
  {
    PGresult *res;

    res = PQexec(AH->connection, qry->data);

    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      warn_or_exit_horribly(AH, "could not set search_path to \"%s\": %s", schemaName, PQerrorMessage(AH->connection));
    }

    PQclear(res);
  }
  else
  {
    ahprintf(AH, "%s;\n\n", qry->data);
  }

  if (AH->currSchema)
  {
    free(AH->currSchema);
  }
  AH->currSchema = pg_strdup(schemaName);

  destroyPQExpBuffer(qry);
}

   
                                                                            
                           
   
static void
_selectTablespace(ArchiveHandle *AH, const char *tablespace)
{
  RestoreOptions *ropt = AH->public.ropt;
  PQExpBuffer qry;
  const char *want, *have;

                                           
  if (ropt->noTablespace)
  {
    return;
  }

  have = AH->currTablespace;
  want = tablespace;

                                                        
  if (!want)
  {
    return;
  }

  if (have && strcmp(want, have) == 0)
  {
    return;                             
  }

  qry = createPQExpBuffer();

  if (strcmp(want, "") == 0)
  {
                                                             
    appendPQExpBufferStr(qry, "SET default_tablespace = ''");
  }
  else
  {
                                        
    appendPQExpBuffer(qry, "SET default_tablespace = %s", fmtId(want));
  }

  if (RestoringToDB(AH))
  {
    PGresult *res;

    res = PQexec(AH->connection, qry->data);

    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      warn_or_exit_horribly(AH, "could not set default_tablespace to %s: %s", fmtId(want), PQerrorMessage(AH->connection));
    }

    PQclear(res);
  }
  else
  {
    ahprintf(AH, "%s;\n\n", qry->data);
  }

  if (AH->currTablespace)
  {
    free(AH->currTablespace);
  }
  AH->currTablespace = pg_strdup(want);

  destroyPQExpBuffer(qry);
}

   
                                                                   
   
static void
_selectTableAccessMethod(ArchiveHandle *AH, const char *tableam)
{
  PQExpBuffer cmd;
  const char *want, *have;

  have = AH->currTableAm;
  want = tableam;

  if (!want)
  {
    return;
  }

  if (have && strcmp(want, have) == 0)
  {
    return;
  }

  cmd = createPQExpBuffer();
  appendPQExpBuffer(cmd, "SET default_table_access_method = %s;", fmtId(want));

  if (RestoringToDB(AH))
  {
    PGresult *res;

    res = PQexec(AH->connection, cmd->data);

    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      warn_or_exit_horribly(AH, "could not set default_table_access_method: %s", PQerrorMessage(AH->connection));
    }

    PQclear(res);
  }
  else
  {
    ahprintf(AH, "%s\n\n", cmd->data);
  }

  destroyPQExpBuffer(cmd);

  AH->currTableAm = pg_strdup(want);
}

   
                                                                        
   
                                        
   
static void
_getObjectDescription(PQExpBuffer buf, TocEntry *te, ArchiveHandle *AH)
{
  const char *type = te->desc;

                                               
  if (strcmp(type, "VIEW") == 0 || strcmp(type, "SEQUENCE") == 0 || strcmp(type, "MATERIALIZED VIEW") == 0)
  {
    type = "TABLE";
  }

                                                     
  if (strcmp(type, "COLLATION") == 0 || strcmp(type, "CONVERSION") == 0 || strcmp(type, "DOMAIN") == 0 || strcmp(type, "TABLE") == 0 || strcmp(type, "TYPE") == 0 || strcmp(type, "FOREIGN TABLE") == 0 || strcmp(type, "TEXT SEARCH DICTIONARY") == 0 || strcmp(type, "TEXT SEARCH CONFIGURATION") == 0 || strcmp(type, "STATISTICS") == 0 ||
                                        
      strcmp(type, "DATABASE") == 0 || strcmp(type, "PROCEDURAL LANGUAGE") == 0 || strcmp(type, "SCHEMA") == 0 || strcmp(type, "EVENT TRIGGER") == 0 || strcmp(type, "FOREIGN DATA WRAPPER") == 0 || strcmp(type, "SERVER") == 0 || strcmp(type, "PUBLICATION") == 0 || strcmp(type, "SUBSCRIPTION") == 0 || strcmp(type, "USER MAPPING") == 0)
  {
    appendPQExpBuffer(buf, "%s ", type);
    if (te->namespace && *te->namespace)
    {
      appendPQExpBuffer(buf, "%s.", fmtId(te->namespace));
    }
    appendPQExpBufferStr(buf, fmtId(te->tag));
    return;
  }

                                                                      
  if (strcmp(type, "BLOB") == 0)
  {
    appendPQExpBuffer(buf, "LARGE OBJECT %s", te->tag);
    return;
  }

     
                                                                         
                                                               
     
  if (strcmp(type, "AGGREGATE") == 0 || strcmp(type, "FUNCTION") == 0 || strcmp(type, "OPERATOR") == 0 || strcmp(type, "OPERATOR CLASS") == 0 || strcmp(type, "OPERATOR FAMILY") == 0 || strcmp(type, "PROCEDURE") == 0)
  {
                                                               
    char *first = pg_strdup(te->dropStmt + 5);
    char *last;

                                           
    last = first + strlen(first) - 1;

                                              
    while (last >= first && (*last == '\n' || *last == ';'))
    {
      last--;
    }
    *(last + 1) = '\0';

    appendPQExpBufferStr(buf, first);

    free(first);
    return;
  }

  pg_log_warning("don't know how to set owner for object type \"%s\"", type);
}

   
                                                                         
   
                                                                        
                                                                          
                                                                         
   
static void
_printTocEntry(ArchiveHandle *AH, TocEntry *te, bool isData)
{
  RestoreOptions *ropt = AH->public.ropt;

                                                                    
  _becomeOwner(AH, te);
  _selectOutputSchema(AH, te->namespace);
  _selectTablespace(AH, te->tablespace);
  _selectTableAccessMethod(AH, te->tableam);

                                    
  if (!AH->noTocComments)
  {
    const char *pfx;
    char *sanitized_name;
    char *sanitized_schema;
    char *sanitized_owner;

    if (isData)
    {
      pfx = "Data for ";
    }
    else
    {
      pfx = "";
    }

    ahprintf(AH, "--\n");
    if (AH->public.verbose)
    {
      ahprintf(AH, "-- TOC entry %d (class %u OID %u)\n", te->dumpId, te->catalogId.tableoid, te->catalogId.oid);
      if (te->nDeps > 0)
      {
        int i;

        ahprintf(AH, "-- Dependencies:");
        for (i = 0; i < te->nDeps; i++)
        {
          ahprintf(AH, " %d", te->dependencies[i]);
        }
        ahprintf(AH, "\n");
      }
    }

    sanitized_name = sanitize_line(te->tag, false);
    sanitized_schema = sanitize_line(te->namespace, true);
    sanitized_owner = sanitize_line(ropt->noOwner ? NULL : te->owner, true);

    ahprintf(AH, "-- %sName: %s; Type: %s; Schema: %s; Owner: %s", pfx, sanitized_name, te->desc, sanitized_schema, sanitized_owner);

    free(sanitized_name);
    free(sanitized_schema);
    free(sanitized_owner);

    if (te->tablespace && strlen(te->tablespace) > 0 && !ropt->noTablespace)
    {
      char *sanitized_tablespace;

      sanitized_tablespace = sanitize_line(te->tablespace, false);
      ahprintf(AH, "; Tablespace: %s", sanitized_tablespace);
      free(sanitized_tablespace);
    }
    ahprintf(AH, "\n");

    if (AH->PrintExtraTocPtr != NULL)
    {
      AH->PrintExtraTocPtr(AH, te);
    }
    ahprintf(AH, "--\n\n");
  }

     
                                    
     
                                                                             
                                                                          
                                                                      
     
  if (ropt->noOwner && strcmp(te->desc, "SCHEMA") == 0)
  {
    ahprintf(AH, "CREATE SCHEMA %s;\n\n\n", fmtId(te->tag));
  }
  else
  {
    if (te->defn && strlen(te->defn) > 0)
    {
      ahprintf(AH, "%s\n\n", te->defn);
    }
  }

     
                                                                         
                                                                            
                                                                            
                                                              
     
  if (!ropt->noOwner && !ropt->use_setsessauth && te->owner && strlen(te->owner) > 0 && te->dropStmt && strlen(te->dropStmt) > 0)
  {
    if (strcmp(te->desc, "AGGREGATE") == 0 || strcmp(te->desc, "BLOB") == 0 || strcmp(te->desc, "COLLATION") == 0 || strcmp(te->desc, "CONVERSION") == 0 || strcmp(te->desc, "DATABASE") == 0 || strcmp(te->desc, "DOMAIN") == 0 || strcmp(te->desc, "FUNCTION") == 0 || strcmp(te->desc, "OPERATOR") == 0 || strcmp(te->desc, "OPERATOR CLASS") == 0 || strcmp(te->desc, "OPERATOR FAMILY") == 0 || strcmp(te->desc, "PROCEDURE") == 0 || strcmp(te->desc, "PROCEDURAL LANGUAGE") == 0 || strcmp(te->desc, "SCHEMA") == 0 || strcmp(te->desc, "EVENT TRIGGER") == 0 || strcmp(te->desc, "TABLE") == 0 || strcmp(te->desc, "TYPE") == 0 || strcmp(te->desc, "VIEW") == 0 || strcmp(te->desc, "MATERIALIZED VIEW") == 0 || strcmp(te->desc, "SEQUENCE") == 0 || strcmp(te->desc, "FOREIGN TABLE") == 0 || strcmp(te->desc, "TEXT SEARCH DICTIONARY") == 0 || strcmp(te->desc, "TEXT SEARCH CONFIGURATION") == 0 || strcmp(te->desc, "FOREIGN DATA WRAPPER") == 0 || strcmp(te->desc, "SERVER") == 0 ||
        strcmp(te->desc, "STATISTICS") == 0 || strcmp(te->desc, "PUBLICATION") == 0 || strcmp(te->desc, "SUBSCRIPTION") == 0)
    {
      PQExpBuffer temp = createPQExpBuffer();

      appendPQExpBufferStr(temp, "ALTER ");
      _getObjectDescription(temp, te, AH);
      appendPQExpBuffer(temp, " OWNER TO %s;", fmtId(te->owner));
      ahprintf(AH, "%s\n\n", temp->data);
      destroyPQExpBuffer(temp);
    }
    else if (strcmp(te->desc, "CAST") == 0 || strcmp(te->desc, "CHECK CONSTRAINT") == 0 || strcmp(te->desc, "CONSTRAINT") == 0 || strcmp(te->desc, "DATABASE PROPERTIES") == 0 || strcmp(te->desc, "DEFAULT") == 0 || strcmp(te->desc, "FK CONSTRAINT") == 0 || strcmp(te->desc, "INDEX") == 0 || strcmp(te->desc, "RULE") == 0 || strcmp(te->desc, "TRIGGER") == 0 || strcmp(te->desc, "ROW SECURITY") == 0 || strcmp(te->desc, "POLICY") == 0 || strcmp(te->desc, "USER MAPPING") == 0)
    {
                                                         
    }
    else
    {
      pg_log_warning("don't know how to set owner for object type \"%s\"", te->desc);
    }
  }

     
                                                                      
                                                                            
     
  if (_tocEntryIsACL(te))
  {
    if (AH->currUser)
    {
      free(AH->currUser);
    }
    AH->currUser = NULL;
  }
}

   
                                                                         
                                                                              
                                                                          
                                                                           
                                                                       
                               
   
                                                                          
                                                                               
                     
   
                                                                             
                                                                                
                                                                               
   
static char *
sanitize_line(const char *str, bool want_hyphen)
{
  char *result;
  char *s;

  if (!str)
  {
    return pg_strdup(want_hyphen ? "-" : "");
  }

  result = pg_strdup(str);

  for (s = result; *s != '\0'; s++)
  {
    if (*s == '\n' || *s == '\r')
    {
      *s = ' ';
    }
  }

  return result;
}

   
                                                     
   
void
WriteHead(ArchiveHandle *AH)
{
  struct tm crtm;

  AH->WriteBufPtr(AH, "PGDMP", 5);                 
  AH->WriteBytePtr(AH, ARCHIVE_MAJOR(AH->version));
  AH->WriteBytePtr(AH, ARCHIVE_MINOR(AH->version));
  AH->WriteBytePtr(AH, ARCHIVE_REV(AH->version));
  AH->WriteBytePtr(AH, AH->intSize);
  AH->WriteBytePtr(AH, AH->offSize);
  AH->WriteBytePtr(AH, AH->format);
  WriteInt(AH, AH->compression);
  crtm = *localtime(&AH->createDate);
  WriteInt(AH, crtm.tm_sec);
  WriteInt(AH, crtm.tm_min);
  WriteInt(AH, crtm.tm_hour);
  WriteInt(AH, crtm.tm_mday);
  WriteInt(AH, crtm.tm_mon);
  WriteInt(AH, crtm.tm_year);
  WriteInt(AH, crtm.tm_isdst);
  WriteStr(AH, PQdb(AH->connection));
  WriteStr(AH, AH->public.remoteVersionStr);
  WriteStr(AH, PG_VERSION);
}

void
ReadHead(ArchiveHandle *AH)
{
  char vmaj, vmin, vrev;
  int fmt;

     
                                                   
     
                                                                           
                             
     
  if (!AH->readHeader)
  {
    char tmpMag[7];

    AH->ReadBufPtr(AH, tmpMag, 5);

    if (strncmp(tmpMag, "PGDMP", 5) != 0)
    {
      fatal("did not find magic string in file header");
    }
  }

  vmaj = AH->ReadBytePtr(AH);
  vmin = AH->ReadBytePtr(AH);

  if (vmaj > 1 || (vmaj == 1 && vmin > 0))                    
  {
    vrev = AH->ReadBytePtr(AH);
  }
  else
  {
    vrev = 0;
  }

  AH->version = MAKE_ARCHIVE_VERSION(vmaj, vmin, vrev);

  if (AH->version < K_VERS_1_0 || AH->version > K_VERS_MAX)
  {
    fatal("unsupported version (%d.%d) in file header", vmaj, vmin);
  }

  AH->intSize = AH->ReadBytePtr(AH);
  if (AH->intSize > 32)
  {
    fatal("sanity check on integer size (%lu) failed", (unsigned long)AH->intSize);
  }

  if (AH->intSize > sizeof(int))
  {
    pg_log_warning("archive was made on a machine with larger integers, some operations might fail");
  }

  if (AH->version >= K_VERS_1_7)
  {
    AH->offSize = AH->ReadBytePtr(AH);
  }
  else
  {
    AH->offSize = AH->intSize;
  }

  fmt = AH->ReadBytePtr(AH);

  if (AH->format != fmt)
  {
    fatal("expected format (%d) differs from format found in file (%d)", AH->format, fmt);
  }

  if (AH->version >= K_VERS_1_2)
  {
    if (AH->version < K_VERS_1_4)
    {
      AH->compression = AH->ReadBytePtr(AH);
    }
    else
    {
      AH->compression = ReadInt(AH);
    }
  }
  else
  {
    AH->compression = Z_DEFAULT_COMPRESSION;
  }

#ifndef HAVE_LIBZ
  if (AH->compression != 0)
  {
    pg_log_warning("archive is compressed, but this installation does not support compression -- no data will be available");
  }
#endif

  if (AH->version >= K_VERS_1_4)
  {
    struct tm crtm;

    crtm.tm_sec = ReadInt(AH);
    crtm.tm_min = ReadInt(AH);
    crtm.tm_hour = ReadInt(AH);
    crtm.tm_mday = ReadInt(AH);
    crtm.tm_mon = ReadInt(AH);
    crtm.tm_year = ReadInt(AH);
    crtm.tm_isdst = ReadInt(AH);

       
                                                                           
                                                                         
                                                                      
                                                                        
                                          
       
                                                                
                                                                  
                                                                          
                                                                     
                          
       
    AH->createDate = mktime(&crtm);
    if (AH->createDate == (time_t)-1)
    {
      crtm.tm_isdst = -1;
      AH->createDate = mktime(&crtm);
      if (AH->createDate == (time_t)-1)
      {
        pg_log_warning("invalid creation date in header");
      }
    }
  }

  if (AH->version >= K_VERS_1_4)
  {
    AH->archdbname = ReadStr(AH);
  }

  if (AH->version >= K_VERS_1_10)
  {
    AH->archiveRemoteVersion = ReadStr(AH);
    AH->archiveDumpVersion = ReadStr(AH);
  }
}

   
             
                                                   
   
bool
checkSeek(FILE *fp)
{
  pgoff_t tpos;

     
                                                                          
                                                                  
     
#ifndef HAVE_FSEEKO
  if (sizeof(pgoff_t) > sizeof(long))
  {
    return false;
  }
#endif

                                            
  tpos = ftello(fp);
  if (tpos < 0)
  {
    return false;
  }

     
                                                                         
                                                                            
                                                                   
     
  if (fseeko(fp, tpos, SEEK_SET) != 0)
  {
    return false;
  }

  return true;
}

   
                 
   
static void
dumpTimestamp(ArchiveHandle *AH, const char *msg, time_t tim)
{
  char buf[64];

  if (strftime(buf, sizeof(buf), PGDUMP_STRFTIME_FMT, localtime(&tim)) != 0)
  {
    ahprintf(AH, "-- %s %s\n\n", msg, buf);
  }
}

   
                                     
   
                                                                   
                                                                         
                                                                      
                                                                      
                                                            
   
static void
restore_toc_entries_prefork(ArchiveHandle *AH, TocEntry *pending_list)
{
  bool skipped_some;
  TocEntry *next_work_item;

  pg_log_debug("entering restore_toc_entries_prefork");

                                     
  fix_dependencies(AH);

     
                                                                             
                                                                         
                                                                           
                                                                        
                                                                            
                                                   
     
                                                                        
                                                                             
                                                                         
                                                                            
           
     
                                                                             
                                                                            
                                                                           
                                                                           
                                            
     
  AH->restorePass = RESTORE_PASS_MAIN;
  skipped_some = false;
  for (next_work_item = AH->toc->next; next_work_item != AH->toc; next_work_item = next_work_item->next)
  {
    bool do_now = true;

    if (next_work_item->section != SECTION_PRE_DATA)
    {
                                                             
      if (next_work_item->section == SECTION_DATA || next_work_item->section == SECTION_POST_DATA)
      {
        do_now = false;
        skipped_some = true;
      }
      else
      {
           
                                                                      
                                                                      
                                                                    
                                                                     
           
        if (skipped_some)
        {
          do_now = false;
        }
      }
    }

       
                                                                          
                                                                           
                                    
       
    if (_tocEntryRestorePass(next_work_item) != RESTORE_PASS_MAIN)
    {
      do_now = false;
    }

    if (do_now)
    {
                                                            
      pg_log_info("processing item %d %s %s", next_work_item->dumpId, next_work_item->desc, next_work_item->tag);

      (void)restore_toc_entry(AH, next_work_item, false);

                                                                      
      reduce_dependencies(AH, next_work_item, NULL);
    }
    else
    {
                                           
      pending_list_append(pending_list, next_work_item);
    }
  }

     
                                                                         
                                                                            
                  
     
  DisconnectDatabase(&AH->public);

                                                             
  if (AH->currUser)
  {
    free(AH->currUser);
  }
  AH->currUser = NULL;
  if (AH->currSchema)
  {
    free(AH->currSchema);
  }
  AH->currSchema = NULL;
  if (AH->currTablespace)
  {
    free(AH->currTablespace);
  }
  AH->currTablespace = NULL;
  if (AH->currTableAm)
  {
    free(AH->currTableAm);
  }
  AH->currTableAm = NULL;
}

   
                                     
   
                                                                    
                                                                      
                                                                   
                                                                        
                                                                          
                                                                        
                                                                     
   
static void
restore_toc_entries_parallel(ArchiveHandle *AH, ParallelState *pstate, TocEntry *pending_list)
{
  ParallelReadyList ready_list;
  TocEntry *next_work_item;

  pg_log_debug("entering restore_toc_entries_parallel");

                                                                  
  ready_list_init(&ready_list, AH->tocCount);

     
                                                                            
                                                                          
                                                                            
                                                                      
                                                                      
                                          
     
  AH->restorePass = RESTORE_PASS_MAIN;
  move_to_ready_list(pending_list, &ready_list, AH->restorePass);

     
                      
     
                                                                            
                                                                           
                                                            
     
  pg_log_info("entering main parallel loop");

  for (;;)
  {
                                                             
    next_work_item = pop_next_work_item(AH, &ready_list, pstate);
    if (next_work_item != NULL)
    {
                                                                      
      if ((next_work_item->reqs & (REQ_SCHEMA | REQ_DATA)) == 0)
      {
        pg_log_info("skipping item %d %s %s", next_work_item->dumpId, next_work_item->desc, next_work_item->tag);
                                                                 
        reduce_dependencies(AH, next_work_item, &ready_list);
                                                                   
        continue;
      }

      pg_log_info("launching item %d %s %s", next_work_item->dumpId, next_work_item->desc, next_work_item->tag);

                                   
      DispatchJobForTocEntry(AH, pstate, next_work_item, ACT_RESTORE, mark_restore_job_done, &ready_list);
    }
    else if (IsEveryWorkerIdle(pstate))
    {
         
                                                                       
                                                           
         
      if (AH->restorePass == RESTORE_PASS_LAST)
      {
        break;                                              
      }

                                        
      AH->restorePass++;
                                                            
      move_to_ready_list(pending_list, &ready_list, AH->restorePass);
                                                      
      continue;
    }
    else
    {
         
                                                                      
                                         
         
    }

       
                                                                    
                                                                       
                                                                      
                                                                           
                                                                         
                                                                        
                                                                       
                                                                           
                                                                 
                                                                         
                                        
       
    WaitForWorkers(AH, pstate, next_work_item ? WFW_ONE_IDLE : WFW_GOT_STATUS);
  }

                                                  
  Assert(ready_list.first_te > ready_list.last_te);

  ready_list_free(&ready_list);

  pg_log_info("finished main parallel loop");
}

   
                                     
   
                                                                   
                                                                    
                                                                       
                                                                         
                                                                
   
static void
restore_toc_entries_postfork(ArchiveHandle *AH, TocEntry *pending_list)
{
  RestoreOptions *ropt = AH->public.ropt;
  TocEntry *te;

  pg_log_debug("entering restore_toc_entries_postfork");

     
                                                 
     
  ConnectDatabase((Archive *)AH, &ropt->cparams, true);

                                
  _doSetFixedOutputState(AH);

     
                                                                            
                                                                           
                                                                            
                            
     
  for (te = pending_list->pending_next; te != pending_list; te = te->pending_next)
  {
    pg_log_info("processing missed item %d %s %s", te->dumpId, te->desc, te->tag);
    (void)restore_toc_entry(AH, te, false);
  }
}

   
                                                                            
                                                                        
   
static bool
has_lock_conflicts(TocEntry *te1, TocEntry *te2)
{
  int j, k;

  for (j = 0; j < te1->nLockDeps; j++)
  {
    for (k = 0; k < te2->nDeps; k++)
    {
      if (te1->lockDeps[j] == te2->dependencies[k])
      {
        return true;
      }
    }
  }
  return false;
}

   
                                                    
   
                                                                          
                                                                            
                                                     
   
static void
pending_list_header_init(TocEntry *l)
{
  l->pending_prev = l->pending_next = l;
}

                                                          
static void
pending_list_append(TocEntry *l, TocEntry *te)
{
  te->pending_prev = l->pending_prev;
  l->pending_prev->pending_next = te;
  l->pending_prev = te;
  te->pending_next = l;
}

                                     
static void
pending_list_remove(TocEntry *te)
{
  te->pending_prev->pending_next = te->pending_next;
  te->pending_next->pending_prev = te->pending_prev;
  te->pending_prev = NULL;
  te->pending_next = NULL;
}

   
                                                                          
   
static void
ready_list_init(ParallelReadyList *ready_list, int tocCount)
{
  ready_list->tes = (TocEntry **)pg_malloc(tocCount * sizeof(TocEntry *));
  ready_list->first_te = 0;
  ready_list->last_te = -1;
  ready_list->sorted = false;
}

   
                                  
   
static void
ready_list_free(ParallelReadyList *ready_list)
{
  pg_free(ready_list->tes);
}

                              
static void
ready_list_insert(ParallelReadyList *ready_list, TocEntry *te)
{
  ready_list->tes[++ready_list->last_te] = te;
                                              
  ready_list->sorted = false;
}

                                             
static void
ready_list_remove(ParallelReadyList *ready_list, int i)
{
  int f = ready_list->first_te;

  Assert(i >= f && i <= ready_list->last_te);

     
                                                                         
                                                                           
                                                                             
                                                                          
                                       
     
  if (i > f)
  {
    TocEntry **first_te_ptr = &ready_list->tes[f];

    memmove(first_te_ptr + 1, first_te_ptr, (i - f) * sizeof(TocEntry *));
  }
  ready_list->first_te++;
}

                                                
static void
ready_list_sort(ParallelReadyList *ready_list)
{
  if (!ready_list->sorted)
  {
    int n = ready_list->last_te - ready_list->first_te + 1;

    if (n > 1)
    {
      qsort(ready_list->tes + ready_list->first_te, n, sizeof(TocEntry *), TocEntrySizeCompare);
    }
    ready_list->sorted = true;
  }
}

                                                           
static int
TocEntrySizeCompare(const void *p1, const void *p2)
{
  const TocEntry *te1 = *(const TocEntry *const *)p1;
  const TocEntry *te2 = *(const TocEntry *const *)p2;

                                     
  if (te1->dataLength > te2->dataLength)
  {
    return -1;
  }
  if (te1->dataLength < te2->dataLength)
  {
    return 1;
  }

                                                                
  if (te1->dumpId < te2->dumpId)
  {
    return -1;
  }
  if (te1->dumpId > te2->dumpId)
  {
    return 1;
  }

  return 0;
}

   
                                                                     
   
                                                                         
                                                                            
                                                
   
static void
move_to_ready_list(TocEntry *pending_list, ParallelReadyList *ready_list, RestorePass pass)
{
  TocEntry *te;
  TocEntry *next_te;

  for (te = pending_list->pending_next; te != pending_list; te = next_te)
  {
                                                                   
    next_te = te->pending_next;

    if (te->depCount == 0 && _tocEntryRestorePass(te) == pass)
    {
                                           
      pending_list_remove(te);
                                     
      ready_list_insert(ready_list, te);
    }
  }
}

   
                                                                      
                                      
   
                                                     
   
                                                            
                                                            
                                                                       
                                                                       
   
static TocEntry *
pop_next_work_item(ArchiveHandle *AH, ParallelReadyList *ready_list, ParallelState *pstate)
{
     
                                                                 
     
  ready_list_sort(ready_list);

     
                                                          
     
  for (int i = ready_list->first_te; i <= ready_list->last_te; i++)
  {
    TocEntry *te = ready_list->tes[i];
    bool conflicts = false;

       
                                                                       
                                                                           
                                                    
       
    for (int k = 0; k < pstate->numWorkers; k++)
    {
      TocEntry *running_te = pstate->te[k];

      if (running_te == NULL)
      {
        continue;
      }
      if (has_lock_conflicts(te, running_te) || has_lock_conflicts(running_te, te))
      {
        conflicts = true;
        break;
      }
    }

    if (conflicts)
    {
      continue;
    }

                                                
    ready_list_remove(ready_list, i);
    return te;
  }

  pg_log_debug("no item ready");
  return NULL;
}

   
                                                     
   
                                                                               
                                                                               
                                                                             
                                                                            
   
int
parallel_restore(ArchiveHandle *AH, TocEntry *te)
{
  int status;

  Assert(AH->connection != NULL);

                                                        
  AH->public.n_errors = 0;

                            
  status = restore_toc_entry(AH, te, true);

  return status;
}

   
                                                                           
                           
   
                                                                         
   
static void
mark_restore_job_done(ArchiveHandle *AH, TocEntry *te, int status, void *callback_data)
{
  ParallelReadyList *ready_list = (ParallelReadyList *)callback_data;

  pg_log_info("finished item %d %s %s", te->dumpId, te->desc, te->tag);

  if (status == WORKER_CREATE_DONE)
  {
    mark_create_done(AH, te);
  }
  else if (status == WORKER_INHIBIT_DATA)
  {
    inhibit_data_for_failed_table(AH, te);
    AH->public.n_errors++;
  }
  else if (status == WORKER_IGNORED_ERRORS)
  {
    AH->public.n_errors++;
  }
  else if (status != 0)
  {
    fatal("worker process failed: exit code %d", status);
  }

  reduce_dependencies(AH, te, ready_list);
}

   
                                                                               
   
                                                                        
                                                                           
                                                       
                                                                    
                                                                          
                                    
   
                                                                        
                                                
   
static void
fix_dependencies(ArchiveHandle *AH)
{
  TocEntry *te;
  int i;

     
                                                                            
                                                                    
     
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    te->depCount = te->nDeps;
    te->revDeps = NULL;
    te->nRevDeps = 0;
    te->pending_prev = NULL;
    te->pending_next = NULL;
  }

     
                                                                       
                                                                            
                                                         
     
  repoint_table_dependencies(AH);

     
                                                                            
                                                                           
                                       
     
  if (AH->version < K_VERS_1_11)
  {
    for (te = AH->toc->next; te != AH->toc; te = te->next)
    {
      if (strcmp(te->desc, "BLOB COMMENTS") == 0 && te->nDeps == 0)
      {
        TocEntry *te2;

        for (te2 = AH->toc->next; te2 != AH->toc; te2 = te2->next)
        {
          if (strcmp(te2->desc, "BLOBS") == 0)
          {
            te->dependencies = (DumpId *)pg_malloc(sizeof(DumpId));
            te->dependencies[0] = te2->dumpId;
            te->nDeps++;
            te->depCount++;
            break;
          }
        }
        break;
      }
    }
  }

     
                                                                            
                                                      
     

     
                                                                          
                                                                         
                                                                             
                                                         
     
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    for (i = 0; i < te->nDeps; i++)
    {
      DumpId depid = te->dependencies[i];

      if (depid <= AH->maxDumpId && AH->tocsByDumpId[depid] != NULL)
      {
        AH->tocsByDumpId[depid]->nRevDeps++;
      }
      else
      {
        te->depCount--;
      }
    }
  }

     
                                                                           
                            
     
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    if (te->nRevDeps > 0)
    {
      te->revDeps = (DumpId *)pg_malloc(te->nRevDeps * sizeof(DumpId));
    }
    te->nRevDeps = 0;
  }

     
                                                                          
                                        
     
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    for (i = 0; i < te->nDeps; i++)
    {
      DumpId depid = te->dependencies[i];

      if (depid <= AH->maxDumpId && AH->tocsByDumpId[depid] != NULL)
      {
        TocEntry *otherte = AH->tocsByDumpId[depid];

        otherte->revDeps[otherte->nRevDeps++] = te->dumpId;
      }
    }
  }

     
                                                
     
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    te->lockDeps = NULL;
    te->nLockDeps = 0;
    identify_locking_dependencies(AH, te);
  }
}

   
                                                                             
                                
   
                                                                           
                                                                           
                                                                       
                                                                           
                                                                    
   
static void
repoint_table_dependencies(ArchiveHandle *AH)
{
  TocEntry *te;
  int i;
  DumpId olddep;

  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    if (te->section != SECTION_POST_DATA)
    {
      continue;
    }
    for (i = 0; i < te->nDeps; i++)
    {
      olddep = te->dependencies[i];
      if (olddep <= AH->maxDumpId && AH->tableDataId[olddep] != 0)
      {
        DumpId tabledataid = AH->tableDataId[olddep];
        TocEntry *tabledatate = AH->tocsByDumpId[tabledataid];

        te->dependencies[i] = tabledataid;
        te->dataLength = Max(te->dataLength, tabledatate->dataLength);
        pg_log_debug("transferring dependency %d -> %d to %d", te->dumpId, olddep, tabledataid);
      }
    }
  }
}

   
                                                                           
                                                                         
                                                                    
   
static void
identify_locking_dependencies(ArchiveHandle *AH, TocEntry *te)
{
  DumpId *lockids;
  int nlockids;
  int i;

     
                                                                          
                                                                        
     
  if (te->section != SECTION_POST_DATA)
  {
    return;
  }

                                            
  if (te->nDeps == 0)
  {
    return;
  }

     
                                                                             
                                                                           
                                                                            
                                                  
     
  if (strcmp(te->desc, "INDEX") == 0)
  {
    return;
  }

     
                                                                             
                                                                             
                                                                           
                                                                             
                                                                         
                                                                           
                                                                        
                                                         
     
  lockids = (DumpId *)pg_malloc(te->nDeps * sizeof(DumpId));
  nlockids = 0;
  for (i = 0; i < te->nDeps; i++)
  {
    DumpId depid = te->dependencies[i];

    if (depid <= AH->maxDumpId && AH->tocsByDumpId[depid] != NULL && ((strcmp(AH->tocsByDumpId[depid]->desc, "TABLE DATA") == 0) || strcmp(AH->tocsByDumpId[depid]->desc, "TABLE") == 0))
    {
      lockids[nlockids++] = depid;
    }
  }

  if (nlockids == 0)
  {
    free(lockids);
    return;
  }

  te->lockDeps = pg_realloc(lockids, nlockids * sizeof(DumpId));
  te->nLockDeps = nlockids;
}

   
                                                                             
                                                                         
                                                                        
   
static void
reduce_dependencies(ArchiveHandle *AH, TocEntry *te, ParallelReadyList *ready_list)
{
  int i;

  pg_log_debug("reducing dependencies for %d", te->dumpId);

  for (i = 0; i < te->nRevDeps; i++)
  {
    TocEntry *otherte = AH->tocsByDumpId[te->revDeps[i]];

    Assert(otherte->depCount > 0);
    otherte->depCount--;

       
                                                                         
                                                                     
                                                                       
                                                                    
                                                                        
                            
       
    if (otherte->depCount == 0 && _tocEntryRestorePass(otherte) == AH->restorePass && otherte->pending_prev != NULL && ready_list != NULL)
    {
                                           
      pending_list_remove(otherte);
                                     
      ready_list_insert(ready_list, otherte);
    }
  }
}

   
                                                                      
                
   
static void
mark_create_done(ArchiveHandle *AH, TocEntry *te)
{
  if (AH->tableDataId[te->dumpId] != 0)
  {
    TocEntry *ted = AH->tocsByDumpId[AH->tableDataId[te->dumpId]];

    ted->created = true;
  }
}

   
                                                                
                 
   
static void
inhibit_data_for_failed_table(ArchiveHandle *AH, TocEntry *te)
{
  pg_log_info("table \"%s\" could not be created, will not restore its data", te->tag);

  if (AH->tableDataId[te->dumpId] != 0)
  {
    TocEntry *ted = AH->tocsByDumpId[AH->tableDataId[te->dumpId]];

    ted->reqs = 0;
  }
}

   
                                                             
   
                                                                
                                                                 
   
ArchiveHandle *
CloneArchive(ArchiveHandle *AH)
{
  ArchiveHandle *clone;

                          
  clone = (ArchiveHandle *)pg_malloc(sizeof(ArchiveHandle));
  memcpy(clone, AH, sizeof(ArchiveHandle));

                                        
  memset(&(clone->sqlparse), 0, sizeof(clone->sqlparse));

                                                                             
  clone->connection = NULL;
  clone->connCancel = NULL;
  clone->currUser = NULL;
  clone->currSchema = NULL;
  clone->currTablespace = NULL;

                                                                         
  if (clone->savedPassword)
  {
    clone->savedPassword = pg_strdup(clone->savedPassword);
  }

                                          
  clone->public.n_errors = 0;

     
                                                                             
                                                  
     
  ConnectDatabase((Archive *)clone, &clone->public.ropt->cparams, true);

                                
  if (AH->mode == archModeRead)
  {
    _doSetFixedOutputState(clone);
  }
                                                                   

                                                      
  clone->ClonePtr(clone);

  Assert(clone->connection != NULL);
  return clone;
}

   
                                
   
                                                                  
   
void
DeCloneArchive(ArchiveHandle *AH)
{
                                                   
  Assert(AH->connection == NULL);

                                   
  AH->DeClonePtr(AH);

                                             
  if (AH->sqlparse.curCmd)
  {
    destroyPQExpBuffer(AH->sqlparse.curCmd);
  }

                                        
  if (AH->currUser)
  {
    free(AH->currUser);
  }
  if (AH->currSchema)
  {
    free(AH->currSchema);
  }
  if (AH->currTablespace)
  {
    free(AH->currTablespace);
  }
  if (AH->currTableAm)
  {
    free(AH->currTableAm);
  }
  if (AH->savedPassword)
  {
    free(AH->savedPassword);
  }

  free(AH);
}
