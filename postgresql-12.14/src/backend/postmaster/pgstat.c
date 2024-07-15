              
            
   
                                                                       
   
                                                            
                             
   
                                                     
   
                                                          
                                                               
   
                                                                
   
                                   
              
   
#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "pgstat.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "catalog/pg_database.h"
#include "catalog/pg_proc.h"
#include "common/ip.h"
#include "libpq/libpq.h"
#include "libpq/pqsignal.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "postmaster/autovacuum.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "replication/walsender.h"
#include "storage/backendid.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lmgr.h"
#include "storage/pg_shmem.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "utils/ascii.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

              
                      
              
   
#define PGSTAT_STAT_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  500                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                     

#define PGSTAT_RETRY_DELAY                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  10                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                     

#define PGSTAT_MAX_WAIT_TIME                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  10000                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
                                           

#define PGSTAT_INQ_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  640                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                      

#define PGSTAT_RESTART_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  60                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                   

#define PGSTAT_POLL_LOOP_COUNT (PGSTAT_MAX_WAIT_TIME / PGSTAT_RETRY_DELAY)
#define PGSTAT_INQ_LOOP_COUNT (PGSTAT_INQ_INTERVAL / PGSTAT_RETRY_DELAY)

                                                             
#define PGSTAT_MIN_RCVBUF (100 * 1024)

              
                                                                     
              
   
#define PGSTAT_DB_HASH_SIZE 16
#define PGSTAT_TAB_HASH_SIZE 512
#define PGSTAT_FUNCTION_HASH_SIZE 512

              
                                                
   
                                                                    
                                                                       
                                                                       
                                                               
              
   
#define NumBackendStatSlots (MaxBackends + NUM_AUXPROCTYPES)

              
                  
              
   
bool pgstat_track_activities = false;
bool pgstat_track_counts = false;
int pgstat_track_functions = TRACK_FUNC_OFF;
int pgstat_track_activity_query_size = 1024;

              
                            
              
   
char *pgstat_stat_directory = NULL;
char *pgstat_stat_filename = NULL;
char *pgstat_stat_tmpname = NULL;

   
                                                                    
                                                                  
                                                                           
   
PgStat_MsgBgWriter BgWriterStats;

              
              
              
   
NON_EXEC_STATIC pgsocket pgStatSock = PGINVALID_SOCKET;

static struct sockaddr_storage pgStatAddr;

static time_t last_pgstat_start_time;

static bool pgStatRunningInCollector = false;

   
                                                                          
                          
   
                                                                              
                                                                          
                                                                               
                                                                               
                                                                       
                                           
   
#define TABSTAT_QUANTUM 100                                   

typedef struct TabStatusArray
{
  struct TabStatusArray *tsa_next;                                                 
  int tsa_used;                                                                  
  PgStat_TableStatus tsa_entries[TABSTAT_QUANTUM];                     
} TabStatusArray;

static TabStatusArray *pgStatTabList = NULL;

   
                                                                            
   
typedef struct TabStatHashEntry
{
  Oid t_id;
  PgStat_TableStatus *tsa_entry;
} TabStatHashEntry;

   
                                                
   
static HTAB *pgStatTabHash = NULL;

   
                                                                               
                                                 
   
static HTAB *pgStatFunctions = NULL;

   
                                                                   
                          
   
static bool have_function_stats = false;

   
                                                                               
                                                                           
                                                                           
                                                                                
                                                        
   
typedef struct PgStat_SubXactStatus
{
  int nest_level;                                                   
  struct PgStat_SubXactStatus *prev;                                  
  PgStat_TableXactStatus *first;                                        
} PgStat_SubXactStatus;

static PgStat_SubXactStatus *pgStatXactStack = NULL;

static int pgStatXactCommit = 0;
static int pgStatXactRollback = 0;
PgStat_Counter pgStatBlockReadTime = 0;
PgStat_Counter pgStatBlockWriteTime = 0;

                                                                            
typedef struct TwoPhasePgStatRecord
{
  PgStat_Counter tuples_inserted;                                 
  PgStat_Counter tuples_updated;                                 
  PgStat_Counter tuples_deleted;                                 
  PgStat_Counter inserted_pre_trunc;                                        
  PgStat_Counter updated_pre_trunc;                                        
  PgStat_Counter deleted_pre_trunc;                                        
  Oid t_id;                                           
  bool t_shared;                                                  
  bool t_truncated;                                                   
} TwoPhasePgStatRecord;

   
                                               
   
static MemoryContext pgStatLocalContext = NULL;
static HTAB *pgStatDBHash = NULL;

                                             
static LocalPgBackendStatus *localBackendStatusTable = NULL;

                                                  
static int localNumBackends = 0;

   
                                                         
                                                           
                 
   
static PgStat_ArchiverStats archiverStats;
static PgStat_GlobalStats globalStats;

   
                                                                               
                                                                           
                                                        
   
static List *pending_write_requests = NIL;

                          
static volatile bool need_exit = false;
static volatile bool got_SIGHUP = false;

   
                                                                  
                                                                 
                                         
   
static instr_time total_func_time;

              
                                       
              
   
#ifdef EXEC_BACKEND
static pid_t
pgstat_forkexec(void);
#endif

NON_EXEC_STATIC void
PgstatCollectorMain(int argc, char *argv[]) pg_attribute_noreturn();
static void pgstat_exit(SIGNAL_ARGS);
static void
pgstat_beshutdown_hook(int code, Datum arg);
static void pgstat_sighup_handler(SIGNAL_ARGS);

static PgStat_StatDBEntry *
pgstat_get_db_entry(Oid databaseid, bool create);
static PgStat_StatTabEntry *
pgstat_get_tab_entry(PgStat_StatDBEntry *dbentry, Oid tableoid, bool create);
static void
pgstat_write_statsfiles(bool permanent, bool allDbs);
static void
pgstat_write_db_statsfile(PgStat_StatDBEntry *dbentry, bool permanent);
static HTAB *
pgstat_read_statsfiles(Oid onlydb, bool permanent, bool deep);
static void
pgstat_read_db_statsfile(Oid databaseid, HTAB *tabhash, HTAB *funchash, bool permanent);
static void
backend_read_statsfile(void);
static void
pgstat_read_current_status(void);

static bool
pgstat_write_statsfile_needed(void);
static bool
pgstat_db_requested(Oid databaseid);

static void
pgstat_send_tabstat(PgStat_MsgTabstat *tsmsg);
static void
pgstat_send_funcstats(void);
static HTAB *
pgstat_collect_oids(Oid catalogid, AttrNumber anum_oid);

static PgStat_TableStatus *
get_tabstat_entry(Oid rel_id, bool isshared);

static void
pgstat_setup_memcxt(void);

static const char *
pgstat_get_wait_activity(WaitEventActivity w);
static const char *
pgstat_get_wait_client(WaitEventClient w);
static const char *
pgstat_get_wait_ipc(WaitEventIPC w);
static const char *
pgstat_get_wait_timeout(WaitEventTimeout w);
static const char *
pgstat_get_wait_io(WaitEventIO w);

static void
pgstat_setheader(PgStat_MsgHdr *hdr, StatMsgType mtype);
static void
pgstat_send(void *msg, int len);

static void
pgstat_recv_inquiry(PgStat_MsgInquiry *msg, int len);
static void
pgstat_recv_tabstat(PgStat_MsgTabstat *msg, int len);
static void
pgstat_recv_tabpurge(PgStat_MsgTabpurge *msg, int len);
static void
pgstat_recv_dropdb(PgStat_MsgDropdb *msg, int len);
static void
pgstat_recv_resetcounter(PgStat_MsgResetcounter *msg, int len);
static void
pgstat_recv_resetsharedcounter(PgStat_MsgResetsharedcounter *msg, int len);
static void
pgstat_recv_resetsinglecounter(PgStat_MsgResetsinglecounter *msg, int len);
static void
pgstat_recv_autovac(PgStat_MsgAutovacStart *msg, int len);
static void
pgstat_recv_vacuum(PgStat_MsgVacuum *msg, int len);
static void
pgstat_recv_analyze(PgStat_MsgAnalyze *msg, int len);
static void
pgstat_recv_archiver(PgStat_MsgArchiver *msg, int len);
static void
pgstat_recv_bgwriter(PgStat_MsgBgWriter *msg, int len);
static void
pgstat_recv_funcstat(PgStat_MsgFuncstat *msg, int len);
static void
pgstat_recv_funcpurge(PgStat_MsgFuncpurge *msg, int len);
static void
pgstat_recv_recoveryconflict(PgStat_MsgRecoveryConflict *msg, int len);
static void
pgstat_recv_deadlock(PgStat_MsgDeadlock *msg, int len);
static void
pgstat_recv_checksum_failure(PgStat_MsgChecksumFailure *msg, int len);
static void
pgstat_recv_tempfile(PgStat_MsgTempFile *msg, int len);

                                                                
                                                  
                                                                
   

              
                   
   
                                                                    
                                                                    
                                                                     
             
              
   
void
pgstat_init(void)
{
  ACCEPT_TYPE_ARG3 alen;
  struct addrinfo *addrs = NULL, *addr, hints;
  int ret;
  fd_set rset;
  struct timeval tv;
  char test_byte;
  int sel_res;
  int tries = 0;

#define TESTBYTEVAL ((char)199)

     
                                                                            
                                                                       
                                                                           
                                                                            
                                              
     
  StaticAssertStmt(sizeof(PgStat_Msg) <= PGSTAT_MAX_MSG_SIZE, "maximum stats message size exceeds PGSTAT_MAX_MSG_SIZE");

     
                                                                        
     
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_addrlen = 0;
  hints.ai_addr = NULL;
  hints.ai_canonname = NULL;
  hints.ai_next = NULL;
  ret = pg_getaddrinfo_all("localhost", NULL, &hints, &addrs);
  if (ret || !addrs)
  {
    ereport(LOG, (errmsg("could not resolve \"localhost\": %s", gai_strerror(ret))));
    goto startup_failed;
  }

     
                                                                           
                                                                            
                                                                         
                                                                          
                                                                      
                                                     
     
  for (addr = addrs; addr; addr = addr->ai_next)
  {
#ifdef HAVE_UNIX_SOCKETS
                                                      
    if (addr->ai_family == AF_UNIX)
    {
      continue;
    }
#endif

    if (++tries > 1)
    {
      ereport(LOG, (errmsg("trying another address for the statistics collector")));
    }

       
                          
       
    if ((pgStatSock = socket(addr->ai_family, SOCK_DGRAM, 0)) == PGINVALID_SOCKET)
    {
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not create socket for statistics collector: %m")));
      continue;
    }

       
                                                                           
                               
       
    if (bind(pgStatSock, addr->ai_addr, addr->ai_addrlen) < 0)
    {
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not bind socket for statistics collector: %m")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

    alen = sizeof(pgStatAddr);
    if (getsockname(pgStatSock, (struct sockaddr *)&pgStatAddr, &alen) < 0)
    {
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not get address of socket for statistics collector: %m")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

       
                                                                          
                                                                           
                                                                      
                                 
       
    if (connect(pgStatSock, (struct sockaddr *)&pgStatAddr, alen) < 0)
    {
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not connect socket for statistics collector: %m")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

       
                                                                           
                                                                           
                                                                         
                          
       
    test_byte = TESTBYTEVAL;

  retry1:
    if (send(pgStatSock, &test_byte, 1, 0) != 1)
    {
      if (errno == EINTR)
      {
        goto retry1;                                 
      }
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not send test message on socket for statistics collector: %m")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

       
                                                                        
                                                                           
                    
       
    for (;;)                                  
    {
      FD_ZERO(&rset);
      FD_SET(pgStatSock, &rset);

      tv.tv_sec = 0;
      tv.tv_usec = 500000;
      sel_res = select(pgStatSock + 1, &rset, NULL, NULL, &tv);
      if (sel_res >= 0 || errno != EINTR)
      {
        break;
      }
    }
    if (sel_res < 0)
    {
      ereport(LOG, (errcode_for_socket_access(), errmsg("select() failed in statistics collector: %m")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }
    if (sel_res == 0 || !FD_ISSET(pgStatSock, &rset))
    {
         
                                                                        
                                         
         
                                                                   
         
      ereport(LOG, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("test message did not get through on socket for statistics collector")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

    test_byte++;                                         

  retry2:
    if (recv(pgStatSock, &test_byte, 1, 0) != 1)
    {
      if (errno == EINTR)
      {
        goto retry2;                                 
      }
      ereport(LOG, (errcode_for_socket_access(), errmsg("could not receive test message on socket for statistics collector: %m")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

    if (test_byte != TESTBYTEVAL)                            
    {
      ereport(LOG, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("incorrect test message transmission on socket for statistics collector")));
      closesocket(pgStatSock);
      pgStatSock = PGINVALID_SOCKET;
      continue;
    }

                                                  
    break;
  }

                                      
  if (!addr || pgStatSock == PGINVALID_SOCKET)
  {
    goto startup_failed;
  }

     
                                                                            
                                                                         
                                                      
     
  if (!pg_set_noblock(pgStatSock))
  {
    ereport(LOG, (errcode_for_socket_access(), errmsg("could not set statistics collector socket to nonblocking mode: %m")));
    goto startup_failed;
  }

     
                                                                
                                                                        
                                                                             
                                                                             
                                                                         
     
  {
    int old_rcvbuf;
    int new_rcvbuf;
    ACCEPT_TYPE_ARG3 rcvbufsize = sizeof(old_rcvbuf);

    if (getsockopt(pgStatSock, SOL_SOCKET, SO_RCVBUF, (char *)&old_rcvbuf, &rcvbufsize) < 0)
    {
      elog(LOG, "getsockopt(SO_RCVBUF) failed: %m");
                                                               
      old_rcvbuf = 0;
    }

    new_rcvbuf = PGSTAT_MIN_RCVBUF;
    if (old_rcvbuf < new_rcvbuf)
    {
      if (setsockopt(pgStatSock, SOL_SOCKET, SO_RCVBUF, (char *)&new_rcvbuf, sizeof(new_rcvbuf)) < 0)
      {
        elog(LOG, "setsockopt(SO_RCVBUF) failed: %m");
      }
    }
  }

  pg_freeaddrinfo_all(hints.ai_family, addrs);

  return;

startup_failed:
  ereport(LOG, (errmsg("disabling statistics collector for lack of working socket")));

  if (addrs)
  {
    pg_freeaddrinfo_all(hints.ai_family, addrs);
  }

  if (pgStatSock != PGINVALID_SOCKET)
  {
    closesocket(pgStatSock);
  }
  pgStatSock = PGINVALID_SOCKET;

     
                                                                          
                                                                          
                                                                            
                                                
     
  SetConfigOption("track_counts", "off", PGC_INTERNAL, PGC_S_OVERRIDE);
}

   
                                   
   
static void
pgstat_reset_remove_files(const char *directory)
{
  DIR *dir;
  struct dirent *entry;
  char fname[MAXPGPATH * 2];

  dir = AllocateDir(directory);
  while ((entry = ReadDir(dir, directory)) != NULL)
  {
    int nchars;
    Oid tmp_oid;

       
                                                                        
                                                                  
       
    if (strncmp(entry->d_name, "global.", 7) == 0)
    {
      nchars = 7;
    }
    else
    {
      nchars = 0;
      (void)sscanf(entry->d_name, "db_%u.%n", &tmp_oid, &nchars);
      if (nchars <= 0)
      {
        continue;
      }
                                                        
      if (strchr("0123456789", entry->d_name[3]) == NULL)
      {
        continue;
      }
    }

    if (strcmp(entry->d_name + nchars, "tmp") != 0 && strcmp(entry->d_name + nchars, "stat") != 0)
    {
      continue;
    }

    snprintf(fname, sizeof(fname), "%s/%s", directory, entry->d_name);
    unlink(fname);
  }
  FreeDir(dir);
}

   
                        
   
                                                               
                                     
   
void
pgstat_reset_all(void)
{
  pgstat_reset_remove_files(pgstat_stat_directory);
  pgstat_reset_remove_files(PGSTAT_STAT_PERMANENT_DIRECTORY);
}

#ifdef EXEC_BACKEND

   
                       
   
                                                                               
   
static pid_t
pgstat_forkexec(void)
{
  char *av[10];
  int ac = 0;

  av[ac++] = "postgres";
  av[ac++] = "--forkcol";
  av[ac++] = NULL;                                       

  av[ac] = NULL;
  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}
#endif                   

   
                    
   
                                                                    
                                                           
   
                                               
   
                                                                         
   
int
pgstat_start(void)
{
  time_t curtime;
  pid_t pgStatPid;

     
                                                                           
                     
     
  if (pgStatSock == PGINVALID_SOCKET)
  {
    return 0;
  }

     
                                                                          
                                                                           
                                                                           
                                                                      
     
  curtime = time(NULL);
  if ((unsigned int)(curtime - last_pgstat_start_time) < (unsigned int)PGSTAT_RESTART_INTERVAL)
  {
    return 0;
  }
  last_pgstat_start_time = curtime;

     
                                   
     
#ifdef EXEC_BACKEND
  switch ((pgStatPid = pgstat_forkexec()))
#else
  switch ((pgStatPid = fork_process()))
#endif
  {
  case -1:
    ereport(LOG, (errmsg("could not fork statistics collector: %m")));
    return 0;

#ifndef EXEC_BACKEND
  case 0:
                                 
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

                                                                    
    dsm_detach_all();
    PGSharedMemoryDetach();

    PgstatCollectorMain(0, NULL);
    break;
#endif

  default:
    return (int)pgStatPid;
  }

                          
  return 0;
}

void
allow_immediate_pgstat_restart(void)
{
  last_pgstat_start_time = 0;
}

                                                                
                                            
                                                               
   

              
                          
   
                                                                           
                                                                     
                                                                             
                                                                      
                                                              
              
   
void
pgstat_report_stat(bool force)
{
                                           
  static const PgStat_TableCounts all_zeroes;
  static TimestampTz last_report = 0;

  TimestampTz now;
  PgStat_MsgTabstat regular_msg;
  PgStat_MsgTabstat shared_msg;
  TabStatusArray *tsa;
  int i;

                                                   
  if ((pgStatTabList == NULL || pgStatTabList->tsa_used == 0) && pgStatXactCommit == 0 && pgStatXactRollback == 0 && !have_function_stats)
  {
    return;
  }

     
                                                                         
                                                                          
     
  now = GetCurrentTransactionStopTimestamp();
  if (!force && !TimestampDifferenceExceeds(last_report, now, PGSTAT_STAT_INTERVAL))
  {
    return;
  }
  last_report = now;

     
                                                                          
                                                                            
                                                                  
                                                                        
                                      
     
  if (pgStatTabHash)
  {
    hash_destroy(pgStatTabHash);
  }
  pgStatTabHash = NULL;

     
                                                                            
                                                                          
                                                                             
                                   
     
  regular_msg.m_databaseid = MyDatabaseId;
  shared_msg.m_databaseid = InvalidOid;
  regular_msg.m_nentries = 0;
  shared_msg.m_nentries = 0;

  for (tsa = pgStatTabList; tsa != NULL; tsa = tsa->tsa_next)
  {
    for (i = 0; i < tsa->tsa_used; i++)
    {
      PgStat_TableStatus *entry = &tsa->tsa_entries[i];
      PgStat_MsgTabstat *this_msg;
      PgStat_TableEntry *this_ent;

                                                                   
      Assert(entry->trans == NULL);

         
                                                                       
                                                                  
         
      if (memcmp(&entry->t_counts, &all_zeroes, sizeof(PgStat_TableCounts)) == 0)
      {
        continue;
      }

         
                                                                         
         
      this_msg = entry->t_shared ? &shared_msg : &regular_msg;
      this_ent = &this_msg->m_entry[this_msg->m_nentries];
      this_ent->t_id = entry->t_id;
      memcpy(&this_ent->t_counts, &entry->t_counts, sizeof(PgStat_TableCounts));
      if (++this_msg->m_nentries >= PGSTAT_NUM_TABENTRIES)
      {
        pgstat_send_tabstat(this_msg);
        this_msg->m_nentries = 0;
      }
    }
                                                
    MemSet(tsa->tsa_entries, 0, tsa->tsa_used * sizeof(PgStat_TableStatus));
    tsa->tsa_used = 0;
  }

     
                                                                          
                                                             
     
  if (regular_msg.m_nentries > 0 || pgStatXactCommit > 0 || pgStatXactRollback > 0)
  {
    pgstat_send_tabstat(&regular_msg);
  }
  if (shared_msg.m_nentries > 0)
  {
    pgstat_send_tabstat(&shared_msg);
  }

                                     
  pgstat_send_funcstats();
}

   
                                                                        
   
static void
pgstat_send_tabstat(PgStat_MsgTabstat *tsmsg)
{
  int n;
  int len;

                                                                            
  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

     
                                                                       
                                               
     
  if (OidIsValid(tsmsg->m_databaseid))
  {
    tsmsg->m_xact_commit = pgStatXactCommit;
    tsmsg->m_xact_rollback = pgStatXactRollback;
    tsmsg->m_block_read_time = pgStatBlockReadTime;
    tsmsg->m_block_write_time = pgStatBlockWriteTime;
    pgStatXactCommit = 0;
    pgStatXactRollback = 0;
    pgStatBlockReadTime = 0;
    pgStatBlockWriteTime = 0;
  }
  else
  {
    tsmsg->m_xact_commit = 0;
    tsmsg->m_xact_rollback = 0;
    tsmsg->m_block_read_time = 0;
    tsmsg->m_block_write_time = 0;
  }

  n = tsmsg->m_nentries;
  len = offsetof(PgStat_MsgTabstat, m_entry[0]) + n * sizeof(PgStat_TableEntry);

  pgstat_setheader(&tsmsg->m_hdr, PGSTAT_MTYPE_TABSTAT);
  pgstat_send(tsmsg, len);
}

   
                                                                                
   
static void
pgstat_send_funcstats(void)
{
                                           
  static const PgStat_FunctionCounts all_zeroes;

  PgStat_MsgFuncstat msg;
  PgStat_BackendFunctionEntry *entry;
  HASH_SEQ_STATUS fstat;

  if (pgStatFunctions == NULL)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_FUNCSTAT);
  msg.m_databaseid = MyDatabaseId;
  msg.m_nentries = 0;

  hash_seq_init(&fstat, pgStatFunctions);
  while ((entry = (PgStat_BackendFunctionEntry *)hash_seq_search(&fstat)) != NULL)
  {
    PgStat_FunctionEntry *m_ent;

                                                          
    if (memcmp(&entry->f_counts, &all_zeroes, sizeof(PgStat_FunctionCounts)) == 0)
    {
      continue;
    }

                                                     
    m_ent = &msg.m_entry[msg.m_nentries];
    m_ent->f_id = entry->f_id;
    m_ent->f_numcalls = entry->f_counts.f_numcalls;
    m_ent->f_total_time = INSTR_TIME_GET_MICROSEC(entry->f_counts.f_total_time);
    m_ent->f_self_time = INSTR_TIME_GET_MICROSEC(entry->f_counts.f_self_time);

    if (++msg.m_nentries >= PGSTAT_NUM_FUNCENTRIES)
    {
      pgstat_send(&msg, offsetof(PgStat_MsgFuncstat, m_entry[0]) + msg.m_nentries * sizeof(PgStat_FunctionEntry));
      msg.m_nentries = 0;
    }

                                  
    MemSet(&entry->f_counts, 0, sizeof(PgStat_FunctionCounts));
  }

  if (msg.m_nentries > 0)
  {
    pgstat_send(&msg, offsetof(PgStat_MsgFuncstat, m_entry[0]) + msg.m_nentries * sizeof(PgStat_FunctionEntry));
  }

  have_function_stats = false;
}

              
                          
   
                                                            
              
   
void
pgstat_vacuum_stat(void)
{
  HTAB *htab;
  PgStat_MsgTabpurge msg;
  PgStat_MsgFuncpurge f_msg;
  HASH_SEQ_STATUS hstat;
  PgStat_StatDBEntry *dbentry;
  PgStat_StatTabEntry *tabentry;
  PgStat_StatFuncEntry *funcentry;
  int len;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

     
                                                                           
                                 
     
  backend_read_statsfile();

     
                                                                        
     
  htab = pgstat_collect_oids(DatabaseRelationId, Anum_pg_database_oid);

     
                                                                    
                             
     
  hash_seq_init(&hstat, pgStatDBHash);
  while ((dbentry = (PgStat_StatDBEntry *)hash_seq_search(&hstat)) != NULL)
  {
    Oid dbid = dbentry->databaseid;

    CHECK_FOR_INTERRUPTS();

                                                                           
    if (OidIsValid(dbid) && hash_search(htab, (void *)&dbid, HASH_FIND, NULL) == NULL)
    {
      pgstat_drop_database(dbid);
    }
  }

                
  hash_destroy(htab);

     
                                                                      
     
  dbentry = (PgStat_StatDBEntry *)hash_search(pgStatDBHash, (void *)&MyDatabaseId, HASH_FIND, NULL);
  if (dbentry == NULL || dbentry->tables == NULL)
  {
    return;
  }

     
                                                                        
     
  htab = pgstat_collect_oids(RelationRelationId, Anum_pg_class_oid);

     
                                                   
     
  msg.m_nentries = 0;

     
                                                                         
     
  hash_seq_init(&hstat, dbentry->tables);
  while ((tabentry = (PgStat_StatTabEntry *)hash_seq_search(&hstat)) != NULL)
  {
    Oid tabid = tabentry->tableid;

    CHECK_FOR_INTERRUPTS();

    if (hash_search(htab, (void *)&tabid, HASH_FIND, NULL) != NULL)
    {
      continue;
    }

       
                                                         
       
    msg.m_tableid[msg.m_nentries++] = tabid;

       
                                                                     
       
    if (msg.m_nentries >= PGSTAT_NUM_TABPURGE)
    {
      len = offsetof(PgStat_MsgTabpurge, m_tableid[0]) + msg.m_nentries * sizeof(Oid);

      pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_TABPURGE);
      msg.m_databaseid = MyDatabaseId;
      pgstat_send(&msg, len);

      msg.m_nentries = 0;
    }
  }

     
                   
     
  if (msg.m_nentries > 0)
  {
    len = offsetof(PgStat_MsgTabpurge, m_tableid[0]) + msg.m_nentries * sizeof(Oid);

    pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_TABPURGE);
    msg.m_databaseid = MyDatabaseId;
    pgstat_send(&msg, len);
  }

                
  hash_destroy(htab);

     
                                                                           
                                                                     
     
  if (dbentry->functions != NULL && hash_get_num_entries(dbentry->functions) > 0)
  {
    htab = pgstat_collect_oids(ProcedureRelationId, Anum_pg_proc_oid);

    pgstat_setheader(&f_msg.m_hdr, PGSTAT_MTYPE_FUNCPURGE);
    f_msg.m_databaseid = MyDatabaseId;
    f_msg.m_nentries = 0;

    hash_seq_init(&hstat, dbentry->functions);
    while ((funcentry = (PgStat_StatFuncEntry *)hash_seq_search(&hstat)) != NULL)
    {
      Oid funcid = funcentry->functionid;

      CHECK_FOR_INTERRUPTS();

      if (hash_search(htab, (void *)&funcid, HASH_FIND, NULL) != NULL)
      {
        continue;
      }

         
                                                              
         
      f_msg.m_functionid[f_msg.m_nentries++] = funcid;

         
                                                                       
         
      if (f_msg.m_nentries >= PGSTAT_NUM_FUNCPURGE)
      {
        len = offsetof(PgStat_MsgFuncpurge, m_functionid[0]) + f_msg.m_nentries * sizeof(Oid);

        pgstat_send(&f_msg, len);

        f_msg.m_nentries = 0;
      }
    }

       
                     
       
    if (f_msg.m_nentries > 0)
    {
      len = offsetof(PgStat_MsgFuncpurge, m_functionid[0]) + f_msg.m_nentries * sizeof(Oid);

      pgstat_send(&f_msg, len);
    }

    hash_destroy(htab);
  }
}

              
                           
   
                                                                          
                                                                       
                                                                           
                                                            
              
   
static HTAB *
pgstat_collect_oids(Oid catalogid, AttrNumber anum_oid)
{
  HTAB *htab;
  HASHCTL hash_ctl;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tup;
  Snapshot snapshot;

  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(Oid);
  hash_ctl.hcxt = CurrentMemoryContext;
  htab = hash_create("Temporary table of OIDs", PGSTAT_TAB_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  rel = table_open(catalogid, AccessShareLock);
  snapshot = RegisterSnapshot(GetLatestSnapshot());
  scan = table_beginscan(rel, snapshot, 0, NULL);
  while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Oid thisoid;
    bool isnull;

    thisoid = heap_getattr(tup, anum_oid, RelationGetDescr(rel), &isnull);
    Assert(!isnull);

    CHECK_FOR_INTERRUPTS();

    (void)hash_search(htab, (void *)&thisoid, HASH_ENTER, NULL);
  }
  table_endscan(scan);
  UnregisterSnapshot(snapshot);
  table_close(rel, AccessShareLock);

  return htab;
}

              
                            
   
                                                       
                                                                         
                                                    
              
   
void
pgstat_drop_database(Oid databaseid)
{
  PgStat_MsgDropdb msg;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_DROPDB);
  msg.m_databaseid = databaseid;
  pgstat_send(&msg, sizeof(msg));
}

              
                            
   
                                                       
                                                                            
                                                    
   
                                                                     
                                                                      
              
   
#ifdef NOT_USED
void
pgstat_drop_relation(Oid relid)
{
  PgStat_MsgTabpurge msg;
  int len;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  msg.m_tableid[0] = relid;
  msg.m_nentries = 1;

  len = offsetof(PgStat_MsgTabpurge, m_tableid[0]) + sizeof(Oid);

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_TABPURGE);
  msg.m_databaseid = MyDatabaseId;
  pgstat_send(&msg, len);
}
#endif               

              
                             
   
                                                                     
   
                                                                       
                 
              
   
void
pgstat_reset_counters(void)
{
  PgStat_MsgResetcounter msg;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_RESETCOUNTER);
  msg.m_databaseid = MyDatabaseId;
  pgstat_send(&msg, sizeof(msg));
}

              
                                    
   
                                                                        
   
                                                                       
                 
              
   
void
pgstat_reset_shared_counters(const char *target)
{
  PgStat_MsgResetsharedcounter msg;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  if (strcmp(target, "archiver") == 0)
  {
    msg.m_resettarget = RESET_ARCHIVER;
  }
  else if (strcmp(target, "bgwriter") == 0)
  {
    msg.m_resettarget = RESET_BGWRITER;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized reset target: \"%s\"", target), errhint("Target must be \"archiver\" or \"bgwriter\".")));
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_RESETSHAREDCOUNTER);
  pgstat_send(&msg, sizeof(msg));
}

              
                                   
   
                                                            
   
                                                                       
                 
              
   
void
pgstat_reset_single_counter(Oid objoid, PgStat_Single_Reset_Type type)
{
  PgStat_MsgResetsinglecounter msg;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_RESETSINGLECOUNTER);
  msg.m_databaseid = MyDatabaseId;
  msg.m_resettype = type;
  msg.m_objectid = objoid;

  pgstat_send(&msg, sizeof(msg));
}

              
                             
   
                                                                        
                                                                             
                                          
              
   
void
pgstat_report_autovac(Oid dboid)
{
  PgStat_MsgAutovacStart msg;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_AUTOVAC_START);
  msg.m_databaseid = dboid;
  msg.m_start_time = GetCurrentTimestamp();

  pgstat_send(&msg, sizeof(msg));
}

             
                            
   
                                                        
             
   
void
pgstat_report_vacuum(Oid tableoid, bool shared, PgStat_Counter livetuples, PgStat_Counter deadtuples)
{
  PgStat_MsgVacuum msg;

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_VACUUM);
  msg.m_databaseid = shared ? InvalidOid : MyDatabaseId;
  msg.m_tableoid = tableoid;
  msg.m_autovacuum = IsAutoVacuumWorkerProcess();
  msg.m_vacuumtime = GetCurrentTimestamp();
  msg.m_live_tuples = livetuples;
  msg.m_dead_tuples = deadtuples;
  pgstat_send(&msg, sizeof(msg));
}

            
                             
   
                                                        
   
                                                                         
                                                                       
            
   
void
pgstat_report_analyze(Relation rel, PgStat_Counter livetuples, PgStat_Counter deadtuples, bool resetcounter)
{
  PgStat_MsgAnalyze msg;

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
    return;
  }

     
                                                                           
                                                                            
                                                                          
                                                                           
                                                                           
                                                                           
                                                                     
                  
     
  if (rel->pgstat_info != NULL)
  {
    PgStat_TableXactStatus *trans;

    for (trans = rel->pgstat_info->trans; trans; trans = trans->upper)
    {
      livetuples -= trans->tuples_inserted - trans->tuples_deleted;
      deadtuples -= trans->tuples_updated + trans->tuples_deleted;
    }
                                                               
    deadtuples -= rel->pgstat_info->t_counts.t_delta_dead_tuples;
                                                                         
    livetuples = Max(livetuples, 0);
    deadtuples = Max(deadtuples, 0);
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_ANALYZE);
  msg.m_databaseid = rel->rd_rel->relisshared ? InvalidOid : MyDatabaseId;
  msg.m_tableoid = RelationGetRelid(rel);
  msg.m_autovacuum = IsAutoVacuumWorkerProcess();
  msg.m_resetcounter = resetcounter;
  msg.m_analyzetime = GetCurrentTimestamp();
  msg.m_live_tuples = livetuples;
  msg.m_dead_tuples = deadtuples;
  pgstat_send(&msg, sizeof(msg));
}

            
                                       
   
                                                             
            
   
void
pgstat_report_recovery_conflict(int reason)
{
  PgStat_MsgRecoveryConflict msg;

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_RECOVERYCONFLICT);
  msg.m_databaseid = MyDatabaseId;
  msg.m_reason = reason;
  pgstat_send(&msg, sizeof(msg));
}

            
                              
   
                                                 
            
   
void
pgstat_report_deadlock(void)
{
  PgStat_MsgDeadlock msg;

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_DEADLOCK);
  msg.m_databaseid = MyDatabaseId;
  pgstat_send(&msg, sizeof(msg));
}

            
                                             
   
                                                           
            
   
void
pgstat_report_checksum_failures_in_db(Oid dboid, int failurecount)
{
  PgStat_MsgChecksumFailure msg;

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_CHECKSUMFAILURE);
  msg.m_databaseid = dboid;
  msg.m_failurecount = failurecount;
  msg.m_failure_time = GetCurrentTimestamp();

  pgstat_send(&msg, sizeof(msg));
}

            
                                      
   
                                                
            
   
void
pgstat_report_checksum_failure(void)
{
  pgstat_report_checksum_failures_in_db(MyDatabaseId, 1);
}

            
                              
   
                                              
            
   
void
pgstat_report_tempfile(size_t filesize)
{
  PgStat_MsgTempFile msg;

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_TEMPFILE);
  msg.m_databaseid = MyDatabaseId;
  msg.m_filesize = filesize;
  pgstat_send(&msg, sizeof(msg));
}

              
                   
   
                                                             
              
   
void
pgstat_ping(void)
{
  PgStat_MsgDummy msg;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_DUMMY);
  pgstat_send(&msg, sizeof(msg));
}

              
                           
   
                                             
              
   
static void
pgstat_send_inquiry(TimestampTz clock_time, TimestampTz cutoff_time, Oid databaseid)
{
  PgStat_MsgInquiry msg;

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_INQUIRY);
  msg.clock_time = clock_time;
  msg.cutoff_time = cutoff_time;
  msg.databaseid = databaseid;
  pgstat_send(&msg, sizeof(msg));
}

   
                                        
                                                      
   
void
pgstat_init_function_usage(FunctionCallInfo fcinfo, PgStat_FunctionCallUsage *fcu)
{
  PgStat_BackendFunctionEntry *htabent;
  bool found;

  if (pgstat_track_functions <= fcinfo->flinfo->fn_stats)
  {
                          
    fcu->fs = NULL;
    return;
  }

  if (!pgStatFunctions)
  {
                                                             
    HASHCTL hash_ctl;

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(Oid);
    hash_ctl.entrysize = sizeof(PgStat_BackendFunctionEntry);
    pgStatFunctions = hash_create("Function stat entries", PGSTAT_FUNCTION_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS);
  }

                                                                  
  htabent = hash_search(pgStatFunctions, &fcinfo->flinfo->fn_oid, HASH_ENTER, &found);
  if (!found)
  {
    MemSet(&htabent->f_counts, 0, sizeof(PgStat_FunctionCounts));
  }

  fcu->fs = &htabent->f_counts;

                                                                            
  fcu->save_f_total_time = htabent->f_counts.f_total_time;

                                            
  fcu->save_total = total_func_time;

                                           
  INSTR_TIME_SET_CURRENT(fcu->f_start);
}

   
                                                                             
                           
   
                                                    
   
PgStat_BackendFunctionEntry *
find_funcstat_entry(Oid func_id)
{
  if (pgStatFunctions == NULL)
  {
    return NULL;
  }

  return (PgStat_BackendFunctionEntry *)hash_search(pgStatFunctions, (void *)&func_id, HASH_FIND, NULL);
}

   
                                                           
                                                     
   
                                                                             
                                                                             
                                                                         
                                                  
   
void
pgstat_end_function_usage(PgStat_FunctionCallUsage *fcu, bool finalize)
{
  PgStat_FunctionCounts *fs = fcu->fs;
  instr_time f_total;
  instr_time f_others;
  instr_time f_self;

                         
  if (fs == NULL)
  {
    return;
  }

                                                
  INSTR_TIME_SET_CURRENT(f_total);
  INSTR_TIME_SUBTRACT(f_total, fcu->f_start);

                                                                         
  f_others = total_func_time;
  INSTR_TIME_SUBTRACT(f_others, fcu->save_total);
  f_self = f_total;
  INSTR_TIME_SUBTRACT(f_self, f_others);

                                      
  INSTR_TIME_ADD(total_func_time, f_self);

     
                                                                         
                                                                 
                                                                          
                                                                           
                           
     
  INSTR_TIME_ADD(f_total, fcu->save_f_total_time);

                                               
  if (finalize)
  {
    fs->f_numcalls++;
  }
  fs->f_total_time = f_total;
  INSTR_TIME_ADD(fs->f_self_time, f_self);

                                               
  have_function_stats = true;
}

              
                        
   
                                                           
                                         
   
                                                                    
                                                                           
                                                                          
                                                             
              
   
void
pgstat_initstats(Relation rel)
{
  Oid rel_id = rel->rd_id;
  char relkind = rel->rd_rel->relkind;

                                                        
  if (!(relkind == RELKIND_RELATION || relkind == RELKIND_MATVIEW || relkind == RELKIND_INDEX || relkind == RELKIND_TOASTVALUE || relkind == RELKIND_SEQUENCE))
  {
    rel->pgstat_info = NULL;
    return;
  }

  if (pgStatSock == PGINVALID_SOCKET || !pgstat_track_counts)
  {
                                   
    rel->pgstat_info = NULL;
    return;
  }

     
                                                                            
            
     
  if (rel->pgstat_info != NULL && rel->pgstat_info->t_id == rel_id)
  {
    return;
  }

                                                                       
  rel->pgstat_info = get_tabstat_entry(rel_id, rel->rd_rel->relisshared);
}

   
                                                                         
   
static PgStat_TableStatus *
get_tabstat_entry(Oid rel_id, bool isshared)
{
  TabStatHashEntry *hash_entry;
  PgStat_TableStatus *entry;
  TabStatusArray *tsa;
  bool found;

     
                                                    
     
  if (pgStatTabHash == NULL)
  {
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(Oid);
    ctl.entrysize = sizeof(TabStatHashEntry);

    pgStatTabHash = hash_create("pgstat TabStatusArray lookup hash table", TABSTAT_QUANTUM, &ctl, HASH_ELEM | HASH_BLOBS);
  }

     
                                        
     
  hash_entry = hash_search(pgStatTabHash, &rel_id, HASH_ENTER, &found);
  if (!found)
  {
                                                
    hash_entry->tsa_entry = NULL;
  }

     
                                            
     
  if (hash_entry->tsa_entry)
  {
    return hash_entry->tsa_entry;
  }

     
                                                                             
                                                                             
                                                                    
     
  if (pgStatTabList == NULL)
  {
                                          
    pgStatTabList = (TabStatusArray *)MemoryContextAllocZero(TopMemoryContext, sizeof(TabStatusArray));
  }

  tsa = pgStatTabList;
  while (tsa->tsa_used >= TABSTAT_QUANTUM)
  {
    if (tsa->tsa_next == NULL)
    {
      tsa->tsa_next = (TabStatusArray *)MemoryContextAllocZero(TopMemoryContext, sizeof(TabStatusArray));
    }
    tsa = tsa->tsa_next;
  }

     
                                                                            
                                                                         
     
  entry = &tsa->tsa_entries[tsa->tsa_used++];
  entry->t_id = rel_id;
  entry->t_shared = isshared;

     
                                                 
     
  hash_entry->tsa_entry = entry;

  return entry;
}

   
                                                                           
   
                                                    
   
                                                                                
                                                                             
                                                          
   
PgStat_TableStatus *
find_tabstat_entry(Oid rel_id)
{
  TabStatHashEntry *hash_entry;

                                                               
  if (!pgStatTabHash)
  {
    return NULL;
  }

  hash_entry = hash_search(pgStatTabHash, &rel_id, HASH_FIND, NULL);
  if (!hash_entry)
  {
    return NULL;
  }

                                                                      
  return hash_entry->tsa_entry;
}

   
                                                                              
   
static PgStat_SubXactStatus *
get_tabstat_stack_level(int nest_level)
{
  PgStat_SubXactStatus *xact_state;

  xact_state = pgStatXactStack;
  if (xact_state == NULL || xact_state->nest_level != nest_level)
  {
    xact_state = (PgStat_SubXactStatus *)MemoryContextAlloc(TopTransactionContext, sizeof(PgStat_SubXactStatus));
    xact_state->nest_level = nest_level;
    xact_state->prev = pgStatXactStack;
    xact_state->first = NULL;
    pgStatXactStack = xact_state;
  }
  return xact_state;
}

   
                                                                    
   
static void
add_tabstat_xact_level(PgStat_TableStatus *pgstat_info, int nest_level)
{
  PgStat_SubXactStatus *xact_state;
  PgStat_TableXactStatus *trans;

     
                                                                           
                                                   
     
  xact_state = get_tabstat_stack_level(nest_level);

                                        
  trans = (PgStat_TableXactStatus *)MemoryContextAllocZero(TopTransactionContext, sizeof(PgStat_TableXactStatus));
  trans->nest_level = nest_level;
  trans->upper = pgstat_info->trans;
  trans->parent = pgstat_info;
  trans->next = xact_state->first;
  xact_state->first = trans;
  pgstat_info->trans = trans;
}

   
                                                                  
   
void
pgstat_count_heap_insert(Relation rel, PgStat_Counter n)
{
  PgStat_TableStatus *pgstat_info = rel->pgstat_info;

  if (pgstat_info != NULL)
  {
                                                                     
    int nest_level = GetCurrentTransactionNestLevel();

    if (pgstat_info->trans == NULL || pgstat_info->trans->nest_level != nest_level)
    {
      add_tabstat_xact_level(pgstat_info, nest_level);
    }

    pgstat_info->trans->tuples_inserted += n;
  }
}

   
                                                   
   
void
pgstat_count_heap_update(Relation rel, bool hot)
{
  PgStat_TableStatus *pgstat_info = rel->pgstat_info;

  if (pgstat_info != NULL)
  {
                                                                     
    int nest_level = GetCurrentTransactionNestLevel();

    if (pgstat_info->trans == NULL || pgstat_info->trans->nest_level != nest_level)
    {
      add_tabstat_xact_level(pgstat_info, nest_level);
    }

    pgstat_info->trans->tuples_updated++;

                                                                      
    if (hot)
    {
      pgstat_info->t_counts.t_tuples_hot_updated++;
    }
  }
}

   
                                                     
   
void
pgstat_count_heap_delete(Relation rel)
{
  PgStat_TableStatus *pgstat_info = rel->pgstat_info;

  if (pgstat_info != NULL)
  {
                                                                     
    int nest_level = GetCurrentTransactionNestLevel();

    if (pgstat_info->trans == NULL || pgstat_info->trans->nest_level != nest_level)
    {
      add_tabstat_xact_level(pgstat_info, nest_level);
    }

    pgstat_info->trans->tuples_deleted++;
  }
}

   
                                 
   
                                                                              
                                                                             
                                                                                
                                                                    
   
static void
pgstat_truncate_save_counters(PgStat_TableXactStatus *trans)
{
  if (!trans->truncated)
  {
    trans->inserted_pre_trunc = trans->tuples_inserted;
    trans->updated_pre_trunc = trans->tuples_updated;
    trans->deleted_pre_trunc = trans->tuples_deleted;
    trans->truncated = true;
  }
}

   
                                                                              
   
static void
pgstat_truncate_restore_counters(PgStat_TableXactStatus *trans)
{
  if (trans->truncated)
  {
    trans->tuples_inserted = trans->inserted_pre_trunc;
    trans->tuples_updated = trans->updated_pre_trunc;
    trans->tuples_deleted = trans->deleted_pre_trunc;
  }
}

   
                                                                 
   
void
pgstat_count_truncate(Relation rel)
{
  PgStat_TableStatus *pgstat_info = rel->pgstat_info;

  if (pgstat_info != NULL)
  {
                                                                     
    int nest_level = GetCurrentTransactionNestLevel();

    if (pgstat_info->trans == NULL || pgstat_info->trans->nest_level != nest_level)
    {
      add_tabstat_xact_level(pgstat_info, nest_level);
    }

    pgstat_truncate_save_counters(pgstat_info->trans);
    pgstat_info->trans->tuples_inserted = 0;
    pgstat_info->trans->tuples_updated = 0;
    pgstat_info->trans->tuples_deleted = 0;
  }
}

   
                                                             
   
                                                                        
                                                                     
                                                                           
                                          
   
void
pgstat_update_heap_dead_tuples(Relation rel, int delta)
{
  PgStat_TableStatus *pgstat_info = rel->pgstat_info;

  if (pgstat_info != NULL)
  {
    pgstat_info->t_counts.t_delta_dead_tuples -= delta;
  }
}

              
                   
   
                                                                            
              
   
void
AtEOXact_PgStat(bool isCommit, bool parallel)
{
  PgStat_SubXactStatus *xact_state;

                                                     
  if (!parallel)
  {
       
                                                                      
                                                                    
       
    if (isCommit)
    {
      pgStatXactCommit++;
    }
    else
    {
      pgStatXactRollback++;
    }
  }

     
                                                                       
                                                                             
                                                                
     
  xact_state = pgStatXactStack;
  if (xact_state != NULL)
  {
    PgStat_TableXactStatus *trans;

    Assert(xact_state->nest_level == 1);
    Assert(xact_state->prev == NULL);
    for (trans = xact_state->first; trans != NULL; trans = trans->next)
    {
      PgStat_TableStatus *tabstat;

      Assert(trans->nest_level == 1);
      Assert(trans->upper == NULL);
      tabstat = trans->parent;
      Assert(tabstat->trans == trans);
                                                                       
      if (!isCommit)
      {
        pgstat_truncate_restore_counters(trans);
      }
                                                              
      tabstat->t_counts.t_tuples_inserted += trans->tuples_inserted;
      tabstat->t_counts.t_tuples_updated += trans->tuples_updated;
      tabstat->t_counts.t_tuples_deleted += trans->tuples_deleted;
      if (isCommit)
      {
        tabstat->t_counts.t_truncated = trans->truncated;
        if (trans->truncated)
        {
                                                               
          tabstat->t_counts.t_delta_live_tuples = 0;
          tabstat->t_counts.t_delta_dead_tuples = 0;
        }
                                                          
        tabstat->t_counts.t_delta_live_tuples += trans->tuples_inserted - trans->tuples_deleted;
                                                        
        tabstat->t_counts.t_delta_dead_tuples += trans->tuples_updated + trans->tuples_deleted;
                                                                   
        tabstat->t_counts.t_changed_tuples += trans->tuples_inserted + trans->tuples_updated + trans->tuples_deleted;
      }
      else
      {
                                                                     
        tabstat->t_counts.t_delta_dead_tuples += trans->tuples_inserted + trans->tuples_updated;
                                                               
      }
      tabstat->trans = NULL;
    }
  }
  pgStatXactStack = NULL;

                                                   
  pgstat_clear_snapshot();
}

              
                      
   
                                                                     
              
   
void
AtEOSubXact_PgStat(bool isCommit, int nestDepth)
{
  PgStat_SubXactStatus *xact_state;

     
                                                                      
                           
     
  xact_state = pgStatXactStack;
  if (xact_state != NULL && xact_state->nest_level >= nestDepth)
  {
    PgStat_TableXactStatus *trans;
    PgStat_TableXactStatus *next_trans;

                                                                         
    pgStatXactStack = xact_state->prev;

    for (trans = xact_state->first; trans != NULL; trans = next_trans)
    {
      PgStat_TableStatus *tabstat;

      next_trans = trans->next;
      Assert(trans->nest_level == nestDepth);
      tabstat = trans->parent;
      Assert(tabstat->trans == trans);
      if (isCommit)
      {
        if (trans->upper && trans->upper->nest_level == nestDepth - 1)
        {
          if (trans->truncated)
          {
                                                            
            pgstat_truncate_save_counters(trans->upper);
                                                    
            trans->upper->tuples_inserted = trans->tuples_inserted;
            trans->upper->tuples_updated = trans->tuples_updated;
            trans->upper->tuples_deleted = trans->tuples_deleted;
          }
          else
          {
            trans->upper->tuples_inserted += trans->tuples_inserted;
            trans->upper->tuples_updated += trans->tuples_updated;
            trans->upper->tuples_deleted += trans->tuples_deleted;
          }
          tabstat->trans = trans->upper;
          pfree(trans);
        }
        else
        {
             
                                                                     
                                                         
                                                               
                                                                   
                                                                
                                                           
             
          PgStat_SubXactStatus *upper_xact_state;

          upper_xact_state = get_tabstat_stack_level(nestDepth - 1);
          trans->next = upper_xact_state->first;
          upper_xact_state->first = trans;
          trans->nest_level = nestDepth - 1;
        }
      }
      else
      {
           
                                                                      
                          
           

                                                          
        pgstat_truncate_restore_counters(trans);
                                                                
        tabstat->t_counts.t_tuples_inserted += trans->tuples_inserted;
        tabstat->t_counts.t_tuples_updated += trans->tuples_updated;
        tabstat->t_counts.t_tuples_deleted += trans->tuples_deleted;
                                                                     
        tabstat->t_counts.t_delta_dead_tuples += trans->tuples_inserted + trans->tuples_updated;
        tabstat->trans = trans->upper;
        pfree(trans);
      }
    }
    pfree(xact_state);
  }
}

   
                    
                                                                   
   
                                                                  
                                     
   
void
AtPrepare_PgStat(void)
{
  PgStat_SubXactStatus *xact_state;

  xact_state = pgStatXactStack;
  if (xact_state != NULL)
  {
    PgStat_TableXactStatus *trans;

    Assert(xact_state->nest_level == 1);
    Assert(xact_state->prev == NULL);
    for (trans = xact_state->first; trans != NULL; trans = trans->next)
    {
      PgStat_TableStatus *tabstat;
      TwoPhasePgStatRecord record;

      Assert(trans->nest_level == 1);
      Assert(trans->upper == NULL);
      tabstat = trans->parent;
      Assert(tabstat->trans == trans);

      record.tuples_inserted = trans->tuples_inserted;
      record.tuples_updated = trans->tuples_updated;
      record.tuples_deleted = trans->tuples_deleted;
      record.inserted_pre_trunc = trans->inserted_pre_trunc;
      record.updated_pre_trunc = trans->updated_pre_trunc;
      record.deleted_pre_trunc = trans->deleted_pre_trunc;
      record.t_id = tabstat->t_id;
      record.t_shared = tabstat->t_shared;
      record.t_truncated = trans->truncated;

      RegisterTwoPhaseRecord(TWOPHASE_RM_PGSTAT_ID, 0, &record, sizeof(TwoPhasePgStatRecord));
    }
  }
}

   
                      
                                       
   
                                                                      
                                                                       
                                                                          
                                                              
   
                                                       
   
void
PostPrepare_PgStat(void)
{
  PgStat_SubXactStatus *xact_state;

     
                                                                            
                                                       
     
  xact_state = pgStatXactStack;
  if (xact_state != NULL)
  {
    PgStat_TableXactStatus *trans;

    for (trans = xact_state->first; trans != NULL; trans = trans->next)
    {
      PgStat_TableStatus *tabstat;

      tabstat = trans->parent;
      tabstat->trans = NULL;
    }
  }
  pgStatXactStack = NULL;

                                                   
  pgstat_clear_snapshot();
}

   
                                                    
   
                                                       
   
void
pgstat_twophase_postcommit(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  TwoPhasePgStatRecord *rec = (TwoPhasePgStatRecord *)recdata;
  PgStat_TableStatus *pgstat_info;

                                                  
  pgstat_info = get_tabstat_entry(rec->t_id, rec->t_shared);

                                                    
  pgstat_info->t_counts.t_tuples_inserted += rec->tuples_inserted;
  pgstat_info->t_counts.t_tuples_updated += rec->tuples_updated;
  pgstat_info->t_counts.t_tuples_deleted += rec->tuples_deleted;
  pgstat_info->t_counts.t_truncated = rec->t_truncated;
  if (rec->t_truncated)
  {
                                                         
    pgstat_info->t_counts.t_delta_live_tuples = 0;
    pgstat_info->t_counts.t_delta_dead_tuples = 0;
  }
  pgstat_info->t_counts.t_delta_live_tuples += rec->tuples_inserted - rec->tuples_deleted;
  pgstat_info->t_counts.t_delta_dead_tuples += rec->tuples_updated + rec->tuples_deleted;
  pgstat_info->t_counts.t_changed_tuples += rec->tuples_inserted + rec->tuples_updated + rec->tuples_deleted;
}

   
                                                      
   
                                                                      
               
   
void
pgstat_twophase_postabort(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  TwoPhasePgStatRecord *rec = (TwoPhasePgStatRecord *)recdata;
  PgStat_TableStatus *pgstat_info;

                                                  
  pgstat_info = get_tabstat_entry(rec->t_id, rec->t_shared);

                                                   
  if (rec->t_truncated)
  {
    rec->tuples_inserted = rec->inserted_pre_trunc;
    rec->tuples_updated = rec->updated_pre_trunc;
    rec->tuples_deleted = rec->deleted_pre_trunc;
  }
  pgstat_info->t_counts.t_tuples_inserted += rec->tuples_inserted;
  pgstat_info->t_counts.t_tuples_updated += rec->tuples_updated;
  pgstat_info->t_counts.t_tuples_deleted += rec->tuples_deleted;
  pgstat_info->t_counts.t_delta_dead_tuples += rec->tuples_inserted + rec->tuples_updated;
}

              
                                 
   
                                                                    
                                                                        
                                                                    
                                                                  
              
   
PgStat_StatDBEntry *
pgstat_fetch_stat_dbentry(Oid dbid)
{
     
                                                                           
                                 
     
  backend_read_statsfile();

     
                                                             
     
  return (PgStat_StatDBEntry *)hash_search(pgStatDBHash, (void *)&dbid, HASH_FIND, NULL);
}

              
                                  
   
                                                                    
                                                                     
                                                                 
                                                                  
              
   
PgStat_StatTabEntry *
pgstat_fetch_stat_tabentry(Oid relid)
{
  Oid dbid;
  PgStat_StatDBEntry *dbentry;
  PgStat_StatTabEntry *tabentry;

     
                                                                           
                                 
     
  backend_read_statsfile();

     
                                                             
     
  dbid = MyDatabaseId;
  dbentry = (PgStat_StatDBEntry *)hash_search(pgStatDBHash, (void *)&dbid, HASH_FIND, NULL);
  if (dbentry != NULL && dbentry->tables != NULL)
  {
    tabentry = (PgStat_StatTabEntry *)hash_search(dbentry->tables, (void *)&relid, HASH_FIND, NULL);
    if (tabentry)
    {
      return tabentry;
    }
  }

     
                                                      
     
  dbid = InvalidOid;
  dbentry = (PgStat_StatDBEntry *)hash_search(pgStatDBHash, (void *)&dbid, HASH_FIND, NULL);
  if (dbentry != NULL && dbentry->tables != NULL)
  {
    tabentry = (PgStat_StatTabEntry *)hash_search(dbentry->tables, (void *)&relid, HASH_FIND, NULL);
    if (tabentry)
    {
      return tabentry;
    }
  }

  return NULL;
}

              
                                   
   
                                                                    
                                                      
              
   
PgStat_StatFuncEntry *
pgstat_fetch_stat_funcentry(Oid func_id)
{
  PgStat_StatDBEntry *dbentry;
  PgStat_StatFuncEntry *funcentry = NULL;

                                     
  backend_read_statsfile();

                                                               
  dbentry = pgstat_fetch_stat_dbentry(MyDatabaseId);
  if (dbentry != NULL && dbentry->functions != NULL)
  {
    funcentry = (PgStat_StatFuncEntry *)hash_search(dbentry->functions, (void *)&func_id, HASH_FIND, NULL);
  }

  return funcentry;
}

              
                                 
   
                                                                    
                                                                 
   
                                                                         
                                           
              
   
PgBackendStatus *
pgstat_fetch_stat_beentry(int beid)
{
  pgstat_read_current_status();

  if (beid < 1 || beid > localNumBackends)
  {
    return NULL;
  }

  return &localBackendStatusTable[beid - 1].backendStatus;
}

              
                                       
   
                                                                              
                                       
   
                                                                         
                                           
              
   
LocalPgBackendStatus *
pgstat_fetch_stat_local_beentry(int beid)
{
  pgstat_read_current_status();

  if (beid < 1 || beid > localNumBackends)
  {
    return NULL;
  }

  return &localBackendStatusTable[beid - 1];
}

              
                                     
   
                                                                    
                                   
              
   
int
pgstat_fetch_stat_numbackends(void)
{
  pgstat_read_current_status();

  return localNumBackends;
}

   
             
                                  
   
                                                                    
                                                
             
   
PgStat_ArchiverStats *
pgstat_fetch_stat_archiver(void)
{
  backend_read_statsfile();

  return &archiverStats;
}

   
             
                           
   
                                                                    
                                              
             
   
PgStat_GlobalStats *
pgstat_fetch_global(void)
{
  backend_read_statsfile();

  return &globalStats;
}

                                                                
                                                                       
                                                                
   

static PgBackendStatus *BackendStatusArray = NULL;
static PgBackendStatus *MyBEEntry = NULL;
static char *BackendAppnameBuffer = NULL;
static char *BackendClientHostnameBuffer = NULL;
static char *BackendActivityBuffer = NULL;
static Size BackendActivityBufferSize = 0;
#ifdef USE_SSL
static PgBackendSSLStatus *BackendSslStatusBuffer = NULL;
#endif
#ifdef ENABLE_GSS
static PgBackendGSSStatus *BackendGssStatusBuffer = NULL;
#endif

   
                                                                   
   
Size
BackendStatusShmemSize(void)
{
  Size size;

                           
  size = mul_size(sizeof(PgBackendStatus), NumBackendStatSlots);
                             
  size = add_size(size, mul_size(NAMEDATALEN, NumBackendStatSlots));
                                    
  size = add_size(size, mul_size(NAMEDATALEN, NumBackendStatSlots));
                              
  size = add_size(size, mul_size(pgstat_track_activity_query_size, NumBackendStatSlots));
#ifdef USE_SSL
                               
  size = add_size(size, mul_size(sizeof(PgBackendSSLStatus), NumBackendStatSlots));
#endif
#ifdef ENABLE_GSS
                               
  size = add_size(size, mul_size(sizeof(PgBackendGSSStatus), NumBackendStatSlots));
#endif
  return size;
}

   
                                                                 
                              
   
void
CreateSharedBackendStatus(void)
{
  Size size;
  bool found;
  int i;
  char *buffer;

                                            
  size = mul_size(sizeof(PgBackendStatus), NumBackendStatSlots);
  BackendStatusArray = (PgBackendStatus *)ShmemInitStruct("Backend Status Array", size, &found);

  if (!found)
  {
       
                                     
       
    MemSet(BackendStatusArray, 0, size);
  }

                                                     
  size = mul_size(NAMEDATALEN, NumBackendStatSlots);
  BackendAppnameBuffer = (char *)ShmemInitStruct("Backend Application Name Buffer", size, &found);

  if (!found)
  {
    MemSet(BackendAppnameBuffer, 0, size);

                                         
    buffer = BackendAppnameBuffer;
    for (i = 0; i < NumBackendStatSlots; i++)
    {
      BackendStatusArray[i].st_appname = buffer;
      buffer += NAMEDATALEN;
    }
  }

                                                             
  size = mul_size(NAMEDATALEN, NumBackendStatSlots);
  BackendClientHostnameBuffer = (char *)ShmemInitStruct("Backend Client Host Name Buffer", size, &found);

  if (!found)
  {
    MemSet(BackendClientHostnameBuffer, 0, size);

                                                
    buffer = BackendClientHostnameBuffer;
    for (i = 0; i < NumBackendStatSlots; i++)
    {
      BackendStatusArray[i].st_clienthostname = buffer;
      buffer += NAMEDATALEN;
    }
  }

                                                      
  BackendActivityBufferSize = mul_size(pgstat_track_activity_query_size, NumBackendStatSlots);
  BackendActivityBuffer = (char *)ShmemInitStruct("Backend Activity Buffer", BackendActivityBufferSize, &found);

  if (!found)
  {
    MemSet(BackendActivityBuffer, 0, BackendActivityBufferSize);

                                          
    buffer = BackendActivityBuffer;
    for (i = 0; i < NumBackendStatSlots; i++)
    {
      BackendStatusArray[i].st_activity_raw = buffer;
      buffer += pgstat_track_activity_query_size;
    }
  }

#ifdef USE_SSL
                                                        
  size = mul_size(sizeof(PgBackendSSLStatus), NumBackendStatSlots);
  BackendSslStatusBuffer = (PgBackendSSLStatus *)ShmemInitStruct("Backend SSL Status Buffer", size, &found);

  if (!found)
  {
    PgBackendSSLStatus *ptr;

    MemSet(BackendSslStatusBuffer, 0, size);

                                           
    ptr = BackendSslStatusBuffer;
    for (i = 0; i < NumBackendStatSlots; i++)
    {
      BackendStatusArray[i].st_sslstatus = ptr;
      ptr++;
    }
  }
#endif

#ifdef ENABLE_GSS
                                                           
  size = mul_size(sizeof(PgBackendGSSStatus), NumBackendStatSlots);
  BackendGssStatusBuffer = (PgBackendGSSStatus *)ShmemInitStruct("Backend GSS Status Buffer", size, &found);

  if (!found)
  {
    PgBackendGSSStatus *ptr;

    MemSet(BackendGssStatusBuffer, 0, size);

                                           
    ptr = BackendGssStatusBuffer;
    for (i = 0; i < NumBackendStatSlots; i++)
    {
      BackendStatusArray[i].st_gssstatus = ptr;
      ptr++;
    }
  }
#endif
}

              
                         
   
                                                               
                                                                             
                                                               
                                                               
                                                        
                                                                             
              
   
void
pgstat_initialize(void)
{
                            
  if (MyBackendId != InvalidBackendId)
  {
    Assert(MyBackendId >= 1 && MyBackendId <= MaxBackends);
    MyBEEntry = &BackendStatusArray[MyBackendId - 1];
  }
  else
  {
                                      
    Assert(MyAuxProcType != NotAnAuxProcess);

       
                                                                        
                                                                       
                                                                           
                                                                 
                                                                        
                          
       
    MyBEEntry = &BackendStatusArray[MaxBackends + MyAuxProcType];
  }

                                              
  on_shmem_exit(pgstat_beshutdown_hook, 0);
}

              
                      
   
                                                                 
                             
   
                                                              
                                                          
                                                                    
                                                                       
                                                                      
              
   
void
pgstat_bestart(void)
{
  volatile PgBackendStatus *vbeentry = MyBEEntry;
  PgBackendStatus lbeentry;
#ifdef USE_SSL
  PgBackendSSLStatus lsslstatus;
#endif
#ifdef ENABLE_GSS
  PgBackendGSSStatus lgssstatus;
#endif

                                                                  
  Assert(vbeentry != NULL);

     
                                                                         
                                                                         
                                                                           
                                                                         
                                                               
     
                                                                     
                                                                    
                                                         
     
  memcpy(&lbeentry, unvolatize(PgBackendStatus *, vbeentry), sizeof(PgBackendStatus));

                                                                  
#ifdef USE_SSL
  memset(&lsslstatus, 0, sizeof(lsslstatus));
#endif
#ifdef ENABLE_GSS
  memset(&lgssstatus, 0, sizeof(lgssstatus));
#endif

     
                                                                         
                                                                    
     
  lbeentry.st_procpid = MyProcPid;

  if (MyBackendId != InvalidBackendId)
  {
    if (IsAutoVacuumLauncherProcess())
    {
                               
      lbeentry.st_backendType = B_AUTOVAC_LAUNCHER;
    }
    else if (IsAutoVacuumWorkerProcess())
    {
                             
      lbeentry.st_backendType = B_AUTOVAC_WORKER;
    }
    else if (am_walsender)
    {
                      
      lbeentry.st_backendType = B_WAL_SENDER;
    }
    else if (IsBackgroundWorker)
    {
                    
      lbeentry.st_backendType = B_BG_WORKER;
    }
    else
    {
                          
      lbeentry.st_backendType = B_BACKEND;
    }
  }
  else
  {
                                      
    Assert(MyAuxProcType != NotAnAuxProcess);
    switch (MyAuxProcType)
    {
    case StartupProcess:
      lbeentry.st_backendType = B_STARTUP;
      break;
    case BgWriterProcess:
      lbeentry.st_backendType = B_BG_WRITER;
      break;
    case CheckpointerProcess:
      lbeentry.st_backendType = B_CHECKPOINTER;
      break;
    case WalWriterProcess:
      lbeentry.st_backendType = B_WAL_WRITER;
      break;
    case WalReceiverProcess:
      lbeentry.st_backendType = B_WAL_RECEIVER;
      break;
    default:
      elog(FATAL, "unrecognized process type: %d", (int)MyAuxProcType);
    }
  }

  lbeentry.st_proc_start_timestamp = MyStartTimestamp;
  lbeentry.st_activity_start_timestamp = 0;
  lbeentry.st_state_start_timestamp = 0;
  lbeentry.st_xact_start_timestamp = 0;
  lbeentry.st_databaseid = MyDatabaseId;

                                                                             
  if (lbeentry.st_backendType == B_BACKEND || lbeentry.st_backendType == B_WAL_SENDER || lbeentry.st_backendType == B_BG_WORKER)
  {
    lbeentry.st_userid = GetSessionUserId();
  }
  else
  {
    lbeentry.st_userid = InvalidOid;
  }

     
                                                                           
                                                                            
                                                                          
     
  if (MyProcPort)
  {
    memcpy(&lbeentry.st_clientaddr, &MyProcPort->raddr, sizeof(lbeentry.st_clientaddr));
  }
  else
  {
    MemSet(&lbeentry.st_clientaddr, 0, sizeof(lbeentry.st_clientaddr));
  }

#ifdef USE_SSL
  if (MyProcPort && MyProcPort->ssl != NULL)
  {
    lbeentry.st_ssl = true;
    lsslstatus.ssl_bits = be_tls_get_cipher_bits(MyProcPort);
    lsslstatus.ssl_compression = be_tls_get_compression(MyProcPort);
    strlcpy(lsslstatus.ssl_version, be_tls_get_version(MyProcPort), NAMEDATALEN);
    strlcpy(lsslstatus.ssl_cipher, be_tls_get_cipher(MyProcPort), NAMEDATALEN);
    be_tls_get_peer_subject_name(MyProcPort, lsslstatus.ssl_client_dn, NAMEDATALEN);
    be_tls_get_peer_serial(MyProcPort, lsslstatus.ssl_client_serial, NAMEDATALEN);
    be_tls_get_peer_issuer_name(MyProcPort, lsslstatus.ssl_issuer_dn, NAMEDATALEN);
  }
  else
  {
    lbeentry.st_ssl = false;
  }
#else
  lbeentry.st_ssl = false;
#endif

#ifdef ENABLE_GSS
  if (MyProcPort && MyProcPort->gss != NULL)
  {
    const char *princ = be_gssapi_get_princ(MyProcPort);

    lbeentry.st_gss = true;
    lgssstatus.gss_auth = be_gssapi_get_auth(MyProcPort);
    lgssstatus.gss_enc = be_gssapi_get_enc(MyProcPort);
    if (princ)
    {
      strlcpy(lgssstatus.gss_princ, princ, NAMEDATALEN);
    }
  }
  else
  {
    lbeentry.st_gss = false;
  }
#else
  lbeentry.st_gss = false;
#endif

  lbeentry.st_state = STATE_UNDEFINED;
  lbeentry.st_progress_command = PROGRESS_COMMAND_INVALID;
  lbeentry.st_progress_command_target = InvalidOid;

     
                                                                        
                                                                          
                                   
     

     
                                                                            
                                                                            
                                                                       
                                                                  
     
  PGSTAT_BEGIN_WRITE_ACTIVITY(vbeentry);

                                                           
  lbeentry.st_changecount = vbeentry->st_changecount;

  memcpy(unvolatize(PgBackendStatus *, vbeentry), &lbeentry, sizeof(PgBackendStatus));

     
                                                                         
                                                                      
     
  lbeentry.st_appname[0] = '\0';
  if (MyProcPort && MyProcPort->remote_hostname)
  {
    strlcpy(lbeentry.st_clienthostname, MyProcPort->remote_hostname, NAMEDATALEN);
  }
  else
  {
    lbeentry.st_clienthostname[0] = '\0';
  }
  lbeentry.st_activity_raw[0] = '\0';
                                                                    
  lbeentry.st_appname[NAMEDATALEN - 1] = '\0';
  lbeentry.st_clienthostname[NAMEDATALEN - 1] = '\0';
  lbeentry.st_activity_raw[pgstat_track_activity_query_size - 1] = '\0';

#ifdef USE_SSL
  memcpy(lbeentry.st_sslstatus, &lsslstatus, sizeof(PgBackendSSLStatus));
#endif
#ifdef ENABLE_GSS
  memcpy(lbeentry.st_gssstatus, &lgssstatus, sizeof(PgBackendGSSStatus));
#endif

  PGSTAT_END_WRITE_ACTIVITY(vbeentry);

                                              
  if (application_name)
  {
    pgstat_report_appname(application_name);
  }
}

   
                                                                      
   
                                                               
                                                                   
                                           
   
                                                             
   
static void
pgstat_beshutdown_hook(int code, Datum arg)
{
  volatile PgBackendStatus *beentry = MyBEEntry;

     
                                                                             
                                                                     
                                                                          
                                                            
     
  if (OidIsValid(MyDatabaseId))
  {
    pgstat_report_stat(true);
  }

     
                                                                             
                                                                     
                                       
     
  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);

  beentry->st_procpid = 0;                   

  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                              
   
                                                                            
                                                     
   
                                                                  
                                                                       
                                                
              
   
void
pgstat_report_activity(BackendState state, const char *cmd_str)
{
  volatile PgBackendStatus *beentry = MyBEEntry;
  TimestampTz start_timestamp;
  TimestampTz current_timestamp;
  int len = 0;

  TRACE_POSTGRESQL_STATEMENT_STATUS(cmd_str);

  if (!beentry)
  {
    return;
  }

  if (!pgstat_track_activities)
  {
    if (beentry->st_state != STATE_DISABLED)
    {
      volatile PGPROC *proc = MyProc;

         
                                                              
                                                                        
                                                       
         
      PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);
      beentry->st_state = STATE_DISABLED;
      beentry->st_state_start_timestamp = 0;
      beentry->st_activity_raw[0] = '\0';
      beentry->st_activity_start_timestamp = 0;
                                                                         
      beentry->st_xact_start_timestamp = 0;
      proc->wait_event_info = 0;
      PGSTAT_END_WRITE_ACTIVITY(beentry);
    }
    return;
  }

     
                                                                       
                                                                          
     
  start_timestamp = GetCurrentStatementStartTimestamp();
  if (cmd_str != NULL)
  {
       
                                                                   
                                                                           
                                  
       
    len = Min(strlen(cmd_str), pgstat_track_activity_query_size - 1);
  }
  current_timestamp = GetCurrentTimestamp();

     
                                 
     
  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);

  beentry->st_state = state;
  beentry->st_state_start_timestamp = current_timestamp;

  if (cmd_str != NULL)
  {
    memcpy((char *)beentry->st_activity_raw, cmd_str, len);
    beentry->st_activity_raw[len] = '\0';
    beentry->st_activity_start_timestamp = start_timestamp;
  }

  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                                     
   
                                                                           
                                                          
              
   
void
pgstat_progress_start_command(ProgressCommandType cmdtype, Oid relid)
{
  volatile PgBackendStatus *beentry = MyBEEntry;

  if (!beentry || !pgstat_track_activities)
  {
    return;
  }

  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);
  beentry->st_progress_command = cmdtype;
  beentry->st_progress_command_target = relid;
  MemSet(&beentry->st_progress_param, 0, sizeof(beentry->st_progress_param));
  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                                    
   
                                                                       
              
   
void
pgstat_progress_update_param(int index, int64 val)
{
  volatile PgBackendStatus *beentry = MyBEEntry;

  Assert(index >= 0 && index < PGSTAT_NUM_PROGRESS_PARAM);

  if (!beentry || !pgstat_track_activities)
  {
    return;
  }

  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);
  beentry->st_progress_param[index] = val;
  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                                          
   
                                                                        
                                                          
              
   
void
pgstat_progress_update_multi_param(int nparam, const int *index, const int64 *val)
{
  volatile PgBackendStatus *beentry = MyBEEntry;
  int i;

  if (!beentry || !pgstat_track_activities || nparam == 0)
  {
    return;
  }

  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);

  for (i = 0; i < nparam; ++i)
  {
    Assert(index[i] >= 0 && index[i] < PGSTAT_NUM_PROGRESS_PARAM);

    beentry->st_progress_param[index[i]] = val[i];
  }

  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                                   
   
                                                                             
                                                
              
   
void
pgstat_progress_end_command(void)
{
  volatile PgBackendStatus *beentry = MyBEEntry;

  if (!beentry || !pgstat_track_activities)
  {
    return;
  }

  if (beentry->st_progress_command == PROGRESS_COMMAND_INVALID)
  {
    return;
  }

  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);
  beentry->st_progress_command = PROGRESS_COMMAND_INVALID;
  beentry->st_progress_command_target = InvalidOid;
  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                             
   
                                          
              
   
void
pgstat_report_appname(const char *appname)
{
  volatile PgBackendStatus *beentry = MyBEEntry;
  int len;

  if (!beentry)
  {
    return;
  }

                                                                  
  len = pg_mbcliplen(appname, strlen(appname), NAMEDATALEN - 1);

     
                                                               
                                                                         
                                                  
     
  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);

  memcpy((char *)beentry->st_appname, appname, len);
  beentry->st_appname[len] = '\0';

  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

   
                                                                      
                                              
   
void
pgstat_report_xact_timestamp(TimestampTz tstamp)
{
  volatile PgBackendStatus *beentry = MyBEEntry;

  if (!pgstat_track_activities || !beentry)
  {
    return;
  }

     
                                                               
                                                                         
                                                  
     
  PGSTAT_BEGIN_WRITE_ACTIVITY(beentry);

  beentry->st_xact_start_timestamp = tstamp;

  PGSTAT_END_WRITE_ACTIVITY(beentry);
}

              
                                  
   
                                                                           
                                            
              
   
static void
pgstat_read_current_status(void)
{
  volatile PgBackendStatus *beentry;
  LocalPgBackendStatus *localtable;
  LocalPgBackendStatus *localentry;
  char *localappname, *localclienthostname, *localactivity;
#ifdef USE_SSL
  PgBackendSSLStatus *localsslstatus;
#endif
#ifdef ENABLE_GSS
  PgBackendGSSStatus *localgssstatus;
#endif
  int i;

  Assert(!pgStatRunningInCollector);
  if (localBackendStatusTable)
  {
    return;                   
  }

  pgstat_setup_memcxt();

     
                                                                         
                                                                           
                                                                       
                                                                   
                                                                        
                                     
     
  localtable = (LocalPgBackendStatus *)MemoryContextAlloc(pgStatLocalContext, sizeof(LocalPgBackendStatus) * NumBackendStatSlots);
  localappname = (char *)MemoryContextAlloc(pgStatLocalContext, NAMEDATALEN * NumBackendStatSlots);
  localclienthostname = (char *)MemoryContextAlloc(pgStatLocalContext, NAMEDATALEN * NumBackendStatSlots);
  localactivity = (char *)MemoryContextAllocHuge(pgStatLocalContext, pgstat_track_activity_query_size * NumBackendStatSlots);
#ifdef USE_SSL
  localsslstatus = (PgBackendSSLStatus *)MemoryContextAlloc(pgStatLocalContext, sizeof(PgBackendSSLStatus) * NumBackendStatSlots);
#endif
#ifdef ENABLE_GSS
  localgssstatus = (PgBackendGSSStatus *)MemoryContextAlloc(pgStatLocalContext, sizeof(PgBackendGSSStatus) * NumBackendStatSlots);
#endif

  localNumBackends = 0;

  beentry = BackendStatusArray;
  localentry = localtable;
  for (i = 1; i <= NumBackendStatSlots; i++)
  {
       
                                                                          
                                                                        
                                                                           
                                                                         
                                                                    
       
    for (;;)
    {
      int before_changecount;
      int after_changecount;

      pgstat_begin_read_activity(beentry, before_changecount);

      localentry->backendStatus.st_procpid = beentry->st_procpid;
                                                                 
      if (localentry->backendStatus.st_procpid > 0)
      {
        memcpy(&localentry->backendStatus, unvolatize(PgBackendStatus *, beentry), sizeof(PgBackendStatus));

           
                                                                      
                                                                      
                                                         
           
                                                                       
                                                                 
           
        strcpy(localappname, (char *)beentry->st_appname);
        localentry->backendStatus.st_appname = localappname;
        strcpy(localclienthostname, (char *)beentry->st_clienthostname);
        localentry->backendStatus.st_clienthostname = localclienthostname;
        strcpy(localactivity, (char *)beentry->st_activity_raw);
        localentry->backendStatus.st_activity_raw = localactivity;
#ifdef USE_SSL
        if (beentry->st_ssl)
        {
          memcpy(localsslstatus, beentry->st_sslstatus, sizeof(PgBackendSSLStatus));
          localentry->backendStatus.st_sslstatus = localsslstatus;
        }
#endif
#ifdef ENABLE_GSS
        if (beentry->st_gss)
        {
          memcpy(localgssstatus, beentry->st_gssstatus, sizeof(PgBackendGSSStatus));
          localentry->backendStatus.st_gssstatus = localgssstatus;
        }
#endif
      }

      pgstat_end_read_activity(beentry, after_changecount);

      if (pgstat_read_activity_complete(before_changecount, after_changecount))
      {
        break;
      }

                                                          
      CHECK_FOR_INTERRUPTS();
    }

    beentry++;
                                                              
    if (localentry->backendStatus.st_procpid > 0)
    {
      BackendIdGetTransactionIds(i, &localentry->backend_xid, &localentry->backend_xmin);

      localentry++;
      localappname += NAMEDATALEN;
      localclienthostname += NAMEDATALEN;
      localactivity += pgstat_track_activity_query_size;
#ifdef USE_SSL
      localsslstatus++;
#endif
#ifdef ENABLE_GSS
      localgssstatus++;
#endif
      localNumBackends++;
    }
  }

                                                              
  localBackendStatusTable = localtable;
}

              
                                  
   
                                                                        
               
   
const char *
pgstat_get_wait_event_type(uint32 wait_event_info)
{
  uint32 classId;
  const char *event_type;

                                      
  if (wait_event_info == 0)
  {
    return NULL;
  }

  classId = wait_event_info & 0xFF000000;

  switch (classId)
  {
  case PG_WAIT_LWLOCK:
    event_type = "LWLock";
    break;
  case PG_WAIT_LOCK:
    event_type = "Lock";
    break;
  case PG_WAIT_BUFFER_PIN:
    event_type = "BufferPin";
    break;
  case PG_WAIT_ACTIVITY:
    event_type = "Activity";
    break;
  case PG_WAIT_CLIENT:
    event_type = "Client";
    break;
  case PG_WAIT_EXTENSION:
    event_type = "Extension";
    break;
  case PG_WAIT_IPC:
    event_type = "IPC";
    break;
  case PG_WAIT_TIMEOUT:
    event_type = "Timeout";
    break;
  case PG_WAIT_IO:
    event_type = "IO";
    break;
  default:
    event_type = "???";
    break;
  }

  return event_type;
}

              
                             
   
                                                                   
               
   
const char *
pgstat_get_wait_event(uint32 wait_event_info)
{
  uint32 classId;
  uint16 eventId;
  const char *event_name;

                                      
  if (wait_event_info == 0)
  {
    return NULL;
  }

  classId = wait_event_info & 0xFF000000;
  eventId = wait_event_info & 0x0000FFFF;

  switch (classId)
  {
  case PG_WAIT_LWLOCK:
    event_name = GetLWLockIdentifier(classId, eventId);
    break;
  case PG_WAIT_LOCK:
    event_name = GetLockNameFromTagType(eventId);
    break;
  case PG_WAIT_BUFFER_PIN:
    event_name = "BufferPin";
    break;
  case PG_WAIT_ACTIVITY:
  {
    WaitEventActivity w = (WaitEventActivity)wait_event_info;

    event_name = pgstat_get_wait_activity(w);
    break;
  }
  case PG_WAIT_CLIENT:
  {
    WaitEventClient w = (WaitEventClient)wait_event_info;

    event_name = pgstat_get_wait_client(w);
    break;
  }
  case PG_WAIT_EXTENSION:
    event_name = "Extension";
    break;
  case PG_WAIT_IPC:
  {
    WaitEventIPC w = (WaitEventIPC)wait_event_info;

    event_name = pgstat_get_wait_ipc(w);
    break;
  }
  case PG_WAIT_TIMEOUT:
  {
    WaitEventTimeout w = (WaitEventTimeout)wait_event_info;

    event_name = pgstat_get_wait_timeout(w);
    break;
  }
  case PG_WAIT_IO:
  {
    WaitEventIO w = (WaitEventIO)wait_event_info;

    event_name = pgstat_get_wait_io(w);
    break;
  }
  default:
    event_name = "unknown wait event";
    break;
  }

  return event_name;
}

              
                                
   
                                        
              
   
static const char *
pgstat_get_wait_activity(WaitEventActivity w)
{
  const char *event_name = "unknown wait event";

  switch (w)
  {
  case WAIT_EVENT_ARCHIVER_MAIN:
    event_name = "ArchiverMain";
    break;
  case WAIT_EVENT_AUTOVACUUM_MAIN:
    event_name = "AutoVacuumMain";
    break;
  case WAIT_EVENT_BGWRITER_HIBERNATE:
    event_name = "BgWriterHibernate";
    break;
  case WAIT_EVENT_BGWRITER_MAIN:
    event_name = "BgWriterMain";
    break;
  case WAIT_EVENT_CHECKPOINTER_MAIN:
    event_name = "CheckpointerMain";
    break;
  case WAIT_EVENT_LOGICAL_APPLY_MAIN:
    event_name = "LogicalApplyMain";
    break;
  case WAIT_EVENT_LOGICAL_LAUNCHER_MAIN:
    event_name = "LogicalLauncherMain";
    break;
  case WAIT_EVENT_PGSTAT_MAIN:
    event_name = "PgStatMain";
    break;
  case WAIT_EVENT_RECOVERY_WAL_ALL:
    event_name = "RecoveryWalAll";
    break;
  case WAIT_EVENT_RECOVERY_WAL_STREAM:
    event_name = "RecoveryWalStream";
    break;
  case WAIT_EVENT_SYSLOGGER_MAIN:
    event_name = "SysLoggerMain";
    break;
  case WAIT_EVENT_WAL_RECEIVER_MAIN:
    event_name = "WalReceiverMain";
    break;
  case WAIT_EVENT_WAL_SENDER_MAIN:
    event_name = "WalSenderMain";
    break;
  case WAIT_EVENT_WAL_WRITER_MAIN:
    event_name = "WalWriterMain";
    break;
                                                     
  }

  return event_name;
}

              
                              
   
                                      
              
   
static const char *
pgstat_get_wait_client(WaitEventClient w)
{
  const char *event_name = "unknown wait event";

  switch (w)
  {
  case WAIT_EVENT_CLIENT_READ:
    event_name = "ClientRead";
    break;
  case WAIT_EVENT_CLIENT_WRITE:
    event_name = "ClientWrite";
    break;
  case WAIT_EVENT_GSS_OPEN_SERVER:
    event_name = "GSSOpenServer";
    break;
  case WAIT_EVENT_LIBPQWALRECEIVER_CONNECT:
    event_name = "LibPQWalReceiverConnect";
    break;
  case WAIT_EVENT_LIBPQWALRECEIVER_RECEIVE:
    event_name = "LibPQWalReceiverReceive";
    break;
  case WAIT_EVENT_SSL_OPEN_SERVER:
    event_name = "SSLOpenServer";
    break;
  case WAIT_EVENT_WAL_RECEIVER_WAIT_START:
    event_name = "WalReceiverWaitStart";
    break;
  case WAIT_EVENT_WAL_SENDER_WAIT_WAL:
    event_name = "WalSenderWaitForWAL";
    break;
  case WAIT_EVENT_WAL_SENDER_WRITE_DATA:
    event_name = "WalSenderWriteData";
    break;
                                                     
  }

  return event_name;
}

              
                           
   
                                   
              
   
static const char *
pgstat_get_wait_ipc(WaitEventIPC w)
{
  const char *event_name = "unknown wait event";

  switch (w)
  {
  case WAIT_EVENT_BGWORKER_SHUTDOWN:
    event_name = "BgWorkerShutdown";
    break;
  case WAIT_EVENT_BGWORKER_STARTUP:
    event_name = "BgWorkerStartup";
    break;
  case WAIT_EVENT_BTREE_PAGE:
    event_name = "BtreePage";
    break;
  case WAIT_EVENT_CHECKPOINT_DONE:
    event_name = "CheckpointDone";
    break;
  case WAIT_EVENT_CHECKPOINT_START:
    event_name = "CheckpointStart";
    break;
  case WAIT_EVENT_CLOG_GROUP_UPDATE:
    event_name = "ClogGroupUpdate";
    break;
  case WAIT_EVENT_EXECUTE_GATHER:
    event_name = "ExecuteGather";
    break;
  case WAIT_EVENT_HASH_BATCH_ALLOCATING:
    event_name = "Hash/Batch/Allocating";
    break;
  case WAIT_EVENT_HASH_BATCH_ELECTING:
    event_name = "Hash/Batch/Electing";
    break;
  case WAIT_EVENT_HASH_BATCH_LOADING:
    event_name = "Hash/Batch/Loading";
    break;
  case WAIT_EVENT_HASH_BUILD_ALLOCATING:
    event_name = "Hash/Build/Allocating";
    break;
  case WAIT_EVENT_HASH_BUILD_ELECTING:
    event_name = "Hash/Build/Electing";
    break;
  case WAIT_EVENT_HASH_BUILD_HASHING_INNER:
    event_name = "Hash/Build/HashingInner";
    break;
  case WAIT_EVENT_HASH_BUILD_HASHING_OUTER:
    event_name = "Hash/Build/HashingOuter";
    break;
  case WAIT_EVENT_HASH_GROW_BATCHES_ALLOCATING:
    event_name = "Hash/GrowBatches/Allocating";
    break;
  case WAIT_EVENT_HASH_GROW_BATCHES_DECIDING:
    event_name = "Hash/GrowBatches/Deciding";
    break;
  case WAIT_EVENT_HASH_GROW_BATCHES_ELECTING:
    event_name = "Hash/GrowBatches/Electing";
    break;
  case WAIT_EVENT_HASH_GROW_BATCHES_FINISHING:
    event_name = "Hash/GrowBatches/Finishing";
    break;
  case WAIT_EVENT_HASH_GROW_BATCHES_REPARTITIONING:
    event_name = "Hash/GrowBatches/Repartitioning";
    break;
  case WAIT_EVENT_HASH_GROW_BUCKETS_ALLOCATING:
    event_name = "Hash/GrowBuckets/Allocating";
    break;
  case WAIT_EVENT_HASH_GROW_BUCKETS_ELECTING:
    event_name = "Hash/GrowBuckets/Electing";
    break;
  case WAIT_EVENT_HASH_GROW_BUCKETS_REINSERTING:
    event_name = "Hash/GrowBuckets/Reinserting";
    break;
  case WAIT_EVENT_LOGICAL_SYNC_DATA:
    event_name = "LogicalSyncData";
    break;
  case WAIT_EVENT_LOGICAL_SYNC_STATE_CHANGE:
    event_name = "LogicalSyncStateChange";
    break;
  case WAIT_EVENT_MQ_INTERNAL:
    event_name = "MessageQueueInternal";
    break;
  case WAIT_EVENT_MQ_PUT_MESSAGE:
    event_name = "MessageQueuePutMessage";
    break;
  case WAIT_EVENT_MQ_RECEIVE:
    event_name = "MessageQueueReceive";
    break;
  case WAIT_EVENT_MQ_SEND:
    event_name = "MessageQueueSend";
    break;
  case WAIT_EVENT_PARALLEL_BITMAP_SCAN:
    event_name = "ParallelBitmapScan";
    break;
  case WAIT_EVENT_PARALLEL_CREATE_INDEX_SCAN:
    event_name = "ParallelCreateIndexScan";
    break;
  case WAIT_EVENT_PARALLEL_FINISH:
    event_name = "ParallelFinish";
    break;
  case WAIT_EVENT_PROCARRAY_GROUP_UPDATE:
    event_name = "ProcArrayGroupUpdate";
    break;
  case WAIT_EVENT_PROMOTE:
    event_name = "Promote";
    break;
  case WAIT_EVENT_REPLICATION_ORIGIN_DROP:
    event_name = "ReplicationOriginDrop";
    break;
  case WAIT_EVENT_REPLICATION_SLOT_DROP:
    event_name = "ReplicationSlotDrop";
    break;
  case WAIT_EVENT_SAFE_SNAPSHOT:
    event_name = "SafeSnapshot";
    break;
  case WAIT_EVENT_SYNC_REP:
    event_name = "SyncRep";
    break;
                                                     
  }

  return event_name;
}

              
                               
   
                                       
              
   
static const char *
pgstat_get_wait_timeout(WaitEventTimeout w)
{
  const char *event_name = "unknown wait event";

  switch (w)
  {
  case WAIT_EVENT_BASE_BACKUP_THROTTLE:
    event_name = "BaseBackupThrottle";
    break;
  case WAIT_EVENT_PG_SLEEP:
    event_name = "PgSleep";
    break;
  case WAIT_EVENT_RECOVERY_APPLY_DELAY:
    event_name = "RecoveryApplyDelay";
    break;
  case WAIT_EVENT_REGISTER_SYNC_REQUEST:
    event_name = "RegisterSyncRequest";
    break;
                                                     
  }

  return event_name;
}

              
                          
   
                                  
              
   
static const char *
pgstat_get_wait_io(WaitEventIO w)
{
  const char *event_name = "unknown wait event";

  switch (w)
  {
  case WAIT_EVENT_BUFFILE_READ:
    event_name = "BufFileRead";
    break;
  case WAIT_EVENT_BUFFILE_WRITE:
    event_name = "BufFileWrite";
    break;
  case WAIT_EVENT_CONTROL_FILE_READ:
    event_name = "ControlFileRead";
    break;
  case WAIT_EVENT_CONTROL_FILE_SYNC:
    event_name = "ControlFileSync";
    break;
  case WAIT_EVENT_CONTROL_FILE_SYNC_UPDATE:
    event_name = "ControlFileSyncUpdate";
    break;
  case WAIT_EVENT_CONTROL_FILE_WRITE:
    event_name = "ControlFileWrite";
    break;
  case WAIT_EVENT_CONTROL_FILE_WRITE_UPDATE:
    event_name = "ControlFileWriteUpdate";
    break;
  case WAIT_EVENT_COPY_FILE_READ:
    event_name = "CopyFileRead";
    break;
  case WAIT_EVENT_COPY_FILE_WRITE:
    event_name = "CopyFileWrite";
    break;
  case WAIT_EVENT_DATA_FILE_EXTEND:
    event_name = "DataFileExtend";
    break;
  case WAIT_EVENT_DATA_FILE_FLUSH:
    event_name = "DataFileFlush";
    break;
  case WAIT_EVENT_DATA_FILE_IMMEDIATE_SYNC:
    event_name = "DataFileImmediateSync";
    break;
  case WAIT_EVENT_DATA_FILE_PREFETCH:
    event_name = "DataFilePrefetch";
    break;
  case WAIT_EVENT_DATA_FILE_READ:
    event_name = "DataFileRead";
    break;
  case WAIT_EVENT_DATA_FILE_SYNC:
    event_name = "DataFileSync";
    break;
  case WAIT_EVENT_DATA_FILE_TRUNCATE:
    event_name = "DataFileTruncate";
    break;
  case WAIT_EVENT_DATA_FILE_WRITE:
    event_name = "DataFileWrite";
    break;
  case WAIT_EVENT_DSM_FILL_ZERO_WRITE:
    event_name = "DSMFillZeroWrite";
    break;
  case WAIT_EVENT_LOCK_FILE_ADDTODATADIR_READ:
    event_name = "LockFileAddToDataDirRead";
    break;
  case WAIT_EVENT_LOCK_FILE_ADDTODATADIR_SYNC:
    event_name = "LockFileAddToDataDirSync";
    break;
  case WAIT_EVENT_LOCK_FILE_ADDTODATADIR_WRITE:
    event_name = "LockFileAddToDataDirWrite";
    break;
  case WAIT_EVENT_LOCK_FILE_CREATE_READ:
    event_name = "LockFileCreateRead";
    break;
  case WAIT_EVENT_LOCK_FILE_CREATE_SYNC:
    event_name = "LockFileCreateSync";
    break;
  case WAIT_EVENT_LOCK_FILE_CREATE_WRITE:
    event_name = "LockFileCreateWrite";
    break;
  case WAIT_EVENT_LOCK_FILE_RECHECKDATADIR_READ:
    event_name = "LockFileReCheckDataDirRead";
    break;
  case WAIT_EVENT_LOGICAL_REWRITE_CHECKPOINT_SYNC:
    event_name = "LogicalRewriteCheckpointSync";
    break;
  case WAIT_EVENT_LOGICAL_REWRITE_MAPPING_SYNC:
    event_name = "LogicalRewriteMappingSync";
    break;
  case WAIT_EVENT_LOGICAL_REWRITE_MAPPING_WRITE:
    event_name = "LogicalRewriteMappingWrite";
    break;
  case WAIT_EVENT_LOGICAL_REWRITE_SYNC:
    event_name = "LogicalRewriteSync";
    break;
  case WAIT_EVENT_LOGICAL_REWRITE_TRUNCATE:
    event_name = "LogicalRewriteTruncate";
    break;
  case WAIT_EVENT_LOGICAL_REWRITE_WRITE:
    event_name = "LogicalRewriteWrite";
    break;
  case WAIT_EVENT_RELATION_MAP_READ:
    event_name = "RelationMapRead";
    break;
  case WAIT_EVENT_RELATION_MAP_SYNC:
    event_name = "RelationMapSync";
    break;
  case WAIT_EVENT_RELATION_MAP_WRITE:
    event_name = "RelationMapWrite";
    break;
  case WAIT_EVENT_REORDER_BUFFER_READ:
    event_name = "ReorderBufferRead";
    break;
  case WAIT_EVENT_REORDER_BUFFER_WRITE:
    event_name = "ReorderBufferWrite";
    break;
  case WAIT_EVENT_REORDER_LOGICAL_MAPPING_READ:
    event_name = "ReorderLogicalMappingRead";
    break;
  case WAIT_EVENT_REPLICATION_SLOT_READ:
    event_name = "ReplicationSlotRead";
    break;
  case WAIT_EVENT_REPLICATION_SLOT_RESTORE_SYNC:
    event_name = "ReplicationSlotRestoreSync";
    break;
  case WAIT_EVENT_REPLICATION_SLOT_SYNC:
    event_name = "ReplicationSlotSync";
    break;
  case WAIT_EVENT_REPLICATION_SLOT_WRITE:
    event_name = "ReplicationSlotWrite";
    break;
  case WAIT_EVENT_SLRU_FLUSH_SYNC:
    event_name = "SLRUFlushSync";
    break;
  case WAIT_EVENT_SLRU_READ:
    event_name = "SLRURead";
    break;
  case WAIT_EVENT_SLRU_SYNC:
    event_name = "SLRUSync";
    break;
  case WAIT_EVENT_SLRU_WRITE:
    event_name = "SLRUWrite";
    break;
  case WAIT_EVENT_SNAPBUILD_READ:
    event_name = "SnapbuildRead";
    break;
  case WAIT_EVENT_SNAPBUILD_SYNC:
    event_name = "SnapbuildSync";
    break;
  case WAIT_EVENT_SNAPBUILD_WRITE:
    event_name = "SnapbuildWrite";
    break;
  case WAIT_EVENT_TIMELINE_HISTORY_FILE_SYNC:
    event_name = "TimelineHistoryFileSync";
    break;
  case WAIT_EVENT_TIMELINE_HISTORY_FILE_WRITE:
    event_name = "TimelineHistoryFileWrite";
    break;
  case WAIT_EVENT_TIMELINE_HISTORY_READ:
    event_name = "TimelineHistoryRead";
    break;
  case WAIT_EVENT_TIMELINE_HISTORY_SYNC:
    event_name = "TimelineHistorySync";
    break;
  case WAIT_EVENT_TIMELINE_HISTORY_WRITE:
    event_name = "TimelineHistoryWrite";
    break;
  case WAIT_EVENT_TWOPHASE_FILE_READ:
    event_name = "TwophaseFileRead";
    break;
  case WAIT_EVENT_TWOPHASE_FILE_SYNC:
    event_name = "TwophaseFileSync";
    break;
  case WAIT_EVENT_TWOPHASE_FILE_WRITE:
    event_name = "TwophaseFileWrite";
    break;
  case WAIT_EVENT_WALSENDER_TIMELINE_HISTORY_READ:
    event_name = "WALSenderTimelineHistoryRead";
    break;
  case WAIT_EVENT_WAL_BOOTSTRAP_SYNC:
    event_name = "WALBootstrapSync";
    break;
  case WAIT_EVENT_WAL_BOOTSTRAP_WRITE:
    event_name = "WALBootstrapWrite";
    break;
  case WAIT_EVENT_WAL_COPY_READ:
    event_name = "WALCopyRead";
    break;
  case WAIT_EVENT_WAL_COPY_SYNC:
    event_name = "WALCopySync";
    break;
  case WAIT_EVENT_WAL_COPY_WRITE:
    event_name = "WALCopyWrite";
    break;
  case WAIT_EVENT_WAL_INIT_SYNC:
    event_name = "WALInitSync";
    break;
  case WAIT_EVENT_WAL_INIT_WRITE:
    event_name = "WALInitWrite";
    break;
  case WAIT_EVENT_WAL_READ:
    event_name = "WALRead";
    break;
  case WAIT_EVENT_WAL_SYNC:
    event_name = "WALSync";
    break;
  case WAIT_EVENT_WAL_SYNC_METHOD_ASSIGN:
    event_name = "WALSyncMethodAssign";
    break;
  case WAIT_EVENT_WAL_WRITE:
    event_name = "WALWrite";
    break;

                                                     
  }

  return event_name;
}

              
                                           
   
                                                                         
                                                                      
                                                                        
                                               
   
                                                                            
                                                                       
                                                                       
                                                                     
                                                                         
                                                                     
                                                     
   
                                                                              
              
   
const char *
pgstat_get_backend_current_activity(int pid, bool checkUser)
{
  PgBackendStatus *beentry;
  int i;

  beentry = BackendStatusArray;
  for (i = 1; i <= MaxBackends; i++)
  {
       
                                                                        
                                                                      
                                                                       
                                                                      
                                                                   
                                                                         
                                                                           
                                                    
       
    volatile PgBackendStatus *vbeentry = beentry;
    bool found;

    for (;;)
    {
      int before_changecount;
      int after_changecount;

      pgstat_begin_read_activity(vbeentry, before_changecount);

      found = (vbeentry->st_procpid == pid);

      pgstat_end_read_activity(vbeentry, after_changecount);

      if (pgstat_read_activity_complete(before_changecount, after_changecount))
      {
        break;
      }

                                                          
      CHECK_FOR_INTERRUPTS();
    }

    if (found)
    {
                                                          
      if (checkUser && !superuser() && beentry->st_userid != GetUserId())
      {
        return "<insufficient privilege>";
      }
      else if (*(beentry->st_activity_raw) == '\0')
      {
        return "<command string not enabled>";
      }
      else
      {
                                                                     
        return pgstat_clip_activity(beentry->st_activity_raw);
      }
    }

    beentry++;
  }

                                              
  return "<backend information not available>";
}

              
                                           
   
                                                                         
                                                                             
                                                                        
                                                                    
                
   
                                                                             
                                                                       
                                                              
                                                                        
                                                                             
              
   
const char *
pgstat_get_crashed_backend_activity(int pid, char *buffer, int buflen)
{
  volatile PgBackendStatus *beentry;
  int i;

  beentry = BackendStatusArray;

     
                                                                          
                  
     
  if (beentry == NULL || BackendActivityBuffer == NULL)
  {
    return NULL;
  }

  for (i = 1; i <= MaxBackends; i++)
  {
    if (beentry->st_procpid == pid)
    {
                                                                       
      const char *activity = beentry->st_activity_raw;
      const char *activity_last;

         
                                                                    
                                                                       
                                                                    
                                                                    
         
      activity_last = BackendActivityBuffer + BackendActivityBufferSize - pgstat_track_activity_query_size;

      if (activity < BackendActivityBuffer || activity > activity_last)
      {
        return NULL;
      }

                                                        
      if (activity[0] == '\0')
      {
        return NULL;
      }

         
                                                                       
                                                                         
                                                                       
                                                                     
         
      ascii_safe_strlcpy(buffer, activity, Min(buflen, pgstat_track_activity_query_size));

      return buffer;
    }

    beentry++;
  }

                     
  return NULL;
}

const char *
pgstat_get_backend_desc(BackendType backendType)
{
  const char *backendDesc = "unknown process type";

  switch (backendType)
  {
  case B_AUTOVAC_LAUNCHER:
    backendDesc = "autovacuum launcher";
    break;
  case B_AUTOVAC_WORKER:
    backendDesc = "autovacuum worker";
    break;
  case B_BACKEND:
    backendDesc = "client backend";
    break;
  case B_BG_WORKER:
    backendDesc = "background worker";
    break;
  case B_BG_WRITER:
    backendDesc = "background writer";
    break;
  case B_CHECKPOINTER:
    backendDesc = "checkpointer";
    break;
  case B_STARTUP:
    backendDesc = "startup";
    break;
  case B_WAL_RECEIVER:
    backendDesc = "walreceiver";
    break;
  case B_WAL_SENDER:
    backendDesc = "walsender";
    break;
  case B_WAL_WRITER:
    backendDesc = "walwriter";
    break;
  }

  return backendDesc;
}

                                                                
                                  
                                                                
   

              
                        
   
                                                     
              
   
static void
pgstat_setheader(PgStat_MsgHdr *hdr, StatMsgType mtype)
{
  hdr->m_type = mtype;
}

              
                   
   
                                                     
              
   
static void
pgstat_send(void *msg, int len)
{
  int rc;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  ((PgStat_MsgHdr *)msg)->m_size = len;

                                                              
  do
  {
    rc = send(pgStatSock, msg, len, 0);
  } while (rc < 0 && errno == EINTR);

#ifdef USE_ASSERT_CHECKING
                                              
  if (rc < 0)
  {
    elog(LOG, "could not send to statistics collector: %m");
  }
#endif
}

              
                            
   
                                                              
                                  
              
   
void
pgstat_send_archiver(const char *xlog, bool failed)
{
  PgStat_MsgArchiver msg;

     
                                  
     
  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_ARCHIVER);
  msg.m_failed = failed;
  StrNCpy(msg.m_xlog, xlog, sizeof(msg.m_xlog));
  msg.m_timestamp = GetCurrentTimestamp();
  pgstat_send(&msg, sizeof(msg));
}

              
                            
   
                                              
              
   
void
pgstat_send_bgwriter(void)
{
                                            
  static const PgStat_MsgBgWriter all_zeroes;

     
                                                                         
                                                                      
                
     
  if (memcmp(&BgWriterStats, &all_zeroes, sizeof(PgStat_MsgBgWriter)) == 0)
  {
    return;
  }

     
                                  
     
  pgstat_setheader(&BgWriterStats.m_hdr, PGSTAT_MTYPE_BGWRITER);
  pgstat_send(&BgWriterStats, sizeof(BgWriterStats));

     
                                                            
     
  MemSet(&BgWriterStats, 0, sizeof(BgWriterStats));
}

              
                           
   
                                                                       
                             
   
                                                                 
              
   
NON_EXEC_STATIC void
PgstatCollectorMain(int argc, char *argv[])
{
  int len;
  PgStat_Msg msg;
  int wr;

     
                                                                        
                                                                         
                                                                  
     
  pqsignal(SIGHUP, pgstat_sighup_handler);
  pqsignal(SIGINT, SIG_IGN);
  pqsignal(SIGTERM, SIG_IGN);
  pqsignal(SIGQUIT, pgstat_exit);
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, SIG_IGN);
  pqsignal(SIGUSR2, SIG_IGN);
                                                                       
  pqsignal(SIGCHLD, SIG_DFL);
  PG_SETMASK(&UnBlockSig);

     
                            
     
  init_ps_display("stats collector", "", "", "");

     
                                                                   
     
  pgStatRunningInCollector = true;
  pgStatDBHash = pgstat_read_statsfiles(InvalidOid, true, true);

     
                                                                        
                                     
     
                                                                             
                                                                           
                                                                             
                                                                         
                                                                    
                                                                             
                                                                            
                                                                         
             
     
  for (;;)
  {
                                           
    ResetLatch(MyLatch);

       
                                                   
       
    if (need_exit)
    {
      break;
    }

       
                                                                         
                              
       
    while (!need_exit)
    {
         
                                                                    
         
      if (got_SIGHUP)
      {
        got_SIGHUP = false;
        ProcessConfigFile(PGC_SIGHUP);
      }

         
                                                                      
                                            
         
      if (pgstat_write_statsfile_needed())
      {
        pgstat_write_statsfiles(false, false);
      }

         
                                                                     
                                                       
         
                                                                     
                                                                     
                                                               
         
#ifdef WIN32
      pgwin32_noblock = 1;
#endif

      len = recv(pgStatSock, (char *)&msg, sizeof(PgStat_Msg), 0);

#ifdef WIN32
      pgwin32_noblock = 0;
#endif

      if (len < 0)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
          break;                        
        }
        ereport(ERROR, (errcode_for_socket_access(), errmsg("could not read statistics message: %m")));
      }

         
                                                                    
         
      if (len < sizeof(PgStat_MsgHdr))
      {
        continue;
      }

         
                                                                 
         
      if (msg.msg_hdr.m_size != len)
      {
        continue;
      }

         
                                                     
         
      switch (msg.msg_hdr.m_type)
      {
      case PGSTAT_MTYPE_DUMMY:
        break;

      case PGSTAT_MTYPE_INQUIRY:
        pgstat_recv_inquiry(&msg.msg_inquiry, len);
        break;

      case PGSTAT_MTYPE_TABSTAT:
        pgstat_recv_tabstat(&msg.msg_tabstat, len);
        break;

      case PGSTAT_MTYPE_TABPURGE:
        pgstat_recv_tabpurge(&msg.msg_tabpurge, len);
        break;

      case PGSTAT_MTYPE_DROPDB:
        pgstat_recv_dropdb(&msg.msg_dropdb, len);
        break;

      case PGSTAT_MTYPE_RESETCOUNTER:
        pgstat_recv_resetcounter(&msg.msg_resetcounter, len);
        break;

      case PGSTAT_MTYPE_RESETSHAREDCOUNTER:
        pgstat_recv_resetsharedcounter(&msg.msg_resetsharedcounter, len);
        break;

      case PGSTAT_MTYPE_RESETSINGLECOUNTER:
        pgstat_recv_resetsinglecounter(&msg.msg_resetsinglecounter, len);
        break;

      case PGSTAT_MTYPE_AUTOVAC_START:
        pgstat_recv_autovac(&msg.msg_autovacuum_start, len);
        break;

      case PGSTAT_MTYPE_VACUUM:
        pgstat_recv_vacuum(&msg.msg_vacuum, len);
        break;

      case PGSTAT_MTYPE_ANALYZE:
        pgstat_recv_analyze(&msg.msg_analyze, len);
        break;

      case PGSTAT_MTYPE_ARCHIVER:
        pgstat_recv_archiver(&msg.msg_archiver, len);
        break;

      case PGSTAT_MTYPE_BGWRITER:
        pgstat_recv_bgwriter(&msg.msg_bgwriter, len);
        break;

      case PGSTAT_MTYPE_FUNCSTAT:
        pgstat_recv_funcstat(&msg.msg_funcstat, len);
        break;

      case PGSTAT_MTYPE_FUNCPURGE:
        pgstat_recv_funcpurge(&msg.msg_funcpurge, len);
        break;

      case PGSTAT_MTYPE_RECOVERYCONFLICT:
        pgstat_recv_recoveryconflict(&msg.msg_recoveryconflict, len);
        break;

      case PGSTAT_MTYPE_DEADLOCK:
        pgstat_recv_deadlock(&msg.msg_deadlock, len);
        break;

      case PGSTAT_MTYPE_TEMPFILE:
        pgstat_recv_tempfile(&msg.msg_tempfile, len);
        break;

      case PGSTAT_MTYPE_CHECKSUMFAILURE:
        pgstat_recv_checksum_failure(&msg.msg_checksumfailure, len);
        break;

      default:
        break;
      }
    }                                           

                                             
#ifndef WIN32
    wr = WaitLatchOrSocket(MyLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH | WL_SOCKET_READABLE, pgStatSock, -1L, WAIT_EVENT_PGSTAT_MAIN);
#else

       
                                                                    
                                                                          
                                                                        
                                                                     
                                                                    
                                                                          
                                                               
                               
       
    wr = WaitLatchOrSocket(MyLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH | WL_SOCKET_READABLE | WL_TIMEOUT, pgStatSock, 2 * 1000L           , WAIT_EVENT_PGSTAT_MAIN);
#endif

       
                                                                       
                                                                
       
    if (wr & WL_POSTMASTER_DEATH)
    {
      break;
    }
  }                        

     
                                                    
     
  pgstat_write_statsfiles(true, true);

  exit(0);
}

                                                  
static void
pgstat_exit(SIGNAL_ARGS)
{
  int save_errno = errno;

  need_exit = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                          
static void
pgstat_sighup_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                                                 
   
                                                         
   
static void
reset_dbentry_counters(PgStat_StatDBEntry *dbentry)
{
  HASHCTL hash_ctl;

  dbentry->n_xact_commit = 0;
  dbentry->n_xact_rollback = 0;
  dbentry->n_blocks_fetched = 0;
  dbentry->n_blocks_hit = 0;
  dbentry->n_tuples_returned = 0;
  dbentry->n_tuples_fetched = 0;
  dbentry->n_tuples_inserted = 0;
  dbentry->n_tuples_updated = 0;
  dbentry->n_tuples_deleted = 0;
  dbentry->last_autovac_time = 0;
  dbentry->n_conflict_tablespace = 0;
  dbentry->n_conflict_lock = 0;
  dbentry->n_conflict_snapshot = 0;
  dbentry->n_conflict_bufferpin = 0;
  dbentry->n_conflict_startup_deadlock = 0;
  dbentry->n_temp_files = 0;
  dbentry->n_temp_bytes = 0;
  dbentry->n_deadlocks = 0;
  dbentry->n_checksum_failures = 0;
  dbentry->last_checksum_failure = 0;
  dbentry->n_block_read_time = 0;
  dbentry->n_block_write_time = 0;

  dbentry->stat_reset_timestamp = GetCurrentTimestamp();
  dbentry->stats_timestamp = 0;

  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(PgStat_StatTabEntry);
  dbentry->tables = hash_create("Per-database table", PGSTAT_TAB_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS);

  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(PgStat_StatFuncEntry);
  dbentry->functions = hash_create("Per-database function", PGSTAT_FUNCTION_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS);
}

   
                                                                      
                                                                       
                      
   
static PgStat_StatDBEntry *
pgstat_get_db_entry(Oid databaseid, bool create)
{
  PgStat_StatDBEntry *result;
  bool found;
  HASHACTION action = (create ? HASH_ENTER : HASH_FIND);

                                                               
  result = (PgStat_StatDBEntry *)hash_search(pgStatDBHash, &databaseid, action, &found);

  if (!create && !found)
  {
    return NULL;
  }

     
                                                                           
                                    
     
  if (!found)
  {
    reset_dbentry_counters(result);
  }

  return result;
}

   
                                                                   
                                                                       
                      
   
static PgStat_StatTabEntry *
pgstat_get_tab_entry(PgStat_StatDBEntry *dbentry, Oid tableoid, bool create)
{
  PgStat_StatTabEntry *result;
  bool found;
  HASHACTION action = (create ? HASH_ENTER : HASH_FIND);

                                                            
  result = (PgStat_StatTabEntry *)hash_search(dbentry->tables, &tableoid, action, &found);

  if (!create && !found)
  {
    return NULL;
  }

                                             
  if (!found)
  {
    result->numscans = 0;
    result->tuples_returned = 0;
    result->tuples_fetched = 0;
    result->tuples_inserted = 0;
    result->tuples_updated = 0;
    result->tuples_deleted = 0;
    result->tuples_hot_updated = 0;
    result->n_live_tuples = 0;
    result->n_dead_tuples = 0;
    result->changes_since_analyze = 0;
    result->blocks_fetched = 0;
    result->blocks_hit = 0;
    result->vacuum_timestamp = 0;
    result->vacuum_count = 0;
    result->autovac_vacuum_timestamp = 0;
    result->autovac_vacuum_count = 0;
    result->analyze_timestamp = 0;
    result->analyze_count = 0;
    result->autovac_analyze_timestamp = 0;
    result->autovac_analyze_count = 0;
  }

  return result;
}

              
                               
                                                                     
   
                                                                            
                                                                             
                                                                           
                                                          
   
                                                                   
                                                                     
                    
              
   
static void
pgstat_write_statsfiles(bool permanent, bool allDbs)
{
  HASH_SEQ_STATUS hstat;
  PgStat_StatDBEntry *dbentry;
  FILE *fpout;
  int32 format_id;
  const char *tmpfile = permanent ? PGSTAT_STAT_PERMANENT_TMPFILE : pgstat_stat_tmpname;
  const char *statfile = permanent ? PGSTAT_STAT_PERMANENT_FILENAME : pgstat_stat_filename;
  int rc;

  elog(DEBUG2, "writing stats file \"%s\"", statfile);

     
                                                                    
     
  fpout = AllocateFile(tmpfile, PG_BINARY_W);
  if (fpout == NULL)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not open temporary statistics file \"%s\": %m", tmpfile)));
    return;
  }

     
                                          
     
  globalStats.stats_timestamp = GetCurrentTimestamp();

     
                                                           
     
  format_id = PGSTAT_FILE_FORMAT_ID;
  rc = fwrite(&format_id, sizeof(format_id), 1, fpout);
  (void)rc;                                        

     
                               
     
  rc = fwrite(&globalStats, sizeof(globalStats), 1, fpout);
  (void)rc;                                        

     
                                 
     
  rc = fwrite(&archiverStats, sizeof(archiverStats), 1, fpout);
  (void)rc;                                        

     
                                      
     
  hash_seq_init(&hstat, pgStatDBHash);
  while ((dbentry = (PgStat_StatDBEntry *)hash_seq_search(&hstat)) != NULL)
  {
       
                                                                   
                                                  
       
    if (allDbs || pgstat_db_requested(dbentry->databaseid))
    {
                                                                
      dbentry->stats_timestamp = globalStats.stats_timestamp;

      pgstat_write_db_statsfile(dbentry, permanent);
    }

       
                                                                      
                                                               
       
    fputc('D', fpout);
    rc = fwrite(dbentry, offsetof(PgStat_StatDBEntry, tables), 1, fpout);
    (void)rc;                                        
  }

     
                                                                        
                                                                         
                                                  
     
  fputc('E', fpout);

  if (ferror(fpout))
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write temporary statistics file \"%s\": %m", tmpfile)));
    FreeFile(fpout);
    unlink(tmpfile);
  }
  else if (FreeFile(fpout) < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not close temporary statistics file \"%s\": %m", tmpfile)));
    unlink(tmpfile);
  }
  else if (rename(tmpfile, statfile) < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not rename temporary statistics file \"%s\" to \"%s\": %m", tmpfile, statfile)));
    unlink(tmpfile);
  }

  if (permanent)
  {
    unlink(pgstat_stat_filename);
  }

     
                                                                            
                                                                
     
  list_free(pending_write_requests);
  pending_write_requests = NIL;
}

   
                                                                          
                  
   
static void
get_dbstat_filename(bool permanent, bool tempname, Oid databaseid, char *filename, int len)
{
  int printed;

                                                                         
  printed = snprintf(filename, len, "%s/db_%u.%s", permanent ? PGSTAT_STAT_PERMANENT_DIRECTORY : pgstat_stat_directory, databaseid, tempname ? "tmp" : "stat");
  if (printed >= len)
  {
    elog(ERROR, "overlength pgstat path");
  }
}

              
                                 
                                               
   
                                                                   
                                                                   
                                                                     
                               
              
   
static void
pgstat_write_db_statsfile(PgStat_StatDBEntry *dbentry, bool permanent)
{
  HASH_SEQ_STATUS tstat;
  HASH_SEQ_STATUS fstat;
  PgStat_StatTabEntry *tabentry;
  PgStat_StatFuncEntry *funcentry;
  FILE *fpout;
  int32 format_id;
  Oid dbid = dbentry->databaseid;
  int rc;
  char tmpfile[MAXPGPATH];
  char statfile[MAXPGPATH];

  get_dbstat_filename(permanent, true, dbid, tmpfile, MAXPGPATH);
  get_dbstat_filename(permanent, false, dbid, statfile, MAXPGPATH);

  elog(DEBUG2, "writing stats file \"%s\"", statfile);

     
                                                                    
     
  fpout = AllocateFile(tmpfile, PG_BINARY_W);
  if (fpout == NULL)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not open temporary statistics file \"%s\": %m", tmpfile)));
    return;
  }

     
                                                           
     
  format_id = PGSTAT_FILE_FORMAT_ID;
  rc = fwrite(&format_id, sizeof(format_id), 1, fpout);
  (void)rc;                                        

     
                                                         
     
  hash_seq_init(&tstat, dbentry->tables);
  while ((tabentry = (PgStat_StatTabEntry *)hash_seq_search(&tstat)) != NULL)
  {
    fputc('T', fpout);
    rc = fwrite(tabentry, sizeof(PgStat_StatTabEntry), 1, fpout);
    (void)rc;                                        
  }

     
                                                       
     
  hash_seq_init(&fstat, dbentry->functions);
  while ((funcentry = (PgStat_StatFuncEntry *)hash_seq_search(&fstat)) != NULL)
  {
    fputc('F', fpout);
    rc = fwrite(funcentry, sizeof(PgStat_StatFuncEntry), 1, fpout);
    (void)rc;                                        
  }

     
                                                                        
                                                                         
                                                  
     
  fputc('E', fpout);

  if (ferror(fpout))
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write temporary statistics file \"%s\": %m", tmpfile)));
    FreeFile(fpout);
    unlink(tmpfile);
  }
  else if (FreeFile(fpout) < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not close temporary statistics file \"%s\": %m", tmpfile)));
    unlink(tmpfile);
  }
  else if (rename(tmpfile, statfile) < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not rename temporary statistics file \"%s\" to \"%s\": %m", tmpfile, statfile)));
    unlink(tmpfile);
  }

  if (permanent)
  {
    get_dbstat_filename(false, false, dbid, statfile, MAXPGPATH);

    elog(DEBUG2, "removing temporary stats file \"%s\"", statfile);
    unlink(statfile);
  }
}

              
                              
   
                                                                     
                                                           
   
                                                                         
                                                                        
                                                                             
                                    
   
                                                                              
                                                                          
                                                                           
                                                                
   
                                                                           
                                                
              
   
static HTAB *
pgstat_read_statsfiles(Oid onlydb, bool permanent, bool deep)
{
  PgStat_StatDBEntry *dbentry;
  PgStat_StatDBEntry dbbuf;
  HASHCTL hash_ctl;
  HTAB *dbhash;
  FILE *fpin;
  int32 format_id;
  bool found;
  const char *statfile = permanent ? PGSTAT_STAT_PERMANENT_FILENAME : pgstat_stat_filename;

     
                                                 
     
  pgstat_setup_memcxt();

     
                             
     
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(PgStat_StatDBEntry);
  hash_ctl.hcxt = pgStatLocalContext;
  dbhash = hash_create("Databases hash", PGSTAT_DB_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

     
                                                                         
                                               
     
  memset(&globalStats, 0, sizeof(globalStats));
  memset(&archiverStats, 0, sizeof(archiverStats));

     
                                                                           
                          
     
  globalStats.stat_reset_timestamp = GetCurrentTimestamp();
  archiverStats.stat_reset_timestamp = globalStats.stat_reset_timestamp;

     
                                                                          
                                                                           
                          
     
                                                                          
                                                                       
                              
     
  if ((fpin = AllocateFile(statfile, PG_BINARY_R)) == NULL)
  {
    if (errno != ENOENT)
    {
      ereport(pgStatRunningInCollector ? LOG : WARNING, (errcode_for_file_access(), errmsg("could not open statistics file \"%s\": %m", statfile)));
    }
    return dbhash;
  }

     
                                         
     
  if (fread(&format_id, 1, sizeof(format_id), fpin) != sizeof(format_id) || format_id != PGSTAT_FILE_FORMAT_ID)
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    goto done;
  }

     
                              
     
  if (fread(&globalStats, 1, sizeof(globalStats), fpin) != sizeof(globalStats))
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    memset(&globalStats, 0, sizeof(globalStats));
    goto done;
  }

     
                                                                          
                                                                             
                                                                            
                                                                            
                          
     
  if (pgStatRunningInCollector)
  {
    globalStats.stats_timestamp = 0;
  }

     
                                
     
  if (fread(&archiverStats, 1, sizeof(archiverStats), fpin) != sizeof(archiverStats))
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    memset(&archiverStats, 0, sizeof(archiverStats));
    goto done;
  }

     
                                                                        
                                   
     
  for (;;)
  {
    switch (fgetc(fpin))
    {
         
                                                               
                  
         
    case 'D':
      if (fread(&dbbuf, 1, offsetof(PgStat_StatDBEntry, tables), fpin) != offsetof(PgStat_StatDBEntry, tables))
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

         
                            
         
      dbentry = (PgStat_StatDBEntry *)hash_search(dbhash, (void *)&dbbuf.databaseid, HASH_ENTER, &found);
      if (found)
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

      memcpy(dbentry, &dbbuf, sizeof(PgStat_StatDBEntry));
      dbentry->tables = NULL;
      dbentry->functions = NULL;

         
                                                                    
                                                                    
                                                                
                  
         
      if (pgStatRunningInCollector)
      {
        dbentry->stats_timestamp = 0;
      }

         
                                                                    
                    
         
      if (onlydb != InvalidOid)
      {
        if (dbbuf.databaseid != onlydb && dbbuf.databaseid != InvalidOid)
        {
          break;
        }
      }

      memset(&hash_ctl, 0, sizeof(hash_ctl));
      hash_ctl.keysize = sizeof(Oid);
      hash_ctl.entrysize = sizeof(PgStat_StatTabEntry);
      hash_ctl.hcxt = pgStatLocalContext;
      dbentry->tables = hash_create("Per-database table", PGSTAT_TAB_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

      hash_ctl.keysize = sizeof(Oid);
      hash_ctl.entrysize = sizeof(PgStat_StatFuncEntry);
      hash_ctl.hcxt = pgStatLocalContext;
      dbentry->functions = hash_create("Per-database function", PGSTAT_FUNCTION_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

         
                                                                
                                                              
         
      if (deep)
      {
        pgstat_read_db_statsfile(dbentry->databaseid, dbentry->tables, dbentry->functions, permanent);
      }

      break;

    case 'E':
      goto done;

    default:
      ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
      goto done;
    }
  }

done:
  FreeFile(fpin);

                                                                    
  if (permanent)
  {
    elog(DEBUG2, "removing permanent stats file \"%s\"", statfile);
    unlink(statfile);
  }

  return dbhash;
}

              
                                
   
                                                                           
                                                           
   
                                                                           
                          
   
                                                                             
                                                                             
                         
              
   
static void
pgstat_read_db_statsfile(Oid databaseid, HTAB *tabhash, HTAB *funchash, bool permanent)
{
  PgStat_StatTabEntry *tabentry;
  PgStat_StatTabEntry tabbuf;
  PgStat_StatFuncEntry funcbuf;
  PgStat_StatFuncEntry *funcentry;
  FILE *fpin;
  int32 format_id;
  bool found;
  char statfile[MAXPGPATH];

  get_dbstat_filename(permanent, false, databaseid, statfile, MAXPGPATH);

     
                                                                          
                                                                           
                          
     
                                                                          
                                                                       
                              
     
  if ((fpin = AllocateFile(statfile, PG_BINARY_R)) == NULL)
  {
    if (errno != ENOENT)
    {
      ereport(pgStatRunningInCollector ? LOG : WARNING, (errcode_for_file_access(), errmsg("could not open statistics file \"%s\": %m", statfile)));
    }
    return;
  }

     
                                         
     
  if (fread(&format_id, 1, sizeof(format_id), fpin) != sizeof(format_id) || format_id != PGSTAT_FILE_FORMAT_ID)
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    goto done;
  }

     
                                                                        
                                   
     
  for (;;)
  {
    switch (fgetc(fpin))
    {
         
                                            
         
    case 'T':
      if (fread(&tabbuf, 1, sizeof(PgStat_StatTabEntry), fpin) != sizeof(PgStat_StatTabEntry))
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

         
                                        
         
      if (tabhash == NULL)
      {
        break;
      }

      tabentry = (PgStat_StatTabEntry *)hash_search(tabhash, (void *)&tabbuf.tableid, HASH_ENTER, &found);

      if (found)
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

      memcpy(tabentry, &tabbuf, sizeof(tabbuf));
      break;

         
                                             
         
    case 'F':
      if (fread(&funcbuf, 1, sizeof(PgStat_StatFuncEntry), fpin) != sizeof(PgStat_StatFuncEntry))
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

         
                                           
         
      if (funchash == NULL)
      {
        break;
      }

      funcentry = (PgStat_StatFuncEntry *)hash_search(funchash, (void *)&funcbuf.functionid, HASH_ENTER, &found);

      if (found)
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

      memcpy(funcentry, &funcbuf, sizeof(funcbuf));
      break;

         
                                                      
         
    case 'E':
      goto done;

    default:
      ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
      goto done;
    }
  }

done:
  FreeFile(fpin);

  if (permanent)
  {
    elog(DEBUG2, "removing permanent stats file \"%s\"", statfile);
    unlink(statfile);
  }
}

              
                                          
   
                                                                     
                                                               
   
                                                                             
                                                                            
   
                                                                              
                          
   
                                                                        
                                                                            
                                         
              
   
static bool
pgstat_read_db_statsfile_timestamp(Oid databaseid, bool permanent, TimestampTz *ts)
{
  PgStat_StatDBEntry dbentry;
  PgStat_GlobalStats myGlobalStats;
  PgStat_ArchiverStats myArchiverStats;
  FILE *fpin;
  int32 format_id;
  const char *statfile = permanent ? PGSTAT_STAT_PERMANENT_FILENAME : pgstat_stat_filename;

     
                                                                             
                        
     
  if ((fpin = AllocateFile(statfile, PG_BINARY_R)) == NULL)
  {
    if (errno != ENOENT)
    {
      ereport(pgStatRunningInCollector ? LOG : WARNING, (errcode_for_file_access(), errmsg("could not open statistics file \"%s\": %m", statfile)));
    }
    return false;
  }

     
                                         
     
  if (fread(&format_id, 1, sizeof(format_id), fpin) != sizeof(format_id) || format_id != PGSTAT_FILE_FORMAT_ID)
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    FreeFile(fpin);
    return false;
  }

     
                              
     
  if (fread(&myGlobalStats, 1, sizeof(myGlobalStats), fpin) != sizeof(myGlobalStats))
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    FreeFile(fpin);
    return false;
  }

     
                                
     
  if (fread(&myArchiverStats, 1, sizeof(myArchiverStats), fpin) != sizeof(myArchiverStats))
  {
    ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
    FreeFile(fpin);
    return false;
  }

                                                                           
  *ts = myGlobalStats.stats_timestamp;

     
                                                                        
                                                                      
     
  for (;;)
  {
    switch (fgetc(fpin))
    {
         
                                                               
                  
         
    case 'D':
      if (fread(&dbentry, 1, offsetof(PgStat_StatDBEntry, tables), fpin) != offsetof(PgStat_StatDBEntry, tables))
      {
        ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
        goto done;
      }

         
                                                                     
                     
         
      if (dbentry.databaseid == databaseid)
      {
        *ts = dbentry.stats_timestamp;
        goto done;
      }

      break;

    case 'E':
      goto done;

    default:
      ereport(pgStatRunningInCollector ? LOG : WARNING, (errmsg("corrupted statistics file \"%s\"", statfile)));
      goto done;
    }
  }

done:
  FreeFile(fpin);
  return true;
}

   
                                                                      
                                                                             
                                                 
   
static void
backend_read_statsfile(void)
{
  TimestampTz min_ts = 0;
  TimestampTz ref_ts = 0;
  Oid inquiry_db;
  int count;

                        
  if (pgStatDBHash)
  {
    return;
  }
  Assert(!pgStatRunningInCollector);

     
                                                                             
                                                                            
                                                                        
                                                                   
     
  if (IsAutoVacuumLauncherProcess())
  {
    inquiry_db = InvalidOid;
  }
  else
  {
    inquiry_db = MyDatabaseId;
  }

     
                                                                            
                                                                          
                                                                       
     
  for (count = 0; count < PGSTAT_POLL_LOOP_COUNT; count++)
  {
    bool ok;
    TimestampTz file_ts = 0;
    TimestampTz cur_ts;

    CHECK_FOR_INTERRUPTS();

    ok = pgstat_read_db_statsfile_timestamp(inquiry_db, false, &file_ts);

    cur_ts = GetCurrentTimestamp();
                                                                  
    if (count == 0 || cur_ts < ref_ts)
    {
         
                                                                         
                                                                      
                                                                         
                                                                       
                                                                   
                                                                 
         
                                                                 
                                                                       
                                                                     
                                                                      
                                                                      
                                                                        
                          
         
      ref_ts = cur_ts;
      if (IsAutoVacuumWorkerProcess())
      {
        min_ts = TimestampTzPlusMilliseconds(ref_ts, -PGSTAT_RETRY_DELAY);
      }
      else
      {
        min_ts = TimestampTzPlusMilliseconds(ref_ts, -PGSTAT_STAT_INTERVAL);
      }
    }

       
                                                                         
                                                                         
                                                                       
                                                                   
                                                                      
       
    if (ok && file_ts > cur_ts)
    {
         
                                                                        
                                                                  
                                                  
         
      if (file_ts >= TimestampTzPlusMilliseconds(cur_ts, 1000))
      {
        char *filetime;
        char *mytime;

                                                                     
        filetime = pstrdup(timestamptz_to_str(file_ts));
        mytime = pstrdup(timestamptz_to_str(cur_ts));
        elog(LOG, "stats collector's time %s is later than backend local time %s", filetime, mytime);
        pfree(filetime);
        pfree(mytime);
      }

      pgstat_send_inquiry(cur_ts, min_ts, inquiry_db);
      break;
    }

                                                                    
    if (ok && file_ts >= min_ts)
    {
      break;
    }

                                                                    
    if ((count % PGSTAT_INQ_LOOP_COUNT) == 0)
    {
      pgstat_send_inquiry(cur_ts, min_ts, inquiry_db);
    }

    pg_usleep(PGSTAT_RETRY_DELAY * 1000L);
  }

  if (count >= PGSTAT_POLL_LOOP_COUNT)
  {
    ereport(LOG, (errmsg("using stale statistics instead of current ones "
                         "because stats collector is not responding")));
  }

     
                                                                             
                                                                           
                                                    
     
  if (IsAutoVacuumLauncherProcess())
  {
    pgStatDBHash = pgstat_read_statsfiles(InvalidOid, false, false);
  }
  else
  {
    pgStatDBHash = pgstat_read_statsfiles(MyDatabaseId, false, true);
  }
}

              
                           
   
                                                   
              
   
static void
pgstat_setup_memcxt(void)
{
  if (!pgStatLocalContext)
  {
    pgStatLocalContext = AllocSetContextCreate(TopMemoryContext, "Statistics snapshot", ALLOCSET_SMALL_SIZES);
  }
}

              
                             
   
                                                                          
                                                
   
                                                                      
                                  
              
   
void
pgstat_clear_snapshot(void)
{
                                            
  if (pgStatLocalContext)
  {
    MemoryContextDelete(pgStatLocalContext);
  }

                       
  pgStatLocalContext = NULL;
  pgStatDBHash = NULL;
  localBackendStatusTable = NULL;
  localNumBackends = 0;
}

              
                           
   
                                  
              
   
static void
pgstat_recv_inquiry(PgStat_MsgInquiry *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

  elog(DEBUG2, "received inquiry for database %u", msg->databaseid);

     
                                                                            
     
                                                                         
                                                                      
                                                                           
                                                                             
                  
     
  if (list_member_oid(pending_write_requests, msg->databaseid))
  {
    return;
  }

     
                                                                            
                                                                            
                                                               
     
                                                                           
                                                                           
                                                                         
                                                                          
                                                                             
                                               
     
  dbentry = pgstat_get_db_entry(msg->databaseid, false);
  if (dbentry == NULL)
  {
       
                                                                          
                                                                     
                                                                           
                                                                           
                                                               
       
  }
  else if (msg->clock_time < dbentry->stats_timestamp)
  {
    TimestampTz cur_ts = GetCurrentTimestamp();

    if (cur_ts < dbentry->stats_timestamp)
    {
         
                                                                         
                                                          
         
      char *writetime;
      char *mytime;

                                                                   
      writetime = pstrdup(timestamptz_to_str(dbentry->stats_timestamp));
      mytime = pstrdup(timestamptz_to_str(cur_ts));
      elog(LOG, "stats_timestamp %s is later than collector's time %s for database %u", writetime, mytime, dbentry->databaseid);
      pfree(writetime);
      pfree(mytime);
    }
    else
    {
         
                                                                       
                                                                    
         
      return;
    }
  }
  else if (msg->cutoff_time <= dbentry->stats_timestamp)
  {
                                  
    return;
  }

     
                                                    
     
  pending_write_requests = lappend_oid(pending_write_requests, msg->databaseid);
}

              
                           
   
                                    
              
   
static void
pgstat_recv_tabstat(PgStat_MsgTabstat *msg, int len)
{
  PgStat_StatDBEntry *dbentry;
  PgStat_StatTabEntry *tabentry;
  int i;
  bool found;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

     
                                 
     
  dbentry->n_xact_commit += (PgStat_Counter)(msg->m_xact_commit);
  dbentry->n_xact_rollback += (PgStat_Counter)(msg->m_xact_rollback);
  dbentry->n_block_read_time += msg->m_block_read_time;
  dbentry->n_block_write_time += msg->m_block_write_time;

     
                                               
     
  for (i = 0; i < msg->m_nentries; i++)
  {
    PgStat_TableEntry *tabmsg = &(msg->m_entry[i]);

    tabentry = (PgStat_StatTabEntry *)hash_search(dbentry->tables, (void *)&(tabmsg->t_id), HASH_ENTER, &found);

    if (!found)
    {
         
                                                                         
                   
         
      tabentry->numscans = tabmsg->t_counts.t_numscans;
      tabentry->tuples_returned = tabmsg->t_counts.t_tuples_returned;
      tabentry->tuples_fetched = tabmsg->t_counts.t_tuples_fetched;
      tabentry->tuples_inserted = tabmsg->t_counts.t_tuples_inserted;
      tabentry->tuples_updated = tabmsg->t_counts.t_tuples_updated;
      tabentry->tuples_deleted = tabmsg->t_counts.t_tuples_deleted;
      tabentry->tuples_hot_updated = tabmsg->t_counts.t_tuples_hot_updated;
      tabentry->n_live_tuples = tabmsg->t_counts.t_delta_live_tuples;
      tabentry->n_dead_tuples = tabmsg->t_counts.t_delta_dead_tuples;
      tabentry->changes_since_analyze = tabmsg->t_counts.t_changed_tuples;
      tabentry->blocks_fetched = tabmsg->t_counts.t_blocks_fetched;
      tabentry->blocks_hit = tabmsg->t_counts.t_blocks_hit;

      tabentry->vacuum_timestamp = 0;
      tabentry->vacuum_count = 0;
      tabentry->autovac_vacuum_timestamp = 0;
      tabentry->autovac_vacuum_count = 0;
      tabentry->analyze_timestamp = 0;
      tabentry->analyze_count = 0;
      tabentry->autovac_analyze_timestamp = 0;
      tabentry->autovac_analyze_count = 0;
    }
    else
    {
         
                                                         
         
      tabentry->numscans += tabmsg->t_counts.t_numscans;
      tabentry->tuples_returned += tabmsg->t_counts.t_tuples_returned;
      tabentry->tuples_fetched += tabmsg->t_counts.t_tuples_fetched;
      tabentry->tuples_inserted += tabmsg->t_counts.t_tuples_inserted;
      tabentry->tuples_updated += tabmsg->t_counts.t_tuples_updated;
      tabentry->tuples_deleted += tabmsg->t_counts.t_tuples_deleted;
      tabentry->tuples_hot_updated += tabmsg->t_counts.t_tuples_hot_updated;
                                                                      
      if (tabmsg->t_counts.t_truncated)
      {
        tabentry->n_live_tuples = 0;
        tabentry->n_dead_tuples = 0;
      }
      tabentry->n_live_tuples += tabmsg->t_counts.t_delta_live_tuples;
      tabentry->n_dead_tuples += tabmsg->t_counts.t_delta_dead_tuples;
      tabentry->changes_since_analyze += tabmsg->t_counts.t_changed_tuples;
      tabentry->blocks_fetched += tabmsg->t_counts.t_blocks_fetched;
      tabentry->blocks_hit += tabmsg->t_counts.t_blocks_hit;
    }

                                                                   
    tabentry->n_live_tuples = Max(tabentry->n_live_tuples, 0);
                                    
    tabentry->n_dead_tuples = Max(tabentry->n_dead_tuples, 0);

       
                                                           
       
    dbentry->n_tuples_returned += tabmsg->t_counts.t_tuples_returned;
    dbentry->n_tuples_fetched += tabmsg->t_counts.t_tuples_fetched;
    dbentry->n_tuples_inserted += tabmsg->t_counts.t_tuples_inserted;
    dbentry->n_tuples_updated += tabmsg->t_counts.t_tuples_updated;
    dbentry->n_tuples_deleted += tabmsg->t_counts.t_tuples_deleted;
    dbentry->n_blocks_fetched += tabmsg->t_counts.t_blocks_fetched;
    dbentry->n_blocks_hit += tabmsg->t_counts.t_blocks_hit;
  }
}

              
                            
   
                                   
              
   
static void
pgstat_recv_tabpurge(PgStat_MsgTabpurge *msg, int len)
{
  PgStat_StatDBEntry *dbentry;
  int i;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, false);

     
                                                          
     
  if (!dbentry || !dbentry->tables)
  {
    return;
  }

     
                                               
     
  for (i = 0; i < msg->m_nentries; i++)
  {
                                                                      
    (void)hash_search(dbentry->tables, (void *)&(msg->m_tableid[i]), HASH_REMOVE, NULL);
  }
}

              
                          
   
                                     
              
   
static void
pgstat_recv_dropdb(PgStat_MsgDropdb *msg, int len)
{
  Oid dbid = msg->m_databaseid;
  PgStat_StatDBEntry *dbentry;

     
                                           
     
  dbentry = pgstat_get_db_entry(dbid, false);

     
                                                       
     
  if (dbentry)
  {
    char statfile[MAXPGPATH];

    get_dbstat_filename(false, false, dbid, statfile, MAXPGPATH);

    elog(DEBUG2, "removing stats file \"%s\"", statfile);
    unlink(statfile);

    if (dbentry->tables != NULL)
    {
      hash_destroy(dbentry->tables);
    }
    if (dbentry->functions != NULL)
    {
      hash_destroy(dbentry->functions);
    }

    if (hash_search(pgStatDBHash, (void *)&dbid, HASH_REMOVE, NULL) == NULL)
    {
      ereport(ERROR, (errmsg("database hash table corrupted during cleanup --- abort")));
    }
  }
}

              
                                
   
                                                    
              
   
static void
pgstat_recv_resetcounter(PgStat_MsgResetcounter *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

     
                                                                        
     
  dbentry = pgstat_get_db_entry(msg->m_databaseid, false);

  if (!dbentry)
  {
    return;
  }

     
                                                                           
                              
     
  if (dbentry->tables != NULL)
  {
    hash_destroy(dbentry->tables);
  }
  if (dbentry->functions != NULL)
  {
    hash_destroy(dbentry->functions);
  }

  dbentry->tables = NULL;
  dbentry->functions = NULL;

     
                                                                          
                           
     
  reset_dbentry_counters(dbentry);
}

              
                               
   
                                                
              
   
static void
pgstat_recv_resetsharedcounter(PgStat_MsgResetsharedcounter *msg, int len)
{
  if (msg->m_resettarget == RESET_BGWRITER)
  {
                                                                        
    memset(&globalStats, 0, sizeof(globalStats));
    globalStats.stat_reset_timestamp = GetCurrentTimestamp();
  }
  else if (msg->m_resettarget == RESET_ARCHIVER)
  {
                                                        
    memset(&archiverStats, 0, sizeof(archiverStats));
    archiverStats.stat_reset_timestamp = GetCurrentTimestamp();
  }

     
                                                                       
                                     
     
}

              
                                      
   
                                          
              
   
static void
pgstat_recv_resetsinglecounter(PgStat_MsgResetsinglecounter *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, false);

  if (!dbentry)
  {
    return;
  }

                                                      
  dbentry->stat_reset_timestamp = GetCurrentTimestamp();

                                                    
  if (msg->m_resettype == RESET_TABLE)
  {
    (void)hash_search(dbentry->tables, (void *)&(msg->m_objectid), HASH_REMOVE, NULL);
  }
  else if (msg->m_resettype == RESET_FUNCTION)
  {
    (void)hash_search(dbentry->functions, (void *)&(msg->m_objectid), HASH_REMOVE, NULL);
  }
}

              
                           
   
                                             
              
   
static void
pgstat_recv_autovac(PgStat_MsgAutovacStart *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

     
                                                                       
     
  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  dbentry->last_autovac_time = msg->m_start_time;
}

              
                          
   
                             
              
   
static void
pgstat_recv_vacuum(PgStat_MsgVacuum *msg, int len)
{
  PgStat_StatDBEntry *dbentry;
  PgStat_StatTabEntry *tabentry;

     
                                                    
     
  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  tabentry = pgstat_get_tab_entry(dbentry, msg->m_tableoid, true);

  tabentry->n_live_tuples = msg->m_live_tuples;
  tabentry->n_dead_tuples = msg->m_dead_tuples;

  if (msg->m_autovacuum)
  {
    tabentry->autovac_vacuum_timestamp = msg->m_vacuumtime;
    tabentry->autovac_vacuum_count++;
  }
  else
  {
    tabentry->vacuum_timestamp = msg->m_vacuumtime;
    tabentry->vacuum_count++;
  }
}

              
                           
   
                               
              
   
static void
pgstat_recv_analyze(PgStat_MsgAnalyze *msg, int len)
{
  PgStat_StatDBEntry *dbentry;
  PgStat_StatTabEntry *tabentry;

     
                                                    
     
  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  tabentry = pgstat_get_tab_entry(dbentry, msg->m_tableoid, true);

  tabentry->n_live_tuples = msg->m_live_tuples;
  tabentry->n_dead_tuples = msg->m_dead_tuples;

     
                                                                          
                                                                           
                                                                
     
  if (msg->m_resetcounter)
  {
    tabentry->changes_since_analyze = 0;
  }

  if (msg->m_autovacuum)
  {
    tabentry->autovac_analyze_timestamp = msg->m_analyzetime;
    tabentry->autovac_analyze_count++;
  }
  else
  {
    tabentry->analyze_timestamp = msg->m_analyzetime;
    tabentry->analyze_count++;
  }
}

              
                            
   
                               
              
   
static void
pgstat_recv_archiver(PgStat_MsgArchiver *msg, int len)
{
  if (msg->m_failed)
  {
                                 
    ++archiverStats.failed_count;
    memcpy(archiverStats.last_failed_wal, msg->m_xlog, sizeof(archiverStats.last_failed_wal));
    archiverStats.last_failed_timestamp = msg->m_timestamp;
  }
  else
  {
                                       
    ++archiverStats.archived_count;
    memcpy(archiverStats.last_archived_wal, msg->m_xlog, sizeof(archiverStats.last_archived_wal));
    archiverStats.last_archived_timestamp = msg->m_timestamp;
  }
}

              
                            
   
                               
              
   
static void
pgstat_recv_bgwriter(PgStat_MsgBgWriter *msg, int len)
{
  globalStats.timed_checkpoints += msg->m_timed_checkpoints;
  globalStats.requested_checkpoints += msg->m_requested_checkpoints;
  globalStats.checkpoint_write_time += msg->m_checkpoint_write_time;
  globalStats.checkpoint_sync_time += msg->m_checkpoint_sync_time;
  globalStats.buf_written_checkpoints += msg->m_buf_written_checkpoints;
  globalStats.buf_written_clean += msg->m_buf_written_clean;
  globalStats.maxwritten_clean += msg->m_maxwritten_clean;
  globalStats.buf_written_backend += msg->m_buf_written_backend;
  globalStats.buf_fsync_backend += msg->m_buf_fsync_backend;
  globalStats.buf_alloc += msg->m_buf_alloc;
}

              
                                    
   
                                       
              
   
static void
pgstat_recv_recoveryconflict(PgStat_MsgRecoveryConflict *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  switch (msg->m_reason)
  {
  case PROCSIG_RECOVERY_CONFLICT_DATABASE:

       
                                                                      
                                                                  
       
    break;
  case PROCSIG_RECOVERY_CONFLICT_TABLESPACE:
    dbentry->n_conflict_tablespace++;
    break;
  case PROCSIG_RECOVERY_CONFLICT_LOCK:
    dbentry->n_conflict_lock++;
    break;
  case PROCSIG_RECOVERY_CONFLICT_SNAPSHOT:
    dbentry->n_conflict_snapshot++;
    break;
  case PROCSIG_RECOVERY_CONFLICT_BUFFERPIN:
    dbentry->n_conflict_bufferpin++;
    break;
  case PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK:
    dbentry->n_conflict_startup_deadlock++;
    break;
  }
}

              
                            
   
                               
              
   
static void
pgstat_recv_deadlock(PgStat_MsgDeadlock *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  dbentry->n_deadlocks++;
}

              
                                    
   
                                      
              
   
static void
pgstat_recv_checksum_failure(PgStat_MsgChecksumFailure *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  dbentry->n_checksum_failures += msg->m_failurecount;
  dbentry->last_checksum_failure = msg->m_failure_time;
}

              
                            
   
                               
              
   
static void
pgstat_recv_tempfile(PgStat_MsgTempFile *msg, int len)
{
  PgStat_StatDBEntry *dbentry;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

  dbentry->n_temp_bytes += msg->m_filesize;
  dbentry->n_temp_files += 1;
}

              
                            
   
                                    
              
   
static void
pgstat_recv_funcstat(PgStat_MsgFuncstat *msg, int len)
{
  PgStat_FunctionEntry *funcmsg = &(msg->m_entry[0]);
  PgStat_StatDBEntry *dbentry;
  PgStat_StatFuncEntry *funcentry;
  int i;
  bool found;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, true);

     
                                                  
     
  for (i = 0; i < msg->m_nentries; i++, funcmsg++)
  {
    funcentry = (PgStat_StatFuncEntry *)hash_search(dbentry->functions, (void *)&(funcmsg->f_id), HASH_ENTER, &found);

    if (!found)
    {
         
                                                                         
                      
         
      funcentry->f_numcalls = funcmsg->f_numcalls;
      funcentry->f_total_time = funcmsg->f_total_time;
      funcentry->f_self_time = funcmsg->f_self_time;
    }
    else
    {
         
                                                         
         
      funcentry->f_numcalls += funcmsg->f_numcalls;
      funcentry->f_total_time += funcmsg->f_total_time;
      funcentry->f_self_time += funcmsg->f_self_time;
    }
  }
}

              
                             
   
                                      
              
   
static void
pgstat_recv_funcpurge(PgStat_MsgFuncpurge *msg, int len)
{
  PgStat_StatDBEntry *dbentry;
  int i;

  dbentry = pgstat_get_db_entry(msg->m_databaseid, false);

     
                                                          
     
  if (!dbentry || !dbentry->functions)
  {
    return;
  }

     
                                                  
     
  for (i = 0; i < msg->m_nentries; i++)
  {
                                                                      
    (void)hash_search(dbentry->functions, (void *)&(msg->m_functionid[i]), HASH_REMOVE, NULL);
  }
}

              
                                     
   
                                            
              
   
static bool
pgstat_write_statsfile_needed(void)
{
  if (pending_write_requests != NIL)
  {
    return true;
  }

                                       
  return false;
}

              
                           
   
                                                                          
              
   
static bool
pgstat_db_requested(Oid databaseid)
{
     
                                                                           
                                                                     
                                                                         
                                                              
     
  if (databaseid == InvalidOid && pending_write_requests != NIL)
  {
    return true;
  }

                                                                        
  if (list_member_oid(pending_write_requests, databaseid))
  {
    return true;
  }

  return false;
}

   
                                                                 
                                                                               
        
   
                                                                              
          
   
char *
pgstat_clip_activity(const char *raw_activity)
{
  char *activity;
  int rawlen;
  int cliplen;

     
                                                                      
                                                                           
                                                                             
                                                                           
                                                                            
            
     
  activity = pnstrdup(raw_activity, pgstat_track_activity_query_size - 1);

                                                  
  rawlen = strlen(activity);

     
                                                                             
                                                                             
                                                                           
                                                                             
                                                                            
                
     
  cliplen = pg_mbcliplen(activity, rawlen, pgstat_track_activity_query_size - 1);

  activity[cliplen] = '\0';

  return activity;
}
