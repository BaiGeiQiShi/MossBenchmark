                                                                            
   
          
                                       
   
   
                                                                         
                                                                        
   
                                     
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/multixact.h"
#include "access/rewriteheap.h"
#include "access/subtrans.h"
#include "access/timeline.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/xloginsert.h"
#include "access/xlogreader.h"
#include "access/xlogutils.h"
#include "catalog/catversion.h"
#include "catalog/pg_control.h"
#include "catalog/pg_database.h"
#include "commands/tablespace.h"
#include "common/controldata_utils.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "postmaster/bgwriter.h"
#include "postmaster/walwriter.h"
#include "postmaster/startup.h"
#include "replication/basebackup.h"
#include "replication/logical.h"
#include "replication/slot.h"
#include "replication/origin.h"
#include "replication/snapbuild.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/large_object.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/reinit.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "storage/sync.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"
#include "pg_trace.h"

extern uint32 bootstrap_data_checksum_version;

                                                                       
#define RECOVERY_COMMAND_FILE "recovery.conf"
#define RECOVERY_COMMAND_DONE "recovery.done"

                              
int max_wal_size_mb = 1024;           
int min_wal_size_mb = 80;              
int wal_keep_segments = 0;
int XLOGbuffers = -1;
int XLogArchiveTimeout = 0;
int XLogArchiveMode = ARCHIVE_MODE_OFF;
char *XLogArchiveCommand = NULL;
bool EnableHotStandby = false;
bool fullPageWrites = true;
bool wal_log_hints = false;
bool wal_compression = false;
char *wal_consistency_checking_string = NULL;
bool *wal_consistency_checking = NULL;
bool wal_init_zero = true;
bool wal_recycle = true;
bool log_checkpoints = false;
int sync_method = DEFAULT_SYNC_METHOD;
int wal_level = WAL_LEVEL_MINIMAL;
int CommitDelay = 0;                                         
int CommitSiblings = 5;                                         
int wal_retrieve_retry_interval = 5000;

#ifdef WAL_DEBUG
bool XLOG_DEBUG = false;
#endif

int wal_segment_size = DEFAULT_XLOG_SEG_SIZE;

   
                                                                               
                                                                           
                                         
   
#define NUM_XLOGINSERT_LOCKS 8

   
                                                                         
               
   
int CheckPointSegments;

                                                      
static double CheckPointDistanceEstimate = 0;
static double PrevCheckPointDistance = 0;

   
               
   
const struct config_enum_entry sync_method_options[] = {{"fsync", SYNC_METHOD_FSYNC, false},
#ifdef HAVE_FSYNC_WRITETHROUGH
    {"fsync_writethrough", SYNC_METHOD_FSYNC_WRITETHROUGH, false},
#endif
#ifdef HAVE_FDATASYNC
    {"fdatasync", SYNC_METHOD_FDATASYNC, false},
#endif
#ifdef OPEN_SYNC_FLAG
    {"open_sync", SYNC_METHOD_OPEN, false},
#endif
#ifdef OPEN_DATASYNC_FLAG
    {"open_datasync", SYNC_METHOD_OPEN_DSYNC, false},
#endif
    {NULL, 0, false}};

   
                                                           
                                                        
   
const struct config_enum_entry archive_mode_options[] = {{"always", ARCHIVE_MODE_ALWAYS, false}, {"on", ARCHIVE_MODE_ON, false}, {"off", ARCHIVE_MODE_OFF, false}, {"true", ARCHIVE_MODE_ON, true}, {"false", ARCHIVE_MODE_OFF, true}, {"yes", ARCHIVE_MODE_ON, true}, {"no", ARCHIVE_MODE_OFF, true}, {"1", ARCHIVE_MODE_ON, true}, {"0", ARCHIVE_MODE_OFF, true}, {NULL, 0, false}};

const struct config_enum_entry recovery_target_action_options[] = {{"pause", RECOVERY_TARGET_ACTION_PAUSE, false}, {"promote", RECOVERY_TARGET_ACTION_PROMOTE, false}, {"shutdown", RECOVERY_TARGET_ACTION_SHUTDOWN, false}, {NULL, 0, false}};

   
                                                                          
                                                                      
                                                        
   
CheckpointStatsData CheckpointStats;

   
                                                                         
                                         
   
TimeLineID ThisTimeLineID = 0;

   
                                    
   
                                                                               
                                                                          
                                                                           
                                                                              
                                                                             
                                                                             
                                           
   
bool InRecovery = false;

                                                                           
HotStandbyState standbyState = STANDBY_DISABLED;

static XLogRecPtr LastRec;

                                        
static XLogRecPtr receivedUpto = 0;
static TimeLineID receiveTLI = 0;

   
                                                                            
                                                                      
                                                                          
            
   
static XLogRecPtr abortedRecPtr;
static XLogRecPtr missingContrecPtr;

   
                                                                            
                                                                             
                                                                            
                                                 
   
static bool lastFullPageWrites;

   
                                                                            
                                                                             
                                                      
   
static bool LocalRecoveryInProgress = true;

   
                                                                            
                                           
   
static bool LocalHotStandbyActive = false;

   
                                        
                                              
                                                  
                                                                    
                                                                               
                                                                           
                                                                              
                                                      
   
static int LocalXLogInsertAllowed = -1;

   
                                                                         
                                                                        
                                                                              
                                 
   
                                                                                
                                                                            
                                                                             
                  
   
bool ArchiveRecoveryRequested = false;
bool InArchiveRecovery = false;

static bool standby_signal_file_found = false;
static bool recovery_signal_file_found = false;

                                                             
static bool restoredFromArchive = false;

                                                            
static char *replay_image_masked = NULL;
static char *master_image_masked = NULL;

                                                                    
char *recoveryRestoreCommand = NULL;
char *recoveryEndCommand = NULL;
char *archiveCleanupCommand = NULL;
RecoveryTargetType recoveryTarget = RECOVERY_TARGET_UNSET;
bool recoveryTargetInclusive = true;
int recoveryTargetAction = RECOVERY_TARGET_ACTION_PAUSE;
TransactionId recoveryTargetXid;
char *recovery_target_time_string;
static TimestampTz recoveryTargetTime;
const char *recoveryTargetName;
XLogRecPtr recoveryTargetLSN;
int recovery_min_apply_delay = 0;
TimestampTz recoveryDelayUntilTime;

                                                                  
bool StandbyModeRequested = false;
char *PrimaryConnInfo = NULL;
char *PrimarySlotName = NULL;
char *PromoteTriggerFile = NULL;

                                       
bool StandbyMode = false;

                                                          
static bool fast_promote = false;

   
                                                                               
              
   
static TransactionId recoveryStopXid;
static TimestampTz recoveryStopTime;
static XLogRecPtr recoveryStopLSN;
static char recoveryStopName[MAXFNAMELEN];
static bool recoveryStopAfter;

   
                                                                               
                                                                            
                                                                             
                                                                              
                                                                        
                    
   
                                                               
   
                                                                                
   
                                                                        
   
                                                                                             
                                                                       
                                                                           
                                                                      
                                       
   
                                                                            
                                                                         
                                                                               
                                                                           
                
   
RecoveryTargetTimeLineGoal recoveryTargetTimeLineGoal = RECOVERY_TARGET_TIMELINE_LATEST;
TimeLineID recoveryTargetTLIRequested = 0;
TimeLineID recoveryTargetTLI = 0;
static List *expectedTLEs;
static TimeLineID curFileTLI;

   
                                                                              
                                                                              
                                                                               
                                                                                
                             
   
                                                                               
                                                                               
                                                                                
                                                                              
                                                                               
                                                                            
                                      
   
XLogRecPtr ProcLastRecPtr = InvalidXLogRecPtr;
XLogRecPtr XactLastRecEnd = InvalidXLogRecPtr;
XLogRecPtr XactLastCommitEnd = InvalidXLogRecPtr;

   
                                                                      
                                                                           
                                                                    
                                                                         
                                                                            
                                                                       
                                                                          
                   
   
static XLogRecPtr RedoRecPtr;

   
                                                                    
                                                                           
                                                 
   
static bool doPageWrites;

                                                           
static bool doRequestWalReceiverReply;

   
                                                                            
                                                                           
                                                                          
                                                                        
                                                                          
                                                                         
                                                                       
                                                              
   
static XLogRecPtr RedoStartLSN = InvalidXLogRecPtr;

             
                                                  
   
                                                                           
                                                                             
                                                                              
                                                                             
                                 
   
                                                                   
                                                                           
                                                                           
                                                                             
                                                                            
                                     
   
                                                                             
                                                                         
   
                                                                             
                                                                           
                                                                       
   
                                                                              
                                                                        
                                                                               
                                                                        
   
                                                                         
               
   
                                                                       
                 
   
                                                                            
                                                                            
                                              
   
             
   

typedef struct XLogwrtRqst
{
  XLogRecPtr Write;                                 
  XLogRecPtr Flush;                             
} XLogwrtRqst;

typedef struct XLogwrtResult
{
  XLogRecPtr Write;                                
  XLogRecPtr Flush;                            
} XLogwrtResult;

   
                                                                          
                                                                            
                                                                            
                                                                            
                                                                    
   
                                                                          
                                                                          
                                                                            
                                                                    
                                                                              
                                                                           
                              
   
                                                                         
                                                                            
                                                                             
                                                                           
                                                                           
              
   
                                                                              
                                                                           
                                                                           
                                                            
   
                                                                              
                                                                      
                                                                              
                                                                        
                                                                         
                                                                               
                                                                         
                                                                      
   
typedef struct
{
  LWLock lock;
  XLogRecPtr insertingAt;
  XLogRecPtr lastImportantAt;
} WALInsertLock;

   
                                                                              
                                                                          
                                                                           
                                                                            
                                             
   
typedef union WALInsertLockPadded
{
  WALInsertLock l;
  char pad[PG_CACHE_LINE_SIZE];
} WALInsertLockPadded;

   
                                                                            
                                                      
   
                                                                          
                                                                           
                                                                     
                                                                         
                     
                                                                          
                                                                       
                        
                                                                        
                     
   
typedef enum ExclusiveBackupState
{
  EXCLUSIVE_BACKUP_NONE = 0,
  EXCLUSIVE_BACKUP_STARTING,
  EXCLUSIVE_BACKUP_IN_PROGRESS,
  EXCLUSIVE_BACKUP_STOPPING
} ExclusiveBackupState;

   
                                                                            
                                        
   
static SessionBackupState sessionBackupState = SESSION_BACKUP_NONE;

   
                                        
   
typedef struct XLogCtlInsert
{
  slock_t insertpos_lck;                                           

     
                                                                     
                                                                         
                                                                            
                                                                    
                                                                     
     
  uint64 CurrBytePos;
  uint64 PrevBytePos;

     
                                                                           
                                                                          
                                                                         
                                                                        
                                                                      
     
  char pad[PG_CACHE_LINE_SIZE];

     
                                                                         
                                                                            
                                                                           
                                                                            
                                                   
     
                                                                            
                                  
     
  XLogRecPtr RedoRecPtr;                                        
  bool forcePageWrites;                                          
  bool fullPageWrites;

     
                                                                          
                                                                             
                                                                            
                                                                         
                                                                           
                                               
     
  ExclusiveBackupState exclusiveBackupState;
  int nonExclusiveBackups;
  XLogRecPtr lastBackupStart;

     
                          
     
  WALInsertLockPadded *WALInsertLocks;
} XLogCtlInsert;

   
                                       
   
typedef struct XLogCtlData
{
  XLogCtlInsert Insert;

                              
  XLogwrtRqst LogwrtRqst;
  XLogRecPtr RedoRecPtr;                                                     
  FullTransactionId ckptFullXid;                                          
  XLogRecPtr asyncXactLSN;                                                
  XLogRecPtr replicationSlotMinLSN;                                    

  XLogSegNo lastRemovedSegNo;                                           

                                                                        
  XLogRecPtr unloggedLSN;
  slock_t ulsn_lck;

                                                                            
  pg_time_t lastSegSwitchTime;
  XLogRecPtr lastSegSwitchLSN;

     
                                                                          
                                  
     
  XLogwrtResult LogwrtResult;

     
                                                                    
     
                                                                           
                                                                        
                                                                           
                                                                     
                                                   
                                   
     
  XLogRecPtr InitializedUpTo;

     
                                                                             
                                                                        
                        
     
  char *pages;                                                
  XLogRecPtr *xlblocks;                                   
  int XLogCacheBlck;                                             

     
                                                                           
                                                                  
                                                                      
                                             
     
  TimeLineID ThisTimeLineID;
  TimeLineID PrevTimeLineID;

     
                                                                      
                                       
     
  RecoveryState SharedRecoveryState;

     
                                                                         
                                       
     
  bool SharedHotStandbyActive;

     
                                                                        
                                                                            
                            
     
  bool WalWriterSleeping;

     
                                                                            
                                                                             
                
     
  Latch recoveryWakeupLatch;

     
                                                                           
                                                                   
                                                                             
                                                          
     
                            
     
  XLogRecPtr lastCheckPointRecPtr;
  XLogRecPtr lastCheckPointEndPtr;
  CheckPoint lastCheckPoint;

     
                                                                           
                                                                      
                                                                       
                                                              
     
  XLogRecPtr lastReplayedEndRecPtr;
  TimeLineID lastReplayedTLI;
  XLogRecPtr replayEndRecPtr;
  TimeLineID replayEndTLI;
                                                                          
  TimestampTz recoveryLastXTime;

     
                                                                           
                                                       
     
  TimestampTz currentChunkStartTime;
                                           
  bool recoveryPause;

     
                                                                   
                                                                         
     
  XLogRecPtr lastFpwDisableRecPtr;

  slock_t info_lck;                                         
} XLogCtlData;

static XLogCtlData *XLogCtl = NULL;

                                                                       
static WALInsertLockPadded *WALInsertLocks = NULL;

   
                                                        
   
static ControlFileData *ControlFile = NULL;

   
                                                                         
                        
   
#define INSERT_FREESPACE(endptr) (((endptr) % XLOG_BLCKSZ == 0) ? 0 : (XLOG_BLCKSZ - (endptr) % XLOG_BLCKSZ))

                                            
#define NextBufIdx(idx) (((idx) == XLogCtl->XLogCacheBlck) ? 0 : ((idx) + 1))

   
                                                                         
                                                                
   
#define XLogRecPtrToBufIdx(recptr) (((recptr) / XLOG_BLCKSZ) % (XLogCtl->XLogCacheBlck + 1))

   
                                                                    
   
#define UsableBytesInPage (XLOG_BLCKSZ - SizeOfXLogShortPHD)

   
                                                                            
                
   
#define ConvertToXSegs(x, segsize) ((x) / ((segsize) / (1024 * 1024)))

                                                               
static int UsableBytesInSegment;

   
                                                              
                         
   
static XLogwrtResult LogwrtResult = {0, 0};

   
                                                                           
                          
   
typedef enum
{
  XLOG_FROM_ANY = 0,                                          
  XLOG_FROM_ARCHIVE,                                     
  XLOG_FROM_PG_WAL,                               
  XLOG_FROM_STREAM                             
} XLogSource;

                                                                
static const char *xlogSourceNames[] = {"any", "archive", "pg_wal", "stream"};

   
                                                                  
                                                                          
                                                                     
   
static int openLogFile = -1;
static XLogSegNo openLogSegNo = 0;

   
                                                                         
                                                                   
                                                                       
                                                        
   
static int readFile = -1;
static XLogSegNo readSegNo = 0;
static uint32 readOff = 0;
static uint32 readLen = 0;
static XLogSource readSource = 0;                       

   
                                                                     
                                                                            
                                                                        
                                                                               
         
   
static XLogSource currentSource = 0;                       
static bool lastSourceFailed = false;

typedef struct XLogPageReadPrivate
{
  int emode;
  bool fetching_ckpt;                                           
  bool randAccess;
} XLogPageReadPrivate;

   
                                                                         
                                                                          
                                                                         
                                                                         
                                                                  
                                                                       
   
static TimestampTz XLogReceiptTime = 0;
static XLogSource XLogReceiptSource = 0;                       

                                        
static XLogRecPtr ReadRecPtr;                                
static XLogRecPtr EndRecPtr;                                 

   
                                                                        
                                                                      
                                                                         
                                                                     
                                                                    
   
static XLogRecPtr minRecoveryPoint;
static TimeLineID minRecoveryPointTLI;
static bool updateMinRecoveryPoint = true;

   
                                                                           
                                                                             
                                                                          
   
bool reachedConsistency = false;

static bool InRedo = false;

                                                
static bool bgwriterLaunched = false;

                                                
static int MyLockNo = 0;
static bool holdingAllLocks = false;

#ifdef WAL_DEBUG
static MemoryContext walDebugCxt = NULL;
#endif

static void
readRecoverySignalFile(void);
static void
validateRecoveryParameters(void);
static void
exitArchiveRecovery(TimeLineID endTLI, XLogRecPtr endOfLog);
static bool
recoveryStopsBefore(XLogReaderState *record);
static bool
recoveryStopsAfter(XLogReaderState *record);
static void
recoveryPausesHere(void);
static bool
recoveryApplyDelay(XLogReaderState *record);
static void
SetLatestXTime(TimestampTz xtime);
static void
SetCurrentChunkStartTime(TimestampTz xtime);
static void
CheckRequiredParameterValues(void);
static void
XLogReportParameters(void);
static void
checkTimeLineSwitch(XLogRecPtr lsn, TimeLineID newTLI, TimeLineID prevTLI);
static void
VerifyOverwriteContrecord(xl_overwrite_contrecord *xlrec, XLogReaderState *state);
static void
LocalSetXLogInsertAllowed(void);
static void
CreateEndOfRecoveryRecord(void);
static XLogRecPtr
CreateOverwriteContrecordRecord(XLogRecPtr aborted_lsn);
static void
CheckPointGuts(XLogRecPtr checkPointRedo, int flags);
static void
KeepLogSeg(XLogRecPtr recptr, XLogSegNo *logSegNo);
static XLogRecPtr
XLogGetReplicationSlotMinimumLSN(void);

static void
AdvanceXLInsertBuffer(XLogRecPtr upto, bool opportunistic);
static bool
XLogCheckpointNeeded(XLogSegNo new_segno);
static void
XLogWrite(XLogwrtRqst WriteRqst, bool flexible);
static bool
InstallXLogFileSegment(XLogSegNo *segno, char *tmppath, bool find_free, XLogSegNo max_segno, bool use_lock);
static int
XLogFileRead(XLogSegNo segno, int emode, TimeLineID tli, int source, bool notfoundOk);
static int
XLogFileReadAnyTLI(XLogSegNo segno, int emode, int source);
static int
XLogPageRead(XLogReaderState *xlogreader, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *readBuf, TimeLineID *readTLI);
static bool
WaitForWALToBecomeAvailable(XLogRecPtr RecPtr, bool randAccess, bool fetching_ckpt, XLogRecPtr tliRecPtr);
static int
emode_for_corrupt_record(int emode, XLogRecPtr RecPtr);
static void
XLogFileClose(void);
static void
PreallocXlogFiles(XLogRecPtr endptr);
static void
RemoveTempXlogFiles(void);
static void
RemoveOldXlogFiles(XLogSegNo segno, XLogRecPtr lastredoptr, XLogRecPtr endptr);
static void
RemoveXlogFile(const char *segname, XLogRecPtr lastredoptr, XLogRecPtr endptr);
static void
UpdateLastRemovedPtr(char *filename);
static void
ValidateXLOGDirectoryStructure(void);
static void
CleanupBackupHistory(void);
static void
UpdateMinRecoveryPoint(XLogRecPtr lsn, bool force);
static XLogRecord *
ReadRecord(XLogReaderState *xlogreader, XLogRecPtr RecPtr, int emode, bool fetching_ckpt);
static void
CheckRecoveryConsistency(void);
static XLogRecord *
ReadCheckpointRecord(XLogReaderState *xlogreader, XLogRecPtr RecPtr, int whichChkpti, bool report);
static bool
rescanLatestTimeLine(void);
static void
WriteControlFile(void);
static void
ReadControlFile(void);
static char *
str_time(pg_time_t tnow);
static bool
CheckForStandbyTrigger(void);

#ifdef WAL_DEBUG
static void
xlog_outrec(StringInfo buf, XLogReaderState *record);
#endif
static void
xlog_outdesc(StringInfo buf, XLogReaderState *record);
static void
pg_start_backup_callback(int code, Datum arg);
static void
pg_stop_backup_callback(int code, Datum arg);
static bool
read_backup_label(XLogRecPtr *checkPointLoc, bool *backupEndRequired, bool *backupFromStandby);
static bool
read_tablespace_map(List **tablespaces);

static void
rm_redo_error_callback(void *arg);
static int
get_sync_bit(int method);

static void
CopyXLogRecordToWAL(int write_len, bool isLogSwitch, XLogRecData *rdata, XLogRecPtr StartPos, XLogRecPtr EndPos);
static void
ReserveXLogInsertLocation(int size, XLogRecPtr *StartPos, XLogRecPtr *EndPos, XLogRecPtr *PrevPtr);
static bool
ReserveXLogSwitch(XLogRecPtr *StartPos, XLogRecPtr *EndPos, XLogRecPtr *PrevPtr);
static XLogRecPtr
WaitXLogInsertionsToFinish(XLogRecPtr upto);
static char *
GetXLogBuffer(XLogRecPtr ptr);
static XLogRecPtr
XLogBytePosToRecPtr(uint64 bytepos);
static XLogRecPtr
XLogBytePosToEndRecPtr(uint64 bytepos);
static uint64
XLogRecPtrToBytePos(XLogRecPtr ptr);
static void
checkXLogConsistency(XLogReaderState *record);

static void
WALInsertLockAcquire(void);
static void
WALInsertLockAcquireExclusive(void);
static void
WALInsertLockRelease(void);
static void
WALInsertLockUpdateInsertingAt(XLogRecPtr insertingAt);

   
                                                                             
                                                                            
                                                            
   
                                                                         
                                                                            
                                                                        
                                                                             
                                                                              
                              
   
                                                                         
                                     
   
                                                                             
                                                                       
                                                                              
                  
   
                                                                     
                                                                         
                                                                       
                                                                       
                                              
   
XLogRecPtr
XLogInsertRecord(XLogRecData *rdata, XLogRecPtr fpw_lsn, uint8 flags)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  pg_crc32c rdata_crc;
  bool inserted;
  XLogRecord *rechdr = (XLogRecord *)rdata->data;
  uint8 info = rechdr->xl_info & ~XLR_INFO_MASK;
  bool isLogSwitch = (rechdr->xl_rmid == RM_XLOG_ID && info == XLOG_SWITCH);
  XLogRecPtr StartPos;
  XLogRecPtr EndPos;
  bool prevDoPageWrites = doPageWrites;

                                                                     
  Assert(rdata->len >= SizeOfXLogRecord);

                                                       
  if (!XLogInsertAllowed())
  {
    elog(ERROR, "cannot make new WAL entries during recovery");
  }

               
     
                                                                        
                                                                         
                                                                  
     
                                                                            
                                                                          
                      
     
                                                                             
                                                                         
                                                                             
     
                                                                              
                                                                              
                                                                             
                                                                       
                                                                         
                                                                             
                                                           
     
                                                                 
                                                                   
     
                                                                            
                                                                        
                                                                            
                                                        
     
               
     
  START_CRIT_SECTION();
  if (isLogSwitch)
  {
    WALInsertLockAcquireExclusive();
  }
  else
  {
    WALInsertLockAcquire();
  }

     
                                                                           
                                                                        
                                                                            
                         
     
                                                                            
                                                                       
                
     
                                                                          
                                                                            
                                                                           
                                                                            
              
     
  if (RedoRecPtr != Insert->RedoRecPtr)
  {
    Assert(RedoRecPtr < Insert->RedoRecPtr);
    RedoRecPtr = Insert->RedoRecPtr;
  }
  doPageWrites = (Insert->fullPageWrites || Insert->forcePageWrites);

  if (doPageWrites && (!prevDoPageWrites || (fpw_lsn != InvalidXLogRecPtr && fpw_lsn <= RedoRecPtr)))
  {
       
                                                                          
                             
       
    WALInsertLockRelease();
    END_CRIT_SECTION();
    return InvalidXLogRecPtr;
  }

     
                                                                         
              
     
  if (isLogSwitch)
  {
    inserted = ReserveXLogSwitch(&StartPos, &EndPos, &rechdr->xl_prev);
  }
  else
  {
    ReserveXLogInsertLocation(rechdr->xl_tot_len, &StartPos, &EndPos, &rechdr->xl_prev);
    inserted = true;
  }

  if (inserted)
  {
       
                                                                        
               
       
    rdata_crc = rechdr->xl_crc;
    COMP_CRC32C(rdata_crc, rechdr, offsetof(XLogRecord, xl_crc));
    FIN_CRC32C(rdata_crc);
    rechdr->xl_crc = rdata_crc;

       
                                                                     
                                                        
       
    CopyXLogRecordToWAL(rechdr->xl_tot_len, isLogSwitch, rdata, StartPos, EndPos);

       
                                                                     
                                                                          
                             
       
    if ((flags & XLOG_MARK_UNIMPORTANT) == 0)
    {
      int lockno = holdingAllLocks ? 0 : MyLockNo;

      WALInsertLocks[lockno].l.lastImportantAt = StartPos;
    }
  }
  else
  {
       
                                                                           
                                                                           
                       
       
  }

     
                                                
     
  WALInsertLockRelease();

  MarkCurrentTransactionIdLoggedIfAny();

  END_CRIT_SECTION();

     
                                                                  
     
  if (StartPos / XLOG_BLCKSZ != EndPos / XLOG_BLCKSZ)
  {
    SpinLockAcquire(&XLogCtl->info_lck);
                                                        
    if (XLogCtl->LogwrtRqst.Write < EndPos)
    {
      XLogCtl->LogwrtRqst.Write = EndPos;
    }
                                                          
    LogwrtResult = XLogCtl->LogwrtResult;
    SpinLockRelease(&XLogCtl->info_lck);
  }

     
                                                                       
                                                                   
                                                      
     
  if (isLogSwitch)
  {
    TRACE_POSTGRESQL_WAL_SWITCH();
    XLogFlush(EndPos);

       
                                                                        
                                                                       
                           
       
    if (inserted)
    {
      EndPos = StartPos + SizeOfXLogRecord;
      if (StartPos / XLOG_BLCKSZ != EndPos / XLOG_BLCKSZ)
      {
        uint64 offset = XLogSegmentOffset(EndPos, wal_segment_size);

        if (offset == EndPos % XLOG_BLCKSZ)
        {
          EndPos += SizeOfXLogLongPHD;
        }
        else
        {
          EndPos += SizeOfXLogShortPHD;
        }
      }
    }
  }

#ifdef WAL_DEBUG
  if (XLOG_DEBUG)
  {
    static XLogReaderState *debug_reader = NULL;
    StringInfoData buf;
    StringInfoData recordBuf;
    char *errormsg = NULL;
    MemoryContext oldCxt;

    oldCxt = MemoryContextSwitchTo(walDebugCxt);

    initStringInfo(&buf);
    appendStringInfo(&buf, "INSERT @ %X/%X: ", (uint32)(EndPos >> 32), (uint32)EndPos);

       
                                                                          
                                                                      
                         
       
    initStringInfo(&recordBuf);
    for (; rdata != NULL; rdata = rdata->next)
    {
      appendBinaryStringInfo(&recordBuf, rdata->data, rdata->len);
    }

    if (!debug_reader)
    {
      debug_reader = XLogReaderAllocate(wal_segment_size, NULL, NULL);
    }

    if (!debug_reader)
    {
      appendStringInfoString(&buf, "error decoding record: out of memory");
    }
    else if (!DecodeXLogRecord(debug_reader, (XLogRecord *)recordBuf.data, &errormsg))
    {
      appendStringInfo(&buf, "error decoding record: %s", errormsg ? errormsg : "no error message");
    }
    else
    {
      appendStringInfoString(&buf, " - ");
      xlog_outdesc(&buf, debug_reader);
    }
    elog(LOG, "%s", buf.data);

    pfree(buf.data);
    pfree(recordBuf.data);
    MemoryContextSwitchTo(oldCxt);
  }
#endif

     
                                 
     
  ProcLastRecPtr = StartPos;
  XactLastRecEnd = EndPos;

  return EndPos;
}

   
                                                                               
                                                                         
                                                                             
                                           
   
                                                                               
                                                                             
                                                                             
                
   
                                                                              
                                                            
   
static void
ReserveXLogInsertLocation(int size, XLogRecPtr *StartPos, XLogRecPtr *EndPos, XLogRecPtr *PrevPtr)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  uint64 startbytepos;
  uint64 endbytepos;
  uint64 prevbytepos;

  size = MAXALIGN(size);

                                                          
  Assert(size > SizeOfXLogRecord);

     
                                                                           
                                                                       
                                                                            
                                                                          
                                                                            
                                                                        
                                                                             
                                                                 
     
  SpinLockAcquire(&Insert->insertpos_lck);

  startbytepos = Insert->CurrBytePos;
  endbytepos = startbytepos + size;
  prevbytepos = Insert->PrevBytePos;
  Insert->CurrBytePos = endbytepos;
  Insert->PrevBytePos = startbytepos;

  SpinLockRelease(&Insert->insertpos_lck);

  *StartPos = XLogBytePosToRecPtr(startbytepos);
  *EndPos = XLogBytePosToEndRecPtr(endbytepos);
  *PrevPtr = XLogBytePosToRecPtr(prevbytepos);

     
                                                                    
                                                       
     
  Assert(XLogRecPtrToBytePos(*StartPos) == startbytepos);
  Assert(XLogRecPtrToBytePos(*EndPos) == endbytepos);
  Assert(XLogRecPtrToBytePos(*PrevPtr) == prevbytepos);
}

   
                                                                    
   
                                                                        
                                                                             
                                                                             
                                                                          
                                                        
   
static bool
ReserveXLogSwitch(XLogRecPtr *StartPos, XLogRecPtr *EndPos, XLogRecPtr *PrevPtr)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  uint64 startbytepos;
  uint64 endbytepos;
  uint64 prevbytepos;
  uint32 size = MAXALIGN(SizeOfXLogRecord);
  XLogRecPtr ptr;
  uint32 segleft;

     
                                                                          
                                                                          
                                                                         
                                                            
     
  SpinLockAcquire(&Insert->insertpos_lck);

  startbytepos = Insert->CurrBytePos;

  ptr = XLogBytePosToEndRecPtr(startbytepos);
  if (XLogSegmentOffset(ptr, wal_segment_size) == 0)
  {
    SpinLockRelease(&Insert->insertpos_lck);
    *EndPos = *StartPos = ptr;
    return false;
  }

  endbytepos = startbytepos + size;
  prevbytepos = Insert->PrevBytePos;

  *StartPos = XLogBytePosToRecPtr(startbytepos);
  *EndPos = XLogBytePosToEndRecPtr(endbytepos);

  segleft = wal_segment_size - XLogSegmentOffset(*EndPos, wal_segment_size);
  if (segleft != wal_segment_size)
  {
                                         
    *EndPos += segleft;
    endbytepos = XLogRecPtrToBytePos(*EndPos);
  }
  Insert->CurrBytePos = endbytepos;
  Insert->PrevBytePos = startbytepos;

  SpinLockRelease(&Insert->insertpos_lck);

  *PrevPtr = XLogBytePosToRecPtr(prevbytepos);

  Assert(XLogSegmentOffset(*EndPos, wal_segment_size) == 0);
  Assert(XLogRecPtrToBytePos(*EndPos) == endbytepos);
  Assert(XLogRecPtrToBytePos(*StartPos) == startbytepos);
  Assert(XLogRecPtrToBytePos(*PrevPtr) == prevbytepos);

  return true;
}

   
                                                                        
                                                                       
                                                                               
                                                                       
                                                                      
                 
   
static void
checkXLogConsistency(XLogReaderState *record)
{
  RmgrId rmid = XLogRecGetRmid(record);
  RelFileNode rnode;
  ForkNumber forknum;
  BlockNumber blkno;
  int block_id;

                                                                          
  if (!XLogRecHasAnyBlockRefs(record))
  {
    return;
  }

  Assert((XLogRecGetInfo(record) & XLR_CHECK_CONSISTENCY) != 0);

  for (block_id = 0; block_id <= record->max_block_id; block_id++)
  {
    Buffer buf;
    Page page;

    if (!XLogRecGetBlockTag(record, block_id, &rnode, &forknum, &blkno))
    {
         
                                                                         
                     
         
      continue;
    }

    Assert(XLogRecHasBlockImage(record, block_id));

    if (XLogRecBlockImageApply(record, block_id))
    {
         
                                                                
                                                                      
                                                
         
      continue;
    }

       
                                                                   
                       
       
    buf = XLogReadBufferExtended(rnode, forknum, blkno, RBM_NORMAL_NO_LOG);
    if (!BufferIsValid(buf))
    {
      continue;
    }

    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
    page = BufferGetPage(buf);

       
                                                                          
                                            
       
    memcpy(replay_image_masked, page, BLCKSZ);

                                                              
    UnlockReleaseBuffer(buf);

       
                                                                      
                                                                 
                  
       
    if (PageGetLSN(replay_image_masked) > record->EndRecPtr)
    {
      continue;
    }

       
                                                                        
                                                                        
                                                                         
                                      
       
    if (!RestoreBlockImage(record, block_id, master_image_masked))
    {
      elog(ERROR, "failed to restore block image");
    }

       
                                                                       
              
       
    if (RmgrTable[rmid].rm_mask != NULL)
    {
      RmgrTable[rmid].rm_mask(replay_image_masked, blkno);
      RmgrTable[rmid].rm_mask(master_image_masked, blkno);
    }

                                                       
    if (memcmp(replay_image_masked, master_image_masked, BLCKSZ) != 0)
    {
      elog(FATAL, "inconsistent page found, rel %u/%u/%u, forknum %u, blkno %u", rnode.spcNode, rnode.dbNode, rnode.relNode, forknum, blkno);
    }
  }
}

   
                                                                               
                    
   
static void
CopyXLogRecordToWAL(int write_len, bool isLogSwitch, XLogRecData *rdata, XLogRecPtr StartPos, XLogRecPtr EndPos)
{
  char *currpos;
  int freespace;
  int written;
  XLogRecPtr CurrPos;
  XLogPageHeader pagehdr;

     
                                                                       
                   
     
  CurrPos = StartPos;
  currpos = GetXLogBuffer(CurrPos);
  freespace = INSERT_FREESPACE(CurrPos);

     
                                                                            
                   
     
  Assert(freespace >= sizeof(uint32));

                        
  written = 0;
  while (rdata != NULL)
  {
    char *rdata_data = rdata->data;
    int rdata_len = rdata->len;

    while (rdata_len > freespace)
    {
         
                                                                      
         
      Assert(CurrPos % XLOG_BLCKSZ >= SizeOfXLogShortPHD || freespace == 0);
      memcpy(currpos, rdata_data, freespace);
      rdata_data += freespace;
      rdata_len -= freespace;
      written += freespace;
      CurrPos += freespace;

         
                                                                        
                                                          
         
                                                                        
                                                                         
                                                                       
                                                             
         
      currpos = GetXLogBuffer(CurrPos);
      pagehdr = (XLogPageHeader)currpos;
      pagehdr->xlp_rem_len = write_len - written;
      pagehdr->xlp_info |= XLP_FIRST_IS_CONTRECORD;

                                     
      if (XLogSegmentOffset(CurrPos, wal_segment_size) == 0)
      {
        CurrPos += SizeOfXLogLongPHD;
        currpos += SizeOfXLogLongPHD;
      }
      else
      {
        CurrPos += SizeOfXLogShortPHD;
        currpos += SizeOfXLogShortPHD;
      }
      freespace = INSERT_FREESPACE(CurrPos);
    }

    Assert(CurrPos % XLOG_BLCKSZ >= SizeOfXLogShortPHD || rdata_len == 0);
    memcpy(currpos, rdata_data, rdata_len);
    currpos += rdata_len;
    CurrPos += rdata_len;
    freespace -= rdata_len;
    written += rdata_len;

    rdata = rdata->next;
  }
  Assert(written == write_len);

     
                                                                             
                                                                             
                                                                        
     
  if (isLogSwitch && XLogSegmentOffset(CurrPos, wal_segment_size) != 0)
  {
                                                                           
    Assert(write_len == SizeOfXLogRecord);

                                                              
    Assert(XLogSegmentOffset(EndPos, wal_segment_size) == 0);

                                                            
    CurrPos += freespace;

       
                                                                           
                                                                           
                                                                     
                                                            
       
    while (CurrPos < EndPos)
    {
         
                                                               
                                                             
                                                                         
                                                                       
                                                            
         
                                                                         
                                                                       
                                                                    
                                                                      
                                                                      
         
                                                                         
                                                                         
                                                                      
                                                                         
                                                                        
         
      currpos = GetXLogBuffer(CurrPos);
      MemSet(currpos, 0, SizeOfXLogShortPHD);

      CurrPos += XLOG_BLCKSZ;
    }
  }
  else
  {
                                                                        
    CurrPos = MAXALIGN64(CurrPos);
  }

  if (CurrPos != EndPos)
  {
    elog(PANIC, "space reserved for WAL record does not match what was written");
  }
}

   
                                                       
   
static void
WALInsertLockAcquire(void)
{
  bool immed;

     
                                                                           
                                                                             
                                                                      
                                                                          
                                                               
     
                                                                    
                                                                             
                                    
     
  static int lockToTry = -1;

  if (lockToTry == -1)
  {
    lockToTry = MyProc->pgprocno % NUM_XLOGINSERT_LOCKS;
  }
  MyLockNo = lockToTry;

     
                                                                       
                          
     
  immed = LWLockAcquire(&WALInsertLocks[MyLockNo].l.lock, LW_EXCLUSIVE);
  if (!immed)
  {
       
                                                                      
                                                                    
                                                                           
                                                                        
                                                                     
                         
       
    lockToTry = (lockToTry + 1) % NUM_XLOGINSERT_LOCKS;
  }
}

   
                                                                             
           
   
static void
WALInsertLockAcquireExclusive(void)
{
  int i;

     
                                                                     
                                                                           
                                                                         
     
  for (i = 0; i < NUM_XLOGINSERT_LOCKS - 1; i++)
  {
    LWLockAcquire(&WALInsertLocks[i].l.lock, LW_EXCLUSIVE);
    LWLockUpdateVar(&WALInsertLocks[i].l.lock, &WALInsertLocks[i].l.insertingAt, PG_UINT64_MAX);
  }
                                            
  LWLockAcquire(&WALInsertLocks[i].l.lock, LW_EXCLUSIVE);

  holdingAllLocks = true;
}

   
                                                                     
   
                                                                             
                                   
   
static void
WALInsertLockRelease(void)
{
  if (holdingAllLocks)
  {
    int i;

    for (i = 0; i < NUM_XLOGINSERT_LOCKS; i++)
    {
      LWLockReleaseClearVar(&WALInsertLocks[i].l.lock, &WALInsertLocks[i].l.insertingAt, 0);
    }

    holdingAllLocks = false;
  }
  else
  {
    LWLockReleaseClearVar(&WALInsertLocks[MyLockNo].l.lock, &WALInsertLocks[MyLockNo].l.insertingAt, 0);
  }
}

   
                                                                        
                               
   
static void
WALInsertLockUpdateInsertingAt(XLogRecPtr insertingAt)
{
  if (holdingAllLocks)
  {
       
                                                                         
                                      
       
    LWLockUpdateVar(&WALInsertLocks[NUM_XLOGINSERT_LOCKS - 1].l.lock, &WALInsertLocks[NUM_XLOGINSERT_LOCKS - 1].l.insertingAt, insertingAt);
  }
  else
  {
    LWLockUpdateVar(&WALInsertLocks[MyLockNo].l.lock, &WALInsertLocks[MyLockNo].l.insertingAt, insertingAt);
  }
}

   
                                                 
   
                                                                           
                                                                           
                                                                           
                                                                
   
                                                                          
                                                                            
                                                                        
                                                                               
                                                                    
   
static XLogRecPtr
WaitXLogInsertionsToFinish(XLogRecPtr upto)
{
  uint64 bytepos;
  XLogRecPtr reservedUpto;
  XLogRecPtr finishedUpto;
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  int i;

  if (MyProc == NULL)
  {
    elog(PANIC, "cannot wait without a PGPROC structure");
  }

                                        
  SpinLockAcquire(&Insert->insertpos_lck);
  bytepos = Insert->CurrBytePos;
  SpinLockRelease(&Insert->insertpos_lck);
  reservedUpto = XLogBytePosToEndRecPtr(bytepos);

     
                                                                         
                                                                           
                                                                       
                                                                           
                                                                       
                                                                        
     
  if (upto > reservedUpto)
  {
    elog(LOG, "request to flush past end of generated WAL; request %X/%X, currpos %X/%X", (uint32)(upto >> 32), (uint32)upto, (uint32)(reservedUpto >> 32), (uint32)reservedUpto);
    upto = reservedUpto;
  }

     
                                                                          
                  
     
                                                                           
                                                                         
                                                                          
                                                     
     
  finishedUpto = reservedUpto;
  for (i = 0; i < NUM_XLOGINSERT_LOCKS; i++)
  {
    XLogRecPtr insertingat = InvalidXLogRecPtr;

    do
    {
         
                                                                        
                                                                    
                                                                       
                                                                        
                                                                     
                                                                        
                                                                       
                                                                   
                                                                   
                   
         
      if (LWLockWaitForVar(&WALInsertLocks[i].l.lock, &WALInsertLocks[i].l.insertingAt, insertingat, &insertingat))
      {
                                                            
        insertingat = InvalidXLogRecPtr;
        break;
      }

         
                                                                       
                                             
         
    } while (insertingat < upto);

    if (insertingat != InvalidXLogRecPtr && insertingat < finishedUpto)
    {
      finishedUpto = insertingat;
    }
  }
  return finishedUpto;
}

   
                                                                        
                     
   
                                                                             
                                                                        
   
                                                                          
                                                                         
                                                                       
                                                                         
                                                                        
                                                                           
                                                                             
                                                           
   
static char *
GetXLogBuffer(XLogRecPtr ptr)
{
  int idx;
  XLogRecPtr endptr;
  static uint64 cachedPage = 0;
  static char *cachedPos = NULL;
  XLogRecPtr expectedEndPtr;

     
                                                                         
                        
     
  if (ptr / XLOG_BLCKSZ == cachedPage)
  {
    Assert(((XLogPageHeader)cachedPos)->xlp_magic == XLOG_PAGE_MAGIC);
    Assert(((XLogPageHeader)cachedPos)->xlp_pageaddr == ptr - (ptr % XLOG_BLCKSZ));
    return cachedPos + ptr % XLOG_BLCKSZ;
  }

     
                                                                             
                                                                             
                                                          
     
  idx = XLogRecPtrToBufIdx(ptr);

     
                                                                          
                                                                            
                                                                            
                                                                            
                          
     
                                                                           
                                                                           
                                                                          
                                                                            
                                                                            
                                                                            
                                                                           
                                                                            
                       
     
  expectedEndPtr = ptr;
  expectedEndPtr += XLOG_BLCKSZ - ptr % XLOG_BLCKSZ;

  endptr = XLogCtl->xlblocks[idx];
  if (expectedEndPtr != endptr)
  {
    XLogRecPtr initializedUpto;

       
                                                                           
                                                              
       
                                                                      
                                                                          
                                                                         
                                                                        
                                                                         
                                                                       
                                                                       
                                                                          
                        
       
    if (ptr % XLOG_BLCKSZ == SizeOfXLogShortPHD && XLogSegmentOffset(ptr, wal_segment_size) > XLOG_BLCKSZ)
    {
      initializedUpto = ptr - SizeOfXLogShortPHD;
    }
    else if (ptr % XLOG_BLCKSZ == SizeOfXLogLongPHD && XLogSegmentOffset(ptr, wal_segment_size) < XLOG_BLCKSZ)
    {
      initializedUpto = ptr - SizeOfXLogLongPHD;
    }
    else
    {
      initializedUpto = ptr;
    }

    WALInsertLockUpdateInsertingAt(initializedUpto);

    AdvanceXLInsertBuffer(ptr, false);
    endptr = XLogCtl->xlblocks[idx];

    if (expectedEndPtr != endptr)
    {
      elog(PANIC, "could not find WAL buffer for %X/%X", (uint32)(ptr >> 32), (uint32)ptr);
    }
  }
  else
  {
       
                                                                      
                                                                          
       
    pg_memory_barrier();
  }

     
                                                                       
                             
     
  cachedPage = ptr / XLOG_BLCKSZ;
  cachedPos = XLogCtl->pages + idx * (Size)XLOG_BLCKSZ;

  Assert(((XLogPageHeader)cachedPos)->xlp_magic == XLOG_PAGE_MAGIC);
  Assert(((XLogPageHeader)cachedPos)->xlp_pageaddr == ptr - (ptr % XLOG_BLCKSZ));

  return cachedPos + ptr % XLOG_BLCKSZ;
}

   
                                                                           
                                                                         
                 
   
static XLogRecPtr
XLogBytePosToRecPtr(uint64 bytepos)
{
  uint64 fullsegs;
  uint64 fullpages;
  uint64 bytesleft;
  uint32 seg_offset;
  XLogRecPtr result;

  fullsegs = bytepos / UsableBytesInSegment;
  bytesleft = bytepos % UsableBytesInSegment;

  if (bytesleft < XLOG_BLCKSZ - SizeOfXLogLongPHD)
  {
                                       
    seg_offset = bytesleft + SizeOfXLogLongPHD;
  }
  else
  {
                                                                
    seg_offset = XLOG_BLCKSZ;
    bytesleft -= XLOG_BLCKSZ - SizeOfXLogLongPHD;

    fullpages = bytesleft / UsableBytesInPage;
    bytesleft = bytesleft % UsableBytesInPage;

    seg_offset += fullpages * XLOG_BLCKSZ + bytesleft + SizeOfXLogShortPHD;
  }

  XLogSegNoOffsetToRecPtr(fullsegs, seg_offset, wal_segment_size, result);

  return result;
}

   
                                                                        
                                                                            
                                                                             
                                                     
   
static XLogRecPtr
XLogBytePosToEndRecPtr(uint64 bytepos)
{
  uint64 fullsegs;
  uint64 fullpages;
  uint64 bytesleft;
  uint32 seg_offset;
  XLogRecPtr result;

  fullsegs = bytepos / UsableBytesInSegment;
  bytesleft = bytepos % UsableBytesInSegment;

  if (bytesleft < XLOG_BLCKSZ - SizeOfXLogLongPHD)
  {
                                       
    if (bytesleft == 0)
    {
      seg_offset = 0;
    }
    else
    {
      seg_offset = bytesleft + SizeOfXLogLongPHD;
    }
  }
  else
  {
                                                                
    seg_offset = XLOG_BLCKSZ;
    bytesleft -= XLOG_BLCKSZ - SizeOfXLogLongPHD;

    fullpages = bytesleft / UsableBytesInPage;
    bytesleft = bytesleft % UsableBytesInPage;

    if (bytesleft == 0)
    {
      seg_offset += fullpages * XLOG_BLCKSZ + bytesleft;
    }
    else
    {
      seg_offset += fullpages * XLOG_BLCKSZ + bytesleft + SizeOfXLogShortPHD;
    }
  }

  XLogSegNoOffsetToRecPtr(fullsegs, seg_offset, wal_segment_size, result);

  return result;
}

   
                                                      
   
static uint64
XLogRecPtrToBytePos(XLogRecPtr ptr)
{
  uint64 fullsegs;
  uint32 fullpages;
  uint32 offset;
  uint64 result;

  XLByteToSeg(ptr, fullsegs, wal_segment_size);

  fullpages = (XLogSegmentOffset(ptr, wal_segment_size)) / XLOG_BLCKSZ;
  offset = ptr % XLOG_BLCKSZ;

  if (fullpages == 0)
  {
    result = fullsegs * UsableBytesInSegment;
    if (offset > 0)
    {
      Assert(offset >= SizeOfXLogLongPHD);
      result += offset - SizeOfXLogLongPHD;
    }
  }
  else
  {
    result = fullsegs * UsableBytesInSegment + (XLOG_BLCKSZ - SizeOfXLogLongPHD) +                             
             (fullpages - 1) * UsableBytesInPage;                                                  
    if (offset > 0)
    {
      Assert(offset >= SizeOfXLogShortPHD);
      result += offset - SizeOfXLogShortPHD;
    }
  }

  return result;
}

   
                                                                          
                                                                             
                                                                        
                                                                              
                         
   
static void
AdvanceXLInsertBuffer(XLogRecPtr upto, bool opportunistic)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  int nextidx;
  XLogRecPtr OldPageRqstPtr;
  XLogwrtRqst WriteRqst;
  XLogRecPtr NewPageEndPtr = InvalidXLogRecPtr;
  XLogRecPtr NewPageBeginPtr;
  XLogPageHeader NewPage;
  int npages pg_attribute_unused() = 0;

  LWLockAcquire(WALBufMappingLock, LW_EXCLUSIVE);

     
                                                                      
              
     
  while (upto >= XLogCtl->InitializedUpTo || opportunistic)
  {
    nextidx = XLogRecPtrToBufIdx(XLogCtl->InitializedUpTo);

       
                                                                         
                                                                          
                            
       
    OldPageRqstPtr = XLogCtl->xlblocks[nextidx];
    if (LogwrtResult.Write < OldPageRqstPtr)
    {
         
                                                                         
                                                  
         
      if (opportunistic)
      {
        break;
      }

                                                                
      SpinLockAcquire(&XLogCtl->info_lck);
      if (XLogCtl->LogwrtRqst.Write < OldPageRqstPtr)
      {
        XLogCtl->LogwrtRqst.Write = OldPageRqstPtr;
      }
      LogwrtResult = XLogCtl->LogwrtResult;
      SpinLockRelease(&XLogCtl->info_lck);

         
                                                                      
                                                                
         
      if (LogwrtResult.Write < OldPageRqstPtr)
      {
           
                                                                     
                                                                     
                                                                    
                     
           
        LWLockRelease(WALBufMappingLock);

        WaitXLogInsertionsToFinish(OldPageRqstPtr);

        LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);

        LogwrtResult = XLogCtl->LogwrtResult;
        if (LogwrtResult.Write >= OldPageRqstPtr)
        {
                                            
          LWLockRelease(WALWriteLock);
        }
        else
        {
                                          
          TRACE_POSTGRESQL_WAL_BUFFER_WRITE_DIRTY_START();
          WriteRqst.Write = OldPageRqstPtr;
          WriteRqst.Flush = 0;
          XLogWrite(WriteRqst, false);
          LWLockRelease(WALWriteLock);
          TRACE_POSTGRESQL_WAL_BUFFER_WRITE_DIRTY_DONE();
        }
                                                    
        LWLockAcquire(WALBufMappingLock, LW_EXCLUSIVE);
        continue;
      }
    }

       
                                                                       
                         
       
    NewPageBeginPtr = XLogCtl->InitializedUpTo;
    NewPageEndPtr = NewPageBeginPtr + XLOG_BLCKSZ;

    Assert(XLogRecPtrToBufIdx(NewPageBeginPtr) == nextidx);

    NewPage = (XLogPageHeader)(XLogCtl->pages + nextidx * (Size)XLOG_BLCKSZ);

       
                                                                     
                                                                   
       
    MemSet((char *)NewPage, 0, XLOG_BLCKSZ);

       
                                  
       
    NewPage->xlp_magic = XLOG_PAGE_MAGIC;

                                 /* done by memset */
    NewPage->xlp_tli = ThisTimeLineID;
    NewPage->xlp_pageaddr = NewPageBeginPtr;

                                    /* done by memset */

       
                                                                        
                                                                     
                                                                           
                                                                          
                                                                          
                                                                    
                                                                          
                                                                          
                                                                         
                                                                          
                               
       
    if (!Insert->forcePageWrites)
    {
      NewPage->xlp_info |= XLP_BKP_REMOVABLE;
    }

       
                                                                      
                                                                       
                                                                        
                                                       
       
    if (missingContrecPtr == NewPageBeginPtr)
    {
      NewPage->xlp_info |= XLP_FIRST_IS_OVERWRITE_CONTRECORD;
      missingContrecPtr = InvalidXLogRecPtr;
    }

       
                                                                     
       
    if ((XLogSegmentOffset(NewPage->xlp_pageaddr, wal_segment_size)) == 0)
    {
      XLogLongPageHeader NewLongPage = (XLogLongPageHeader)NewPage;

      NewLongPage->xlp_sysid = ControlFile->system_identifier;
      NewLongPage->xlp_seg_size = wal_segment_size;
      NewLongPage->xlp_xlog_blcksz = XLOG_BLCKSZ;
      NewPage->xlp_info |= XLP_LONG_HEADER;
    }

       
                                                                          
                                                                          
                       
       
    pg_write_barrier();

    *((volatile XLogRecPtr *)&XLogCtl->xlblocks[nextidx]) = NewPageEndPtr;

    XLogCtl->InitializedUpTo = NewPageEndPtr;

    npages++;
  }
  LWLockRelease(WALBufMappingLock);

#ifdef WAL_DEBUG
  if (XLOG_DEBUG && npages > 0)
  {
    elog(DEBUG1, "initialized %d pages, up to %X/%X", npages, (uint32)(NewPageEndPtr >> 32), (uint32)NewPageEndPtr);
  }
#endif
}

   
                                                             
                                 
   
static void
CalculateCheckpointSegments(void)
{
  double target;

            
                                                                       
                                                                  
     
                                                                         
                                                                      
                                                                       
                                                                         
                                                                       
                                 
                                                                     
                                                        
            
     
  target = (double)ConvertToXSegs(max_wal_size_mb, wal_segment_size) / (1.0 + CheckPointCompletionTarget);

                  
  CheckPointSegments = (int)target;

  if (CheckPointSegments < 1)
  {
    CheckPointSegments = 1;
  }
}

void
assign_max_wal_size(int newval, void *extra)
{
  max_wal_size_mb = newval;
  CalculateCheckpointSegments();
}

void
assign_checkpoint_completion_target(double newval, void *extra)
{
  CheckPointCompletionTarget = newval;
  CalculateCheckpointSegments();
}

   
                                                                            
                                                                           
   
static XLogSegNo
XLOGfileslop(XLogRecPtr lastredoptr)
{
  XLogSegNo minSegNo;
  XLogSegNo maxSegNo;
  double distance;
  XLogSegNo recycleSegNo;

     
                                                                            
                                                                            
                                                       
     
  minSegNo = lastredoptr / wal_segment_size + ConvertToXSegs(min_wal_size_mb, wal_segment_size) - 1;
  maxSegNo = lastredoptr / wal_segment_size + ConvertToXSegs(max_wal_size_mb, wal_segment_size) - 1;

     
                                                                            
                                       
     
                                                                        
                                                                             
                       
     
  distance = (1.0 + CheckPointCompletionTarget) * CheckPointDistanceEstimate;
                                 
  distance *= 1.10;

  recycleSegNo = (XLogSegNo)ceil(((double)lastredoptr + distance) / wal_segment_size);

  if (recycleSegNo < minSegNo)
  {
    recycleSegNo = minSegNo;
  }
  if (recycleSegNo > maxSegNo)
  {
    recycleSegNo = maxSegNo;
  }

  return recycleSegNo;
}

   
                                                                               
   
                                                                        
                                                                          
                                               
   
                                                                      
   
static bool
XLogCheckpointNeeded(XLogSegNo new_segno)
{
  XLogSegNo old_segno;

  XLByteToSeg(RedoRecPtr, old_segno, wal_segment_size);

  if (new_segno >= old_segno + (uint64)(CheckPointSegments - 1))
  {
    return true;
  }
  return false;
}

   
                                                                      
   
                                                                        
                                                                              
                                                                           
                        
   
                                                                                
                                                                              
          
   
static void
XLogWrite(XLogwrtRqst WriteRqst, bool flexible)
{
  bool ispartialpage;
  bool last_iteration;
  bool finishing_seg;
  bool use_existent;
  int curridx;
  int npages;
  int startidx;
  uint32 startoffset;

                                                          
  Assert(CritSectionCount > 0);

     
                                                                          
     
  LogwrtResult = XLogCtl->LogwrtResult;

     
                                                                           
                                                                      
                                                                            
                                                                           
                                                                          
                                                                          
                                             
     
  npages = 0;
  startidx = 0;
  startoffset = 0;

     
                                                                      
                                                                          
                                           
     
  curridx = XLogRecPtrToBufIdx(LogwrtResult.Write);

  while (LogwrtResult.Write < WriteRqst.Write)
  {
       
                                                                           
                                                                           
                                                                   
       
    XLogRecPtr EndPtr = XLogCtl->xlblocks[curridx];

    if (LogwrtResult.Write >= EndPtr)
    {
      elog(PANIC, "xlog write request %X/%X is past end of log %X/%X", (uint32)(LogwrtResult.Write >> 32), (uint32)LogwrtResult.Write, (uint32)(EndPtr >> 32), (uint32)EndPtr);
    }

                                                                  
    LogwrtResult.Write = EndPtr;
    ispartialpage = WriteRqst.Write < LogwrtResult.Write;

    if (!XLByteInPrevSeg(LogwrtResult.Write, openLogSegNo, wal_segment_size))
    {
         
                                                                    
                                                                 
         
      Assert(npages == 0);
      if (openLogFile >= 0)
      {
        XLogFileClose();
      }
      XLByteToPrevSeg(LogwrtResult.Write, openLogSegNo, wal_segment_size);

                                   
      use_existent = true;
      openLogFile = XLogFileInit(openLogSegNo, &use_existent, true);
    }

                                                    
    if (openLogFile < 0)
    {
      XLByteToPrevSeg(LogwrtResult.Write, openLogSegNo, wal_segment_size);
      openLogFile = XLogFileOpen(openLogSegNo);
    }

                                                              
    if (npages == 0)
    {
                          
      startidx = curridx;
      startoffset = XLogSegmentOffset(LogwrtResult.Write - XLOG_BLCKSZ, wal_segment_size);
    }
    npages++;

       
                                                                          
                                                                        
                                                                     
                
       
    last_iteration = WriteRqst.Write <= LogwrtResult.Write;

    finishing_seg = !ispartialpage && (startoffset + npages * XLOG_BLCKSZ) >= wal_segment_size;

    if (last_iteration || curridx == XLogCtl->XLogCacheBlck || finishing_seg)
    {
      char *from;
      Size nbytes;
      Size nleft;
      int written;

                                   
      from = XLogCtl->pages + startidx * (Size)XLOG_BLCKSZ;
      nbytes = npages * (Size)XLOG_BLCKSZ;
      nleft = nbytes;
      do
      {
        errno = 0;
        pgstat_report_wait_start(WAIT_EVENT_WAL_WRITE);
        written = pg_pwrite(openLogFile, from, nleft, startoffset);
        pgstat_report_wait_end();
        if (written <= 0)
        {
          if (errno == EINTR)
          {
            continue;
          }
          ereport(PANIC, (errcode_for_file_access(), errmsg("could not write to log file %s "
                                                            "at offset %u, length %zu: %m",
                                                         XLogFileNameP(ThisTimeLineID, openLogSegNo), startoffset, nleft)));
        }
        nleft -= written;
        from += written;
        startoffset += written;
      } while (nleft > 0);

      npages = 0;

         
                                                                    
                                                                       
                                                                      
                                                                         
                             
         
                                                                      
                                                                         
                                                                      
                                                                 
                     
         
      if (finishing_seg)
      {
        issue_xlog_fsync(openLogFile, openLogSegNo);

                                                            
        WalSndWakeupRequest();

        LogwrtResult.Flush = LogwrtResult.Write;                  

        if (XLogArchivingActive())
        {
          XLogArchiveNotifySeg(openLogSegNo);
        }

        XLogCtl->lastSegSwitchTime = (pg_time_t)time(NULL);
        XLogCtl->lastSegSwitchLSN = LogwrtResult.Flush;

           
                                                                      
                                                                    
                                                                       
                                                                       
                    
           
        if (IsUnderPostmaster && XLogCheckpointNeeded(openLogSegNo))
        {
          (void)GetRedoRecPtr();
          if (XLogCheckpointNeeded(openLogSegNo))
          {
            RequestCheckpoint(CHECKPOINT_CAUSE_XLOG);
          }
        }
      }
    }

    if (ispartialpage)
    {
                                              
      LogwrtResult.Write = WriteRqst.Write;
      break;
    }
    curridx = NextBufIdx(curridx);

                                                                      
    if (flexible && npages == 0)
    {
      break;
    }
  }

  Assert(npages == 0);

     
                              
     
  if (LogwrtResult.Flush < WriteRqst.Flush && LogwrtResult.Flush < LogwrtResult.Write)

  {
       
                                                                           
                                                                       
                                 
       
    if (sync_method != SYNC_METHOD_OPEN && sync_method != SYNC_METHOD_OPEN_DSYNC)
    {
      if (openLogFile >= 0 && !XLByteInPrevSeg(LogwrtResult.Write, openLogSegNo, wal_segment_size))
      {
        XLogFileClose();
      }
      if (openLogFile < 0)
      {
        XLByteToPrevSeg(LogwrtResult.Write, openLogSegNo, wal_segment_size);
        openLogFile = XLogFileOpen(openLogSegNo);
      }

      issue_xlog_fsync(openLogFile, openLogSegNo);
    }

                                                        
    WalSndWakeupRequest();

    LogwrtResult.Flush = LogwrtResult.Write;
  }

     
                                 
     
                                                                          
                                                                           
                                 
     
  {
    SpinLockAcquire(&XLogCtl->info_lck);
    XLogCtl->LogwrtResult = LogwrtResult;
    if (XLogCtl->LogwrtRqst.Write < LogwrtResult.Write)
    {
      XLogCtl->LogwrtRqst.Write = LogwrtResult.Write;
    }
    if (XLogCtl->LogwrtRqst.Flush < LogwrtResult.Flush)
    {
      XLogCtl->LogwrtRqst.Flush = LogwrtResult.Flush;
    }
    SpinLockRelease(&XLogCtl->info_lck);
  }
}

   
                                                               
                                                          
                                                        
   
void
XLogSetAsyncXactLSN(XLogRecPtr asyncXactLSN)
{
  XLogRecPtr WriteRqstPtr = asyncXactLSN;
  bool sleeping;

  SpinLockAcquire(&XLogCtl->info_lck);
  LogwrtResult = XLogCtl->LogwrtResult;
  sleeping = XLogCtl->WalWriterSleeping;
  if (XLogCtl->asyncXactLSN < asyncXactLSN)
  {
    XLogCtl->asyncXactLSN = asyncXactLSN;
  }
  SpinLockRelease(&XLogCtl->info_lck);

     
                                                                            
                                                                          
                             
     
  if (!sleeping)
  {
                                                  
    WriteRqstPtr -= WriteRqstPtr % XLOG_BLCKSZ;

                                                         
    if (WriteRqstPtr <= LogwrtResult.Flush)
    {
      return;
    }
  }

     
                                                                            
                                                                             
                                         
     
  if (ProcGlobal->walwriterLatch)
  {
    SetLatch(ProcGlobal->walwriterLatch);
  }
}

   
                                                                             
                         
   
void
XLogSetReplicationSlotMinimumLSN(XLogRecPtr lsn)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->replicationSlotMinLSN = lsn;
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                                                     
                     
   
static XLogRecPtr
XLogGetReplicationSlotMinimumLSN(void)
{
  XLogRecPtr retval;

  SpinLockAcquire(&XLogCtl->info_lck);
  retval = XLogCtl->replicationSlotMinLSN;
  SpinLockRelease(&XLogCtl->info_lck);

  return retval;
}

   
                                             
   
                                                                          
                           
   
                                                                              
                                                                       
   
static void
UpdateMinRecoveryPoint(XLogRecPtr lsn, bool force)
{
                                                        
  if (!updateMinRecoveryPoint || (!force && lsn <= minRecoveryPoint))
  {
    return;
  }

     
                                                                            
                                                                           
                                                                             
                                                                            
                                                                             
                                                                           
                                                                    
                                                                           
                                                         
     
  if (XLogRecPtrIsInvalid(minRecoveryPoint) && InRecovery)
  {
    updateMinRecoveryPoint = false;
    return;
  }

  LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

                         
  minRecoveryPoint = ControlFile->minRecoveryPoint;
  minRecoveryPointTLI = ControlFile->minRecoveryPointTLI;

  if (XLogRecPtrIsInvalid(minRecoveryPoint))
  {
    updateMinRecoveryPoint = false;
  }
  else if (force || minRecoveryPoint < lsn)
  {
    XLogRecPtr newMinRecoveryPoint;
    TimeLineID newMinRecoveryPointTLI;

       
                                                                          
                                                                        
                                                                         
                                        
       
                                                                         
                                                                           
                                                                       
                                                                          
                                                                        
                                                                
       
    SpinLockAcquire(&XLogCtl->info_lck);
    newMinRecoveryPoint = XLogCtl->replayEndRecPtr;
    newMinRecoveryPointTLI = XLogCtl->replayEndTLI;
    SpinLockRelease(&XLogCtl->info_lck);

    if (!force && newMinRecoveryPoint < lsn)
    {
      elog(WARNING, "xlog min recovery request %X/%X is past current point %X/%X", (uint32)(lsn >> 32), (uint32)lsn, (uint32)(newMinRecoveryPoint >> 32), (uint32)newMinRecoveryPoint);
    }

                             
    if (ControlFile->minRecoveryPoint < newMinRecoveryPoint)
    {
      ControlFile->minRecoveryPoint = newMinRecoveryPoint;
      ControlFile->minRecoveryPointTLI = newMinRecoveryPointTLI;
      UpdateControlFile();
      minRecoveryPoint = newMinRecoveryPoint;
      minRecoveryPointTLI = newMinRecoveryPointTLI;

      ereport(DEBUG2, (errmsg("updated min recovery point to %X/%X on timeline %u", (uint32)(minRecoveryPoint >> 32), (uint32)minRecoveryPoint, newMinRecoveryPointTLI)));
    }
  }
  LWLockRelease(ControlFileLock);
}

   
                                                                            
   
                                                                            
                                                               
   
void
XLogFlush(XLogRecPtr record)
{
  XLogRecPtr WriteRqstPtr;
  XLogwrtRqst WriteRqst;

     
                                                                         
                                                                            
                                                                            
                                                                 
                                                         
     
  if (!XLogInsertAllowed())
  {
    UpdateMinRecoveryPoint(record, false);
    return;
  }

                                           
  if (record <= LogwrtResult.Flush)
  {
    return;
  }

#ifdef WAL_DEBUG
  if (XLOG_DEBUG)
  {
    elog(LOG, "xlog flush request %X/%X; write %X/%X; flush %X/%X", (uint32)(record >> 32), (uint32)record, (uint32)(LogwrtResult.Write >> 32), (uint32)LogwrtResult.Write, (uint32)(LogwrtResult.Flush >> 32), (uint32)LogwrtResult.Flush);
  }
#endif

  START_CRIT_SECTION();

     
                                                                      
                                                                             
                                                                           
                                                                         
                                                                       
     

                                                      
  WriteRqstPtr = record;

     
                                                                          
             
     
  for (;;)
  {
    XLogRecPtr insertpos;

                                                  
    SpinLockAcquire(&XLogCtl->info_lck);
    if (WriteRqstPtr < XLogCtl->LogwrtRqst.Write)
    {
      WriteRqstPtr = XLogCtl->LogwrtRqst.Write;
    }
    LogwrtResult = XLogCtl->LogwrtResult;
    SpinLockRelease(&XLogCtl->info_lck);

                       
    if (record <= LogwrtResult.Flush)
    {
      break;
    }

       
                                                                    
                                                               
       
    insertpos = WaitXLogInsertionsToFinish(WriteRqstPtr);

       
                                                                       
                                                                         
                                                                        
                                                                         
                                                 
       
    if (!LWLockAcquireOrWait(WALWriteLock, LW_EXCLUSIVE))
    {
         
                                                                       
                                                                       
                     
         
      continue;
    }

                                                            
    LogwrtResult = XLogCtl->LogwrtResult;
    if (record <= LogwrtResult.Flush)
    {
      LWLockRelease(WALWriteLock);
      break;
    }

       
                                                                       
                                                                    
                                                                         
                                                      
       
                                                                         
                                                                          
       
    if (CommitDelay > 0 && enableFsync && MinimumActiveBackends(CommitSiblings))
    {
      pg_usleep(CommitDelay);

         
                                                                       
                                                               
                                                                      
                                                                       
                                                                       
                                                                        
                                                                    
                                                           
         
      insertpos = WaitXLogInsertionsToFinish(insertpos);
    }

                                                            
    WriteRqst.Write = insertpos;
    WriteRqst.Flush = insertpos;

    XLogWrite(WriteRqst, false);

    LWLockRelease(WALWriteLock);
              
    break;
  }

  END_CRIT_SECTION();

                                                                          
  WalSndWakeupProcessRequests();

     
                                                                     
                                                                          
                                                                       
     
                                                                       
                                                                             
                                                                             
                                                                        
                                                                     
                                                                            
                                                                            
                                                                           
                                                                           
              
     
                                                                          
                                                                          
                                                                             
                                                   
     
  if (LogwrtResult.Flush < record)
  {
    elog(ERROR, "xlog flush request %X/%X is not satisfied --- flushed only to %X/%X", (uint32)(record >> 32), (uint32)record, (uint32)(LogwrtResult.Flush >> 32), (uint32)LogwrtResult.Flush);
  }
}

   
                                                                
   
                                                                             
                                                                              
                                                                             
                                                           
   
                                                                               
                                                                     
                                                                             
                                                      
   
                                                                      
                                                                               
                                                                           
                                                                              
                                                                      
   
                                                                             
   
                                                                             
                                               
   
bool
XLogBackgroundFlush(void)
{
  XLogwrtRqst WriteRqst;
  bool flexible = true;
  static TimestampTz lastflush;
  TimestampTz now;
  int flushbytes;

                                                  
  if (RecoveryInProgress())
  {
    return false;
  }

                                                
  SpinLockAcquire(&XLogCtl->info_lck);
  LogwrtResult = XLogCtl->LogwrtResult;
  WriteRqst = XLogCtl->LogwrtRqst;
  SpinLockRelease(&XLogCtl->info_lck);

                                                
  WriteRqst.Write -= WriteRqst.Write % XLOG_BLCKSZ;

                                                                          
  if (WriteRqst.Write <= LogwrtResult.Flush)
  {
    SpinLockAcquire(&XLogCtl->info_lck);
    WriteRqst.Write = XLogCtl->asyncXactLSN;
    SpinLockRelease(&XLogCtl->info_lck);
    flexible = false;                                 
  }

     
                                                                        
                                                                       
                                             
     
  if (WriteRqst.Write <= LogwrtResult.Flush)
  {
    if (openLogFile >= 0)
    {
      if (!XLByteInPrevSeg(LogwrtResult.Write, openLogSegNo, wal_segment_size))
      {
        XLogFileClose();
      }
    }
    return false;
  }

     
                                                                       
                                  
     
  now = GetCurrentTimestamp();
  flushbytes = WriteRqst.Write / XLOG_BLCKSZ - LogwrtResult.Flush / XLOG_BLCKSZ;

  if (WalWriterFlushAfter == 0 || lastflush == 0)
  {
                                                    
    WriteRqst.Flush = WriteRqst.Write;
    lastflush = now;
  }
  else if (TimestampDifferenceExceeds(lastflush, now, WalWriterDelay))
  {
       
                                                                           
                                                                          
                 
       
    WriteRqst.Flush = WriteRqst.Write;
    lastflush = now;
  }
  else if (flushbytes >= WalWriterFlushAfter)
  {
                                                       
    WriteRqst.Flush = WriteRqst.Write;
    lastflush = now;
  }
  else
  {
                                      
    WriteRqst.Flush = 0;
  }

#ifdef WAL_DEBUG
  if (XLOG_DEBUG)
  {
    elog(LOG, "xlog bg flush request write %X/%X; flush: %X/%X, current is write %X/%X; flush %X/%X", (uint32)(WriteRqst.Write >> 32), (uint32)WriteRqst.Write, (uint32)(WriteRqst.Flush >> 32), (uint32)WriteRqst.Flush, (uint32)(LogwrtResult.Write >> 32), (uint32)LogwrtResult.Write, (uint32)(LogwrtResult.Flush >> 32), (uint32)LogwrtResult.Flush);
  }
#endif

  START_CRIT_SECTION();

                                                                            
  WaitXLogInsertionsToFinish(WriteRqst.Write);
  LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);
  LogwrtResult = XLogCtl->LogwrtResult;
  if (WriteRqst.Write > LogwrtResult.Write || WriteRqst.Flush > LogwrtResult.Flush)
  {
    XLogWrite(WriteRqst, flexible);
  }
  LWLockRelease(WALWriteLock);

  END_CRIT_SECTION();

                                                                          
  WalSndWakeupProcessRequests();

     
                                                                             
                                                                           
     
  AdvanceXLInsertBuffer(InvalidXLogRecPtr, true);

     
                                                                    
                                                                        
                                  
     
  return true;
}

   
                                                                                
   
                                                                          
                                                         
   
bool
XLogNeedsFlush(XLogRecPtr record)
{
     
                                                                     
                                                                         
                               
     
  if (RecoveryInProgress())
  {
       
                                                                         
                                                                           
                                                                        
                                                                           
                                                                         
                                                                        
       
    if (XLogRecPtrIsInvalid(minRecoveryPoint) && InRecovery)
    {
      updateMinRecoveryPoint = false;
    }

                                                                        
    if (record <= minRecoveryPoint || !updateMinRecoveryPoint)
    {
      return false;
    }

       
                                                                       
                                         
       
    if (!LWLockConditionalAcquire(ControlFileLock, LW_SHARED))
    {
      return true;
    }
    minRecoveryPoint = ControlFile->minRecoveryPoint;
    minRecoveryPointTLI = ControlFile->minRecoveryPointTLI;
    LWLockRelease(ControlFileLock);

       
                                                                     
                                                                         
                                                      
       
    if (XLogRecPtrIsInvalid(minRecoveryPoint))
    {
      updateMinRecoveryPoint = false;
    }

                     
    if (record <= minRecoveryPoint || !updateMinRecoveryPoint)
    {
      return false;
    }
    else
    {
      return true;
    }
  }

                                           
  if (record <= LogwrtResult.Flush)
  {
    return false;
  }

                                                
  SpinLockAcquire(&XLogCtl->info_lck);
  LogwrtResult = XLogCtl->LogwrtResult;
  SpinLockRelease(&XLogCtl->info_lck);

                   
  if (record <= LogwrtResult.Flush)
  {
    return false;
  }

  return true;
}

   
                                                               
   
                                                    
   
                                                                    
                                                                          
                  
   
                                                                     
                                                                          
                                            
   
                              
   
                                                                          
                                                                          
                                                                           
                          
   
int
XLogFileInit(XLogSegNo logsegno, bool *use_existent, bool use_lock)
{
  char path[MAXPGPATH];
  char tmppath[MAXPGPATH];
  PGAlignedXLogBlock zbuffer;
  XLogSegNo installed_segno;
  XLogSegNo max_segno;
  int fd;
  int nbytes;
  int save_errno;

  XLogFilePath(path, ThisTimeLineID, logsegno, wal_segment_size);

     
                                                                             
     
  if (*use_existent)
  {
    fd = BasicOpenFile(path, O_RDWR | PG_BINARY | get_sync_bit(sync_method));
    if (fd < 0)
    {
      if (errno != ENOENT)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
      }
    }
    else
    {
      return fd;
    }
  }

     
                                                                          
                                                                     
                                                                        
                                                       
     
  elog(DEBUG2, "creating and filling new WAL file");

  snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int)getpid());

  unlink(tmppath);

                                                                            
  fd = BasicOpenFile(tmppath, O_RDWR | O_CREAT | O_EXCL | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmppath)));
  }

  memset(zbuffer.data, 0, XLOG_BLCKSZ);

  pgstat_report_wait_start(WAIT_EVENT_WAL_INIT_WRITE);
  save_errno = 0;
  if (wal_init_zero)
  {
       
                                                                          
                                                                     
                                                                      
                                                                       
                                                                   
                                                                     
                                                                         
       
    for (nbytes = 0; nbytes < wal_segment_size; nbytes += XLOG_BLCKSZ)
    {
      errno = 0;
      if (write(fd, zbuffer.data, XLOG_BLCKSZ) != XLOG_BLCKSZ)
      {
                                                             
        save_errno = errno ? errno : ENOSPC;
        break;
      }
    }
  }
  else
  {
       
                                                                    
               
       
    errno = 0;
    if (pg_pwrite(fd, zbuffer.data, 1, wal_segment_size - 1) != 1)
    {
                                                           
      save_errno = errno ? errno : ENOSPC;
    }
  }
  pgstat_report_wait_end();

  if (save_errno)
  {
       
                                                                    
       
    unlink(tmppath);

    close(fd);

    errno = save_errno;

    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
  }

  pgstat_report_wait_start(WAIT_EVENT_WAL_INIT_SYNC);
  if (pg_fsync(fd) != 0)
  {
    int save_errno = errno;

    close(fd);
    errno = save_errno;
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  if (close(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
  }

     
                                                          
     
                                                                      
                                                                            
                                                                         
                                      
     
  installed_segno = logsegno;

     
                                                                            
                                                                            
                                                                             
                                                                       
                                                                          
                                                                             
                         
     
  max_segno = logsegno + CheckPointSegments;
  if (!InstallXLogFileSegment(&installed_segno, tmppath, *use_existent, max_segno, use_lock))
  {
       
                                                                         
                                                                           
                                 
       
    unlink(tmppath);
  }

                                                          
  *use_existent = false;

                                                                        
  fd = BasicOpenFile(path, O_RDWR | PG_BINARY | get_sync_bit(sync_method));
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path), (AmCheckpointerProcess() ? errhint("This is known to fail occasionally during archive recovery, where it is harmless.") : 0)));
  }

  elog(DEBUG2, "done creating and filling new WAL file");

  return fd;
}

   
                                                                 
   
                                              
   
                                                                  
                          
   
                                                                      
           
   
                                                                            
                                                                            
                           
   
static void
XLogFileCopy(XLogSegNo destsegno, TimeLineID srcTLI, XLogSegNo srcsegno, int upto)
{
  char path[MAXPGPATH];
  char tmppath[MAXPGPATH];
  PGAlignedXLogBlock buffer;
  int srcfd;
  int fd;
  int nbytes;

     
                          
     
  XLogFilePath(path, srcTLI, srcsegno, wal_segment_size);
  srcfd = OpenTransientFile(path, O_RDONLY | PG_BINARY);
  if (srcfd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

     
                                 
     
  snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int)getpid());

  unlink(tmppath);

                                                                            
  fd = OpenTransientFile(tmppath, O_RDWR | O_CREAT | O_EXCL | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmppath)));
  }

     
                          
     
  for (nbytes = 0; nbytes < wal_segment_size; nbytes += sizeof(buffer))
  {
    int nread;

    nread = upto - nbytes;

       
                                                                     
              
       
    if (nread < sizeof(buffer))
    {
      memset(buffer.data, 0, sizeof(buffer));
    }

    if (nread > 0)
    {
      int r;

      if (nread > sizeof(buffer))
      {
        nread = sizeof(buffer);
      }
      pgstat_report_wait_start(WAIT_EVENT_WAL_COPY_READ);
      r = read(srcfd, buffer.data, nread);
      if (r != nread)
      {
        if (r < 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, r, (Size)nread)));
        }
      }
      pgstat_report_wait_end();
    }
    errno = 0;
    pgstat_report_wait_start(WAIT_EVENT_WAL_COPY_WRITE);
    if ((int)write(fd, buffer.data, sizeof(buffer)) != (int)sizeof(buffer))
    {
      int save_errno = errno;

         
                                                                      
         
      unlink(tmppath);
                                                                      
      errno = save_errno ? save_errno : ENOSPC;

      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
    }
    pgstat_report_wait_end();
  }

  pgstat_report_wait_start(WAIT_EVENT_WAL_COPY_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
  }

  if (CloseTransientFile(srcfd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }

     
                                                          
     
  if (!InstallXLogFileSegment(&destsegno, tmppath, false, 0, false))
  {
    elog(ERROR, "InstallXLogFileSegment should not have failed");
  }
}

   
                                                                       
   
                                                                          
                                                                     
   
                                                                      
                                                                      
                                                          
   
                                                                             
   
                                                                        
                                                                             
                                                                      
   
                                                                             
                                                                            
              
   
                                                                     
                                                                          
                                            
   
                                                                              
                                                                         
                    
   
static bool
InstallXLogFileSegment(XLogSegNo *segno, char *tmppath, bool find_free, XLogSegNo max_segno, bool use_lock)
{
  char path[MAXPGPATH];
  struct stat stat_buf;

  XLogFilePath(path, ThisTimeLineID, *segno, wal_segment_size);

     
                                                                   
     
  if (use_lock)
  {
    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
  }

  if (!find_free)
  {
                                                                      
    durable_unlink(path, DEBUG1);
  }
  else
  {
                                       
    while (stat(path, &stat_buf) == 0)
    {
      if ((*segno) >= max_segno)
      {
                                                               
        if (use_lock)
        {
          LWLockRelease(ControlFileLock);
        }
        return false;
      }
      (*segno)++;
      XLogFilePath(path, ThisTimeLineID, *segno, wal_segment_size);
    }
  }

     
                                                                            
                                                            
     
  if (durable_link_or_rename(tmppath, path, LOG) != 0)
  {
    if (use_lock)
    {
      LWLockRelease(ControlFileLock);
    }
                                                            
    return false;
  }

  if (use_lock)
  {
    LWLockRelease(ControlFileLock);
  }

  return true;
}

   
                                                    
   
int
XLogFileOpen(XLogSegNo segno)
{
  char path[MAXPGPATH];
  int fd;

  XLogFilePath(path, ThisTimeLineID, segno, wal_segment_size);

  fd = BasicOpenFile(path, O_RDWR | PG_BINARY | get_sync_bit(sync_method));
  if (fd < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

  return fd;
}

   
                                                         
   
                                                                          
                                                              
   
static int
XLogFileRead(XLogSegNo segno, int emode, TimeLineID tli, int source, bool notfoundOk)
{
  char xlogfname[MAXFNAMELEN];
  char activitymsg[MAXFNAMELEN + 16];
  char path[MAXPGPATH];
  int fd;

  XLogFileName(xlogfname, tli, segno, wal_segment_size);

  switch (source)
  {
  case XLOG_FROM_ARCHIVE:
                                                
    snprintf(activitymsg, sizeof(activitymsg), "waiting for %s", xlogfname);
    set_ps_display(activitymsg, false);

    restoredFromArchive = RestoreArchivedFile(path, xlogfname, "RECOVERYXLOG", wal_segment_size, InRedo);
    if (!restoredFromArchive)
    {
      return -1;
    }
    break;

  case XLOG_FROM_PG_WAL:
  case XLOG_FROM_STREAM:
    XLogFilePath(path, tli, segno, wal_segment_size);
    restoredFromArchive = false;
    break;

  default:
    elog(ERROR, "invalid XLogFileRead source %d", source);
  }

     
                                                                            
                                                      
     
  if (source == XLOG_FROM_ARCHIVE)
  {
    KeepFileRestoredFromArchive(path, xlogfname);

       
                                                    
       
    snprintf(path, MAXPGPATH, XLOGDIR "/%s", xlogfname);
  }

  fd = BasicOpenFile(path, O_RDONLY | PG_BINARY);
  if (fd >= 0)
  {
                  
    curFileTLI = tli;

                                                
    snprintf(activitymsg, sizeof(activitymsg), "recovering %s", xlogfname);
    set_ps_display(activitymsg, false);

                                                          
    readSource = source;
    XLogReceiptSource = source;
                                                                 
    if (source != XLOG_FROM_STREAM)
    {
      XLogReceiptTime = GetCurrentTimestamp();
    }

    return fd;
  }
  if (errno != ENOENT || !notfoundOk)                          
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }
  return -1;
}

   
                                                         
   
                                                                              
   
static int
XLogFileReadAnyTLI(XLogSegNo segno, int emode, int source)
{
  char path[MAXPGPATH];
  ListCell *cell;
  int fd;
  List *tles;

     
                                                                           
                                           
     
                                                                          
                                                                             
                                                                             
                                                                         
                   
     
                                                                            
                                                                          
                                                                             
                                                                           
                                                                           
                                                                             
                                          
     
  if (expectedTLEs)
  {
    tles = expectedTLEs;
  }
  else
  {
    tles = readTimeLineHistory(recoveryTargetTLI);
  }

  foreach (cell, tles)
  {
    TimeLineHistoryEntry *hent = (TimeLineHistoryEntry *)lfirst(cell);
    TimeLineID tli = hent->tli;

    if (tli < curFileTLI)
    {
      break;                                           
    }

       
                                                                      
                         
       
    if (hent->begin != InvalidXLogRecPtr)
    {
      XLogSegNo beginseg = 0;

      XLByteToSeg(hent->begin, beginseg, wal_segment_size);

         
                                                                    
                                                                      
                                                                   
                                                                        
                                                                      
                                                                     
                                                                       
                                                  
         
      if (segno < beginseg)
      {
        continue;
      }
    }

    if (source == XLOG_FROM_ANY || source == XLOG_FROM_ARCHIVE)
    {
      fd = XLogFileRead(segno, emode, tli, XLOG_FROM_ARCHIVE, true);
      if (fd != -1)
      {
        elog(DEBUG1, "got WAL segment from archive");
        if (!expectedTLEs)
        {
          expectedTLEs = tles;
        }
        return fd;
      }
    }

    if (source == XLOG_FROM_ANY || source == XLOG_FROM_PG_WAL)
    {
      fd = XLogFileRead(segno, emode, tli, XLOG_FROM_PG_WAL, true);
      if (fd != -1)
      {
        if (!expectedTLEs)
        {
          expectedTLEs = tles;
        }
        return fd;
      }
    }
  }

                                                                        
  XLogFilePath(path, recoveryTargetTLI, segno, wal_segment_size);
  errno = ENOENT;
  ereport(emode, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  return -1;
}

   
                                                  
   
static void
XLogFileClose(void)
{
  Assert(openLogFile >= 0);

     
                                                                             
                                                                            
                                                                          
                                            
     
#if defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
  if (!XLogIsNeeded())
  {
    (void)posix_fadvise(openLogFile, 0, 0, POSIX_FADV_DONTNEED);
  }
#endif

  if (close(openLogFile))
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", XLogFileNameP(ThisTimeLineID, openLogSegNo))));
  }
  openLogFile = -1;
}

   
                                                            
   
                                                                          
                                                                           
                                                                               
                                                                            
                                                                         
                                                                             
   
static void
PreallocXlogFiles(XLogRecPtr endptr)
{
  XLogSegNo _logSegNo;
  int lf;
  bool use_existent;
  uint64 offset;

  XLByteToPrevSeg(endptr, _logSegNo, wal_segment_size);
  offset = XLogSegmentOffset(endptr - 1, wal_segment_size);
  if (offset >= (uint32)(0.75 * wal_segment_size))
  {
    _logSegNo++;
    use_existent = true;
    lf = XLogFileInit(_logSegNo, &use_existent, true);
    close(lf);
    if (!use_existent)
    {
      CheckpointStats.ckpt_segs_added++;
    }
  }
}

   
                                                                        
                                                                         
                                                                      
                                                                
                                            
   
                                                                     
                                                                    
                                                                       
                                                               
   
void
CheckXLogRemoved(XLogSegNo segno, TimeLineID tli)
{
  int save_errno = errno;
  XLogSegNo lastRemovedSegNo;

  SpinLockAcquire(&XLogCtl->info_lck);
  lastRemovedSegNo = XLogCtl->lastRemovedSegNo;
  SpinLockRelease(&XLogCtl->info_lck);

  if (segno <= lastRemovedSegNo)
  {
    char filename[MAXFNAMELEN];

    XLogFileName(filename, tli, segno, wal_segment_size);
    errno = save_errno;
    ereport(ERROR, (errcode_for_file_access(), errmsg("requested WAL segment %s has already been removed", filename)));
  }
  errno = save_errno;
}

   
                                                                            
                  
   
                                                                              
              
   
XLogSegNo
XLogGetLastRemovedSegno(void)
{
  XLogSegNo lastRemovedSegNo;

  SpinLockAcquire(&XLogCtl->info_lck);
  lastRemovedSegNo = XLogCtl->lastRemovedSegNo;
  SpinLockRelease(&XLogCtl->info_lck);

  return lastRemovedSegNo;
}

   
                                                                      
                                              
   
static void
UpdateLastRemovedPtr(char *filename)
{
  uint32 tli;
  XLogSegNo segno;

  XLogFromFileName(filename, &tli, &segno, wal_segment_size);

  SpinLockAcquire(&XLogCtl->info_lck);
  if (segno > XLogCtl->lastRemovedSegNo)
  {
    XLogCtl->lastRemovedSegNo = segno;
  }
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                            
   
                                                                       
                                                             
   
static void
RemoveTempXlogFiles(void)
{
  DIR *xldir;
  struct dirent *xlde;

  elog(DEBUG2, "removing all temporary WAL segments");

  xldir = AllocateDir(XLOGDIR);
  while ((xlde = ReadDir(xldir, XLOGDIR)) != NULL)
  {
    char path[MAXPGPATH];

    if (strncmp(xlde->d_name, "xlogtemp.", 9) != 0)
    {
      continue;
    }

    snprintf(path, MAXPGPATH, XLOGDIR "/%s", xlde->d_name);
    unlink(path);
    elog(DEBUG2, "removed temporary WAL segment \"%s\"", path);
  }
  FreeDir(xldir);
}

   
                                                                   
   
                                                                     
                                                                    
                                                                             
   
static void
RemoveOldXlogFiles(XLogSegNo segno, XLogRecPtr lastredoptr, XLogRecPtr endptr)
{
  DIR *xldir;
  struct dirent *xlde;
  char lastoff[MAXFNAMELEN];

     
                                                                          
                                                                         
                                                      
     
  XLogFileName(lastoff, 0, segno, wal_segment_size);

  elog(DEBUG2, "attempting to remove WAL segments older than log file %s", lastoff);

  xldir = AllocateDir(XLOGDIR);

  while ((xlde = ReadDir(xldir, XLOGDIR)) != NULL)
  {
                                                 
    if (!IsXLogFileName(xlde->d_name) && !IsPartialXLogFileName(xlde->d_name))
    {
      continue;
    }

       
                                                                      
                                                                         
                                                                           
                                                                      
                                                                
                    
       
                                                                           
                                                        
       
    if (strcmp(xlde->d_name + 8, lastoff + 8) <= 0)
    {
      if (XLogArchiveCheckDone(xlde->d_name))
      {
                                                                     
        UpdateLastRemovedPtr(xlde->d_name);

        RemoveXlogFile(xlde->d_name, lastredoptr, endptr);
      }
    }
  }

  FreeDir(xldir);
}

   
                                                                       
   
                                                                      
                                                                          
                                                                           
                                                                                
                                                                           
                                                                           
                                                                        
                                                                 
   
                                                                              
                                                  
   
static void
RemoveNonParentXlogFiles(XLogRecPtr switchpoint, TimeLineID newTLI)
{
  DIR *xldir;
  struct dirent *xlde;
  char switchseg[MAXFNAMELEN];
  XLogSegNo endLogSegNo;

  XLByteToPrevSeg(switchpoint, endLogSegNo, wal_segment_size);

     
                                                          
     
  XLogFileName(switchseg, newTLI, endLogSegNo, wal_segment_size);

  elog(DEBUG2, "attempting to remove WAL segments newer than log file %s", switchseg);

  xldir = AllocateDir(XLOGDIR);

  while ((xlde = ReadDir(xldir, XLOGDIR)) != NULL)
  {
                                                 
    if (!IsXLogFileName(xlde->d_name))
    {
      continue;
    }

       
                                                                        
                                                                           
                     
       
    if (strncmp(xlde->d_name, switchseg, 8) < 0 && strcmp(xlde->d_name + 8, switchseg + 8) > 0)
    {
         
                                                                       
                                                                      
                                                                        
                                                                      
         
      if (!XLogArchiveIsReady(xlde->d_name))
      {
        RemoveXlogFile(xlde->d_name, InvalidXLogRecPtr, switchpoint);
      }
    }
  }

  FreeDir(xldir);
}

   
                                                         
   
                                                                     
                                                                    
                                                                             
                                                                             
                                             
   
static void
RemoveXlogFile(const char *segname, XLogRecPtr lastredoptr, XLogRecPtr endptr)
{
  char path[MAXPGPATH];
#ifdef WIN32
  char newpath[MAXPGPATH];
#endif
  struct stat statbuf;
  XLogSegNo endlogSegNo;
  XLogSegNo recycleSegNo;

  if (wal_recycle)
  {
       
                                                         
       
    XLByteToSeg(endptr, endlogSegNo, wal_segment_size);
    if (lastredoptr == InvalidXLogRecPtr)
    {
      recycleSegNo = endlogSegNo + 10;
    }
    else
    {
      recycleSegNo = XLOGfileslop(lastredoptr);
    }
  }
  else
  {
    recycleSegNo = 0;                          
  }

  snprintf(path, MAXPGPATH, XLOGDIR "/%s", segname);

     
                                                                         
                                                                           
                                                              
     
  if (wal_recycle && endlogSegNo <= recycleSegNo && lstat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode) && InstallXLogFileSegment(&endlogSegNo, path, true, recycleSegNo, true))
  {
    ereport(DEBUG2, (errmsg("recycled write-ahead log file \"%s\"", segname)));
    CheckpointStats.ckpt_segs_recycled++;
                                                        
    endlogSegNo++;
  }
  else
  {
                                                 
    int rc;

    ereport(DEBUG2, (errmsg("removing write-ahead log file \"%s\"", segname)));

#ifdef WIN32

       
                                                                           
                                                                         
                                                                        
                                                                        
                                                                         
       
                                                                        
                                                                       
       
    snprintf(newpath, MAXPGPATH, "%s.deleted", path);
    if (rename(path, newpath) != 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not rename file \"%s\": %m", path)));
      return;
    }
    rc = durable_unlink(newpath, LOG);
#else
    rc = durable_unlink(path, LOG);
#endif
    if (rc != 0)
    {
                                                      
      return;
    }
    CheckpointStats.ckpt_segs_removed++;
  }

  XLogArchiveCleanup(segname);
}

   
                                                          
                                              
   
                                                                       
                                                                           
                                                            
   
                                                                       
                                                                          
                                                                           
                                                                        
   
static void
ValidateXLOGDirectoryStructure(void)
{
  char path[MAXPGPATH];
  struct stat stat_buf;

                                                        
  if (stat(XLOGDIR, &stat_buf) != 0 || !S_ISDIR(stat_buf.st_mode))
  {
    ereport(FATAL, (errmsg("required WAL directory \"%s\" does not exist", XLOGDIR)));
  }

                                
  snprintf(path, MAXPGPATH, XLOGDIR "/archive_status");
  if (stat(path, &stat_buf) == 0)
  {
                                                                     
    if (!S_ISDIR(stat_buf.st_mode))
    {
      ereport(FATAL, (errmsg("required WAL directory \"%s\" does not exist", path)));
    }
  }
  else
  {
    ereport(LOG, (errmsg("creating missing WAL directory \"%s\"", path)));
    if (MakePGDirectory(path) < 0)
    {
      ereport(FATAL, (errmsg("could not create missing directory \"%s\": %m", path)));
    }
  }
}

   
                                                                        
                                                                         
                   
   
static void
CleanupBackupHistory(void)
{
  DIR *xldir;
  struct dirent *xlde;
  char path[MAXPGPATH + sizeof(XLOGDIR)];

  xldir = AllocateDir(XLOGDIR);

  while ((xlde = ReadDir(xldir, XLOGDIR)) != NULL)
  {
    if (IsBackupHistoryFileName(xlde->d_name))
    {
      if (XLogArchiveCheckDone(xlde->d_name))
      {
        elog(DEBUG2, "removing WAL backup history file \"%s\"", xlde->d_name);
        snprintf(path, sizeof(path), XLOGDIR "/%s", xlde->d_name);
        unlink(path);
        XLogArchiveCleanup(xlde->d_name);
      }
    }
  }

  FreeDir(xldir);
}

   
                                   
   
                                                                         
                                                                 
   
                                                                              
                                                                             
                        
   
static XLogRecord *
ReadRecord(XLogReaderState *xlogreader, XLogRecPtr RecPtr, int emode, bool fetching_ckpt)
{
  XLogRecord *record;
  XLogPageReadPrivate *private = (XLogPageReadPrivate *)xlogreader->private_data;

                                               
  private->fetching_ckpt = fetching_ckpt;
  private->emode = emode;
  private->randAccess = (RecPtr != InvalidXLogRecPtr);

                                                    
  lastSourceFailed = false;

  for (;;)
  {
    char *errormsg;

    record = XLogReadRecord(xlogreader, RecPtr, &errormsg);
    ReadRecPtr = xlogreader->ReadRecPtr;
    EndRecPtr = xlogreader->EndRecPtr;
    if (record == NULL)
    {
         
                                                                        
                                                                          
                                                                       
                  
         
                                                                       
                                                                       
                                                                     
                                                                      
                                                                       
         
      if (!ArchiveRecoveryRequested && !XLogRecPtrIsInvalid(xlogreader->abortedRecPtr))
      {
        abortedRecPtr = xlogreader->abortedRecPtr;
        missingContrecPtr = xlogreader->missingContrecPtr;
      }

      if (readFile >= 0)
      {
        close(readFile);
        readFile = -1;
      }

         
                                                                   
                                                               
                                                                        
                                              
         
      if (errormsg)
      {
        ereport(emode_for_corrupt_record(emode, RecPtr ? RecPtr : EndRecPtr), (errmsg_internal("%s", errormsg)                         ));
      }
    }

       
                                                     
       
    else if (!tliInHistory(xlogreader->latestPageTLI, expectedTLEs))
    {
      char fname[MAXFNAMELEN];
      XLogSegNo segno;
      int32 offset;

      XLByteToSeg(xlogreader->latestPagePtr, segno, wal_segment_size);
      offset = XLogSegmentOffset(xlogreader->latestPagePtr, wal_segment_size);
      XLogFileName(fname, xlogreader->readPageTLI, segno, wal_segment_size);
      ereport(emode_for_corrupt_record(emode, RecPtr ? RecPtr : EndRecPtr), (errmsg("unexpected timeline ID %u in log segment %s, offset %u", xlogreader->latestPageTLI, fname, offset)));
      record = NULL;
    }

    if (record)
    {
                               
      return record;
    }
    else
    {
                                                      
      lastSourceFailed = true;

         
                                                                    
                                                                        
                                                                    
                                                      
         
                                                                    
                                                                       
                                                                       
                                                                
                                                            
         
      if (!InArchiveRecovery && ArchiveRecoveryRequested && !fetching_ckpt)
      {
        ereport(DEBUG1, (errmsg_internal("reached end of WAL in pg_wal, entering archive recovery")));
        InArchiveRecovery = true;
        if (StandbyModeRequested)
        {
          StandbyMode = true;
        }

                                                        
        LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
        ControlFile->state = DB_IN_ARCHIVE_RECOVERY;
        if (ControlFile->minRecoveryPoint < EndRecPtr)
        {
          ControlFile->minRecoveryPoint = EndRecPtr;
          ControlFile->minRecoveryPointTLI = ThisTimeLineID;
        }
                               
        minRecoveryPoint = ControlFile->minRecoveryPoint;
        minRecoveryPointTLI = ControlFile->minRecoveryPointTLI;

           
                                                            
                                             
           
        updateMinRecoveryPoint = true;

        UpdateControlFile();

           
                                                                   
                                                                   
                   
           
        SpinLockAcquire(&XLogCtl->info_lck);
        XLogCtl->SharedRecoveryState = RECOVERY_STATE_ARCHIVE;
        SpinLockRelease(&XLogCtl->info_lck);

        LWLockRelease(ControlFileLock);

        CheckRecoveryConsistency();

           
                                                                     
                                                   
           
        lastSourceFailed = false;
        currentSource = 0;

        continue;
      }

                                                                    
      if (StandbyMode && !CheckForStandbyTrigger())
      {
        continue;
      }
      else
      {
        return NULL;
      }
    }
  }
}

   
                                                                           
                     
   
                                                                            
                           
   
static bool
rescanLatestTimeLine(void)
{
  List *newExpectedTLEs;
  bool found;
  ListCell *cell;
  TimeLineID newtarget;
  TimeLineID oldtarget = recoveryTargetTLI;
  TimeLineHistoryEntry *currentTle = NULL;

  newtarget = findNewestTimeLine(recoveryTargetTLI);
  if (newtarget == recoveryTargetTLI)
  {
                                
    return false;
  }

     
                                                         
     

  newExpectedTLEs = readTimeLineHistory(newtarget);

     
                                                                             
                              
     
  found = false;
  foreach (cell, newExpectedTLEs)
  {
    currentTle = (TimeLineHistoryEntry *)lfirst(cell);

    if (currentTle->tli == recoveryTargetTLI)
    {
      found = true;
      break;
    }
  }
  if (!found)
  {
    ereport(LOG, (errmsg("new timeline %u is not a child of database system timeline %u", newtarget, ThisTimeLineID)));
    return false;
  }

     
                                                                            
                                                                       
               
     
  if (currentTle->end < EndRecPtr)
  {
    ereport(LOG, (errmsg("new timeline %u forked off current database system timeline %u before current recovery point %X/%X", newtarget, ThisTimeLineID, (uint32)(EndRecPtr >> 32), (uint32)EndRecPtr)));
    return false;
  }

                                                           
  recoveryTargetTLI = newtarget;
  list_free_deep(expectedTLEs);
  expectedTLEs = newExpectedTLEs;

     
                                                                      
                                                      
     
  restoreTimeLineHistoryFiles(oldtarget + 1, newtarget);

  ereport(LOG, (errmsg("new target timeline is %u", recoveryTargetTLI)));

  return true;
}

   
                               
   
                                                                        
                                                                      
                                                                     
                                                                          
                                                                           
   
                                                                           
                                                                    
                                                                        
                                                                             
   
static void
WriteControlFile(void)
{
  int fd;
  char buffer[PG_CONTROL_FILE_SIZE];                          

     
                                                                             
                                                 
     
  StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_MAX_SAFE_SIZE, "pg_control is too large for atomic disk writes");
  StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_FILE_SIZE, "sizeof(ControlFileData) exceeds PG_CONTROL_FILE_SIZE");

     
                                                       
     
  ControlFile->pg_control_version = PG_CONTROL_VERSION;
  ControlFile->catalog_version_no = CATALOG_VERSION_NO;

  ControlFile->maxAlign = MAXIMUM_ALIGNOF;
  ControlFile->floatFormat = FLOATFORMAT_VALUE;

  ControlFile->blcksz = BLCKSZ;
  ControlFile->relseg_size = RELSEG_SIZE;
  ControlFile->xlog_blcksz = XLOG_BLCKSZ;
  ControlFile->xlog_seg_size = wal_segment_size;

  ControlFile->nameDataLen = NAMEDATALEN;
  ControlFile->indexMaxKeys = INDEX_MAX_KEYS;

  ControlFile->toast_max_chunk_size = TOAST_MAX_CHUNK_SIZE;
  ControlFile->loblksize = LOBLKSIZE;

  ControlFile->float4ByVal = FLOAT4PASSBYVAL;
  ControlFile->float8ByVal = FLOAT8PASSBYVAL;

                                         
  INIT_CRC32C(ControlFile->crc);
  COMP_CRC32C(ControlFile->crc, (char *)ControlFile, offsetof(ControlFileData, crc));
  FIN_CRC32C(ControlFile->crc);

     
                                                                           
                                                                        
                                                                             
                                                                        
                                            
     
  memset(buffer, 0, PG_CONTROL_FILE_SIZE);
  memcpy(buffer, ControlFile, sizeof(ControlFileData));

  fd = BasicOpenFile(XLOG_CONTROL_FILE, O_RDWR | O_CREAT | O_EXCL | PG_BINARY);
  if (fd < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", XLOG_CONTROL_FILE)));
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_WRITE);
  if (write(fd, buffer, PG_CONTROL_FILE_SIZE) != PG_CONTROL_FILE_SIZE)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", XLOG_CONTROL_FILE)));
  }
  pgstat_report_wait_end();

  pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", XLOG_CONTROL_FILE)));
  }
  pgstat_report_wait_end();

  if (close(fd))
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", XLOG_CONTROL_FILE)));
  }
}

static void
ReadControlFile(void)
{
  pg_crc32c crc;
  int fd;
  static char wal_segsz_str[20];
  int r;

     
                  
     
  fd = BasicOpenFile(XLOG_CONTROL_FILE, O_RDWR | PG_BINARY);
  if (fd < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", XLOG_CONTROL_FILE)));
  }

  pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_READ);
  r = read(fd, ControlFile, sizeof(ControlFileData));
  if (r != sizeof(ControlFileData))
  {
    if (r < 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", XLOG_CONTROL_FILE)));
    }
    else
    {
      ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", XLOG_CONTROL_FILE, r, sizeof(ControlFileData))));
    }
  }
  pgstat_report_wait_end();

  close(fd);

     
                                                                          
                                                                           
                                                                      
                                                    
     

  if (ControlFile->pg_control_version != PG_CONTROL_VERSION && ControlFile->pg_control_version % 65536 == 0 && ControlFile->pg_control_version / 65536 != 0)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with PG_CONTROL_VERSION %d (0x%08x),"
                                 " but the server was compiled with PG_CONTROL_VERSION %d (0x%08x).",
                           ControlFile->pg_control_version, ControlFile->pg_control_version, PG_CONTROL_VERSION, PG_CONTROL_VERSION),
                       errhint("This could be a problem of mismatched byte ordering.  It looks like you need to initdb.")));
  }

  if (ControlFile->pg_control_version != PG_CONTROL_VERSION)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with PG_CONTROL_VERSION %d,"
                                 " but the server was compiled with PG_CONTROL_VERSION %d.",
                           ControlFile->pg_control_version, PG_CONTROL_VERSION),
                       errhint("It looks like you need to initdb.")));
  }

                          
  INIT_CRC32C(crc);
  COMP_CRC32C(crc, (char *)ControlFile, offsetof(ControlFileData, crc));
  FIN_CRC32C(crc);

  if (!EQ_CRC32C(crc, ControlFile->crc))
  {
    ereport(FATAL, (errmsg("incorrect checksum in control file")));
  }

     
                                                                   
                                                                            
                             
     
  if (ControlFile->catalog_version_no != CATALOG_VERSION_NO)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with CATALOG_VERSION_NO %d,"
                                 " but the server was compiled with CATALOG_VERSION_NO %d.",
                           ControlFile->catalog_version_no, CATALOG_VERSION_NO),
                       errhint("It looks like you need to initdb.")));
  }
  if (ControlFile->maxAlign != MAXIMUM_ALIGNOF)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with MAXALIGN %d,"
                                 " but the server was compiled with MAXALIGN %d.",
                           ControlFile->maxAlign, MAXIMUM_ALIGNOF),
                       errhint("It looks like you need to initdb.")));
  }
  if (ControlFile->floatFormat != FLOATFORMAT_VALUE)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"), errdetail("The database cluster appears to use a different floating-point number format than the server executable."), errhint("It looks like you need to initdb.")));
  }
  if (ControlFile->blcksz != BLCKSZ)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with BLCKSZ %d,"
                                 " but the server was compiled with BLCKSZ %d.",
                           ControlFile->blcksz, BLCKSZ),
                       errhint("It looks like you need to recompile or initdb.")));
  }
  if (ControlFile->relseg_size != RELSEG_SIZE)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with RELSEG_SIZE %d,"
                                 " but the server was compiled with RELSEG_SIZE %d.",
                           ControlFile->relseg_size, RELSEG_SIZE),
                       errhint("It looks like you need to recompile or initdb.")));
  }
  if (ControlFile->xlog_blcksz != XLOG_BLCKSZ)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with XLOG_BLCKSZ %d,"
                                 " but the server was compiled with XLOG_BLCKSZ %d.",
                           ControlFile->xlog_blcksz, XLOG_BLCKSZ),
                       errhint("It looks like you need to recompile or initdb.")));
  }
  if (ControlFile->nameDataLen != NAMEDATALEN)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with NAMEDATALEN %d,"
                                 " but the server was compiled with NAMEDATALEN %d.",
                           ControlFile->nameDataLen, NAMEDATALEN),
                       errhint("It looks like you need to recompile or initdb.")));
  }
  if (ControlFile->indexMaxKeys != INDEX_MAX_KEYS)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with INDEX_MAX_KEYS %d,"
                                 " but the server was compiled with INDEX_MAX_KEYS %d.",
                           ControlFile->indexMaxKeys, INDEX_MAX_KEYS),
                       errhint("It looks like you need to recompile or initdb.")));
  }
  if (ControlFile->toast_max_chunk_size != TOAST_MAX_CHUNK_SIZE)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with TOAST_MAX_CHUNK_SIZE %d,"
                                 " but the server was compiled with TOAST_MAX_CHUNK_SIZE %d.",
                           ControlFile->toast_max_chunk_size, (int)TOAST_MAX_CHUNK_SIZE),
                       errhint("It looks like you need to recompile or initdb.")));
  }
  if (ControlFile->loblksize != LOBLKSIZE)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with LOBLKSIZE %d,"
                                 " but the server was compiled with LOBLKSIZE %d.",
                           ControlFile->loblksize, (int)LOBLKSIZE),
                       errhint("It looks like you need to recompile or initdb.")));
  }

#ifdef USE_FLOAT4_BYVAL
  if (ControlFile->float4ByVal != true)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized without USE_FLOAT4_BYVAL"
                                 " but the server was compiled with USE_FLOAT4_BYVAL."),
                       errhint("It looks like you need to recompile or initdb.")));
  }
#else
  if (ControlFile->float4ByVal != false)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with USE_FLOAT4_BYVAL"
                                 " but the server was compiled without USE_FLOAT4_BYVAL."),
                       errhint("It looks like you need to recompile or initdb.")));
  }
#endif

#ifdef USE_FLOAT8_BYVAL
  if (ControlFile->float8ByVal != true)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized without USE_FLOAT8_BYVAL"
                                 " but the server was compiled with USE_FLOAT8_BYVAL."),
                       errhint("It looks like you need to recompile or initdb.")));
  }
#else
  if (ControlFile->float8ByVal != false)
  {
    ereport(FATAL, (errmsg("database files are incompatible with server"),
                       errdetail("The database cluster was initialized with USE_FLOAT8_BYVAL"
                                 " but the server was compiled without USE_FLOAT8_BYVAL."),
                       errhint("It looks like you need to recompile or initdb.")));
  }
#endif

  wal_segment_size = ControlFile->xlog_seg_size;

  if (!IsValidWalSegSize(wal_segment_size))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg_plural("WAL segment size must be a power of two between 1 MB and 1 GB, but the control file specifies %d byte", "WAL segment size must be a power of two between 1 MB and 1 GB, but the control file specifies %d bytes", wal_segment_size, wal_segment_size)));
  }

  snprintf(wal_segsz_str, sizeof(wal_segsz_str), "%d", wal_segment_size);
  SetConfigOption("wal_segment_size", wal_segsz_str, PGC_INTERNAL, PGC_S_OVERRIDE);

                                                                
  if (ConvertToXSegs(min_wal_size_mb, wal_segment_size) < 2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"min_wal_size\" must be at least twice \"wal_segment_size\"")));
  }

  if (ConvertToXSegs(max_wal_size_mb, wal_segment_size) < 2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"max_wal_size\" must be at least twice \"wal_segment_size\"")));
  }

  UsableBytesInSegment = (wal_segment_size / XLOG_BLCKSZ * UsableBytesInPage) - (SizeOfXLogLongPHD - SizeOfXLogShortPHD);

  CalculateCheckpointSegments();

                                                              
  SetConfigOption("data_checksums", DataChecksumsEnabled() ? "yes" : "no", PGC_INTERNAL, PGC_S_OVERRIDE);
}

   
                                                                      
                      
   
void
UpdateControlFile(void)
{
  update_controlfile(DataDir, ControlFile, true);
}

   
                                                           
   
uint64
GetSystemIdentifier(void)
{
  Assert(ControlFile != NULL);
  return ControlFile->system_identifier;
}

   
                                               
   
char *
GetMockAuthenticationNonce(void)
{
  Assert(ControlFile != NULL);
  return ControlFile->mock_authentication_nonce;
}

   
                                         
   
bool
DataChecksumsEnabled(void)
{
  Assert(ControlFile != NULL);
  return (ControlFile->data_checksum_version > 0);
}

   
                                              
   
                                                                      
                                                                          
                                                                              
                                                                             
                                                                    
   
XLogRecPtr
GetFakeLSNForUnloggedRel(void)
{
  XLogRecPtr nextUnloggedLSN;

                                                        
  SpinLockAcquire(&XLogCtl->ulsn_lck);
  nextUnloggedLSN = XLogCtl->unloggedLSN++;
  SpinLockRelease(&XLogCtl->ulsn_lck);

  return nextUnloggedLSN;
}

   
                                         
   
                                                                             
                                                                            
                                                                               
                                                                              
                                     
   
                                                                          
   
static int
XLOGChooseNumBuffers(void)
{
  int xbuffers;

  xbuffers = NBuffers / 32;
  if (xbuffers > (wal_segment_size / XLOG_BLCKSZ))
  {
    xbuffers = (wal_segment_size / XLOG_BLCKSZ);
  }
  if (xbuffers < 8)
  {
    xbuffers = 8;
  }
  return xbuffers;
}

   
                                  
   
bool
check_wal_buffers(int *newval, void **extra, GucSource source)
{
     
                                           
     
  if (*newval == -1)
  {
       
                                                                         
                                                       
       
    if (XLOGbuffers == -1)
    {
      return true;
    }

                                                   
    *newval = XLOGChooseNumBuffers();
  }

     
                                                                             
                                                                            
                                                                       
                                                                            
               
     
  if (*newval < 4)
  {
    *newval = 4;
  }

  return true;
}

   
                                               
   
                                                                          
                                                                              
                                                                          
                                                                             
                                             
   
                                                                            
                                                                           
   
void
LocalProcessControlFile(bool reset)
{
  Assert(reset || ControlFile == NULL);
  ControlFile = palloc(sizeof(ControlFileData));
  ReadControlFile();
}

   
                                            
   
Size
XLOGShmemSize(void)
{
  Size size;

     
                                                                           
                                                                           
                                                                            
                                                    
     
  if (XLOGbuffers == -1)
  {
    char buf[32];

    snprintf(buf, sizeof(buf), "%d", XLOGChooseNumBuffers());
    SetConfigOption("wal_buffers", buf, PGC_POSTMASTER, PGC_S_OVERRIDE);
  }
  Assert(XLOGbuffers > 0);

               
  size = sizeof(XLogCtlData);

                                           
  size = add_size(size, mul_size(sizeof(WALInsertLockPadded), NUM_XLOGINSERT_LOCKS + 1));
                      
  size = add_size(size, mul_size(sizeof(XLogRecPtr), XLOGbuffers));
                                                    
  size = add_size(size, XLOG_BLCKSZ);
                                  
  size = add_size(size, mul_size(XLOG_BLCKSZ, XLOGbuffers));

     
                                                                             
                                                                      
                                                                
     

  return size;
}

void
XLOGShmemInit(void)
{
  bool foundCFile, foundXLog;
  char *allocptr;
  int i;
  ControlFileData *localControlFile;

#ifdef WAL_DEBUG

     
                                                                             
                                                                             
                                                                          
     
  if (walDebugCxt == NULL)
  {
    walDebugCxt = AllocSetContextCreate(TopMemoryContext, "WAL Debug", ALLOCSET_DEFAULT_SIZES);
    MemoryContextAllowInCriticalSection(walDebugCxt, true);
  }
#endif

  XLogCtl = (XLogCtlData *)ShmemInitStruct("XLOG Ctl", XLOGShmemSize(), &foundXLog);

  localControlFile = ControlFile;
  ControlFile = (ControlFileData *)ShmemInitStruct("Control File", sizeof(ControlFileData), &foundCFile);

  if (foundCFile || foundXLog)
  {
                                           
    Assert(foundCFile && foundXLog);

                                                                          
    WALInsertLocks = XLogCtl->Insert.WALInsertLocks;
    LWLockRegisterTranche(LWTRANCHE_WAL_INSERT, "wal_insert");

    if (localControlFile)
    {
      pfree(localControlFile);
    }
    return;
  }
  memset(XLogCtl, 0, sizeof(XLogCtlData));

     
                                                                            
                                  
     
  if (localControlFile)
  {
    memcpy(ControlFile, localControlFile, sizeof(ControlFileData));
    pfree(localControlFile);
  }

     
                                                                          
                                                                          
                  
     
  allocptr = ((char *)XLogCtl) + sizeof(XLogCtlData);
  XLogCtl->xlblocks = (XLogRecPtr *)allocptr;
  memset(XLogCtl->xlblocks, 0, sizeof(XLogRecPtr) * XLOGbuffers);
  allocptr += sizeof(XLogRecPtr) * XLOGbuffers;

                                                                           
  allocptr += sizeof(WALInsertLockPadded) - ((uintptr_t)allocptr) % sizeof(WALInsertLockPadded);
  WALInsertLocks = XLogCtl->Insert.WALInsertLocks = (WALInsertLockPadded *)allocptr;
  allocptr += sizeof(WALInsertLockPadded) * NUM_XLOGINSERT_LOCKS;

  LWLockRegisterTranche(LWTRANCHE_WAL_INSERT, "wal_insert");
  for (i = 0; i < NUM_XLOGINSERT_LOCKS; i++)
  {
    LWLockInitialize(&WALInsertLocks[i].l.lock, LWTRANCHE_WAL_INSERT);
    WALInsertLocks[i].l.insertingAt = InvalidXLogRecPtr;
    WALInsertLocks[i].l.lastImportantAt = InvalidXLogRecPtr;
  }

     
                                                                             
                                                                     
                            
     
  allocptr = (char *)TYPEALIGN(XLOG_BLCKSZ, allocptr);
  XLogCtl->pages = allocptr;
  memset(XLogCtl->pages, 0, (Size)XLOG_BLCKSZ * XLOGbuffers);

     
                                                                            
                          
     
  XLogCtl->XLogCacheBlck = XLOGbuffers - 1;
  XLogCtl->SharedRecoveryState = RECOVERY_STATE_CRASH;
  XLogCtl->SharedHotStandbyActive = false;
  XLogCtl->WalWriterSleeping = false;

  SpinLockInit(&XLogCtl->Insert.insertpos_lck);
  SpinLockInit(&XLogCtl->info_lck);
  SpinLockInit(&XLogCtl->ulsn_lck);
  InitSharedLatch(&XLogCtl->recoveryWakeupLatch);
}

   
                                                                           
                                 
   
void
BootStrapXLOG(void)
{
  CheckPoint checkPoint;
  char *buffer;
  XLogPageHeader page;
  XLogLongPageHeader longpage;
  XLogRecord *record;
  char *recptr;
  bool use_existent;
  uint64 sysidentifier;
  char mock_auth_nonce[MOCK_AUTH_NONCE_LEN];
  struct timeval tv;
  pg_crc32c crc;

     
                                                                             
                                                                           
                                                                          
                                                                           
                                                                             
                                                                             
                                                                             
                                                                            
                                                                        
                                  
     
  gettimeofday(&tv, NULL);
  sysidentifier = ((uint64)tv.tv_sec) << 32;
  sysidentifier |= ((uint64)tv.tv_usec) << 12;
  sysidentifier |= getpid() & 0xFFF;

     
                                                                            
                                                                            
                                                                             
                                   
     
  if (!pg_strong_random(mock_auth_nonce, MOCK_AUTH_NONCE_LEN))
  {
    ereport(PANIC, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("could not generate secret authorization token")));
  }

                                     
  ThisTimeLineID = 1;

                                                         
  buffer = (char *)palloc(XLOG_BLCKSZ + XLOG_BLCKSZ);
  page = (XLogPageHeader)TYPEALIGN(XLOG_BLCKSZ, buffer);
  memset(page, 0, XLOG_BLCKSZ);

     
                                                          
     
                                                                          
                                                                            
                                                                          
     
  checkPoint.redo = wal_segment_size + SizeOfXLogLongPHD;
  checkPoint.ThisTimeLineID = ThisTimeLineID;
  checkPoint.PrevTimeLineID = ThisTimeLineID;
  checkPoint.fullPageWrites = fullPageWrites;
  checkPoint.nextFullXid = FullTransactionIdFromEpochAndXid(0, FirstNormalTransactionId);
  checkPoint.nextOid = FirstBootstrapObjectId;
  checkPoint.nextMulti = FirstMultiXactId;
  checkPoint.nextMultiOffset = 0;
  checkPoint.oldestXid = FirstNormalTransactionId;
  checkPoint.oldestXidDB = TemplateDbOid;
  checkPoint.oldestMulti = FirstMultiXactId;
  checkPoint.oldestMultiDB = TemplateDbOid;
  checkPoint.oldestCommitTsXid = InvalidTransactionId;
  checkPoint.newestCommitTsXid = InvalidTransactionId;
  checkPoint.time = (pg_time_t)time(NULL);
  checkPoint.oldestActiveXid = InvalidTransactionId;

  ShmemVariableCache->nextFullXid = checkPoint.nextFullXid;
  ShmemVariableCache->nextOid = checkPoint.nextOid;
  ShmemVariableCache->oidCount = 0;
  MultiXactSetNextMXact(checkPoint.nextMulti, checkPoint.nextMultiOffset);
  AdvanceOldestClogXid(checkPoint.oldestXid);
  SetTransactionIdLimit(checkPoint.oldestXid, checkPoint.oldestXidDB);
  SetMultiXactIdLimit(checkPoint.oldestMulti, checkPoint.oldestMultiDB, true);
  SetCommitTsLimit(InvalidTransactionId, InvalidTransactionId);

                                   
  page->xlp_magic = XLOG_PAGE_MAGIC;
  page->xlp_info = XLP_LONG_HEADER;
  page->xlp_tli = ThisTimeLineID;
  page->xlp_pageaddr = wal_segment_size;
  longpage = (XLogLongPageHeader)page;
  longpage->xlp_sysid = sysidentifier;
  longpage->xlp_seg_size = wal_segment_size;
  longpage->xlp_xlog_blcksz = XLOG_BLCKSZ;

                                            
  recptr = ((char *)page + SizeOfXLogLongPHD);
  record = (XLogRecord *)recptr;
  record->xl_prev = 0;
  record->xl_xid = InvalidTransactionId;
  record->xl_tot_len = SizeOfXLogRecord + SizeOfXLogRecordDataHeaderShort + sizeof(checkPoint);
  record->xl_info = XLOG_CHECKPOINT_SHUTDOWN;
  record->xl_rmid = RM_XLOG_ID;
  recptr += SizeOfXLogRecord;
                                                 
  *(recptr++) = (char)XLR_BLOCK_ID_DATA_SHORT;
  *(recptr++) = sizeof(checkPoint);
  memcpy(recptr, &checkPoint, sizeof(checkPoint));
  recptr += sizeof(checkPoint);
  Assert(recptr - (char *)record == record->xl_tot_len);

  INIT_CRC32C(crc);
  COMP_CRC32C(crc, ((char *)record) + SizeOfXLogRecord, record->xl_tot_len - SizeOfXLogRecord);
  COMP_CRC32C(crc, (char *)record, offsetof(XLogRecord, xl_crc));
  FIN_CRC32C(crc);
  record->xl_crc = crc;

                                      
  use_existent = false;
  openLogFile = XLogFileInit(1, &use_existent, false);

                                                    
  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_WAL_BOOTSTRAP_WRITE);
  if (write(openLogFile, page, XLOG_BLCKSZ) != XLOG_BLCKSZ)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not write bootstrap write-ahead log file: %m")));
  }
  pgstat_report_wait_end();

  pgstat_report_wait_start(WAIT_EVENT_WAL_BOOTSTRAP_SYNC);
  if (pg_fsync(openLogFile) != 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync bootstrap write-ahead log file: %m")));
  }
  pgstat_report_wait_end();

  if (close(openLogFile))
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close bootstrap write-ahead log file: %m")));
  }

  openLogFile = -1;

                             

  memset(ControlFile, 0, sizeof(ControlFileData));
                                           
  ControlFile->system_identifier = sysidentifier;
  memcpy(ControlFile->mock_authentication_nonce, mock_auth_nonce, MOCK_AUTH_NONCE_LEN);
  ControlFile->state = DB_SHUTDOWNED;
  ControlFile->time = checkPoint.time;
  ControlFile->checkPoint = checkPoint.redo;
  ControlFile->checkPointCopy = checkPoint;
  ControlFile->unloggedLSN = FirstNormalUnloggedLSN;

                                                                 
  ControlFile->MaxConnections = MaxConnections;
  ControlFile->max_worker_processes = max_worker_processes;
  ControlFile->max_wal_senders = max_wal_senders;
  ControlFile->max_prepared_xacts = max_prepared_xacts;
  ControlFile->max_locks_per_xact = max_locks_per_xact;
  ControlFile->wal_level = wal_level;
  ControlFile->wal_log_hints = wal_log_hints;
  ControlFile->track_commit_timestamp = track_commit_timestamp;
  ControlFile->data_checksum_version = bootstrap_data_checksum_version;

                                                                        

  WriteControlFile();

                                     
  BootStrapCLOG();
  BootStrapCommitTs();
  BootStrapSUBTRANS();
  BootStrapMultiXact();

  pfree(buffer);

     
                                                                           
                                                                             
     
  ReadControlFile();
}

static char *
str_time(pg_time_t tnow)
{
  static char buf[128];

  pg_strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", pg_localtime(&tnow, log_timezone));

  return buf;
}

   
                                                                       
             
   
                                                                      
                                                                
   
static void
readRecoverySignalFile(void)
{
  struct stat stat_buf;

  if (IsBootstrapProcessingMode())
  {
    return;
  }

     
                                                    
     
  if (stat(RECOVERY_COMMAND_FILE, &stat_buf) == 0)
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("using recovery command file \"%s\" is not supported", RECOVERY_COMMAND_FILE)));
  }

     
                                                             
     
  unlink(RECOVERY_COMMAND_DONE);

     
                                                                         
                                                                            
                                            
     
                                                                             
                                           
     
  if (stat(STANDBY_SIGNAL_FILE, &stat_buf) == 0)
  {
    int fd;

    fd = BasicOpenFilePerm(STANDBY_SIGNAL_FILE, O_RDWR | PG_BINARY | get_sync_bit(sync_method), S_IRUSR | S_IWUSR);
    if (fd >= 0)
    {
      (void)pg_fsync(fd);
      close(fd);
    }
    standby_signal_file_found = true;
  }
  else if (stat(RECOVERY_SIGNAL_FILE, &stat_buf) == 0)
  {
    int fd;

    fd = BasicOpenFilePerm(RECOVERY_SIGNAL_FILE, O_RDWR | PG_BINARY | get_sync_bit(sync_method), S_IRUSR | S_IWUSR);
    if (fd >= 0)
    {
      (void)pg_fsync(fd);
      close(fd);
    }
    recovery_signal_file_found = true;
  }

  StandbyModeRequested = false;
  ArchiveRecoveryRequested = false;
  if (standby_signal_file_found)
  {
    StandbyModeRequested = true;
    ArchiveRecoveryRequested = true;
  }
  else if (recovery_signal_file_found)
  {
    StandbyModeRequested = false;
    ArchiveRecoveryRequested = true;
  }
  else
  {
    return;
  }

     
                                                                         
                                                           
     
  if (StandbyModeRequested && !IsUnderPostmaster)
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("standby mode is not supported by single-user servers")));
  }
}

static void
validateRecoveryParameters(void)
{
  if (!ArchiveRecoveryRequested)
  {
    return;
  }

     
                                     
     
  if (StandbyModeRequested)
  {
    if ((PrimaryConnInfo == NULL || strcmp(PrimaryConnInfo, "") == 0) && (recoveryRestoreCommand == NULL || strcmp(recoveryRestoreCommand, "") == 0))
    {
      ereport(WARNING, (errmsg("specified neither primary_conninfo nor restore_command"), errhint("The database server will regularly poll the pg_wal subdirectory to check for files placed there.")));
    }
  }
  else
  {
    if (recoveryRestoreCommand == NULL || strcmp(recoveryRestoreCommand, "") == 0)
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("must specify restore_command when standby mode is not enabled")));
    }
  }

     
                                                                       
                                                                             
                                                        
     
  if (recoveryTargetAction == RECOVERY_TARGET_ACTION_PAUSE && !EnableHotStandby)
  {
    recoveryTargetAction = RECOVERY_TARGET_ACTION_SHUTDOWN;
  }

     
                                                            
                                   
     
  if (recoveryTarget == RECOVERY_TARGET_TIME)
  {
    recoveryTargetTime = DatumGetTimestampTz(DirectFunctionCall3(timestamptz_in, CStringGetDatum(recovery_target_time_string), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1)));
  }

     
                                                                            
                                                                            
                                                                          
                                     
     
  if (recoveryTargetTimeLineGoal == RECOVERY_TARGET_TIMELINE_NUMERIC)
  {
    TimeLineID rtli = recoveryTargetTLIRequested;

                                                                  
    if (rtli != 1 && !existsTimeLineHistory(rtli))
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("recovery target timeline %u does not exist", rtli)));
    }
    recoveryTargetTLI = rtli;
  }
  else if (recoveryTargetTimeLineGoal == RECOVERY_TARGET_TIMELINE_LATEST)
  {
                                                                 
    recoveryTargetTLI = findNewestTimeLine(recoveryTargetTLI);
  }
  else
  {
       
                                                                   
                   
       
    Assert(recoveryTargetTimeLineGoal == RECOVERY_TARGET_TIMELINE_CONTROLFILE);
  }
}

   
                               
   
static void
exitArchiveRecovery(TimeLineID endTLI, XLogRecPtr endOfLog)
{
  char xlogfname[MAXFNAMELEN];
  XLogSegNo endLogSegNo;
  XLogSegNo startLogSegNo;

                                                                 
  Assert(endTLI != ThisTimeLineID);

     
                                                 
     
  InArchiveRecovery = false;

     
                                              
     
  UpdateMinRecoveryPoint(InvalidXLogRecPtr, true);

     
                                                                             
                                                            
     
  if (readFile >= 0)
  {
    close(readFile);
    readFile = -1;
  }

     
                                                                           
                                                                            
                                                                       
                                                      
     
  XLByteToPrevSeg(endOfLog, endLogSegNo, wal_segment_size);
  XLByteToSeg(endOfLog, startLogSegNo, wal_segment_size);

     
                                                                             
                                                                             
                                                                             
                          
     
  if (endLogSegNo == startLogSegNo)
  {
       
                                                    
       
                                                              
                                                                         
                                     
       
    XLogFileCopy(endLogSegNo, endTLI, endLogSegNo, XLogSegmentOffset(endOfLog, wal_segment_size));
  }
  else
  {
       
                                                                          
                                    
       
    bool use_existent = true;
    int fd;

    fd = XLogFileInit(startLogSegNo, &use_existent, true);

    if (close(fd))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", XLogFileNameP(ThisTimeLineID, startLogSegNo))));
    }
  }

     
                                                                          
                          
     
  XLogFileName(xlogfname, ThisTimeLineID, startLogSegNo, wal_segment_size);
  XLogArchiveCleanup(xlogfname);

     
                                                                           
                                                           
     
  if (standby_signal_file_found)
  {
    durable_unlink(STANDBY_SIGNAL_FILE, FATAL);
  }

  if (recovery_signal_file_found)
  {
    durable_unlink(RECOVERY_SIGNAL_FILE, FATAL);
  }

  ereport(LOG, (errmsg("archive recovery complete")));
}

   
                                      
   
                                                                             
                                                                        
                                                                               
               
   
static bool
getRecordTimestamp(XLogReaderState *record, TimestampTz *recordXtime)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
  uint8 xact_info = info & XLOG_XACT_OPMASK;
  uint8 rmid = XLogRecGetRmid(record);

  if (rmid == RM_XLOG_ID && info == XLOG_RESTORE_POINT)
  {
    *recordXtime = ((xl_restore_point *)XLogRecGetData(record))->rp_time;
    return true;
  }
  if (rmid == RM_XACT_ID && (xact_info == XLOG_XACT_COMMIT || xact_info == XLOG_XACT_COMMIT_PREPARED))
  {
    *recordXtime = ((xl_xact_commit *)XLogRecGetData(record))->xact_time;
    return true;
  }
  if (rmid == RM_XACT_ID && (xact_info == XLOG_XACT_ABORT || xact_info == XLOG_XACT_ABORT_PREPARED))
  {
    *recordXtime = ((xl_xact_abort *)XLogRecGetData(record))->xact_time;
    return true;
  }
  return false;
}

   
                                                                        
                                                     
   
                                                                       
                                                                           
                                
   
static bool
recoveryStopsBefore(XLogReaderState *record)
{
  bool stopsHere = false;
  uint8 xact_info;
  bool isCommit;
  TimestampTz recordXtime = 0;
  TransactionId recordXid;

     
                                                                           
                                
     
  if (!ArchiveRecoveryRequested)
  {
    return false;
  }

                                                               
  if (recoveryTarget == RECOVERY_TARGET_IMMEDIATE && reachedConsistency)
  {
    ereport(LOG, (errmsg("recovery stopping after reaching consistency")));

    recoveryStopAfter = false;
    recoveryStopXid = InvalidTransactionId;
    recoveryStopLSN = InvalidXLogRecPtr;
    recoveryStopTime = 0;
    recoveryStopName[0] = '\0';
    return true;
  }

                                            
  if (recoveryTarget == RECOVERY_TARGET_LSN && !recoveryTargetInclusive && record->ReadRecPtr >= recoveryTargetLSN)
  {
    recoveryStopAfter = false;
    recoveryStopXid = InvalidTransactionId;
    recoveryStopLSN = record->ReadRecPtr;
    recoveryStopTime = 0;
    recoveryStopName[0] = '\0';
    ereport(LOG, (errmsg("recovery stopping before WAL location (LSN) \"%X/%X\"", (uint32)(recoveryStopLSN >> 32), (uint32)recoveryStopLSN)));
    return true;
  }

                                                                           
  if (XLogRecGetRmid(record) != RM_XACT_ID)
  {
    return false;
  }

  xact_info = XLogRecGetInfo(record) & XLOG_XACT_OPMASK;

  if (xact_info == XLOG_XACT_COMMIT)
  {
    isCommit = true;
    recordXid = XLogRecGetXid(record);
  }
  else if (xact_info == XLOG_XACT_COMMIT_PREPARED)
  {
    xl_xact_commit *xlrec = (xl_xact_commit *)XLogRecGetData(record);
    xl_xact_parsed_commit parsed;

    isCommit = true;
    ParseCommitRecord(XLogRecGetInfo(record), xlrec, &parsed);
    recordXid = parsed.twophase_xid;
  }
  else if (xact_info == XLOG_XACT_ABORT)
  {
    isCommit = false;
    recordXid = XLogRecGetXid(record);
  }
  else if (xact_info == XLOG_XACT_ABORT_PREPARED)
  {
    xl_xact_abort *xlrec = (xl_xact_abort *)XLogRecGetData(record);
    xl_xact_parsed_abort parsed;

    isCommit = false;
    ParseAbortRecord(XLogRecGetInfo(record), xlrec, &parsed);
    recordXid = parsed.twophase_xid;
  }
  else
  {
    return false;
  }

  if (recoveryTarget == RECOVERY_TARGET_XID && !recoveryTargetInclusive)
  {
       
                                                                    
                     
       
                                                                      
                                                                        
                                                                           
                          
       
    stopsHere = (recordXid == recoveryTargetXid);
  }

     
                                                                           
                                                                            
                                                                   
     
  if (getRecordTimestamp(record, &recordXtime) && recoveryTarget == RECOVERY_TARGET_TIME)
  {
       
                                                                          
                                                                       
                                     
       
    if (recoveryTargetInclusive)
    {
      stopsHere = (recordXtime > recoveryTargetTime);
    }
    else
    {
      stopsHere = (recordXtime >= recoveryTargetTime);
    }
  }

  if (stopsHere)
  {
    recoveryStopAfter = false;
    recoveryStopXid = recordXid;
    recoveryStopTime = recordXtime;
    recoveryStopLSN = InvalidXLogRecPtr;
    recoveryStopName[0] = '\0';

    if (isCommit)
    {
      ereport(LOG, (errmsg("recovery stopping before commit of transaction %u, time %s", recoveryStopXid, timestamptz_to_str(recoveryStopTime))));
    }
    else
    {
      ereport(LOG, (errmsg("recovery stopping before abort of transaction %u, time %s", recoveryStopXid, timestamptz_to_str(recoveryStopTime))));
    }
  }

  return stopsHere;
}

   
                                                                      
   
                                                                  
                                         
   
static bool
recoveryStopsAfter(XLogReaderState *record)
{
  uint8 info;
  uint8 xact_info;
  uint8 rmid;
  TimestampTz recordXtime;

     
                                                                           
                                
     
  if (!ArchiveRecoveryRequested)
  {
    return false;
  }

  info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
  rmid = XLogRecGetRmid(record);

     
                                                                           
                    
     
  if (recoveryTarget == RECOVERY_TARGET_NAME && rmid == RM_XLOG_ID && info == XLOG_RESTORE_POINT)
  {
    xl_restore_point *recordRestorePointData;

    recordRestorePointData = (xl_restore_point *)XLogRecGetData(record);

    if (strcmp(recordRestorePointData->rp_name, recoveryTargetName) == 0)
    {
      recoveryStopAfter = true;
      recoveryStopXid = InvalidTransactionId;
      recoveryStopLSN = InvalidXLogRecPtr;
      (void)getRecordTimestamp(record, &recoveryStopTime);
      strlcpy(recoveryStopName, recordRestorePointData->rp_name, MAXFNAMELEN);

      ereport(LOG, (errmsg("recovery stopping at restore point \"%s\", time %s", recoveryStopName, timestamptz_to_str(recoveryStopTime))));
      return true;
    }
  }

                                                
  if (recoveryTarget == RECOVERY_TARGET_LSN && recoveryTargetInclusive && record->ReadRecPtr >= recoveryTargetLSN)
  {
    recoveryStopAfter = true;
    recoveryStopXid = InvalidTransactionId;
    recoveryStopLSN = record->ReadRecPtr;
    recoveryStopTime = 0;
    recoveryStopName[0] = '\0';
    ereport(LOG, (errmsg("recovery stopping after WAL location (LSN) \"%X/%X\"", (uint32)(recoveryStopLSN >> 32), (uint32)recoveryStopLSN)));
    return true;
  }

  if (rmid != RM_XACT_ID)
  {
    return false;
  }

  xact_info = info & XLOG_XACT_OPMASK;

  if (xact_info == XLOG_XACT_COMMIT || xact_info == XLOG_XACT_COMMIT_PREPARED || xact_info == XLOG_XACT_ABORT || xact_info == XLOG_XACT_ABORT_PREPARED)
  {
    TransactionId recordXid;

                                                       
    if (getRecordTimestamp(record, &recordXtime))
    {
      SetLatestXTime(recordXtime);
    }

                                                              
    if (xact_info == XLOG_XACT_COMMIT_PREPARED)
    {
      xl_xact_commit *xlrec = (xl_xact_commit *)XLogRecGetData(record);
      xl_xact_parsed_commit parsed;

      ParseCommitRecord(XLogRecGetInfo(record), xlrec, &parsed);
      recordXid = parsed.twophase_xid;
    }
    else if (xact_info == XLOG_XACT_ABORT_PREPARED)
    {
      xl_xact_abort *xlrec = (xl_xact_abort *)XLogRecGetData(record);
      xl_xact_parsed_abort parsed;

      ParseAbortRecord(XLogRecGetInfo(record), xlrec, &parsed);
      recordXid = parsed.twophase_xid;
    }
    else
    {
      recordXid = XLogRecGetXid(record);
    }

       
                                                                    
                     
       
                                                                      
                                                                        
                                                                           
                          
       
    if (recoveryTarget == RECOVERY_TARGET_XID && recoveryTargetInclusive && recordXid == recoveryTargetXid)
    {
      recoveryStopAfter = true;
      recoveryStopXid = recordXid;
      recoveryStopTime = recordXtime;
      recoveryStopLSN = InvalidXLogRecPtr;
      recoveryStopName[0] = '\0';

      if (xact_info == XLOG_XACT_COMMIT || xact_info == XLOG_XACT_COMMIT_PREPARED)
      {
        ereport(LOG, (errmsg("recovery stopping after commit of transaction %u, time %s", recoveryStopXid, timestamptz_to_str(recoveryStopTime))));
      }
      else if (xact_info == XLOG_XACT_ABORT || xact_info == XLOG_XACT_ABORT_PREPARED)
      {
        ereport(LOG, (errmsg("recovery stopping after abort of transaction %u, time %s", recoveryStopXid, timestamptz_to_str(recoveryStopTime))));
      }
      return true;
    }
  }

                                                               
  if (recoveryTarget == RECOVERY_TARGET_IMMEDIATE && reachedConsistency)
  {
    ereport(LOG, (errmsg("recovery stopping after reaching consistency")));

    recoveryStopAfter = true;
    recoveryStopXid = InvalidTransactionId;
    recoveryStopTime = 0;
    recoveryStopLSN = InvalidXLogRecPtr;
    recoveryStopName[0] = '\0';
    return true;
  }

  return false;
}

   
                                                    
   
                                                                          
                                                                            
                                                   
   
static void
recoveryPausesHere(void)
{
                                             
  if (!LocalHotStandbyActive)
  {
    return;
  }

  ereport(LOG, (errmsg("recovery has paused"), errhint("Execute pg_wal_replay_resume() to continue.")));

  while (RecoveryIsPaused())
  {
    pg_usleep(1000000L);              
    HandleStartupProcInterrupts();
  }
}

bool
RecoveryIsPaused(void)
{
  bool recoveryPause;

  SpinLockAcquire(&XLogCtl->info_lck);
  recoveryPause = XLogCtl->recoveryPause;
  SpinLockRelease(&XLogCtl->info_lck);

  return recoveryPause;
}

void
SetRecoveryPause(bool recoveryPause)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->recoveryPause = recoveryPause;
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                                                          
                                                                              
   
                              
   
                                                                         
                                                                           
                                                                         
                                                                          
                                                                         
              
   
static bool
recoveryApplyDelay(XLogReaderState *record)
{
  uint8 xact_info;
  TimestampTz xtime;
  long msecs;

                                            
  if (recovery_min_apply_delay <= 0)
  {
    return false;
  }

                                                            
  if (!reachedConsistency)
  {
    return false;
  }

                                                    
  if (!ArchiveRecoveryRequested)
  {
    return false;
  }

     
                            
     
                                                                             
                                                                           
                                                                             
               
     
  if (XLogRecGetRmid(record) != RM_XACT_ID)
  {
    return false;
  }

  xact_info = XLogRecGetInfo(record) & XLOG_XACT_OPMASK;

  if (xact_info != XLOG_XACT_COMMIT && xact_info != XLOG_XACT_COMMIT_PREPARED)
  {
    return false;
  }

  if (!getRecordTimestamp(record, &xtime))
  {
    return false;
  }

  recoveryDelayUntilTime = TimestampTzPlusMilliseconds(xtime, recovery_min_apply_delay);

     
                                                                           
            
     
  msecs = TimestampDifferenceMilliseconds(GetCurrentTimestamp(), recoveryDelayUntilTime);
  if (msecs <= 0)
  {
    return false;
  }

  while (true)
  {
    ResetLatch(&XLogCtl->recoveryWakeupLatch);

       
                                                                        
                 
       
    HandleStartupProcInterrupts();

    if (CheckForStandbyTrigger())
    {
      break;
    }

       
                                                                      
                                                      
       
    recoveryDelayUntilTime = TimestampTzPlusMilliseconds(xtime, recovery_min_apply_delay);

       
                                                             
                              
       
    msecs = TimestampDifferenceMilliseconds(GetCurrentTimestamp(), recoveryDelayUntilTime);

    if (msecs <= 0)
    {
      break;
    }

    elog(DEBUG2, "recovery apply delay %ld milliseconds", msecs);

    (void)WaitLatch(&XLogCtl->recoveryWakeupLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, msecs, WAIT_EVENT_RECOVERY_APPLY_DELAY);
  }
  return true;
}

   
                                                           
   
                                                                            
                                                                         
                                                            
   
static void
SetLatestXTime(TimestampTz xtime)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->recoveryLastXTime = xtime;
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                                            
   
TimestampTz
GetLatestXTime(void)
{
  TimestampTz xtime;

  SpinLockAcquire(&XLogCtl->info_lck);
  xtime = XLogCtl->recoveryLastXTime;
  SpinLockRelease(&XLogCtl->info_lck);

  return xtime;
}

   
                                                             
   
                                                                            
                         
   
static void
SetCurrentChunkStartTime(TimestampTz xtime)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->currentChunkStartTime = xtime;
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                                            
                                                                       
   
TimestampTz
GetCurrentChunkReplayStartTime(void)
{
  TimestampTz xtime;

  SpinLockAcquire(&XLogCtl->info_lck);
  xtime = XLogCtl->currentChunkStartTime;
  SpinLockRelease(&XLogCtl->info_lck);

  return xtime;
}

   
                                                                     
                                                                        
   
void
GetXLogReceiptTime(TimestampTz *rtime, bool *fromStream)
{
     
                                                                             
                                      
     
  Assert(InRecovery);

  *rtime = XLogReceiptTime;
  *fromStream = (XLogReceiptSource == XLOG_FROM_STREAM);
}

   
                                                                          
               
   
#define RecoveryRequiresIntParameter(param_name, currValue, minValue)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((currValue) < (minValue))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("hot standby is not possible because "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
                                                                       "%s = %d is a lower setting than on the master server "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
                                                                       "(its value was %d)",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
                                                                    param_name, currValue, minValue)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  } while (0)

   
                                                                          
                                              
   
                                                                     
                                                                         
                                                        
   
static void
CheckRequiredParameterValues(void)
{
     
                                                                             
                
     
  if (ArchiveRecoveryRequested && ControlFile->wal_level == WAL_LEVEL_MINIMAL)
  {
    ereport(WARNING, (errmsg("WAL was generated with wal_level=minimal, data may be missing"), errhint("This happens if you temporarily set wal_level=minimal without taking a new base backup.")));
  }

     
                                                                            
                                                              
     
  if (ArchiveRecoveryRequested && EnableHotStandby)
  {
    if (ControlFile->wal_level < WAL_LEVEL_REPLICA)
    {
      ereport(ERROR, (errmsg("hot standby is not possible because wal_level was not set to \"replica\" or higher on the master server"), errhint("Either set wal_level to \"replica\" on the master, or turn off hot_standby here.")));
    }

                                                                  
    RecoveryRequiresIntParameter("max_connections", MaxConnections, ControlFile->MaxConnections);
    RecoveryRequiresIntParameter("max_worker_processes", max_worker_processes, ControlFile->max_worker_processes);
    RecoveryRequiresIntParameter("max_wal_senders", max_wal_senders, ControlFile->max_wal_senders);
    RecoveryRequiresIntParameter("max_prepared_transactions", max_prepared_xacts, ControlFile->max_prepared_xacts);
    RecoveryRequiresIntParameter("max_locks_per_transaction", max_locks_per_xact, ControlFile->max_locks_per_xact);
  }
}

   
                                                                            
   
void
StartupXLOG(void)
{
  XLogCtlInsert *Insert;
  CheckPoint checkPoint;
  bool wasShutdown;
  bool reachedStopPoint = false;
  bool haveBackupLabel = false;
  bool haveTblspcMap = false;
  XLogRecPtr RecPtr, checkPointLoc, EndOfLog;
  TimeLineID EndOfLogTLI;
  TimeLineID PrevTimeLineID;
  XLogRecord *record;
  TransactionId oldestActiveXID;
  bool backupEndRequired = false;
  bool backupFromStandby = false;
  DBState dbstate_at_startup;
  XLogReaderState *xlogreader;
  XLogPageReadPrivate private;
  bool fast_promoted = false;
  struct stat st;

     
                                                                            
                                                               
     
  Assert(AuxProcessResourceOwner != NULL);
  Assert(CurrentResourceOwner == NULL || CurrentResourceOwner == AuxProcessResourceOwner);
  CurrentResourceOwner = AuxProcessResourceOwner;

     
                                     
     
  if (ControlFile->state < DB_SHUTDOWNED || ControlFile->state > DB_IN_PRODUCTION || !XRecOffIsValid(ControlFile->checkPoint))
  {
    ereport(FATAL, (errmsg("control file contains invalid data")));
  }

  if (ControlFile->state == DB_SHUTDOWNED)
  {
                                                                          
    ereport(IsPostmasterEnvironment ? LOG : NOTICE, (errmsg("database system was shut down at %s", str_time(ControlFile->time))));
  }
  else if (ControlFile->state == DB_SHUTDOWNED_IN_RECOVERY)
  {
    ereport(LOG, (errmsg("database system was shut down in recovery at %s", str_time(ControlFile->time))));
  }
  else if (ControlFile->state == DB_SHUTDOWNING)
  {
    ereport(LOG, (errmsg("database system shutdown was interrupted; last known up at %s", str_time(ControlFile->time))));
  }
  else if (ControlFile->state == DB_IN_CRASH_RECOVERY)
  {
    ereport(LOG, (errmsg("database system was interrupted while in recovery at %s", str_time(ControlFile->time)), errhint("This probably means that some data is corrupted and"
                                                                                                                          " you will have to use the last backup for recovery.")));
  }
  else if (ControlFile->state == DB_IN_ARCHIVE_RECOVERY)
  {
    ereport(LOG, (errmsg("database system was interrupted while in recovery at log time %s", str_time(ControlFile->checkPointCopy.time)), errhint("If this has occurred more than once some data might be corrupted"
                                                                                                                                                  " and you might need to choose an earlier recovery target.")));
  }
  else if (ControlFile->state == DB_IN_PRODUCTION)
  {
    ereport(LOG, (errmsg("database system was interrupted; last known up at %s", str_time(ControlFile->time))));
  }

                                                                          
#ifdef XLOG_REPLAY_DELAY
  if (ControlFile->state != DB_SHUTDOWNED)
  {
    pg_usleep(60000000L);
  }
#endif

     
                                                                         
                                                                            
                                         
     
  ValidateXLOGDirectoryStructure();

               
                                                            
                                                                          
                                                                       
                                                                            
                     
                                                                        
                                                                           
                                                                         
                                                                        
                                                                 
              
     
  if (ControlFile->state != DB_SHUTDOWNED && ControlFile->state != DB_SHUTDOWNED_IN_RECOVERY)
  {
    RemoveTempXlogFiles();
    SyncDataDirectory();
  }

     
                                                                            
                                            
     
  if (ControlFile->minRecoveryPointTLI > ControlFile->checkPointCopy.ThisTimeLineID)
  {
    recoveryTargetTLI = ControlFile->minRecoveryPointTLI;
  }
  else
  {
    recoveryTargetTLI = ControlFile->checkPointCopy.ThisTimeLineID;
  }

     
                                                                         
     
  readRecoverySignalFile();
  validateRecoveryParameters();

  if (ArchiveRecoveryRequested)
  {
    if (StandbyModeRequested)
    {
      ereport(LOG, (errmsg("entering standby mode")));
    }
    else if (recoveryTarget == RECOVERY_TARGET_XID)
    {
      ereport(LOG, (errmsg("starting point-in-time recovery to XID %u", recoveryTargetXid)));
    }
    else if (recoveryTarget == RECOVERY_TARGET_TIME)
    {
      ereport(LOG, (errmsg("starting point-in-time recovery to %s", timestamptz_to_str(recoveryTargetTime))));
    }
    else if (recoveryTarget == RECOVERY_TARGET_NAME)
    {
      ereport(LOG, (errmsg("starting point-in-time recovery to \"%s\"", recoveryTargetName)));
    }
    else if (recoveryTarget == RECOVERY_TARGET_LSN)
    {
      ereport(LOG, (errmsg("starting point-in-time recovery to WAL location (LSN) \"%X/%X\"", (uint32)(recoveryTargetLSN >> 32), (uint32)recoveryTargetLSN)));
    }
    else if (recoveryTarget == RECOVERY_TARGET_IMMEDIATE)
    {
      ereport(LOG, (errmsg("starting point-in-time recovery to earliest consistent point")));
    }
    else
    {
      ereport(LOG, (errmsg("starting archive recovery")));
    }
  }

     
                                                                       
               
     
  if (ArchiveRecoveryRequested)
  {
    OwnLatch(&XLogCtl->recoveryWakeupLatch);
  }

                                   
  MemSet(&private, 0, sizeof(XLogPageReadPrivate));
  xlogreader = XLogReaderAllocate(wal_segment_size, &XLogPageRead, &private);
  if (!xlogreader)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed while allocating a WAL reading processor.")));
  }
  xlogreader->system_identifier = ControlFile->system_identifier;

     
                                                                           
                                                                          
                                                                             
                                                                     
                                                                  
     
  replay_image_masked = (char *)palloc(BLCKSZ);
  master_image_masked = (char *)palloc(BLCKSZ);

  if (read_backup_label(&checkPointLoc, &backupEndRequired, &backupFromStandby))
  {
    List *tablespaces = NIL;

       
                                                                      
                                                                           
                                  
       
    InArchiveRecovery = true;
    if (StandbyModeRequested)
    {
      StandbyMode = true;
    }

       
                                                                         
                                                                   
       
    record = ReadCheckpointRecord(xlogreader, checkPointLoc, 0, true);
    if (record != NULL)
    {
      memcpy(&checkPoint, XLogRecGetData(xlogreader), sizeof(CheckPoint));
      wasShutdown = ((record->xl_info & ~XLR_INFO_MASK) == XLOG_CHECKPOINT_SHUTDOWN);
      ereport(DEBUG1, (errmsg("checkpoint record is at %X/%X", (uint32)(checkPointLoc >> 32), (uint32)checkPointLoc)));
      InRecovery = true;                                        

         
                                                                       
                                                                    
                                                                  
                                
         
      if (checkPoint.redo < checkPointLoc)
      {
        if (!ReadRecord(xlogreader, checkPoint.redo, LOG, false))
        {
          ereport(FATAL, (errmsg("could not find redo location referenced by checkpoint record"), errhint("If you are restoring from a backup, touch \"%s/recovery.signal\" and add required recovery options.\n"
                                                                                                          "If you are not restoring from a backup, try removing the file \"%s/backup_label\".\n"
                                                                                                          "Be careful: removing \"%s/backup_label\" will result in a corrupt cluster if restoring from a backup.",
                                                                                                      DataDir, DataDir, DataDir)));
        }
      }
    }
    else
    {
      ereport(FATAL, (errmsg("could not locate required checkpoint record"), errhint("If you are restoring from a backup, touch \"%s/recovery.signal\" and add required recovery options.\n"
                                                                                     "If you are not restoring from a backup, try removing the file \"%s/backup_label\".\n"
                                                                                     "Be careful: removing \"%s/backup_label\" will result in a corrupt cluster if restoring from a backup.",
                                                                                 DataDir, DataDir, DataDir)));
      wasShutdown = false;                          
    }

                                                                      
    if (read_tablespace_map(&tablespaces))
    {
      ListCell *lc;

      foreach (lc, tablespaces)
      {
        tablespaceinfo *ti = lfirst(lc);
        char *linkloc;

        linkloc = psprintf("pg_tblspc/%s", ti->oid);

           
                                                                     
                         
           
        remove_tablespace_symlink(linkloc);

        if (symlink(ti->path, linkloc) < 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not create symbolic link \"%s\": %m", linkloc)));
        }

        pfree(ti->oid);
        pfree(ti->path);
        pfree(ti);
      }

                                       
      haveTblspcMap = true;
    }

                                     
    haveBackupLabel = true;
  }
  else
  {
       
                                                                          
                                                                         
                                                                      
                                                                      
                                                                          
                                                                          
                                                                      
                                                  
       
    if (stat(TABLESPACE_MAP, &st) == 0)
    {
      unlink(TABLESPACE_MAP_OLD);
      if (durable_rename(TABLESPACE_MAP, TABLESPACE_MAP_OLD, DEBUG1) == 0)
      {
        ereport(LOG, (errmsg("ignoring file \"%s\" because no file \"%s\" exists", TABLESPACE_MAP, BACKUP_LABEL_FILE), errdetail("File \"%s\" was renamed to \"%s\".", TABLESPACE_MAP, TABLESPACE_MAP_OLD)));
      }
      else
      {
        ereport(LOG, (errmsg("ignoring file \"%s\" because no file \"%s\" exists", TABLESPACE_MAP, BACKUP_LABEL_FILE), errdetail("Could not rename file \"%s\" to \"%s\": %m.", TABLESPACE_MAP, TABLESPACE_MAP_OLD)));
      }
    }

       
                                                                       
                                                                           
                                                                    
                                                                           
                                                                         
                                                                      
             
       
                                                                     
                                                                       
                            
       
                                                                         
                                                                     
                                                                          
       
    if (ArchiveRecoveryRequested && (ControlFile->minRecoveryPoint != InvalidXLogRecPtr || ControlFile->backupEndRequired || ControlFile->backupEndPoint != InvalidXLogRecPtr || ControlFile->state == DB_SHUTDOWNED))
    {
      InArchiveRecovery = true;
      if (StandbyModeRequested)
      {
        StandbyMode = true;
      }
    }

                                               
    checkPointLoc = ControlFile->checkPoint;
    RedoStartLSN = ControlFile->checkPointCopy.redo;
    record = ReadCheckpointRecord(xlogreader, checkPointLoc, 1, true);
    if (record != NULL)
    {
      ereport(DEBUG1, (errmsg("checkpoint record is at %X/%X", (uint32)(checkPointLoc >> 32), (uint32)checkPointLoc)));
    }
    else
    {
         
                                                                        
                                                                         
                                                                  
                                                 
         
      ereport(PANIC, (errmsg("could not locate a valid checkpoint record")));
    }
    memcpy(&checkPoint, XLogRecGetData(xlogreader), sizeof(CheckPoint));
    wasShutdown = ((record->xl_info & ~XLR_INFO_MASK) == XLOG_CHECKPOINT_SHUTDOWN);
  }

     
                                                                           
                                                                         
                                                                             
                                                                       
                                                                          
                                                                      
                                                                            
                                                                        
                                                                      
               
     
  RelationCacheInitFileRemove();

     
                                                                     
                                                                           
                                                                      
     
  Assert(expectedTLEs);                                          
                                    
  if (tliOfPointInHistory(checkPointLoc, expectedTLEs) != checkPoint.ThisTimeLineID)
  {
    XLogRecPtr switchpoint;

       
                                                                          
                                   
       
    switchpoint = tliSwitchPoint(ControlFile->checkPointCopy.ThisTimeLineID, expectedTLEs, NULL);
    ereport(FATAL, (errmsg("requested timeline %u is not a child of this server's history", recoveryTargetTLI), errdetail("Latest checkpoint is at %X/%X on timeline %u, but in the history of the requested timeline, the server forked off from that timeline at %X/%X.", (uint32)(ControlFile->checkPoint >> 32), (uint32)ControlFile->checkPoint, ControlFile->checkPointCopy.ThisTimeLineID, (uint32)(switchpoint >> 32), (uint32)switchpoint)));
  }

     
                                                                       
                   
     
  if (!XLogRecPtrIsInvalid(ControlFile->minRecoveryPoint) && tliOfPointInHistory(ControlFile->minRecoveryPoint - 1, expectedTLEs) != ControlFile->minRecoveryPointTLI)
  {
    ereport(FATAL, (errmsg("requested timeline %u does not contain minimum recovery point %X/%X on timeline %u", recoveryTargetTLI, (uint32)(ControlFile->minRecoveryPoint >> 32), (uint32)ControlFile->minRecoveryPoint, ControlFile->minRecoveryPointTLI)));
  }

  LastRec = RecPtr = checkPointLoc;

  ereport(DEBUG1, (errmsg_internal("redo record is at %X/%X; shutdown %s", (uint32)(checkPoint.redo >> 32), (uint32)checkPoint.redo, wasShutdown ? "true" : "false")));
  ereport(DEBUG1, (errmsg_internal("next transaction ID: " UINT64_FORMAT "; next OID: %u", U64FromFullTransactionId(checkPoint.nextFullXid), checkPoint.nextOid)));
  ereport(DEBUG1, (errmsg_internal("next MultiXactId: %u; next MultiXactOffset: %u", checkPoint.nextMulti, checkPoint.nextMultiOffset)));
  ereport(DEBUG1, (errmsg_internal("oldest unfrozen transaction ID: %u, in database %u", checkPoint.oldestXid, checkPoint.oldestXidDB)));
  ereport(DEBUG1, (errmsg_internal("oldest MultiXactId: %u, in database %u", checkPoint.oldestMulti, checkPoint.oldestMultiDB)));
  ereport(DEBUG1, (errmsg_internal("commit timestamp Xid oldest/newest: %u/%u", checkPoint.oldestCommitTsXid, checkPoint.newestCommitTsXid)));
  if (!TransactionIdIsNormal(XidFromFullTransactionId(checkPoint.nextFullXid)))
  {
    ereport(PANIC, (errmsg("invalid next transaction ID")));
  }

                                                                     
  ShmemVariableCache->nextFullXid = checkPoint.nextFullXid;
  ShmemVariableCache->nextOid = checkPoint.nextOid;
  ShmemVariableCache->oidCount = 0;
  MultiXactSetNextMXact(checkPoint.nextMulti, checkPoint.nextMultiOffset);
  AdvanceOldestClogXid(checkPoint.oldestXid);
  SetTransactionIdLimit(checkPoint.oldestXid, checkPoint.oldestXidDB);
  SetMultiXactIdLimit(checkPoint.oldestMulti, checkPoint.oldestMultiDB, true);
  SetCommitTsLimit(checkPoint.oldestCommitTsXid, checkPoint.newestCommitTsXid);
  XLogCtl->ckptFullXid = checkPoint.nextFullXid;

     
                                                                     
                         
     
  StartupReplicationSlots();

     
                                                                         
                            
     
  StartupReorderBuffer();

     
                                                                      
                  
     
  StartupMultiXact();

     
                                                                           
                                                                           
                                                                          
                                                                       
     
  if (ControlFile->track_commit_timestamp)
  {
    StartupCommitTs();
  }

     
                                                                            
     
  StartupReplicationOrigin();

     
                                                                          
                                                                          
                                                
     
  if (ControlFile->state == DB_SHUTDOWNED)
  {
    XLogCtl->unloggedLSN = ControlFile->unloggedLSN;
  }
  else
  {
    XLogCtl->unloggedLSN = FirstNormalUnloggedLSN;
  }

     
                                                                            
                                                                          
                        
     
  ThisTimeLineID = checkPoint.ThisTimeLineID;

     
                                                                            
                                                                             
                                                                             
                                                                            
                                                                       
                                                                             
                                                                             
                                                                           
                                                                             
                   
     
  restoreTimeLineHistoryFiles(ThisTimeLineID, recoveryTargetTLI);

     
                                                                            
                                                                        
                                                                            
                                                                             
                                                                           
                                    
     
  restoreTwoPhaseData();

  lastFullPageWrites = checkPoint.fullPageWrites;

  RedoRecPtr = XLogCtl->RedoRecPtr = XLogCtl->Insert.RedoRecPtr = checkPoint.redo;
  doPageWrites = lastFullPageWrites;

  if (RecPtr < checkPoint.redo)
  {
    ereport(PANIC, (errmsg("invalid redo in checkpoint record")));
  }

     
                                                                         
                                                                            
                                     
     
  if (checkPoint.redo < RecPtr)
  {
    if (wasShutdown)
    {
      ereport(PANIC, (errmsg("invalid redo record in shutdown checkpoint")));
    }
    InRecovery = true;
  }
  else if (ControlFile->state != DB_SHUTDOWNED)
  {
    InRecovery = true;
  }
  else if (ArchiveRecoveryRequested)
  {
                                                                
    InRecovery = true;
  }

     
                                                               
     
  abortedRecPtr = InvalidXLogRecPtr;
  missingContrecPtr = InvalidXLogRecPtr;

            
  if (InRecovery)
  {
    int rmid;

       
                                                                        
                                                                           
                                                                       
                            
       
    dbstate_at_startup = ControlFile->state;
    if (InArchiveRecovery)
    {
      ControlFile->state = DB_IN_ARCHIVE_RECOVERY;

      SpinLockAcquire(&XLogCtl->info_lck);
      XLogCtl->SharedRecoveryState = RECOVERY_STATE_ARCHIVE;
      SpinLockRelease(&XLogCtl->info_lck);
    }
    else
    {
      ereport(LOG, (errmsg("database system was not properly shut down; "
                           "automatic recovery in progress")));
      if (recoveryTargetTLI > ControlFile->checkPointCopy.ThisTimeLineID)
      {
        ereport(LOG, (errmsg("crash recovery starts in timeline %u "
                             "and has target timeline %u",
                         ControlFile->checkPointCopy.ThisTimeLineID, recoveryTargetTLI)));
      }
      ControlFile->state = DB_IN_CRASH_RECOVERY;

      SpinLockAcquire(&XLogCtl->info_lck);
      XLogCtl->SharedRecoveryState = RECOVERY_STATE_CRASH;
      SpinLockRelease(&XLogCtl->info_lck);
    }
    ControlFile->checkPoint = checkPointLoc;
    ControlFile->checkPointCopy = checkPoint;
    if (InArchiveRecovery)
    {
                                                      
      if (ControlFile->minRecoveryPoint < checkPoint.redo)
      {
        ControlFile->minRecoveryPoint = checkPoint.redo;
        ControlFile->minRecoveryPointTLI = checkPoint.ThisTimeLineID;
      }
    }

       
                                                                           
       
                                                                          
                                                                        
                                                                         
                                                                           
                                                                   
                                                                        
                                                                   
                                     
       
                                                                          
                                                     
       
    if (haveBackupLabel)
    {
      ControlFile->backupStartPoint = checkPoint.redo;
      ControlFile->backupEndRequired = backupEndRequired;

      if (backupFromStandby)
      {
        if (dbstate_at_startup != DB_IN_ARCHIVE_RECOVERY && dbstate_at_startup != DB_SHUTDOWNED_IN_RECOVERY)
        {
          ereport(FATAL, (errmsg("backup_label contains data inconsistent with control file"), errhint("This means that the backup is corrupted and you will "
                                                                                                       "have to use another backup for recovery.")));
        }
        ControlFile->backupEndPoint = ControlFile->minRecoveryPoint;
      }
    }
    ControlFile->time = (pg_time_t)time(NULL);
                                                                      
    UpdateControlFile();

       
                                                                        
                                                                          
                                                                    
                                                                          
                                                                         
                                                                        
                                                                  
                                                      
       
    if (InArchiveRecovery)
    {
      minRecoveryPoint = ControlFile->minRecoveryPoint;
      minRecoveryPointTLI = ControlFile->minRecoveryPointTLI;
    }
    else
    {
      minRecoveryPoint = InvalidXLogRecPtr;
      minRecoveryPointTLI = 0;
    }

       
                                                                    
       
    pgstat_reset_all();

       
                                                                        
                                                                        
                                                                        
                                                                          
                                                                          
                                                                 
       
    if (haveBackupLabel)
    {
      unlink(BACKUP_LABEL_OLD);
      durable_rename(BACKUP_LABEL_FILE, BACKUP_LABEL_OLD, FATAL);
    }

       
                                                                     
                                                                       
                                                                         
                                                                      
                                         
       
    if (haveTblspcMap)
    {
      unlink(TABLESPACE_MAP_OLD);
      durable_rename(TABLESPACE_MAP, TABLESPACE_MAP_OLD, FATAL);
    }

                                                                     
    CheckRequiredParameterValues();

       
                                                                           
                                                               
                                                                          
                                         
       
    ResetUnloggedRelations(UNLOGGED_RELATION_CLEANUP);

       
                                                                           
                                   
       
    DeleteAllExportedSnapshotFiles();

       
                                                                        
                                                                        
                                                                     
                                 
       
    if (ArchiveRecoveryRequested && EnableHotStandby)
    {
      TransactionId *xids;
      int nxids;

      ereport(DEBUG1, (errmsg("initializing for hot standby")));

      InitRecoveryTransactionEnvironment();

      if (wasShutdown)
      {
        oldestActiveXID = PrescanPreparedTransactions(&xids, &nxids);
      }
      else
      {
        oldestActiveXID = checkPoint.oldestActiveXid;
      }
      Assert(TransactionIdIsValid(oldestActiveXID));

                                                                      
      ProcArrayInitRecovery(XidFromFullTransactionId(ShmemVariableCache->nextFullXid));

         
                                                                     
                                                                        
                                                                 
         
      StartupCLOG();
      StartupSUBTRANS(oldestActiveXID);

         
                                                                   
                                                                        
                                                                       
                                                             
         
      if (wasShutdown)
      {
        RunningTransactionsData running;
        TransactionId latestCompletedXid;

           
                                                                   
                                                                   
                                                                   
                                                                       
           
        running.xcnt = nxids;
        running.subxcnt = 0;
        running.subxid_overflow = false;
        running.nextXid = XidFromFullTransactionId(checkPoint.nextFullXid);
        running.oldestRunningXid = oldestActiveXID;
        latestCompletedXid = XidFromFullTransactionId(checkPoint.nextFullXid);
        TransactionIdRetreat(latestCompletedXid);
        Assert(TransactionIdIsNormal(latestCompletedXid));
        running.latestCompletedXid = latestCompletedXid;
        running.xids = xids;

        ProcArrayApplyRecoveryInfo(&running);

        StandbyRecoverPreparedTransactions();
      }
    }

                                      
    for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
    {
      if (RmgrTable[rmid].rm_startup != NULL)
      {
        RmgrTable[rmid].rm_startup();
      }
    }

       
                                                                           
                                                                           
                                                                 
       
    SpinLockAcquire(&XLogCtl->info_lck);
    if (checkPoint.redo < RecPtr)
    {
      XLogCtl->replayEndRecPtr = checkPoint.redo;
    }
    else
    {
      XLogCtl->replayEndRecPtr = EndRecPtr;
    }
    XLogCtl->replayEndTLI = ThisTimeLineID;
    XLogCtl->lastReplayedEndRecPtr = XLogCtl->replayEndRecPtr;
    XLogCtl->lastReplayedTLI = XLogCtl->replayEndTLI;
    XLogCtl->recoveryLastXTime = 0;
    XLogCtl->currentChunkStartTime = 0;
    XLogCtl->recoveryPause = false;
    SpinLockRelease(&XLogCtl->info_lck);

                                                      
    XLogReceiptTime = GetCurrentTimestamp();

       
                                                                         
                                                                      
                                                                    
                                                                          
                                                                          
              
       
                                                                     
                                                                    
                                                                    
       
    if (ArchiveRecoveryRequested && IsUnderPostmaster)
    {
      PublishStartupProcessInformation();
      EnableSyncRequestForwarding();
      SendPostmasterSignal(PMSIGNAL_RECOVERY_STARTED);
      bgwriterLaunched = true;
    }

       
                                                                   
                
       
    CheckRecoveryConsistency();

       
                                                                          
                                            
       
    if (checkPoint.redo < RecPtr)
    {
                                      
      record = ReadRecord(xlogreader, checkPoint.redo, PANIC, false);
    }
    else
    {
                                                          
      record = ReadRecord(xlogreader, InvalidXLogRecPtr, LOG, false);
    }

    if (record != NULL)
    {
      ErrorContextCallback errcallback;
      TimestampTz xtime;

      InRedo = true;

      ereport(LOG, (errmsg("redo starts at %X/%X", (uint32)(ReadRecPtr >> 32), (uint32)ReadRecPtr)));

         
                              
         
      do
      {
        bool switchedTLI = false;

#ifdef WAL_DEBUG
        if (XLOG_DEBUG || (rmid == RM_XACT_ID && trace_recovery_messages <= DEBUG2) || (rmid != RM_XACT_ID && trace_recovery_messages <= DEBUG3))
        {
          StringInfoData buf;

          initStringInfo(&buf);
          appendStringInfo(&buf, "REDO @ %X/%X; LSN %X/%X: ", (uint32)(ReadRecPtr >> 32), (uint32)ReadRecPtr, (uint32)(EndRecPtr >> 32), (uint32)EndRecPtr);
          xlog_outrec(&buf, xlogreader);
          appendStringInfoString(&buf, " - ");
          xlog_outdesc(&buf, xlogreader);
          elog(LOG, "%s", buf.data);
          pfree(buf.data);
        }
#endif

                                                         
        HandleStartupProcInterrupts();

           
                                                                       
                               
           
                                                                       
                                                                    
                                                                  
                                                                  
                                                                
                                                                  
                                                                      
                                                          
           
        if (((volatile XLogCtlData *)XLogCtl)->recoveryPause)
        {
          recoveryPausesHere();
        }

           
                                                
           
        if (recoveryStopsBefore(xlogreader))
        {
          reachedStopPoint = true;                
          break;
        }

           
                                                                      
                                   
           
        if (recoveryApplyDelay(xlogreader))
        {
             
                                                                  
                                                                   
                                                                 
                                                                   
                   
             
          if (((volatile XLogCtlData *)XLogCtl)->recoveryPause)
          {
            recoveryPausesHere();
          }
        }

                                                         
        errcallback.callback = rm_redo_error_callback;
        errcallback.arg = (void *)xlogreader;
        errcallback.previous = error_context_stack;
        error_context_stack = &errcallback;

           
                                                                   
                
           
        AdvanceNextFullTransactionIdPastXid(record->xl_xid);

           
                                                                     
                                                                 
                                                                   
                                                                   
                                                               
                                                                     
                             
           
        if (record->xl_rmid == RM_XLOG_ID)
        {
          TimeLineID newTLI = ThisTimeLineID;
          TimeLineID prevTLI = ThisTimeLineID;
          uint8 info = record->xl_info & ~XLR_INFO_MASK;

          if (info == XLOG_CHECKPOINT_SHUTDOWN)
          {
            CheckPoint checkPoint;

            memcpy(&checkPoint, XLogRecGetData(xlogreader), sizeof(CheckPoint));
            newTLI = checkPoint.ThisTimeLineID;
            prevTLI = checkPoint.PrevTimeLineID;
          }
          else if (info == XLOG_END_OF_RECOVERY)
          {
            xl_end_of_recovery xlrec;

            memcpy(&xlrec, XLogRecGetData(xlogreader), sizeof(xl_end_of_recovery));
            newTLI = xlrec.ThisTimeLineID;
            prevTLI = xlrec.PrevTimeLineID;
          }

          if (newTLI != ThisTimeLineID)
          {
                                                          
            checkTimeLineSwitch(EndRecPtr, newTLI, prevTLI);

                                                                  
            ThisTimeLineID = newTLI;
            switchedTLI = true;
          }
        }

           
                                                                       
                                                                     
           
        SpinLockAcquire(&XLogCtl->info_lck);
        XLogCtl->replayEndRecPtr = EndRecPtr;
        XLogCtl->replayEndTLI = ThisTimeLineID;
        SpinLockRelease(&XLogCtl->info_lck);

           
                                                                   
                       
           
        if (standbyState >= STANDBY_INITIALIZED && TransactionIdIsValid(record->xl_xid))
        {
          RecordKnownAssignedTransactionIds(record->xl_xid);
        }

                                             
        RmgrTable[record->xl_rmid].rm_redo(xlogreader);

           
                                                                      
                                                                       
                                                                       
                   
           
        if ((record->xl_info & XLR_CHECK_CONSISTENCY) != 0)
        {
          checkXLogConsistency(xlogreader);
        }

                                         
        error_context_stack = errcallback.previous;

           
                                                                   
                                  
           
        SpinLockAcquire(&XLogCtl->info_lck);
        XLogCtl->lastReplayedEndRecPtr = EndRecPtr;
        XLogCtl->lastReplayedTLI = ThisTimeLineID;
        SpinLockRelease(&XLogCtl->info_lck);

           
                                                                       
                                                          
                                                                  
           
        if (doRequestWalReceiverReply)
        {
          doRequestWalReceiverReply = false;
          WalRcvForceReply();
        }

                                                          
        LastRec = ReadRecPtr;

                                                                 
        CheckRecoveryConsistency();

                                        
        if (switchedTLI)
        {
             
                                                                  
                                                             
                       
             
          RemoveNonParentXlogFiles(EndRecPtr, ThisTimeLineID);

             
                                                                   
                       
             
          if (switchedTLI && AllowCascadeReplication())
          {
            WalSndWakeup();
          }
        }

                                                               
        if (recoveryStopsAfter(xlogreader))
        {
          reachedStopPoint = true;
          break;
        }

                                                    
        record = ReadRecord(xlogreader, InvalidXLogRecPtr, LOG, false);
      } while (record != NULL);

         
                                     
         

      if (reachedStopPoint)
      {
        if (!reachedConsistency)
        {
          ereport(FATAL, (errmsg("requested recovery stop point is before consistent recovery point")));
        }

           
                                                                       
                                                                      
                                                              
                                                  
           
        switch (recoveryTargetAction)
        {
        case RECOVERY_TARGET_ACTION_SHUTDOWN:

             
                                                               
                                                      
                         
             
          proc_exit(3);

        case RECOVERY_TARGET_ACTION_PAUSE:
          SetRecoveryPause(true);
          recoveryPausesHere();

                                 

        case RECOVERY_TARGET_ACTION_PROMOTE:
          break;
        }
      }

                                                               
      for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
      {
        if (RmgrTable[rmid].rm_cleanup != NULL)
        {
          RmgrTable[rmid].rm_cleanup();
        }
      }

      ereport(LOG, (errmsg("redo done at %X/%X", (uint32)(ReadRecPtr >> 32), (uint32)ReadRecPtr)));
      xtime = GetLatestXTime();
      if (xtime)
      {
        ereport(LOG, (errmsg("last completed transaction was at log time %s", timestamptz_to_str(xtime))));
      }

      InRedo = false;
    }
    else
    {
                                                             
      ereport(LOG, (errmsg("redo is not required")));
    }
  }

     
                                                                           
                                                                          
                                                                        
                        
     
  ShutdownWalRcv();

     
                                                                          
                                                                             
                                                                      
                                                                         
                                 
     
  if (InRecovery)
  {
    ResetUnloggedRelations(UNLOGGED_RELATION_INIT);
  }

     
                                                                            
                                                   
     
  if (ArchiveRecoveryRequested)
  {
    DisownLatch(&XLogCtl->recoveryWakeupLatch);
  }

     
                                                                      
                                                                             
                                                                    
     
                                                                           
                                     
     
  Assert(!WalRcvStreaming());
  StandbyMode = false;

     
                                                
     
                                                                           
                                                                        
                                                                            
                                                
     
  record = ReadRecord(xlogreader, LastRec, PANIC, false);
  EndOfLog = EndRecPtr;

     
                                                                           
                                                                           
                                                                           
                                                                          
               
     
  EndOfLogTLI = xlogreader->readPageTLI;

     
                                                                         
                                                                             
                                                                            
                                                                         
                                           
     
  if (InRecovery && (EndOfLog < minRecoveryPoint || !XLogRecPtrIsInvalid(ControlFile->backupStartPoint)))
  {
       
                                                                       
                                                                        
                                                               
                                                                      
                                                                          
                                                                        
                                                                  
       
    if (ArchiveRecoveryRequested || ControlFile->backupEndRequired)
    {
      if (ControlFile->backupEndRequired)
      {
        ereport(FATAL, (errmsg("WAL ends before end of online backup"), errhint("All WAL generated while online backup was taken must be available at recovery.")));
      }
      else if (!XLogRecPtrIsInvalid(ControlFile->backupStartPoint))
      {
        ereport(FATAL, (errmsg("WAL ends before end of online backup"), errhint("Online backup started with pg_start_backup() must be ended with pg_stop_backup(), and all WAL up to that point must be available at recovery.")));
      }
      else
      {
        ereport(FATAL, (errmsg("WAL ends before consistent recovery point")));
      }
    }
  }

     
                                                                           
                                                                            
                                                                           
     
  oldestActiveXID = PrescanPreparedTransactions(NULL, NULL);

     
                                                           
     
                                                                           
                                                                        
                                                                             
                                                                          
                                                                            
                                                                          
                                                                        
                                                                      
     
                                                                             
     
  PrevTimeLineID = ThisTimeLineID;
  if (ArchiveRecoveryRequested)
  {
    char reason[200];
    char recoveryPath[MAXPGPATH];

    Assert(InArchiveRecovery);

    ThisTimeLineID = findNewestTimeLine(recoveryTargetTLI) + 1;
    ereport(LOG, (errmsg("selected new timeline ID: %u", ThisTimeLineID)));

       
                                                                      
                         
       
    if (recoveryTarget == RECOVERY_TARGET_XID)
    {
      snprintf(reason, sizeof(reason), "%s transaction %u", recoveryStopAfter ? "after" : "before", recoveryStopXid);
    }
    else if (recoveryTarget == RECOVERY_TARGET_TIME)
    {
      snprintf(reason, sizeof(reason), "%s %s\n", recoveryStopAfter ? "after" : "before", timestamptz_to_str(recoveryStopTime));
    }
    else if (recoveryTarget == RECOVERY_TARGET_LSN)
    {
      snprintf(reason, sizeof(reason), "%s LSN %X/%X\n", recoveryStopAfter ? "after" : "before", (uint32)(recoveryStopLSN >> 32), (uint32)recoveryStopLSN);
    }
    else if (recoveryTarget == RECOVERY_TARGET_NAME)
    {
      snprintf(reason, sizeof(reason), "at restore point \"%s\"", recoveryStopName);
    }
    else if (recoveryTarget == RECOVERY_TARGET_IMMEDIATE)
    {
      snprintf(reason, sizeof(reason), "reached consistency");
    }
    else
    {
      snprintf(reason, sizeof(reason), "no recovery target specified");
    }

       
                                                                          
                                                                        
                                                                          
                                         
       
    exitArchiveRecovery(EndOfLogTLI, EndOfLog);

       
                                                                         
                                                                        
                                                                    
                                                                  
                                                                          
                                                                         
                                                                        
                                                            
       
    writeTimeLineHistory(ThisTimeLineID, recoveryTargetTLI, EndRecPtr, reason);

       
                                                                          
                  
       
    snprintf(recoveryPath, MAXPGPATH, XLOGDIR "/RECOVERYXLOG");
    unlink(recoveryPath);                       

                                                                       
    snprintf(recoveryPath, MAXPGPATH, XLOGDIR "/RECOVERYHISTORY");
    unlink(recoveryPath);                       
  }

                                                          
  XLogCtl->ThisTimeLineID = ThisTimeLineID;
  XLogCtl->PrevTimeLineID = PrevTimeLineID;

     
                                                                         
                                                                         
                                                                          
                                                          
     
  if (!XLogRecPtrIsInvalid(missingContrecPtr))
  {
       
                                                                         
                                                                         
                                                                           
                                                                          
                  
       
    Assert(ThisTimeLineID == PrevTimeLineID);
    Assert(!XLogRecPtrIsInvalid(abortedRecPtr));
    EndOfLog = missingContrecPtr;
  }

     
                                                                       
                                                                      
                           
     
  Insert = &XLogCtl->Insert;
  Insert->PrevBytePos = XLogRecPtrToBytePos(LastRec);
  Insert->CurrBytePos = XLogRecPtrToBytePos(EndOfLog);

     
                                                                           
                                                                           
                         
     
  if (EndOfLog % XLOG_BLCKSZ != 0)
  {
    char *page;
    int len;
    int firstIdx;
    XLogRecPtr pageBeginPtr;

    pageBeginPtr = EndOfLog - (EndOfLog % XLOG_BLCKSZ);
    Assert(readOff == XLogSegmentOffset(pageBeginPtr, wal_segment_size));

    firstIdx = XLogRecPtrToBufIdx(EndOfLog);

                                                                  
    page = &XLogCtl->pages[firstIdx * XLOG_BLCKSZ];
    len = EndOfLog % XLOG_BLCKSZ;
    memcpy(page, xlogreader->readBuf, len);
    memset(page + len, 0, XLOG_BLCKSZ - len);

    XLogCtl->xlblocks[firstIdx] = pageBeginPtr + XLOG_BLCKSZ;
    XLogCtl->InitializedUpTo = pageBeginPtr + XLOG_BLCKSZ;
  }
  else
  {
       
                                                                        
                                                                           
               
       
    XLogCtl->InitializedUpTo = EndOfLog;
  }

  LogwrtResult.Write = LogwrtResult.Flush = EndOfLog;

  XLogCtl->LogwrtResult = LogwrtResult;

  XLogCtl->LogwrtRqst.Write = EndOfLog;
  XLogCtl->LogwrtRqst.Flush = EndOfLog;

  LocalSetXLogInsertAllowed();

                                                                           
  if (!XLogRecPtrIsInvalid(abortedRecPtr))
  {
    Assert(!XLogRecPtrIsInvalid(missingContrecPtr));
    CreateOverwriteContrecordRecord(abortedRecPtr);
    abortedRecPtr = InvalidXLogRecPtr;
    missingContrecPtr = InvalidXLogRecPtr;
  }

     
                                                                           
                                                                             
                        
     
  Insert->fullPageWrites = lastFullPageWrites;
  UpdateFullPageWrites();
  LocalXLogInsertAllowed = -1;

  if (InRecovery)
  {
       
                                                                         
       
                                                                       
                                                                   
                                                                          
                                                                     
                                                      
       
                                                                           
                                                                      
                                                                    
                
       
    if (bgwriterLaunched)
    {
      if (fast_promote)
      {
        checkPointLoc = ControlFile->checkPoint;

           
                                                                      
                            
           
        record = ReadCheckpointRecord(xlogreader, checkPointLoc, 1, false);
        if (record != NULL)
        {
          fast_promoted = true;

             
                                                            
                                                                
                                                                  
                                                                  
                                                                
                                                                
                                                         
                                                            
                                                       
             
          CreateEndOfRecoveryRecord();
        }
      }

      if (!fast_promoted)
      {
        RequestCheckpoint(CHECKPOINT_END_OF_RECOVERY | CHECKPOINT_IMMEDIATE | CHECKPOINT_WAIT);
      }
    }
    else
    {
      CreateCheckPoint(CHECKPOINT_END_OF_RECOVERY | CHECKPOINT_IMMEDIATE);
    }
  }

  if (ArchiveRecoveryRequested)
  {
       
                                                              
       
    if (recoveryEndCommand && strcmp(recoveryEndCommand, "") != 0)
    {
      ExecuteRecoveryCommand(recoveryEndCommand, "recovery_end_command", true);
    }

       
                                                                   
                 
       
                                                                      
                                                                         
                                                                         
                                                                 
       
    RemoveNonParentXlogFiles(EndOfLog, ThisTimeLineID);

       
                                                                          
                                                                          
                                                                        
                                                                           
                                                                          
                                                                          
                                                                           
                                                                        
                                                                          
                                                                       
                                                                           
                                                                          
                                                                          
                                                                       
       
                                                                     
                                                                       
                                                                          
                                                                         
                                                                         
            
       
                                                                      
                                                                           
                                                                     
                                                                        
                    
       
    if (XLogSegmentOffset(EndOfLog, wal_segment_size) != 0 && XLogArchivingActive())
    {
      char origfname[MAXFNAMELEN];
      XLogSegNo endLogSegNo;

      XLByteToPrevSeg(EndOfLog, endLogSegNo, wal_segment_size);
      XLogFileName(origfname, EndOfLogTLI, endLogSegNo, wal_segment_size);

      if (!XLogArchiveIsReadyOrDone(origfname))
      {
        char origpath[MAXPGPATH];
        char partialfname[MAXFNAMELEN];
        char partialpath[MAXPGPATH];

        XLogFilePath(origpath, EndOfLogTLI, endLogSegNo, wal_segment_size);
        snprintf(partialfname, MAXFNAMELEN, "%s.partial", origfname);
        snprintf(partialpath, MAXPGPATH, "%s.partial", origpath);

           
                                                                      
                 
           
        XLogArchiveCleanup(partialfname);

        durable_rename(origpath, partialpath, ERROR);
        XLogArchiveNotify(partialfname);
      }
    }
  }

     
                                                  
     
  PreallocXlogFiles(EndOfLog);

     
                                
     
  InRecovery = false;

                                                       
  XLogCtl->lastSegSwitchTime = (pg_time_t)time(NULL);
  XLogCtl->lastSegSwitchLSN = EndOfLog;

                                                          
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  ShmemVariableCache->latestCompletedXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  TransactionIdRetreat(ShmemVariableCache->latestCompletedXid);
  LWLockRelease(ProcArrayLock);

     
                                                                       
                                                                    
     
  if (standbyState == STANDBY_DISABLED)
  {
    StartupCLOG();
    StartupSUBTRANS(oldestActiveXID);
  }

     
                                                                 
     
  TrimCLOG();
  TrimMultiXact();

                                                            
  RecoverPreparedTransactions();

                            
  if (readFile >= 0)
  {
    close(readFile);
    readFile = -1;
  }
  XLogReaderFree(xlogreader);

     
                                                                        
                            
     
  LocalSetXLogInsertAllowed();
  XLogReportParameters();

     
                                                                         
                       
     
  CompleteCommitTsInitialization();

     
                                            
     
                                                                           
                                                                      
                                                                             
                                                                           
                                                                         
                                                                         
                                                      
     
                                                                        
                                                                       
                                                                        
             
     
  LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
  ControlFile->state = DB_IN_PRODUCTION;
  ControlFile->time = (pg_time_t)time(NULL);

  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->SharedRecoveryState = RECOVERY_STATE_DONE;
  SpinLockRelease(&XLogCtl->info_lck);

  UpdateControlFile();
  LWLockRelease(ControlFileLock);

     
                                                               
                                                                          
                                                                          
                                                                           
                                                                     
                                                                           
                                                               
     
  if (standbyState != STANDBY_DISABLED)
  {
    ShutdownRecoveryTransactionEnvironment();
  }

     
                                                                            
                                                          
     
  WalSndWakeup();

     
                                                                            
                                                                            
                                                                          
                                                                     
     
  if (fast_promoted)
  {
    RequestCheckpoint(CHECKPOINT_FORCE);
  }
}

   
                                                                       
                
   
                                                                          
                                                                              
                                                                         
                                                                         
                                
   
                                                                       
                                                                            
                    
   
static void
CheckTablespaceDirectory(void)
{
  DIR *dir;
  struct dirent *de;

  dir = AllocateDir("pg_tblspc");
  while ((de = ReadDir(dir, "pg_tblspc")) != NULL)
  {
    char path[MAXPGPATH + 10];
#ifndef WIN32
    struct stat st;
#endif

                                       
    if (strspn(de->d_name, "0123456789") != strlen(de->d_name))
    {
      continue;
    }

    snprintf(path, sizeof(path), "pg_tblspc/%s", de->d_name);

#ifndef WIN32
    if (lstat(path, &st) < 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", path)));
    }
    else if (!S_ISLNK(st.st_mode))
#else            
    if (!pgwin32_is_junction(path))
#endif
      ereport(allow_in_place_tablespaces ? WARNING : PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("unexpected directory entry \"%s\" found in %s", de->d_name, "pg_tblspc/"), errdetail("All directory entries in pg_tblspc/ should be symbolic links."), errhint("Remove those directories, or set allow_in_place_tablespaces to ON transiently to let recovery complete.")));
  }
}

   
                                                                          
                                                                          
                                                      
   
static void
CheckRecoveryConsistency(void)
{
  XLogRecPtr lastReplayedEndRecPtr;

     
                                                                          
                           
     
  if (XLogRecPtrIsInvalid(minRecoveryPoint))
  {
    return;
  }

  Assert(InArchiveRecovery);

     
                                                                            
                                          
     
  lastReplayedEndRecPtr = XLogCtl->lastReplayedEndRecPtr;

     
                                                                    
     
  if (!XLogRecPtrIsInvalid(ControlFile->backupEndPoint) && ControlFile->backupEndPoint <= lastReplayedEndRecPtr)
  {
       
                                                                           
                                                                      
                                                                         
                                                                         
                                      
       
    elog(DEBUG1, "end of backup reached");

    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

    if (ControlFile->minRecoveryPoint < lastReplayedEndRecPtr)
    {
      ControlFile->minRecoveryPoint = lastReplayedEndRecPtr;
    }

    ControlFile->backupStartPoint = InvalidXLogRecPtr;
    ControlFile->backupEndPoint = InvalidXLogRecPtr;
    ControlFile->backupEndRequired = false;
    UpdateControlFile();

    LWLockRelease(ControlFileLock);
  }

     
                                                                           
                                                                          
                                                             
                                                                   
                     
     
  if (!reachedConsistency && !ControlFile->backupEndRequired && minRecoveryPoint <= lastReplayedEndRecPtr && XLogRecPtrIsInvalid(ControlFile->backupStartPoint))
  {
       
                                                                  
                                          
       
    XLogCheckInvalidPages();

       
                                                                         
                                                                           
                                                                         
                    
       
    CheckTablespaceDirectory();

    reachedConsistency = true;
    ereport(LOG, (errmsg("consistent recovery state reached at %X/%X", (uint32)(lastReplayedEndRecPtr >> 32), (uint32)lastReplayedEndRecPtr)));
  }

     
                                                                         
                                                                             
                           
     
  if (standbyState == STANDBY_SNAPSHOT_READY && !LocalHotStandbyActive && reachedConsistency && IsUnderPostmaster)
  {
    SpinLockAcquire(&XLogCtl->info_lck);
    XLogCtl->SharedHotStandbyActive = true;
    SpinLockRelease(&XLogCtl->info_lck);

    LocalHotStandbyActive = true;

    SendPostmasterSignal(PMSIGNAL_BEGIN_HOT_STANDBY);
  }
}

   
                                    
   
                                                                            
                  
   
                                                                       
                                                              
   
bool
RecoveryInProgress(void)
{
     
                                                                           
                                                                            
                                               
     
  if (!LocalRecoveryInProgress)
  {
    return false;
  }
  else
  {
       
                                                                     
                        
       
    volatile XLogCtlData *xlogctl = XLogCtl;

    LocalRecoveryInProgress = (xlogctl->SharedRecoveryState != RECOVERY_STATE_DONE);

       
                                                                           
                                                                        
                                                                           
                                                  
       
    if (!LocalRecoveryInProgress)
    {
         
                                                                      
                                                                      
                           
         
      pg_memory_barrier();
      InitXLOGAccess();
    }

       
                                                                          
                                                                      
                                                                         
       

    return LocalRecoveryInProgress;
  }
}

   
                                                      
   
                                                                           
                                                                            
   
RecoveryState
GetRecoveryState(void)
{
  RecoveryState retval;

  SpinLockAcquire(&XLogCtl->info_lck);
  retval = XLogCtl->SharedRecoveryState;
  SpinLockRelease(&XLogCtl->info_lck);

  return retval;
}

   
                                                                        
                                                                          
                                                                        
   
                                                                              
                                                                            
            
   
bool
HotStandbyActive(void)
{
     
                                                                          
                                                                        
                                                        
     
  if (LocalHotStandbyActive)
  {
    return true;
  }
  else
  {
                                                                      
    SpinLockAcquire(&XLogCtl->info_lck);
    LocalHotStandbyActive = XLogCtl->SharedHotStandbyActive;
    SpinLockRelease(&XLogCtl->info_lck);

    return LocalHotStandbyActive;
  }
}

   
                                                                    
                                                                   
   
bool
HotStandbyActiveInReplay(void)
{
  Assert(AmStartupProcess() || !IsPostmasterEnvironment);
  return LocalHotStandbyActive;
}

   
                                                      
   
                                                                       
                                                                        
                                                             
   
bool
XLogInsertAllowed(void)
{
     
                                                                         
                                                                           
           
     
  if (LocalXLogInsertAllowed >= 0)
  {
    return (bool)LocalXLogInsertAllowed;
  }

     
                                                         
     
  if (RecoveryInProgress())
  {
    return false;
  }

     
                                                                            
                               
     
  LocalXLogInsertAllowed = 1;
  return true;
}

   
                                                                     
   
                                                                          
                                                               
   
static void
LocalSetXLogInsertAllowed(void)
{
  Assert(LocalXLogInsertAllowed == -1);
  LocalXLogInsertAllowed = 1;

                                                                        
  InitXLOGAccess();
}

   
                                                                      
   
                                                                         
                                                 
   
static XLogRecord *
ReadCheckpointRecord(XLogReaderState *xlogreader, XLogRecPtr RecPtr, int whichChkpt, bool report)
{
  XLogRecord *record;
  uint8 info;

  if (!XRecOffIsValid(RecPtr))
  {
    if (!report)
    {
      return NULL;
    }

    switch (whichChkpt)
    {
    case 1:
      ereport(LOG, (errmsg("invalid primary checkpoint link in control file")));
      break;
    default:
      ereport(LOG, (errmsg("invalid checkpoint link in backup_label file")));
      break;
    }
    return NULL;
  }

  record = ReadRecord(xlogreader, RecPtr, LOG, true);

  if (record == NULL)
  {
    if (!report)
    {
      return NULL;
    }

    switch (whichChkpt)
    {
    case 1:
      ereport(LOG, (errmsg("invalid primary checkpoint record")));
      break;
    default:
      ereport(LOG, (errmsg("invalid checkpoint record")));
      break;
    }
    return NULL;
  }
  if (record->xl_rmid != RM_XLOG_ID)
  {
    switch (whichChkpt)
    {
    case 1:
      ereport(LOG, (errmsg("invalid resource manager ID in primary checkpoint record")));
      break;
    default:
      ereport(LOG, (errmsg("invalid resource manager ID in checkpoint record")));
      break;
    }
    return NULL;
  }
  info = record->xl_info & ~XLR_INFO_MASK;
  if (info != XLOG_CHECKPOINT_SHUTDOWN && info != XLOG_CHECKPOINT_ONLINE)
  {
    switch (whichChkpt)
    {
    case 1:
      ereport(LOG, (errmsg("invalid xl_info in primary checkpoint record")));
      break;
    default:
      ereport(LOG, (errmsg("invalid xl_info in checkpoint record")));
      break;
    }
    return NULL;
  }
  if (record->xl_tot_len != SizeOfXLogRecord + SizeOfXLogRecordDataHeaderShort + sizeof(CheckPoint))
  {
    switch (whichChkpt)
    {
    case 1:
      ereport(LOG, (errmsg("invalid length of primary checkpoint record")));
      break;
    default:
      ereport(LOG, (errmsg("invalid length of checkpoint record")));
      break;
    }
    return NULL;
  }
  return record;
}

   
                                                                        
                                                                              
                                                                    
   
                                                                            
                                                                          
                                                                               
   
void
InitXLOGAccess(void)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;

                                                                   
  ThisTimeLineID = XLogCtl->ThisTimeLineID;
  Assert(ThisTimeLineID != 0 || IsBootstrapProcessingMode());

                            
  wal_segment_size = ControlFile->xlog_seg_size;

                                                       
  (void)GetRedoRecPtr();
                                             
  doPageWrites = (Insert->fullPageWrites || Insert->forcePageWrites);

                                                                      
  InitXLogInsert();
}

   
                                                       
   
                                                           
   
XLogRecPtr
GetRedoRecPtr(void)
{
  XLogRecPtr ptr;

     
                                                                       
                                                                         
                                                   
     
  SpinLockAcquire(&XLogCtl->info_lck);
  ptr = XLogCtl->RedoRecPtr;
  SpinLockRelease(&XLogCtl->info_lck);

  if (RedoRecPtr < ptr)
  {
    RedoRecPtr = ptr;
  }

  return RedoRecPtr;
}

   
                                                                        
                                                     
   
                                                                          
                                                                      
                                                         
   
void
GetFullPageWriteInfo(XLogRecPtr *RedoRecPtr_p, bool *doPageWrites_p)
{
  *RedoRecPtr_p = RedoRecPtr;
  *doPageWrites_p = doPageWrites;
}

   
                                                           
   
                                                                        
                                                                         
                                                                       
                                                                   
   
XLogRecPtr
GetInsertRecPtr(void)
{
  XLogRecPtr recptr;

  SpinLockAcquire(&XLogCtl->info_lck);
  recptr = XLogCtl->LogwrtRqst.Write;
  SpinLockRelease(&XLogCtl->info_lck);

  return recptr;
}

   
                                                                          
                                         
   
XLogRecPtr
GetFlushRecPtr(void)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  LogwrtResult = XLogCtl->LogwrtResult;
  SpinLockRelease(&XLogCtl->info_lck);

  return LogwrtResult.Flush;
}

   
                                                                          
                                                                             
              
   
                                                     
                                      
   
XLogRecPtr
GetLastImportantRecPtr(void)
{
  XLogRecPtr res = InvalidXLogRecPtr;
  int i;

  for (i = 0; i < NUM_XLOGINSERT_LOCKS; i++)
  {
    XLogRecPtr last_important;

       
                                                                       
                                                                          
                                                       
       
    LWLockAcquire(&WALInsertLocks[i].l.lock, LW_EXCLUSIVE);
    last_important = WALInsertLocks[i].l.lastImportantAt;
    LWLockRelease(&WALInsertLocks[i].l.lock);

    if (res < last_important)
    {
      res = last_important;
    }
  }

  return res;
}

   
                                                        
   
pg_time_t
GetLastSegSwitchData(XLogRecPtr *lastSwitchLSN)
{
  pg_time_t result;

                                                        
  LWLockAcquire(WALWriteLock, LW_SHARED);
  result = XLogCtl->lastSegSwitchTime;
  *lastSwitchLSN = XLogCtl->lastSegSwitchLSN;
  LWLockRelease(WALWriteLock);

  return result;
}

   
                                                                             
   
void
ShutdownXLOG(int code, Datum arg)
{
     
                                                                            
                                                               
     
  Assert(AuxProcessResourceOwner != NULL);
  Assert(CurrentResourceOwner == NULL || CurrentResourceOwner == AuxProcessResourceOwner);
  CurrentResourceOwner = AuxProcessResourceOwner;

                                          
  ereport(IsPostmasterEnvironment ? LOG : NOTICE, (errmsg("shutting down")));

     
                                                  
     
  WalSndInitStopping();

     
                                                                           
                           
     
  WalSndWaitStopping();

  if (RecoveryInProgress())
  {
    CreateRestartPoint(CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_IMMEDIATE);
  }
  else
  {
       
                                                                          
                                                                        
                                                                     
                                                                         
       
    if (XLogArchivingActive() && XLogArchiveCommandSet())
    {
      RequestXLogSwitch(false);
    }

    CreateCheckPoint(CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_IMMEDIATE);
  }
  ShutdownCLOG();
  ShutdownCommitTs();
  ShutdownSUBTRANS();
  ShutdownMultiXact();
}

   
                              
   
static void
LogCheckpointStart(int flags, bool restartpoint)
{
  elog(LOG, "%s starting:%s%s%s%s%s%s%s%s", restartpoint ? "restartpoint" : "checkpoint", (flags & CHECKPOINT_IS_SHUTDOWN) ? " shutdown" : "", (flags & CHECKPOINT_END_OF_RECOVERY) ? " end-of-recovery" : "", (flags & CHECKPOINT_IMMEDIATE) ? " immediate" : "", (flags & CHECKPOINT_FORCE) ? " force" : "", (flags & CHECKPOINT_WAIT) ? " wait" : "", (flags & CHECKPOINT_CAUSE_XLOG) ? " wal" : "", (flags & CHECKPOINT_CAUSE_TIME) ? " time" : "", (flags & CHECKPOINT_FLUSH_ALL) ? " flush-all" : "");
}

   
                            
   
static void
LogCheckpointEnd(bool restartpoint)
{
  long write_msecs, sync_msecs, total_msecs, longest_msecs, average_msecs;
  uint64 average_sync_time;

  CheckpointStats.ckpt_end_t = GetCurrentTimestamp();

  write_msecs = TimestampDifferenceMilliseconds(CheckpointStats.ckpt_write_t, CheckpointStats.ckpt_sync_t);

  sync_msecs = TimestampDifferenceMilliseconds(CheckpointStats.ckpt_sync_t, CheckpointStats.ckpt_sync_end_t);

                                                                   
  BgWriterStats.m_checkpoint_write_time += write_msecs;
  BgWriterStats.m_checkpoint_sync_time += sync_msecs;

     
                                                                     
                                                 
     
  if (!log_checkpoints)
  {
    return;
  }

  total_msecs = TimestampDifferenceMilliseconds(CheckpointStats.ckpt_start_t, CheckpointStats.ckpt_end_t);

     
                                                                      
                                                      
     
  longest_msecs = (long)((CheckpointStats.ckpt_longest_sync + 999) / 1000);

  average_sync_time = 0;
  if (CheckpointStats.ckpt_sync_rels > 0)
  {
    average_sync_time = CheckpointStats.ckpt_agg_sync_time / CheckpointStats.ckpt_sync_rels;
  }
  average_msecs = (long)((average_sync_time + 999) / 1000);

  elog(LOG,
      "%s complete: wrote %d buffers (%.1f%%); "
      "%d WAL file(s) added, %d removed, %d recycled; "
      "write=%ld.%03d s, sync=%ld.%03d s, total=%ld.%03d s; "
      "sync files=%d, longest=%ld.%03d s, average=%ld.%03d s; "
      "distance=%d kB, estimate=%d kB",
      restartpoint ? "restartpoint" : "checkpoint", CheckpointStats.ckpt_bufs_written, (double)CheckpointStats.ckpt_bufs_written * 100 / NBuffers, CheckpointStats.ckpt_segs_added, CheckpointStats.ckpt_segs_removed, CheckpointStats.ckpt_segs_recycled, write_msecs / 1000, (int)(write_msecs % 1000), sync_msecs / 1000, (int)(sync_msecs % 1000), total_msecs / 1000, (int)(total_msecs % 1000), CheckpointStats.ckpt_sync_rels, longest_msecs / 1000, (int)(longest_msecs % 1000), average_msecs / 1000, (int)(average_msecs % 1000), (int)(PrevCheckPointDistance / 1024.0), (int)(CheckPointDistanceEstimate / 1024.0));
}

   
                                                        
   
                                                                        
                                     
   
static void
UpdateCheckPointDistanceEstimate(uint64 nbytes)
{
     
                                                                             
                                                                          
                                                                         
                                                                        
                                                                          
                                                                         
           
     
                                                                             
                                            
     
                                                                         
                                                                        
                                                                   
                                                                             
                                                                           
                                                                            
                                                                     
                                                                  
                                   
     
  PrevCheckPointDistance = nbytes;
  if (CheckPointDistanceEstimate < nbytes)
  {
    CheckPointDistanceEstimate = nbytes;
  }
  else
  {
    CheckPointDistanceEstimate = (0.90 * CheckPointDistanceEstimate + 0.10 * (double)nbytes);
  }
}

   
                                                                  
   
                                           
                                                                
                                                                      
                                                     
                                                     
                                                                              
                                                             
                                 
                                                                
   
                                                                                
                                                                        
                                 
   
                                                                                 
                                                                              
                                                                           
                                                                         
                                                                          
                                                                             
                                                                           
                                                                                 
                                                                            
                                                                               
                                                                       
   
void
CreateCheckPoint(int flags)
{
  bool shutdown;
  CheckPoint checkPoint;
  XLogRecPtr recptr;
  XLogSegNo _logSegNo;
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  uint32 freespace;
  XLogRecPtr PriorRedoPtr;
  XLogRecPtr curInsert;
  XLogRecPtr last_important_lsn;
  VirtualTransactionId *vxids;
  int nvxids;

     
                                                                         
                                 
     
  if (flags & (CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_END_OF_RECOVERY))
  {
    shutdown = true;
  }
  else
  {
    shutdown = false;
  }

                    
  if (RecoveryInProgress() && (flags & CHECKPOINT_END_OF_RECOVERY) == 0)
  {
    elog(ERROR, "can't create a checkpoint during recovery");
  }

     
                                                                          
                                                           
                                                                            
                                                                          
                                                                           
                            
     
  InitXLogInsert();

     
                                                                             
                                                                             
                                                                        
            
     
  LWLockAcquire(CheckpointLock, LW_EXCLUSIVE);

     
                                       
     
                                                                        
                                                              
                                       
     
  MemSet(&CheckpointStats, 0, sizeof(CheckpointStats));
  CheckpointStats.ckpt_start_t = GetCurrentTimestamp();

     
                                                                     
                                                                           
                                                                        
                           
     
  SyncPreCheckpoint();

     
                                                                      
     
  START_CRIT_SECTION();

  if (shutdown)
  {
    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
    ControlFile->state = DB_SHUTDOWNING;
    ControlFile->time = (pg_time_t)time(NULL);
    UpdateControlFile();
    LWLockRelease(ControlFileLock);
  }

                                                  
  MemSet(&checkPoint, 0, sizeof(checkPoint));
  checkPoint.time = (pg_time_t)time(NULL);

     
                                                                        
                                                                           
                                                  
     
  if (!shutdown && XLogStandbyInfoActive())
  {
    checkPoint.oldestActiveXid = GetOldestActiveTransactionId();
  }
  else
  {
    checkPoint.oldestActiveXid = InvalidTransactionId;
  }

     
                                                                             
                                                     
     
  last_important_lsn = GetLastImportantRecPtr();

     
                                                                         
                                            
     
  WALInsertLockAcquireExclusive();
  curInsert = XLogBytePosToRecPtr(Insert->CurrBytePos);

     
                                                                             
                                                                        
                                                                    
     
  if ((flags & (CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_END_OF_RECOVERY | CHECKPOINT_FORCE)) == 0)
  {
    if (last_important_lsn == ControlFile->checkPoint)
    {
      WALInsertLockRelease();
      LWLockRelease(CheckpointLock);
      END_CRIT_SECTION();
      ereport(DEBUG1, (errmsg("checkpoint skipped because system is idle")));
      return;
    }
  }

     
                                                                          
                                                                        
                                                                     
                                                                    
     
  if (flags & CHECKPOINT_END_OF_RECOVERY)
  {
    LocalSetXLogInsertAllowed();
  }

  checkPoint.ThisTimeLineID = ThisTimeLineID;
  if (flags & CHECKPOINT_END_OF_RECOVERY)
  {
    checkPoint.PrevTimeLineID = XLogCtl->PrevTimeLineID;
  }
  else
  {
    checkPoint.PrevTimeLineID = ThisTimeLineID;
  }

  checkPoint.fullPageWrites = Insert->fullPageWrites;

     
                                                                 
     
                                                                             
                                                                             
                                                                        
                                                              
     
  freespace = INSERT_FREESPACE(curInsert);
  if (freespace == 0)
  {
    if (XLogSegmentOffset(curInsert, wal_segment_size) == 0)
    {
      curInsert += SizeOfXLogLongPHD;
    }
    else
    {
      curInsert += SizeOfXLogShortPHD;
    }
  }
  checkPoint.redo = curInsert;

     
                                                                            
                                                         
     
                                                                          
                                                                           
                                                                        
                                                                            
                                                                           
                                                              
     
  RedoRecPtr = XLogCtl->Insert.RedoRecPtr = checkPoint.redo;

     
                                                                         
                                                 
     
  WALInsertLockRelease();

                                                                
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->RedoRecPtr = checkPoint.redo;
  SpinLockRelease(&XLogCtl->info_lck);

     
                                                                             
                                                           
     
  if (log_checkpoints)
  {
    LogCheckpointStart(flags, false);
  }

  TRACE_POSTGRESQL_CHECKPOINT_START(flags);

     
                                                           
     
                                                                            
                                                                            
                                                                         
            
     
  LWLockAcquire(XidGenLock, LW_SHARED);
  checkPoint.nextFullXid = ShmemVariableCache->nextFullXid;
  checkPoint.oldestXid = ShmemVariableCache->oldestXid;
  checkPoint.oldestXidDB = ShmemVariableCache->oldestXidDB;
  LWLockRelease(XidGenLock);

  LWLockAcquire(CommitTsLock, LW_SHARED);
  checkPoint.oldestCommitTsXid = ShmemVariableCache->oldestCommitTsXid;
  checkPoint.newestCommitTsXid = ShmemVariableCache->newestCommitTsXid;
  LWLockRelease(CommitTsLock);

  LWLockAcquire(OidGenLock, LW_SHARED);
  checkPoint.nextOid = ShmemVariableCache->nextOid;
  if (!shutdown)
  {
    checkPoint.nextOid += ShmemVariableCache->oidCount;
  }
  LWLockRelease(OidGenLock);

  MultiXactGetCheckptMulti(shutdown, &checkPoint.nextMulti, &checkPoint.nextMultiOffset, &checkPoint.oldestMulti, &checkPoint.oldestMultiDB);

     
                                                                             
                                                 
     
                                                                      
                                                                       
                                                               
     
  END_CRIT_SECTION();

     
                                                                          
                                                                   
                                                                         
                                         
     
                                                                             
                                                                          
                                                                          
                                                                           
                                                                            
                                                                             
                                             
     
                                                                            
                                                                             
                                                                             
                                                                          
                                                                             
                                                                             
                                                                           
                                  
     
                                                                             
                                                                         
                                                                           
                                                                          
                                
     
  vxids = GetVirtualXIDsDelayingChkpt(&nvxids);
  if (nvxids > 0)
  {
    do
    {
      pg_usleep(10000L);                       
    } while (HaveVirtualXIDsDelayingChkpt(vxids, nvxids));
  }
  pfree(vxids);

  CheckPointGuts(checkPoint.redo, flags);

  vxids = GetVirtualXIDsDelayingChkptEnd(&nvxids);
  if (nvxids > 0)
  {
    do
    {
      pg_usleep(10000L);                       
    } while (HaveVirtualXIDsDelayingChkptEnd(vxids, nvxids));
  }
  pfree(vxids);

     
                                                                         
                                                                       
                                                                 
     
                                                                     
                                                        
     
  if (!shutdown && XLogStandbyInfoActive())
  {
    LogStandbySnapshot();
  }

  START_CRIT_SECTION();

     
                                                 
     
  XLogBeginInsert();
  XLogRegisterData((char *)(&checkPoint), sizeof(checkPoint));
  recptr = XLogInsert(RM_XLOG_ID, shutdown ? XLOG_CHECKPOINT_SHUTDOWN : XLOG_CHECKPOINT_ONLINE);

  XLogFlush(recptr);

     
                                                                             
                                                                            
                                                                             
                                                                     
               
     
  if (shutdown)
  {
    if (flags & CHECKPOINT_END_OF_RECOVERY)
    {
      LocalXLogInsertAllowed = -1;                              
    }
    else
    {
      LocalXLogInsertAllowed = 0;                            
    }
  }

     
                                                                            
                                        
     
  if (shutdown && checkPoint.redo != ProcLastRecPtr)
  {
    ereport(PANIC, (errmsg("concurrent write-ahead log activity while database system is shutting down")));
  }

     
                                                  
                                        
     
  PriorRedoPtr = ControlFile->checkPointCopy.redo;

     
                              
     
  LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
  if (shutdown)
  {
    ControlFile->state = DB_SHUTDOWNED;
  }
  ControlFile->checkPoint = ProcLastRecPtr;
  ControlFile->checkPointCopy = checkPoint;
  ControlFile->time = (pg_time_t)time(NULL);
                                                              
  ControlFile->minRecoveryPoint = InvalidXLogRecPtr;
  ControlFile->minRecoveryPointTLI = 0;

     
                                                                           
                                                                             
                             
     
  SpinLockAcquire(&XLogCtl->ulsn_lck);
  ControlFile->unloggedLSN = XLogCtl->unloggedLSN;
  SpinLockRelease(&XLogCtl->ulsn_lck);

  UpdateControlFile();
  LWLockRelease(ControlFileLock);

                                                         
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->ckptFullXid = checkPoint.nextFullXid;
  SpinLockRelease(&XLogCtl->info_lck);

     
                                                                           
                                                       
     
  END_CRIT_SECTION();

     
                                                                   
     
  SyncPostCheckpoint();

     
                                                                             
             
     
  if (PriorRedoPtr != InvalidXLogRecPtr)
  {
    UpdateCheckPointDistanceEstimate(RedoRecPtr - PriorRedoPtr);
  }

     
                                                                         
                                                          
     
  XLByteToSeg(RedoRecPtr, _logSegNo, wal_segment_size);
  KeepLogSeg(recptr, &_logSegNo);
  _logSegNo--;
  RemoveOldXlogFiles(_logSegNo, RedoRecPtr, recptr);

     
                                                                         
                                                                
     
  if (!shutdown)
  {
    PreallocXlogFiles(recptr);
  }

     
                                                                          
                                                                             
                                                                             
                                                                          
                                             
     
  if (!RecoveryInProgress())
  {
    TruncateSUBTRANS(GetOldestXmin(NULL, PROCARRAY_FLAGS_DEFAULT));
  }

                                                                          
  LogCheckpointEnd(false);

  TRACE_POSTGRESQL_CHECKPOINT_DONE(CheckpointStats.ckpt_bufs_written, NBuffers, CheckpointStats.ckpt_segs_added, CheckpointStats.ckpt_segs_removed, CheckpointStats.ckpt_segs_recycled);

  LWLockRelease(CheckpointLock);
}

   
                                                                             
                                                                       
                                                                      
                                                             
   
                                                                          
                                                                              
   
static void
CreateEndOfRecoveryRecord(void)
{
  xl_end_of_recovery xlrec;
  XLogRecPtr recptr;

                    
  if (!RecoveryInProgress())
  {
    elog(ERROR, "can only be used to end recovery");
  }

  xlrec.end_time = GetCurrentTimestamp();

  WALInsertLockAcquireExclusive();
  xlrec.ThisTimeLineID = ThisTimeLineID;
  xlrec.PrevTimeLineID = XLogCtl->PrevTimeLineID;
  WALInsertLockRelease();

  LocalSetXLogInsertAllowed();

  START_CRIT_SECTION();

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, sizeof(xl_end_of_recovery));
  recptr = XLogInsert(RM_XLOG_ID, XLOG_END_OF_RECOVERY);

  XLogFlush(recptr);

     
                                                                            
                            
     
  LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
  ControlFile->time = (pg_time_t)time(NULL);
  ControlFile->minRecoveryPoint = recptr;
  ControlFile->minRecoveryPointTLI = ThisTimeLineID;
  UpdateControlFile();
  LWLockRelease(ControlFileLock);

  END_CRIT_SECTION();

  LocalXLogInsertAllowed = -1;                              
}

   
                                          
   
                                                                             
                                                                           
                                                                            
                                                                       
                                                                      
                                                                            
                                                                        
                                                                               
                                                                         
                                                                          
             
   
                                                                             
                                                                        
                                                                               
                                                                       
   
static XLogRecPtr
CreateOverwriteContrecordRecord(XLogRecPtr aborted_lsn)
{
  xl_overwrite_contrecord xlrec;
  XLogRecPtr recptr;

                    
  if (!RecoveryInProgress())
  {
    elog(ERROR, "can only be used at end of recovery");
  }

  xlrec.overwritten_lsn = aborted_lsn;
  xlrec.overwrite_time = GetCurrentTimestamp();

  START_CRIT_SECTION();

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, sizeof(xl_overwrite_contrecord));

  recptr = XLogInsert(RM_XLOG_ID, XLOG_OVERWRITE_CONTRECORD);

  XLogFlush(recptr);

  END_CRIT_SECTION();

  return recptr;
}

   
                                                      
   
                                                                  
                           
   
static void
CheckPointGuts(XLogRecPtr checkPointRedo, int flags)
{
  CheckPointCLOG();
  CheckPointCommitTs();
  CheckPointSUBTRANS();
  CheckPointMultiXact();
  CheckPointPredicate();
  CheckPointRelationMap();
  CheckPointReplicationSlots();
  CheckPointSnapBuild();
  CheckPointLogicalRewriteHeap();
  CheckPointBuffers(flags);                                   
  CheckPointReplicationOrigin();
                                                                   
  CheckPointTwoPhase(checkPointRedo);
}

   
                                                         
   
                                                                            
                                                                              
                                                                          
                                                                         
                                                                        
                     
   
static void
RecoveryRestartPoint(const CheckPoint *checkPoint)
{
     
                                                                   
                                                                    
                                                                     
                                                                        
            
     
  if (XLogHaveInvalidPages())
  {
    elog(trace_recovery(DEBUG2),
        "could not record restart point at %X/%X because there "
        "are unresolved references to invalid pages",
        (uint32)(checkPoint->redo >> 32), (uint32)checkPoint->redo);
    return;
  }

     
                                                                           
                                                                
     
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->lastCheckPointRecPtr = ReadRecPtr;
  XLogCtl->lastCheckPointEndPtr = EndRecPtr;
  XLogCtl->lastCheckPoint = *checkPoint;
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                         
   
                                                                        
                                                                     
                                      
   
                                                                             
                                                                          
                 
   
bool
CreateRestartPoint(int flags)
{
  XLogRecPtr lastCheckPointRecPtr;
  XLogRecPtr lastCheckPointEndPtr;
  CheckPoint lastCheckPoint;
  XLogRecPtr PriorRedoPtr;
  XLogRecPtr receivePtr;
  XLogRecPtr replayPtr;
  TimeLineID replayTLI;
  XLogRecPtr endptr;
  XLogSegNo _logSegNo;
  TimestampTz xtime;

     
                                                                          
                        
     
  LWLockAcquire(CheckpointLock, LW_EXCLUSIVE);

                                                            
  SpinLockAcquire(&XLogCtl->info_lck);
  lastCheckPointRecPtr = XLogCtl->lastCheckPointRecPtr;
  lastCheckPointEndPtr = XLogCtl->lastCheckPointEndPtr;
  lastCheckPoint = XLogCtl->lastCheckPoint;
  SpinLockRelease(&XLogCtl->info_lck);

     
                                                                          
                                                               
     
  if (!RecoveryInProgress())
  {
    ereport(DEBUG2, (errmsg("skipping restartpoint, recovery has already ended")));
    LWLockRelease(CheckpointLock);
    return false;
  }

     
                                                                      
                                                                         
                                                                          
                                                                       
                                                                             
                                                                         
                                                                             
                                 
     
                                                                      
                                                                            
                  
     
  if (XLogRecPtrIsInvalid(lastCheckPointRecPtr) || lastCheckPoint.redo <= ControlFile->checkPointCopy.redo)
  {
    ereport(DEBUG2, (errmsg("skipping restartpoint, already performed at %X/%X", (uint32)(lastCheckPoint.redo >> 32), (uint32)lastCheckPoint.redo)));

    UpdateMinRecoveryPoint(InvalidXLogRecPtr, true);
    if (flags & CHECKPOINT_IS_SHUTDOWN)
    {
      LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
      ControlFile->state = DB_SHUTDOWNED_IN_RECOVERY;
      ControlFile->time = (pg_time_t)time(NULL);
      UpdateControlFile();
      LWLockRelease(ControlFileLock);
    }
    LWLockRelease(CheckpointLock);
    return false;
  }

     
                                                                            
                                                                            
                                                    
     
                                                                            
                                                                           
                
     
  WALInsertLockAcquireExclusive();
  RedoRecPtr = XLogCtl->Insert.RedoRecPtr = lastCheckPoint.redo;
  WALInsertLockRelease();

                                               
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->RedoRecPtr = lastCheckPoint.redo;
  SpinLockRelease(&XLogCtl->info_lck);

     
                                       
     
                                                                        
                                                              
                                       
     
  MemSet(&CheckpointStats, 0, sizeof(CheckpointStats));
  CheckpointStats.ckpt_start_t = GetCurrentTimestamp();

  if (log_checkpoints)
  {
    LogCheckpointStart(flags, true);
  }

  CheckPointGuts(lastCheckPoint.redo, flags);

     
                                                  
                                        
     
  PriorRedoPtr = ControlFile->checkPointCopy.redo;

     
                                                                          
                                                                          
                                                                 
                                 
     
  LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
  if (ControlFile->checkPointCopy.redo < lastCheckPoint.redo)
  {
       
                                                                          
                                                                         
                                
       
    ControlFile->checkPoint = lastCheckPointRecPtr;
    ControlFile->checkPointCopy = lastCheckPoint;
    ControlFile->time = (pg_time_t)time(NULL);

       
                                                                           
                                                                          
                                                                        
                                                                          
                                                                         
                                                                         
                                                                          
                                                                           
                                                                           
                                                                         
                                                                         
                                                                         
                                                                          
                                                                           
                                                                           
                                                                         
       
    if (ControlFile->state == DB_IN_ARCHIVE_RECOVERY)
    {
      if (ControlFile->minRecoveryPoint < lastCheckPointEndPtr)
      {
        ControlFile->minRecoveryPoint = lastCheckPointEndPtr;
        ControlFile->minRecoveryPointTLI = lastCheckPoint.ThisTimeLineID;

                               
        minRecoveryPoint = ControlFile->minRecoveryPoint;
        minRecoveryPointTLI = ControlFile->minRecoveryPointTLI;
      }
      if (flags & CHECKPOINT_IS_SHUTDOWN)
      {
        ControlFile->state = DB_SHUTDOWNED_IN_RECOVERY;
      }
    }
    UpdateControlFile();
  }
  LWLockRelease(ControlFileLock);

     
                                                                          
                              
     
  if (PriorRedoPtr != InvalidXLogRecPtr)
  {
    UpdateCheckPointDistanceEstimate(RedoRecPtr - PriorRedoPtr);
  }

     
                                                                           
                                                          
     
  XLByteToSeg(RedoRecPtr, _logSegNo, wal_segment_size);

     
                                                                           
                         
     
  receivePtr = GetWalRcvWriteRecPtr(NULL, NULL);
  replayPtr = GetXLogReplayRecPtr(&replayTLI);
  endptr = (receivePtr < replayPtr) ? replayPtr : receivePtr;
  KeepLogSeg(endptr, &_logSegNo);
  _logSegNo--;

     
                                                                          
                                                                           
                                                                          
                                                                         
                
     
                                                                       
                                                                          
                                                                             
                                                                            
                
     
  if (RecoveryInProgress())
  {
    ThisTimeLineID = replayTLI;
  }

  RemoveOldXlogFiles(_logSegNo, RedoRecPtr, endptr);

     
                                                                         
                                                                
     
  PreallocXlogFiles(endptr);

     
                                                                      
                                                                           
                                                                           
                                                                    
     
  if (RecoveryInProgress())
  {
    ThisTimeLineID = 0;
  }

     
                                                                          
                                                                             
                                                                             
                                                                          
                                                          
     
  if (EnableHotStandby)
  {
    TruncateSUBTRANS(GetOldestXmin(NULL, PROCARRAY_FLAGS_DEFAULT));
  }

                                                                    
  LogCheckpointEnd(true);

  xtime = GetLatestXTime();
  ereport((log_checkpoints ? LOG : DEBUG2), (errmsg("recovery restart point at %X/%X", (uint32)(lastCheckPoint.redo >> 32), (uint32)lastCheckPoint.redo), xtime ? errdetail("Last completed transaction was at log time %s.", timestamptz_to_str(xtime)) : 0));

  LWLockRelease(CheckpointLock);

     
                                                       
     
  if (archiveCleanupCommand && strcmp(archiveCleanupCommand, "") != 0)
  {
    ExecuteRecoveryCommand(archiveCleanupCommand, "archive_cleanup_command", false);
  }

  return true;
}

   
                                                                           
                                                  
   
                                                                           
                                                                     
                                     
   
static void
KeepLogSeg(XLogRecPtr recptr, XLogSegNo *logSegNo)
{
  XLogSegNo segno;
  XLogRecPtr keep;

  XLByteToSeg(recptr, segno, wal_segment_size);
  keep = XLogGetReplicationSlotMinimumLSN();

                                                 
  if (wal_keep_segments > 0)
  {
                                           
    if (segno <= wal_keep_segments)
    {
      segno = 1;
    }
    else
    {
      segno = segno - wal_keep_segments;
    }
  }

                                                      
  if (max_replication_slots > 0 && keep != InvalidXLogRecPtr)
  {
    XLogSegNo slotSegNo;

    XLByteToSeg(keep, slotSegNo, wal_segment_size);

    if (slotSegNo <= 0)
    {
      segno = 1;
    }
    else if (slotSegNo < segno)
    {
      segno = slotSegNo;
    }
  }

                                                                   
  if (segno < *logSegNo)
  {
    *logSegNo = segno;
  }
}

   
                              
   
void
XLogPutNextOid(Oid nextOid)
{
  XLogBeginInsert();
  XLogRegisterData((char *)(&nextOid), sizeof(Oid));
  (void)XLogInsert(RM_XLOG_ID, XLOG_NEXTOID);

     
                                                                          
                                                                            
                                                                             
                                                                            
                                                                            
           
     
                                                                            
                                                                            
                                                                          
                                                                           
                                                                          
                                                                             
                                                                          
                                                                             
                                                                    
     
}

   
                                
   
                                                                    
                                            
   
                                                                      
                                                                   
                                                                  
   
XLogRecPtr
RequestXLogSwitch(bool mark_unimportant)
{
  XLogRecPtr RecPtr;

                               
  XLogBeginInsert();

  if (mark_unimportant)
  {
    XLogSetRecordFlags(XLOG_MARK_UNIMPORTANT);
  }
  RecPtr = XLogInsert(RM_XLOG_ID, XLOG_SWITCH);

  return RecPtr;
}

   
                                
   
XLogRecPtr
XLogRestorePoint(const char *rpName)
{
  XLogRecPtr RecPtr;
  xl_restore_point xlrec;

  xlrec.rp_time = GetCurrentTimestamp();
  strlcpy(xlrec.rp_name, rpName, MAXFNAMELEN);

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, sizeof(xl_restore_point));

  RecPtr = XLogInsert(RM_XLOG_ID, XLOG_RESTORE_POINT);

  ereport(LOG, (errmsg("restore point \"%s\" created at %X/%X", rpName, (uint32)(RecPtr >> 32), (uint32)RecPtr)));

  return RecPtr;
}

   
                                                                        
                                                                       
   
static void
XLogReportParameters(void)
{
  if (wal_level != ControlFile->wal_level || wal_log_hints != ControlFile->wal_log_hints || MaxConnections != ControlFile->MaxConnections || max_worker_processes != ControlFile->max_worker_processes || max_wal_senders != ControlFile->max_wal_senders || max_prepared_xacts != ControlFile->max_prepared_xacts || max_locks_per_xact != ControlFile->max_locks_per_xact || track_commit_timestamp != ControlFile->track_commit_timestamp)
  {
       
                                                                           
                                                                        
                                                                     
                                                                          
                                                   
       
    if (wal_level != ControlFile->wal_level || XLogIsNeeded())
    {
      xl_parameter_change xlrec;
      XLogRecPtr recptr;

      xlrec.MaxConnections = MaxConnections;
      xlrec.max_worker_processes = max_worker_processes;
      xlrec.max_wal_senders = max_wal_senders;
      xlrec.max_prepared_xacts = max_prepared_xacts;
      xlrec.max_locks_per_xact = max_locks_per_xact;
      xlrec.wal_level = wal_level;
      xlrec.wal_log_hints = wal_log_hints;
      xlrec.track_commit_timestamp = track_commit_timestamp;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, sizeof(xlrec));

      recptr = XLogInsert(RM_XLOG_ID, XLOG_PARAMETER_CHANGE);
      XLogFlush(recptr);
    }

    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

    ControlFile->MaxConnections = MaxConnections;
    ControlFile->max_worker_processes = max_worker_processes;
    ControlFile->max_wal_senders = max_wal_senders;
    ControlFile->max_prepared_xacts = max_prepared_xacts;
    ControlFile->max_locks_per_xact = max_locks_per_xact;
    ControlFile->wal_level = wal_level;
    ControlFile->wal_log_hints = wal_log_hints;
    ControlFile->track_commit_timestamp = track_commit_timestamp;
    UpdateControlFile();

    LWLockRelease(ControlFileLock);
  }
}

   
                                                          
                                        
   
                                                                 
                                      
   
void
UpdateFullPageWrites(void)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  bool recoveryInProgress;

     
                                                          
     
                                                                      
                                                                           
                    
     
  if (fullPageWrites == Insert->fullPageWrites)
  {
    return;
  }

     
                                                                  
                                                                    
                        
     
  recoveryInProgress = RecoveryInProgress();

  START_CRIT_SECTION();

     
                                                                       
                                                                             
                                                                        
                                                                             
           
     
  if (fullPageWrites)
  {
    WALInsertLockAcquireExclusive();
    Insert->fullPageWrites = true;
    WALInsertLockRelease();
  }

     
                                                                      
                                                            
     
  if (XLogStandbyInfoActive() && !recoveryInProgress)
  {
    XLogBeginInsert();
    XLogRegisterData((char *)(&fullPageWrites), sizeof(bool));

    XLogInsert(RM_XLOG_ID, XLOG_FPW_CHANGE);
  }

  if (!fullPageWrites)
  {
    WALInsertLockAcquireExclusive();
    Insert->fullPageWrites = false;
    WALInsertLockRelease();
  }
  END_CRIT_SECTION();
}

   
                                                                 
   
                                                                         
                                                                           
   
static void
checkTimeLineSwitch(XLogRecPtr lsn, TimeLineID newTLI, TimeLineID prevTLI)
{
                                                                          
  if (prevTLI != ThisTimeLineID)
  {
    ereport(PANIC, (errmsg("unexpected previous timeline ID %u (current timeline ID %u) in checkpoint record", prevTLI, ThisTimeLineID)));
  }

     
                                                                           
                                                                     
     
  if (newTLI < ThisTimeLineID || !tliInHistory(newTLI, expectedTLEs))
  {
    ereport(PANIC, (errmsg("unexpected timeline ID %u (after %u) in checkpoint record", newTLI, ThisTimeLineID)));
  }

     
                                                                       
                                                                        
                                                                       
                                                                            
                                                                      
                                                                        
                                             
     
  if (!XLogRecPtrIsInvalid(minRecoveryPoint) && lsn < minRecoveryPoint && newTLI > minRecoveryPointTLI)
  {
    ereport(PANIC, (errmsg("unexpected timeline ID %u in checkpoint record, before reaching minimum recovery point %X/%X on timeline %u", newTLI, (uint32)(minRecoveryPoint >> 32), (uint32)minRecoveryPoint, minRecoveryPointTLI)));
  }

                  
}

   
                                    
   
                                                                          
                                                             
   
void
xlog_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
  XLogRecPtr lsn = record->EndRecPtr;

                                                                     
  Assert(info == XLOG_FPI || info == XLOG_FPI_FOR_HINT || !XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_NEXTOID)
  {
    Oid nextOid;

       
                                                                         
                                                                         
                                                                          
                                                                         
                                                            
       
    memcpy(&nextOid, XLogRecGetData(record), sizeof(Oid));
    LWLockAcquire(OidGenLock, LW_EXCLUSIVE);
    ShmemVariableCache->nextOid = nextOid;
    ShmemVariableCache->oidCount = 0;
    LWLockRelease(OidGenLock);
  }
  else if (info == XLOG_CHECKPOINT_SHUTDOWN)
  {
    CheckPoint checkPoint;

    memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
                                                                
    LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
    ShmemVariableCache->nextFullXid = checkPoint.nextFullXid;
    LWLockRelease(XidGenLock);
    LWLockAcquire(OidGenLock, LW_EXCLUSIVE);
    ShmemVariableCache->nextOid = checkPoint.nextOid;
    ShmemVariableCache->oidCount = 0;
    LWLockRelease(OidGenLock);
    MultiXactSetNextMXact(checkPoint.nextMulti, checkPoint.nextMultiOffset);

    MultiXactAdvanceOldest(checkPoint.oldestMulti, checkPoint.oldestMultiDB);

       
                                                                       
                                                                    
       
    SetTransactionIdLimit(checkPoint.oldestXid, checkPoint.oldestXidDB);

       
                                                                          
                                                                         
                     
       
    if (ArchiveRecoveryRequested && !XLogRecPtrIsInvalid(ControlFile->backupStartPoint) && XLogRecPtrIsInvalid(ControlFile->backupEndPoint))
    {
      ereport(PANIC, (errmsg("online backup was canceled, recovery cannot continue")));
    }

       
                                                                         
                                                                      
                                                                          
                                  
       
    if (standbyState >= STANDBY_INITIALIZED)
    {
      TransactionId *xids;
      int nxids;
      TransactionId oldestActiveXID;
      TransactionId latestCompletedXid;
      RunningTransactionsData running;

      oldestActiveXID = PrescanPreparedTransactions(&xids, &nxids);

         
                                                                      
                                                                         
                                                                       
                                                  
         
      running.xcnt = nxids;
      running.subxcnt = 0;
      running.subxid_overflow = false;
      running.nextXid = XidFromFullTransactionId(checkPoint.nextFullXid);
      running.oldestRunningXid = oldestActiveXID;
      latestCompletedXid = XidFromFullTransactionId(checkPoint.nextFullXid);
      TransactionIdRetreat(latestCompletedXid);
      Assert(TransactionIdIsNormal(latestCompletedXid));
      running.latestCompletedXid = latestCompletedXid;
      running.xids = xids;

      ProcArrayApplyRecoveryInfo(&running);

      StandbyRecoverPreparedTransactions();
    }

                                                                       
    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
    ControlFile->checkPointCopy.nextFullXid = checkPoint.nextFullXid;
    LWLockRelease(ControlFileLock);

                                                           
    SpinLockAcquire(&XLogCtl->info_lck);
    XLogCtl->ckptFullXid = checkPoint.nextFullXid;
    SpinLockRelease(&XLogCtl->info_lck);

       
                                                                          
               
       
    if (checkPoint.ThisTimeLineID != ThisTimeLineID)
    {
      ereport(PANIC, (errmsg("unexpected timeline ID %u (should be %u) in checkpoint record", checkPoint.ThisTimeLineID, ThisTimeLineID)));
    }

    RecoveryRestartPoint(&checkPoint);
  }
  else if (info == XLOG_CHECKPOINT_ONLINE)
  {
    CheckPoint checkPoint;

    memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
                                                                     
    LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
    if (FullTransactionIdPrecedes(ShmemVariableCache->nextFullXid, checkPoint.nextFullXid))
    {
      ShmemVariableCache->nextFullXid = checkPoint.nextFullXid;
    }
    LWLockRelease(XidGenLock);

       
                                                                         
                                                                          
                                                                           
                                                                         
                                                                      
                                                                          
                                                                       
                                                                        
                                                                        
       

                          
    MultiXactAdvanceNextMXact(checkPoint.nextMulti, checkPoint.nextMultiOffset);

       
                                                                    
                                      
       
    MultiXactAdvanceOldest(checkPoint.oldestMulti, checkPoint.oldestMultiDB);
    if (TransactionIdPrecedes(ShmemVariableCache->oldestXid, checkPoint.oldestXid))
    {
      SetTransactionIdLimit(checkPoint.oldestXid, checkPoint.oldestXidDB);
    }
                                                                       
    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
    ControlFile->checkPointCopy.nextFullXid = checkPoint.nextFullXid;
    LWLockRelease(ControlFileLock);

                                                           
    SpinLockAcquire(&XLogCtl->info_lck);
    XLogCtl->ckptFullXid = checkPoint.nextFullXid;
    SpinLockRelease(&XLogCtl->info_lck);

                                                        
    if (checkPoint.ThisTimeLineID != ThisTimeLineID)
    {
      ereport(PANIC, (errmsg("unexpected timeline ID %u (should be %u) in checkpoint record", checkPoint.ThisTimeLineID, ThisTimeLineID)));
    }

    RecoveryRestartPoint(&checkPoint);
  }
  else if (info == XLOG_OVERWRITE_CONTRECORD)
  {
    xl_overwrite_contrecord xlrec;

    memcpy(&xlrec, XLogRecGetData(record), sizeof(xl_overwrite_contrecord));
    VerifyOverwriteContrecord(&xlrec, record);
  }
  else if (info == XLOG_END_OF_RECOVERY)
  {
    xl_end_of_recovery xlrec;

    memcpy(&xlrec, XLogRecGetData(record), sizeof(xl_end_of_recovery));

       
                                                                        
                                                                         
                                                         
       

       
                                                                          
               
       
    if (xlrec.ThisTimeLineID != ThisTimeLineID)
    {
      ereport(PANIC, (errmsg("unexpected timeline ID %u (should be %u) in checkpoint record", xlrec.ThisTimeLineID, ThisTimeLineID)));
    }
  }
  else if (info == XLOG_NOOP)
  {
                            
  }
  else if (info == XLOG_SWITCH)
  {
                            
  }
  else if (info == XLOG_RESTORE_POINT)
  {
                            
  }
  else if (info == XLOG_FPI || info == XLOG_FPI_FOR_HINT)
  {
       
                                                                       
                                                                     
                                                                        
                    
       
                                                                           
                                                                        
                                                  
       
                                                                       
                                                                         
                                                                      
                                                                         
                                                              
       
    for (uint8 block_id = 0; block_id <= record->max_block_id; block_id++)
    {
      Buffer buffer;

      if (XLogReadBufferForRedo(record, block_id, &buffer) != BLK_RESTORED)
      {
        elog(ERROR, "unexpected XLogReadBufferForRedo result when restoring backup block");
      }
      UnlockReleaseBuffer(buffer);
    }
  }
  else if (info == XLOG_BACKUP_END)
  {
    XLogRecPtr startpoint;

    memcpy(&startpoint, XLogRecGetData(record), sizeof(startpoint));

    if (ControlFile->backupStartPoint == startpoint)
    {
         
                                                                 
                                                                        
                                                                     
                                                                     
                                                            
         
      elog(DEBUG1, "end of backup reached");

      LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

      if (ControlFile->minRecoveryPoint < lsn)
      {
        ControlFile->minRecoveryPoint = lsn;
        ControlFile->minRecoveryPointTLI = ThisTimeLineID;
      }
      ControlFile->backupStartPoint = InvalidXLogRecPtr;
      ControlFile->backupEndRequired = false;
      UpdateControlFile();

      LWLockRelease(ControlFileLock);
    }
  }
  else if (info == XLOG_PARAMETER_CHANGE)
  {
    xl_parameter_change xlrec;

                                                         
    memcpy(&xlrec, XLogRecGetData(record), sizeof(xl_parameter_change));

    LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
    ControlFile->MaxConnections = xlrec.MaxConnections;
    ControlFile->max_worker_processes = xlrec.max_worker_processes;
    ControlFile->max_wal_senders = xlrec.max_wal_senders;
    ControlFile->max_prepared_xacts = xlrec.max_prepared_xacts;
    ControlFile->max_locks_per_xact = xlrec.max_locks_per_xact;
    ControlFile->wal_level = xlrec.wal_level;
    ControlFile->wal_log_hints = xlrec.wal_log_hints;

       
                                                                         
                                                                        
                                                                        
                                                                       
                                                                   
                                                           
       
    if (InArchiveRecovery)
    {
      minRecoveryPoint = ControlFile->minRecoveryPoint;
      minRecoveryPointTLI = ControlFile->minRecoveryPointTLI;
    }
    if (minRecoveryPoint != InvalidXLogRecPtr && minRecoveryPoint < lsn)
    {
      ControlFile->minRecoveryPoint = lsn;
      ControlFile->minRecoveryPointTLI = ThisTimeLineID;
    }

    CommitTsParameterChange(xlrec.track_commit_timestamp, ControlFile->track_commit_timestamp);
    ControlFile->track_commit_timestamp = xlrec.track_commit_timestamp;

    UpdateControlFile();
    LWLockRelease(ControlFileLock);

                                                                          
    CheckRequiredParameterValues();
  }
  else if (info == XLOG_FPW_CHANGE)
  {
    bool fpw;

    memcpy(&fpw, XLogRecGetData(record), sizeof(bool));

       
                                                                          
                                                                      
                                                                
       
    if (!fpw)
    {
      SpinLockAcquire(&XLogCtl->info_lck);
      if (XLogCtl->lastFpwDisableRecPtr < ReadRecPtr)
      {
        XLogCtl->lastFpwDisableRecPtr = ReadRecPtr;
      }
      SpinLockRelease(&XLogCtl->info_lck);
    }

                                        
    lastFullPageWrites = fpw;
  }
}

   
                                                             
   
static void
VerifyOverwriteContrecord(xl_overwrite_contrecord *xlrec, XLogReaderState *state)
{
  if (xlrec->overwritten_lsn != state->overwrittenRecPtr)
  {
    elog(FATAL, "mismatching overwritten LSN %X/%X -> %X/%X", (uint32)(xlrec->overwritten_lsn >> 32), (uint32)xlrec->overwritten_lsn, (uint32)(state->overwrittenRecPtr >> 32), (uint32)state->overwrittenRecPtr);
  }

                                                 
  abortedRecPtr = InvalidXLogRecPtr;
  missingContrecPtr = InvalidXLogRecPtr;

  ereport(LOG, (errmsg("successfully skipped missing contrecord at %X/%X, overwritten at %s", (uint32)(xlrec->overwritten_lsn >> 32), (uint32)xlrec->overwritten_lsn, timestamptz_to_str(xlrec->overwrite_time))));

                                                    
  state->overwrittenRecPtr = InvalidXLogRecPtr;
}

#ifdef WAL_DEBUG

static void
xlog_outrec(StringInfo buf, XLogReaderState *record)
{
  int block_id;

  appendStringInfo(buf, "prev %X/%X; xid %u", (uint32)(XLogRecGetPrev(record) >> 32), (uint32)XLogRecGetPrev(record), XLogRecGetXid(record));

  appendStringInfo(buf, "; len %u", XLogRecGetDataLen(record));

                               
  for (block_id = 0; block_id <= record->max_block_id; block_id++)
  {
    RelFileNode rnode;
    ForkNumber forknum;
    BlockNumber blk;

    if (!XLogRecHasBlockRef(record, block_id))
    {
      continue;
    }

    XLogRecGetBlockTag(record, block_id, &rnode, &forknum, &blk);
    if (forknum != MAIN_FORKNUM)
    {
      appendStringInfo(buf, "; blkref #%u: rel %u/%u/%u, fork %u, blk %u", block_id, rnode.spcNode, rnode.dbNode, rnode.relNode, forknum, blk);
    }
    else
    {
      appendStringInfo(buf, "; blkref #%u: rel %u/%u/%u, blk %u", block_id, rnode.spcNode, rnode.dbNode, rnode.relNode, blk);
    }
    if (XLogRecHasBlockImage(record, block_id))
    {
      appendStringInfoString(buf, " FPW");
    }
  }
}
#endif                

   
                                                                         
                                                                       
   
static void
xlog_outdesc(StringInfo buf, XLogReaderState *record)
{
  RmgrId rmid = XLogRecGetRmid(record);
  uint8 info = XLogRecGetInfo(record);
  const char *id;

  appendStringInfoString(buf, RmgrTable[rmid].rm_name);
  appendStringInfoChar(buf, '/');

  id = RmgrTable[rmid].rm_identify(info);
  if (id == NULL)
  {
    appendStringInfo(buf, "UNKNOWN (%X): ", info & ~XLR_INFO_MASK);
  }
  else
  {
    appendStringInfo(buf, "%s: ", id);
  }

  RmgrTable[rmid].rm_desc(buf, record);
}

   
                                                                             
                                     
   
static int
get_sync_bit(int method)
{
  int o_direct_flag = 0;

                                                     
  if (!enableFsync)
  {
    return 0;
  }

     
                                                                        
                                                                          
                                                                            
                                                                            
                                                            
                                                                             
             
     
                                                                            
                                                                         
                                                                           
                                                                      
     
  if (!XLogIsNeeded() && !AmWalReceiverProcess())
  {
    o_direct_flag = PG_O_DIRECT;
  }

  switch (method)
  {
       
                                                                     
                                                                    
                                                                       
                     
       
  case SYNC_METHOD_FSYNC:
  case SYNC_METHOD_FSYNC_WRITETHROUGH:
  case SYNC_METHOD_FDATASYNC:
    return 0;
#ifdef OPEN_SYNC_FLAG
  case SYNC_METHOD_OPEN:
    return OPEN_SYNC_FLAG | o_direct_flag;
#endif
#ifdef OPEN_DATASYNC_FLAG
  case SYNC_METHOD_OPEN_DSYNC:
    return OPEN_DATASYNC_FLAG | o_direct_flag;
#endif
  default:
                                                                    
    elog(ERROR, "unrecognized wal_sync_method: %d", method);
    return 0;                      
  }
}

   
               
   
void
assign_xlog_sync_method(int new_sync_method, void *extra)
{
  if (sync_method != new_sync_method)
  {
       
                                                                       
                                                                       
                                                                          
                         
       
    if (openLogFile >= 0)
    {
      pgstat_report_wait_start(WAIT_EVENT_WAL_SYNC_METHOD_ASSIGN);
      if (pg_fsync(openLogFile) != 0)
      {
        ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", XLogFileNameP(ThisTimeLineID, openLogSegNo))));
      }
      pgstat_report_wait_end();
      if (get_sync_bit(sync_method) != get_sync_bit(new_sync_method))
      {
        XLogFileClose();
      }
    }
  }
}

   
                                                                     
   
                                                              
                                            
   
void
issue_xlog_fsync(int fd, XLogSegNo segno)
{
  pgstat_report_wait_start(WAIT_EVENT_WAL_SYNC);
  switch (sync_method)
  {
  case SYNC_METHOD_FSYNC:
    if (pg_fsync_no_writethrough(fd) != 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", XLogFileNameP(ThisTimeLineID, segno))));
    }
    break;
#ifdef HAVE_FSYNC_WRITETHROUGH
  case SYNC_METHOD_FSYNC_WRITETHROUGH:
    if (pg_fsync_writethrough(fd) != 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync write-through file \"%s\": %m", XLogFileNameP(ThisTimeLineID, segno))));
    }
    break;
#endif
#ifdef HAVE_FDATASYNC
  case SYNC_METHOD_FDATASYNC:
    if (pg_fdatasync(fd) != 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not fdatasync file \"%s\": %m", XLogFileNameP(ThisTimeLineID, segno))));
    }
    break;
#endif
  case SYNC_METHOD_OPEN:
  case SYNC_METHOD_OPEN_DSYNC:
                                 
    break;
  default:
    elog(PANIC, "unrecognized wal_sync_method: %d", sync_method);
    break;
  }
  pgstat_report_wait_end();
}

   
                                                                   
   
char *
XLogFileNameP(TimeLineID tli, XLogSegNo segno)
{
  char *result = palloc(MAXFNAMELEN);

  XLogFileName(result, tli, segno, wal_segment_size);
  return result;
}

   
                      
   
                                                                            
                                                                       
   
                                                                            
                                                                              
                                                                             
                                                                            
                                
   
                                                                      
                                                                              
                                                                              
                                                                                   
                                                                             
                                                                                
                                                                             
           
   
                                                                                
                                                                              
                                                                
   
                                                                     
                                                                        
                                                                    
                                                                       
   
                                                                              
                                                             
   
                                                                              
                                                
   
                                                                         
                                    
   
XLogRecPtr
do_pg_start_backup(const char *backupidstr, bool fast, TimeLineID *starttli_p, StringInfo labelfile, List **tablespaces, StringInfo tblspcmapfile, bool infotbssize, bool needtblspcmapfile)
{
  bool exclusive = (labelfile == NULL);
  bool backup_started_in_recovery = false;
  XLogRecPtr checkpointloc;
  XLogRecPtr startpoint;
  TimeLineID starttli;
  pg_time_t stamp_time;
  char strfbuf[128];
  char xlogfilename[MAXFNAMELEN];
  XLogSegNo _logSegNo;
  struct stat stat_buf;
  FILE *fp;

  backup_started_in_recovery = RecoveryInProgress();

     
                                                                       
     
  if (backup_started_in_recovery && exclusive)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery.")));
  }

     
                                                                        
                                                                           
     
  if (!backup_started_in_recovery && !XLogIsNeeded())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("WAL level not sufficient for making an online backup"), errhint("wal_level must be set to \"replica\" or \"logical\" at server start.")));
  }

  if (strlen(backupidstr) > MAXPGPATH)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("backup label too long (max %d bytes)", MAXPGPATH)));
  }

     
                                                                           
                                                                           
                                                                           
                                                                             
                                                                         
                                                                           
                                                                          
                                                                         
                                                                             
                                                                         
                                                                            
                                                       
     
                                                                          
                  
     
                                                                 
                                                              
                         
     
  WALInsertLockAcquireExclusive();
  if (exclusive)
  {
       
                                                                      
                                                                 
                                              
       
    if (XLogCtl->Insert.exclusiveBackupState != EXCLUSIVE_BACKUP_NONE)
    {
      WALInsertLockRelease();
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("a backup is already in progress"), errhint("Run pg_stop_backup() and try again.")));
    }
    XLogCtl->Insert.exclusiveBackupState = EXCLUSIVE_BACKUP_STARTING;
  }
  else
  {
    XLogCtl->Insert.nonExclusiveBackups++;
  }
  XLogCtl->Insert.forcePageWrites = true;
  WALInsertLockRelease();

                                                       
  PG_ENSURE_ERROR_CLEANUP(pg_start_backup_callback, (Datum)BoolGetDatum(exclusive));
  {
    bool gotUniqueStartpoint = false;
    DIR *tblspcdir;
    struct dirent *de;
    tablespaceinfo *ti;
    int datadirpathlen;

       
                                                                           
                                                                           
                                                                    
                                                                        
                                                                        
                                                                          
                                                                           
                                                                        
                                                           
       
                                                                         
                                                                   
                                                                     
                                                                      
                               
       
                                                                           
                                                                         
                                      
       
    if (!backup_started_in_recovery)
    {
      RequestXLogSwitch(false);
    }

    do
    {
      bool checkpointfpw;

         
                                                                         
                                                                        
                                                                      
                                                                  
         
                                                                       
                                                                       
                                                                        
                    
         
                                                                   
                                                                        
                                                          
         
                                                                    
                                                                
         
      RequestCheckpoint(CHECKPOINT_FORCE | CHECKPOINT_WAIT | (fast ? CHECKPOINT_IMMEDIATE : 0));

         
                                                                       
                                                                         
                                                                       
                  
         
      LWLockAcquire(ControlFileLock, LW_SHARED);
      checkpointloc = ControlFile->checkPoint;
      startpoint = ControlFile->checkPointCopy.redo;
      starttli = ControlFile->checkPointCopy.ThisTimeLineID;
      checkpointfpw = ControlFile->checkPointCopy.fullPageWrites;
      LWLockRelease(ControlFileLock);

      if (backup_started_in_recovery)
      {
        XLogRecPtr recptr;

           
                                                                 
                                                                  
                                                 
           
        SpinLockAcquire(&XLogCtl->info_lck);
        recptr = XLogCtl->lastFpwDisableRecPtr;
        SpinLockRelease(&XLogCtl->info_lck);

        if (!checkpointfpw || startpoint <= recptr)
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                             errmsg("WAL generated with full_page_writes=off was replayed "
                                    "since last restartpoint"),
                             errhint("This means that the backup being taken on the standby "
                                     "is corrupt and should not be used. "
                                     "Enable full_page_writes and run CHECKPOINT on the master, "
                                     "and then try an online backup again.")));
        }

           
                                                                     
                                                               
                                                                       
                                                                    
                                                      
           
        gotUniqueStartpoint = true;
      }

         
                                                                         
                                                                  
                                                                        
                                                                        
                                                                       
                                                                        
                                                                    
                                                                       
                                                                
         
      WALInsertLockAcquireExclusive();
      if (XLogCtl->Insert.lastBackupStart < startpoint)
      {
        XLogCtl->Insert.lastBackupStart = startpoint;
        gotUniqueStartpoint = true;
      }
      WALInsertLockRelease();
    } while (!gotUniqueStartpoint);

    XLByteToSeg(startpoint, _logSegNo, wal_segment_size);
    XLogFileName(xlogfilename, starttli, _logSegNo, wal_segment_size);

       
                                     
       
    if (exclusive)
    {
      tblspcmapfile = makeStringInfo();
    }

    datadirpathlen = strlen(DataDir);

                                                   
    tblspcdir = AllocateDir("pg_tblspc");
    while ((de = ReadDir(tblspcdir, "pg_tblspc")) != NULL)
    {
      char fullpath[MAXPGPATH + 10];
      char linkpath[MAXPGPATH];
      char *relpath = NULL;
      int rllen;
      StringInfoData buflinkpath;
      char *s = linkpath;
#ifndef WIN32
      struct stat st;
#endif

                              
      if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      {
        continue;
      }

      snprintf(fullpath, sizeof(fullpath), "pg_tblspc/%s", de->d_name);

         
                                                                         
                                                               
                                                                       
         
#ifndef WIN32
      if (lstat(fullpath, &st) < 0)
      {
        ereport(LOG, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", fullpath)));
      }
      else if (!S_ISLNK(st.st_mode))
#else            
      if (!pgwin32_is_junction(fullpath))
#endif
        continue;

#if defined(HAVE_READLINK) || defined(WIN32)
      rllen = readlink(fullpath, linkpath, sizeof(linkpath));
      if (rllen < 0)
      {
        ereport(WARNING, (errmsg("could not read symbolic link \"%s\": %m", fullpath)));
        continue;
      }
      else if (rllen >= sizeof(linkpath))
      {
        ereport(WARNING, (errmsg("symbolic link \"%s\" target is too long", fullpath)));
        continue;
      }
      linkpath[rllen] = '\0';

         
                                                                     
                                                                   
                                                                      
                                       
         
      initStringInfo(&buflinkpath);

      while (*s)
      {
        if ((*s == '\n' || *s == '\r') && needtblspcmapfile)
        {
          appendStringInfoChar(&buflinkpath, '\\');
        }
        appendStringInfoChar(&buflinkpath, *s++);
      }

         
                                                                     
                                                                  
                    
         
      if (rllen > datadirpathlen && strncmp(linkpath, DataDir, datadirpathlen) == 0 && IS_DIR_SEP(linkpath[datadirpathlen]))
      {
        relpath = linkpath + datadirpathlen + 1;
      }

      ti = palloc(sizeof(tablespaceinfo));
      ti->oid = pstrdup(de->d_name);
      ti->path = pstrdup(buflinkpath.data);
      ti->rpath = relpath ? pstrdup(relpath) : NULL;
      ti->size = infotbssize ? sendTablespace(fullpath, true) : -1;

      if (tablespaces)
      {
        *tablespaces = lappend(*tablespaces, ti);
      }

      appendStringInfo(tblspcmapfile, "%s %s\n", ti->oid, ti->path);

      pfree(buflinkpath.data);
#else

         
                                                                        
                                                                      
                                         
         
      ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tablespaces are not supported on this platform")));
#endif
    }
    FreeDir(tblspcdir);

       
                                   
       
    if (exclusive)
    {
      labelfile = makeStringInfo();
    }

                                                             
    stamp_time = (pg_time_t)time(NULL);
    pg_strftime(strfbuf, sizeof(strfbuf), "%Y-%m-%d %H:%M:%S %Z", pg_localtime(&stamp_time, log_timezone));
    appendStringInfo(labelfile, "START WAL LOCATION: %X/%X (file %s)\n", (uint32)(startpoint >> 32), (uint32)startpoint, xlogfilename);
    appendStringInfo(labelfile, "CHECKPOINT LOCATION: %X/%X\n", (uint32)(checkpointloc >> 32), (uint32)checkpointloc);
    appendStringInfo(labelfile, "BACKUP METHOD: %s\n", exclusive ? "pg_start_backup" : "streamed");
    appendStringInfo(labelfile, "BACKUP FROM: %s\n", backup_started_in_recovery ? "standby" : "master");
    appendStringInfo(labelfile, "START TIME: %s\n", strfbuf);
    appendStringInfo(labelfile, "LABEL: %s\n", backupidstr);
    appendStringInfo(labelfile, "START TIMELINE: %u\n", starttli);

       
                                                               
       
    if (exclusive)
    {
         
                                                                         
                                                                   
                                                                   
                
         
      if (stat(BACKUP_LABEL_FILE, &stat_buf) != 0)
      {
        if (errno != ENOENT)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", BACKUP_LABEL_FILE)));
        }
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("a backup is already in progress"), errhint("If you're sure there is no backup in progress, remove file \"%s\" and try again.", BACKUP_LABEL_FILE)));
      }

      fp = AllocateFile(BACKUP_LABEL_FILE, "w");

      if (!fp)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", BACKUP_LABEL_FILE)));
      }
      if (fwrite(labelfile->data, labelfile->len, 1, fp) != 1 || fflush(fp) != 0 || pg_fsync(fileno(fp)) != 0 || ferror(fp) || FreeFile(fp))
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", BACKUP_LABEL_FILE)));
      }
                                                                       
      pfree(labelfile->data);
      pfree(labelfile);

                                             
      if (tblspcmapfile->len > 0)
      {
        if (stat(TABLESPACE_MAP, &stat_buf) != 0)
        {
          if (errno != ENOENT)
          {
            ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", TABLESPACE_MAP)));
          }
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("a backup is already in progress"), errhint("If you're sure there is no backup in progress, remove file \"%s\" and try again.", TABLESPACE_MAP)));
        }

        fp = AllocateFile(TABLESPACE_MAP, "w");

        if (!fp)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", TABLESPACE_MAP)));
        }
        if (fwrite(tblspcmapfile->data, tblspcmapfile->len, 1, fp) != 1 || fflush(fp) != 0 || pg_fsync(fileno(fp)) != 0 || ferror(fp) || FreeFile(fp))
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", TABLESPACE_MAP)));
        }
      }

                                                                       
      pfree(tblspcmapfile->data);
      pfree(tblspcmapfile);
    }
  }
  PG_END_ENSURE_ERROR_CLEANUP(pg_start_backup_callback, (Datum)BoolGetDatum(exclusive));

     
                                                                           
                                                                    
     
                                                                           
                                                                    
                                                                         
     
  if (exclusive)
  {
    WALInsertLockAcquireExclusive();
    XLogCtl->Insert.exclusiveBackupState = EXCLUSIVE_BACKUP_IN_PROGRESS;

                                
    sessionBackupState = SESSION_BACKUP_EXCLUSIVE;
    WALInsertLockRelease();
  }
  else
  {
    sessionBackupState = SESSION_BACKUP_NON_EXCLUSIVE;
  }

     
                                                                      
     
  if (starttli_p)
  {
    *starttli_p = starttli;
  }
  return startpoint;
}

                                                
static void
pg_start_backup_callback(int code, Datum arg)
{
  bool exclusive = DatumGetBool(arg);

                                                             
  WALInsertLockAcquireExclusive();
  if (exclusive)
  {
    Assert(XLogCtl->Insert.exclusiveBackupState == EXCLUSIVE_BACKUP_STARTING);
    XLogCtl->Insert.exclusiveBackupState = EXCLUSIVE_BACKUP_NONE;
  }
  else
  {
    Assert(XLogCtl->Insert.nonExclusiveBackups > 0);
    XLogCtl->Insert.nonExclusiveBackups--;
  }

  if (XLogCtl->Insert.exclusiveBackupState == EXCLUSIVE_BACKUP_NONE && XLogCtl->Insert.nonExclusiveBackups == 0)
  {
    XLogCtl->Insert.forcePageWrites = false;
  }
  WALInsertLockRelease();
}

   
                                             
   
static void
pg_stop_backup_callback(int code, Datum arg)
{
  bool exclusive = DatumGetBool(arg);

                                       
  WALInsertLockAcquireExclusive();
  if (exclusive)
  {
    Assert(XLogCtl->Insert.exclusiveBackupState == EXCLUSIVE_BACKUP_STOPPING);
    XLogCtl->Insert.exclusiveBackupState = EXCLUSIVE_BACKUP_IN_PROGRESS;
  }
  WALInsertLockRelease();
}

   
                                                                          
   
SessionBackupState
get_backup_status(void)
{
  return sessionBackupState;
}

   
                     
   
                                                                            
                                                                         
   
                                                                              
                                                      
   
                                                                           
                                                            
   
                                                                         
                                    
   
XLogRecPtr
do_pg_stop_backup(char *labelfile, bool waitforarchive, TimeLineID *stoptli_p)
{
  bool exclusive = (labelfile == NULL);
  bool backup_started_in_recovery = false;
  XLogRecPtr startpoint;
  XLogRecPtr stoppoint;
  TimeLineID stoptli;
  pg_time_t stamp_time;
  char strfbuf[128];
  char histfilepath[MAXPGPATH];
  char startxlogfilename[MAXFNAMELEN];
  char stopxlogfilename[MAXFNAMELEN];
  char lastxlogfilename[MAXFNAMELEN];
  char histfilename[MAXFNAMELEN];
  char backupfrom[20];
  XLogSegNo _logSegNo;
  FILE *lfp;
  FILE *fp;
  char ch;
  int seconds_before_warning;
  int waits = 0;
  bool reported_waiting = false;
  char *remaining;
  char *ptr;
  uint32 hi, lo;

  backup_started_in_recovery = RecoveryInProgress();

     
                                                                       
     
  if (backup_started_in_recovery && exclusive)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery.")));
  }

     
                                                                        
                                                                           
     
  if (!backup_started_in_recovery && !XLogIsNeeded())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("WAL level not sufficient for making an online backup"), errhint("wal_level must be set to \"replica\" or \"logical\" at server start.")));
  }

  if (exclusive)
  {
       
                                                                      
                                                                 
                                              
       
    WALInsertLockAcquireExclusive();
    if (XLogCtl->Insert.exclusiveBackupState != EXCLUSIVE_BACKUP_IN_PROGRESS)
    {
      WALInsertLockRelease();
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("exclusive backup not in progress")));
    }
    XLogCtl->Insert.exclusiveBackupState = EXCLUSIVE_BACKUP_STOPPING;
    WALInsertLockRelease();

       
                                                                           
                                               
       
    PG_ENSURE_ERROR_CLEANUP(pg_stop_backup_callback, (Datum)BoolGetDatum(exclusive));
    {
         
                                                   
         
      struct stat statbuf;
      int r;

      if (stat(BACKUP_LABEL_FILE, &statbuf))
      {
                                                    
        if (errno != ENOENT)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", BACKUP_LABEL_FILE)));
        }
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("a backup is not in progress")));
      }

      lfp = AllocateFile(BACKUP_LABEL_FILE, "r");
      if (!lfp)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", BACKUP_LABEL_FILE)));
      }
      labelfile = palloc(statbuf.st_size + 1);
      r = fread(labelfile, statbuf.st_size, 1, lfp);
      labelfile[statbuf.st_size] = '\0';

         
                                                
         
      if (r != 1 || ferror(lfp) || FreeFile(lfp))
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", BACKUP_LABEL_FILE)));
      }
      durable_unlink(BACKUP_LABEL_FILE, ERROR);

         
                                                                      
                                
         
      durable_unlink(TABLESPACE_MAP, DEBUG1);
    }
    PG_END_ENSURE_ERROR_CLEANUP(pg_stop_backup_callback, (Datum)BoolGetDatum(exclusive));
  }

     
                                                                           
     
                                                                          
                                                                         
                                   
     
  WALInsertLockAcquireExclusive();
  if (exclusive)
  {
    XLogCtl->Insert.exclusiveBackupState = EXCLUSIVE_BACKUP_NONE;
  }
  else
  {
       
                                                                         
                                                                          
                                                                      
                                                        
       
    Assert(XLogCtl->Insert.nonExclusiveBackups > 0);
    XLogCtl->Insert.nonExclusiveBackups--;
  }

  if (XLogCtl->Insert.exclusiveBackupState == EXCLUSIVE_BACKUP_NONE && XLogCtl->Insert.nonExclusiveBackups == 0)
  {
    XLogCtl->Insert.forcePageWrites = false;
  }

     
                                  
     
                                                                      
                                                                            
                                                        
                                                                        
                           
     
  sessionBackupState = SESSION_BACKUP_NONE;

  WALInsertLockRelease();

     
                                                                            
                                                                   
     
  if (sscanf(labelfile, "START WAL LOCATION: %X/%X (file %24s)%c", &hi, &lo, startxlogfilename, &ch) != 4 || ch != '\n')
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
  }
  startpoint = ((uint64)hi) << 32 | lo;
  remaining = strchr(labelfile, '\n') + 1;                                

     
                                                                            
                                                                           
             
     
  ptr = strstr(remaining, "BACKUP FROM:");
  if (!ptr || sscanf(ptr, "BACKUP FROM: %19s\n", backupfrom) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
  }
  if (strcmp(backupfrom, "standby") == 0 && !backup_started_in_recovery)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("the standby was promoted during online backup"),
                       errhint("This means that the backup being taken is corrupt "
                               "and should not be used. "
                               "Try taking another online backup.")));
  }

     
                                                                             
                                                                         
                                                                  
                                                                        
                                                                      
                                                                            
              
     
                                                                         
                                                                         
                                                                          
                                                                        
                                                                             
                                                                   
                
     
                                                                    
                                                                     
                                                                           
                                                    
     
                                                                        
                                                                          
                                                                           
                                                                          
                                          
     
  if (backup_started_in_recovery)
  {
    XLogRecPtr recptr;

       
                                                                     
                         
       
    SpinLockAcquire(&XLogCtl->info_lck);
    recptr = XLogCtl->lastFpwDisableRecPtr;
    SpinLockRelease(&XLogCtl->info_lck);

    if (startpoint <= recptr)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                         errmsg("WAL generated with full_page_writes=off was replayed "
                                "during online backup"),
                         errhint("This means that the backup being taken on the standby "
                                 "is corrupt and should not be used. "
                                 "Enable full_page_writes and run CHECKPOINT on the master, "
                                 "and then try an online backup again.")));
    }

    LWLockAcquire(ControlFileLock, LW_SHARED);
    stoppoint = ControlFile->minRecoveryPoint;
    stoptli = ControlFile->minRecoveryPointTLI;
    LWLockRelease(ControlFileLock);
  }
  else
  {
       
                                        
       
    XLogBeginInsert();
    XLogRegisterData((char *)(&startpoint), sizeof(startpoint));
    stoppoint = XLogInsert(RM_XLOG_ID, XLOG_BACKUP_END);
    stoptli = ThisTimeLineID;

       
                                                                        
                                                                     
       
    RequestXLogSwitch(false);

    XLByteToPrevSeg(stoppoint, _logSegNo, wal_segment_size);
    XLogFileName(stopxlogfilename, stoptli, _logSegNo, wal_segment_size);

                                                             
    stamp_time = (pg_time_t)time(NULL);
    pg_strftime(strfbuf, sizeof(strfbuf), "%Y-%m-%d %H:%M:%S %Z", pg_localtime(&stamp_time, log_timezone));

       
                                     
       
    XLByteToSeg(startpoint, _logSegNo, wal_segment_size);
    BackupHistoryFilePath(histfilepath, stoptli, _logSegNo, startpoint, wal_segment_size);
    fp = AllocateFile(histfilepath, "w");
    if (!fp)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", histfilepath)));
    }
    fprintf(fp, "START WAL LOCATION: %X/%X (file %s)\n", (uint32)(startpoint >> 32), (uint32)startpoint, startxlogfilename);
    fprintf(fp, "STOP WAL LOCATION: %X/%X (file %s)\n", (uint32)(stoppoint >> 32), (uint32)stoppoint, stopxlogfilename);

       
                                                                      
                     
       
    fprintf(fp, "%s", remaining);
    fprintf(fp, "STOP TIME: %s\n", strfbuf);
    fprintf(fp, "STOP TIMELINE: %u\n", stoptli);
    if (fflush(fp) || ferror(fp) || FreeFile(fp))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", histfilepath)));
    }

       
                                                                        
                                                                        
                                                                
                    
       
    CleanupBackupHistory();
  }

     
                                                                        
                                                                             
                                                                       
                                                                            
                                                                             
                                                                            
                       
     
                                                                       
                                                                             
                                                                        
                       
     
                                                                       
                                                                         
                                                                            
                                                                         
                                                           
     

  if (waitforarchive && ((!backup_started_in_recovery && XLogArchivingActive()) || (backup_started_in_recovery && XLogArchivingAlways())))
  {
    XLByteToPrevSeg(stoppoint, _logSegNo, wal_segment_size);
    XLogFileName(lastxlogfilename, stoptli, _logSegNo, wal_segment_size);

    XLByteToSeg(startpoint, _logSegNo, wal_segment_size);
    BackupHistoryFileName(histfilename, stoptli, _logSegNo, startpoint, wal_segment_size);

    seconds_before_warning = 60;
    waits = 0;

    while (XLogArchiveIsBusy(lastxlogfilename) || XLogArchiveIsBusy(histfilename))
    {
      CHECK_FOR_INTERRUPTS();

      if (!reported_waiting && waits > 5)
      {
        ereport(NOTICE, (errmsg("base backup done, waiting for required WAL segments to be archived")));
        reported_waiting = true;
      }

      pg_usleep(1000000L);

      if (++waits >= seconds_before_warning)
      {
        seconds_before_warning *= 2;                                 
        ereport(WARNING, (errmsg("still waiting for all required WAL segments to be archived (%d seconds elapsed)", waits), errhint("Check that your archive_command is executing properly.  "
                                                                                                                                    "You can safely cancel this backup, "
                                                                                                                                    "but the database backup will not be usable without all the WAL segments.")));
      }
    }

    ereport(NOTICE, (errmsg("all required WAL segments have been archived")));
  }
  else if (waitforarchive)
  {
    ereport(NOTICE, (errmsg("WAL archiving is not enabled; you must ensure that all required WAL segments are copied through other means to complete the backup")));
  }

     
                                                                    
     
  if (stoptli_p)
  {
    *stoptli_p = stoptli;
  }
  return stoppoint;
}

   
                                              
   
                                                                             
                                                                          
                     
   
                                                                               
               
   
                                                                           
                                                                              
                          
   
                                                                            
              
   
void
do_pg_abort_backup(int code, Datum arg)
{
  bool emit_warning = DatumGetBool(arg);

     
                                                                        
                      
     
  if (sessionBackupState != SESSION_BACKUP_NON_EXCLUSIVE)
  {
    return;
  }

  WALInsertLockAcquireExclusive();
  Assert(XLogCtl->Insert.nonExclusiveBackups > 0);
  XLogCtl->Insert.nonExclusiveBackups--;

  if (XLogCtl->Insert.exclusiveBackupState == EXCLUSIVE_BACKUP_NONE && XLogCtl->Insert.nonExclusiveBackups == 0)
  {
    XLogCtl->Insert.forcePageWrites = false;
  }

  sessionBackupState = SESSION_BACKUP_NONE;
  WALInsertLockRelease();

  if (emit_warning)
  {
    ereport(WARNING, (errmsg("aborting backup due to backend exiting before pg_stop_backup was called")));
  }
}

   
                                                                          
                                               
   
void
register_persistent_abort_backup_handler(void)
{
  static bool already_done = false;

  if (already_done)
  {
    return;
  }
  before_shmem_exit(do_pg_abort_backup, DatumGetBool(true));
  already_done = true;
}

   
                                   
   
                                                               
   
XLogRecPtr
GetXLogReplayRecPtr(TimeLineID *replayTLI)
{
  XLogRecPtr recptr;
  TimeLineID tli;

  SpinLockAcquire(&XLogCtl->info_lck);
  recptr = XLogCtl->lastReplayedEndRecPtr;
  tli = XLogCtl->lastReplayedTLI;
  SpinLockRelease(&XLogCtl->info_lck);

  if (replayTLI)
  {
    *replayTLI = tli;
  }
  return recptr;
}

   
                                 
   
XLogRecPtr
GetXLogInsertRecPtr(void)
{
  XLogCtlInsert *Insert = &XLogCtl->Insert;
  uint64 current_bytepos;

  SpinLockAcquire(&Insert->insertpos_lck);
  current_bytepos = Insert->CurrBytePos;
  SpinLockRelease(&Insert->insertpos_lck);

  return XLogBytePosToRecPtr(current_bytepos);
}

   
                                
   
XLogRecPtr
GetXLogWriteRecPtr(void)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  LogwrtResult = XLogCtl->LogwrtResult;
  SpinLockRelease(&XLogCtl->info_lck);

  return LogwrtResult.Write;
}

   
                                                                            
                                                                               
   
void
GetOldestRestartPoint(XLogRecPtr *oldrecptr, TimeLineID *oldtli)
{
  LWLockAcquire(ControlFileLock, LW_SHARED);
  *oldrecptr = ControlFile->checkPointCopy.redo;
  *oldtli = ControlFile->checkPointCopy.ThisTimeLineID;
  LWLockRelease(ControlFileLock);
}

   
                                                                     
   
                                                                              
                                                                              
                                                                            
                                                                            
                                                                          
                                                               
   
                                                                      
                                                                        
                                                                         
                                                                            
                                                                   
   
static bool
read_backup_label(XLogRecPtr *checkPointLoc, bool *backupEndRequired, bool *backupFromStandby)
{
  char startxlogfilename[MAXFNAMELEN];
  TimeLineID tli_from_walseg, tli_from_file;
  FILE *lfp;
  char ch;
  char backuptype[20];
  char backupfrom[20];
  char backuplabel[MAXPGPATH];
  char backuptime[128];
  uint32 hi, lo;

  *backupEndRequired = false;
  *backupFromStandby = false;

     
                                  
     
  lfp = AllocateFile(BACKUP_LABEL_FILE, "r");
  if (!lfp)
  {
    if (errno != ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", BACKUP_LABEL_FILE)));
    }
    return false;                                  
  }

     
                                                                           
                                                                           
              
     
  if (fscanf(lfp, "START WAL LOCATION: %X/%X (file %08X%16s)%c", &hi, &lo, &tli_from_walseg, startxlogfilename, &ch) != 5 || ch != '\n')
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
  }
  RedoStartLSN = ((uint64)hi) << 32 | lo;
  if (fscanf(lfp, "CHECKPOINT LOCATION: %X/%X%c", &hi, &lo, &ch) != 3 || ch != '\n')
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
  }
  *checkPointLoc = ((uint64)hi) << 32 | lo;

     
                                                                          
                                                                         
                                                                         
     
  if (fscanf(lfp, "BACKUP METHOD: %19s\n", backuptype) == 1)
  {
    if (strcmp(backuptype, "streamed") == 0)
    {
      *backupEndRequired = true;
    }
  }

  if (fscanf(lfp, "BACKUP FROM: %19s\n", backupfrom) == 1)
  {
    if (strcmp(backupfrom, "standby") == 0)
    {
      *backupFromStandby = true;
    }
  }

     
                                                                             
                                                                          
                                                                           
                                                                           
                                                                            
                                                                    
                 
     
  if (fscanf(lfp, "START TIME: %127[^\n]\n", backuptime) == 1)
  {
    ereport(DEBUG1, (errmsg("backup time %s in file \"%s\"", backuptime, BACKUP_LABEL_FILE)));
  }

  if (fscanf(lfp, "LABEL: %1023[^\n]\n", backuplabel) == 1)
  {
    ereport(DEBUG1, (errmsg("backup label %s in file \"%s\"", backuplabel, BACKUP_LABEL_FILE)));
  }

     
                                                                             
                                      
     
  if (fscanf(lfp, "START TIMELINE: %u\n", &tli_from_file) == 1)
  {
    if (tli_from_walseg != tli_from_file)
    {
      ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE), errdetail("Timeline ID parsed is %u, but expected %u.", tli_from_file, tli_from_walseg)));
    }

    ereport(DEBUG1, (errmsg("backup timeline %u in file \"%s\"", tli_from_file, BACKUP_LABEL_FILE)));
  }

  if (ferror(lfp) || FreeFile(lfp))
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", BACKUP_LABEL_FILE)));
  }

  return true;
}

   
                                                                         
   
                                                                          
                                                                                
                                                          
   
                                                                       
                                                                            
           
   
static bool
read_tablespace_map(List **tablespaces)
{
  tablespaceinfo *ti;
  FILE *lfp;
  char tbsoid[MAXPGPATH];
  char *tbslinkpath;
  char str[MAXPGPATH];
  int ch, prev_ch = -1, i = 0, n;

     
                                           
     
  lfp = AllocateFile(TABLESPACE_MAP, "r");
  if (!lfp)
  {
    if (errno != ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", TABLESPACE_MAP)));
    }
    return false;                                  
  }

     
                                                                          
                                                                             
                                                                           
                                                                  
                                                                          
                                                                         
                                                                            
     
  while ((ch = fgetc(lfp)) != EOF)
  {
    if ((ch == '\n' || ch == '\r') && prev_ch != '\\')
    {
      str[i] = '\0';
      if (sscanf(str, "%s %n", tbsoid, &n) != 1)
      {
        ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", TABLESPACE_MAP)));
      }
      tbslinkpath = str + n;
      i = 0;

      ti = palloc(sizeof(tablespaceinfo));
      ti->oid = pstrdup(tbsoid);
      ti->path = pstrdup(tbslinkpath);

      *tablespaces = lappend(*tablespaces, ti);
      continue;
    }
    else if ((ch == '\n' || ch == '\r') && prev_ch == '\\')
    {
      str[i - 1] = ch;
    }
    else if (i < sizeof(str) - 1)
    {
      str[i++] = ch;
    }
    prev_ch = ch;
  }

  if (ferror(lfp) || FreeFile(lfp))
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", TABLESPACE_MAP)));
  }

  return true;
}

   
                                                                 
   
static void
rm_redo_error_callback(void *arg)
{
  XLogReaderState *record = (XLogReaderState *)arg;
  StringInfoData buf;

  initStringInfo(&buf);
  xlog_outdesc(&buf, record);

                                                  
  errcontext("WAL redo at %X/%X for %s", (uint32)(record->ReadRecPtr >> 32), (uint32)record->ReadRecPtr, buf.data);

  pfree(buf.data);
}

   
                                                           
   
                                                                      
   
bool
BackupInProgress(void)
{
  struct stat stat_buf;

  return (stat(BACKUP_LABEL_FILE, &stat_buf) == 0);
}

   
                                                                
                                   
   
                                                                                
                                                                         
                         
   
                                                           
                                                                         
           
   
void
CancelBackup(void)
{
  struct stat stat_buf;

                                                     
  if (stat(BACKUP_LABEL_FILE, &stat_buf) < 0)
  {
    return;
  }

                                                                         
  unlink(BACKUP_LABEL_OLD);

  if (durable_rename(BACKUP_LABEL_FILE, BACKUP_LABEL_OLD, DEBUG1) != 0)
  {
    ereport(WARNING, (errcode_for_file_access(), errmsg("online backup mode was not canceled"), errdetail("File \"%s\" could not be renamed to \"%s\": %m.", BACKUP_LABEL_FILE, BACKUP_LABEL_OLD)));
    return;
  }

                                                       
  if (stat(TABLESPACE_MAP, &stat_buf) < 0)
  {
    ereport(LOG, (errmsg("online backup mode canceled"), errdetail("File \"%s\" was renamed to \"%s\".", BACKUP_LABEL_FILE, BACKUP_LABEL_OLD)));
    return;
  }

                                                                         
  unlink(TABLESPACE_MAP_OLD);

  if (durable_rename(TABLESPACE_MAP, TABLESPACE_MAP_OLD, DEBUG1) == 0)
  {
    ereport(LOG, (errmsg("online backup mode canceled"), errdetail("Files \"%s\" and \"%s\" were renamed to "
                                                                   "\"%s\" and \"%s\", respectively.",
                                                             BACKUP_LABEL_FILE, TABLESPACE_MAP, BACKUP_LABEL_OLD, TABLESPACE_MAP_OLD)));
  }
  else
  {
    ereport(WARNING, (errcode_for_file_access(), errmsg("online backup mode canceled"),
                         errdetail("File \"%s\" was renamed to \"%s\", but "
                                   "file \"%s\" could not be renamed to \"%s\": %m.",
                             BACKUP_LABEL_FILE, BACKUP_LABEL_OLD, TABLESPACE_MAP, TABLESPACE_MAP_OLD)));
  }
}

   
                                                                            
                                                                         
                                                                        
                                              
   
                                                                           
                                                                          
   
                                                                          
                                                                          
                                                                             
                                                                       
           
   
                                                                       
                                                                      
                                                  
                                                             
                                                                    
                                                                        
                    
   
static int
XLogPageRead(XLogReaderState *xlogreader, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *readBuf, TimeLineID *readTLI)
{
  XLogPageReadPrivate *private = (XLogPageReadPrivate *)xlogreader->private_data;
  int emode = private->emode;
  uint32 targetPageOff;
  XLogSegNo targetSegNo PG_USED_FOR_ASSERTS_ONLY;
  int r;

  XLByteToSeg(targetPagePtr, targetSegNo, wal_segment_size);
  targetPageOff = XLogSegmentOffset(targetPagePtr, wal_segment_size);

     
                                                                            
                                       
     
  if (readFile >= 0 && !XLByteInSeg(targetPagePtr, readSegNo, wal_segment_size))
  {
       
                                                                        
                 
       
    if (bgwriterLaunched)
    {
      if (XLogCheckpointNeeded(readSegNo))
      {
        (void)GetRedoRecPtr();
        if (XLogCheckpointNeeded(readSegNo))
        {
          RequestCheckpoint(CHECKPOINT_CAUSE_XLOG);
        }
      }
    }

    close(readFile);
    readFile = -1;
    readSource = 0;
  }

  XLByteToSeg(targetPagePtr, readSegNo, wal_segment_size);

retry:
                                            
  if (readFile < 0 || (readSource == XLOG_FROM_STREAM && receivedUpto < targetPagePtr + reqLen))
  {
    if (!WaitForWALToBecomeAvailable(targetPagePtr + reqLen, private->randAccess, private->fetching_ckpt, targetRecPtr))
    {
      if (readFile >= 0)
      {
        close(readFile);
      }
      readFile = -1;
      readLen = 0;
      readSource = 0;

      return -1;
    }
  }

     
                                                                             
                                         
     
  Assert(readFile != -1);

     
                                                                         
                                                                    
                                                                        
                                                                    
     
  if (readSource == XLOG_FROM_STREAM)
  {
    if (((targetPagePtr) / XLOG_BLCKSZ) != (receivedUpto / XLOG_BLCKSZ))
    {
      readLen = XLOG_BLCKSZ;
    }
    else
    {
      readLen = XLogSegmentOffset(receivedUpto, wal_segment_size) - targetPageOff;
    }
  }
  else
  {
    readLen = XLOG_BLCKSZ;
  }

                               
  readOff = targetPageOff;

  pgstat_report_wait_start(WAIT_EVENT_WAL_READ);
  r = pg_pread(readFile, readBuf, XLOG_BLCKSZ, (off_t)readOff);
  if (r != XLOG_BLCKSZ)
  {
    char fname[MAXFNAMELEN];
    int save_errno = errno;

    pgstat_report_wait_end();
    XLogFileName(fname, curFileTLI, readSegNo, wal_segment_size);
    if (r < 0)
    {
      errno = save_errno;
      ereport(emode_for_corrupt_record(emode, targetPagePtr + reqLen), (errcode_for_file_access(), errmsg("could not read from log segment %s, offset %u: %m", fname, readOff)));
    }
    else
    {
      ereport(emode_for_corrupt_record(emode, targetPagePtr + reqLen), (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read from log segment %s, offset %u: read %d of %zu", fname, readOff, r, (Size)XLOG_BLCKSZ)));
    }
    goto next_record_is_invalid;
  }
  pgstat_report_wait_end();

  Assert(targetSegNo == readSegNo);
  Assert(targetPageOff == readOff);
  Assert(reqLen <= readLen);

  *readTLI = curFileTLI;

     
                                                                            
                                                                         
                                                                             
                                                                          
                                                                           
                                                                     
                                                                          
                                                                           
                                                                           
                                                                           
                                                                             
                                                                             
                                                                          
                                                                        
                                                                  
                                                                           
     
                                                                           
                                                                             
                                                                        
                                                                
     
                                                                    
                                                               
     
  if (!XLogReaderValidatePageHeader(xlogreader, targetPagePtr, readBuf))
  {
                                                                       
    xlogreader->errormsg_buf[0] = '\0';
    goto next_record_is_invalid;
  }

  return readLen;

next_record_is_invalid:
  lastSourceFailed = true;

  if (readFile >= 0)
  {
    close(readFile);
  }
  readFile = -1;
  readLen = 0;
  readSource = 0;

                                    
  if (StandbyMode)
  {
    goto retry;
  }
  else
  {
    return -1;
  }
}

   
                                                          
   
                                                                             
                                                                         
                                                                              
                                                                             
                                      
   
                                                                              
                                                              
   
                                                                            
                                                                        
                                                                            
                                                                   
   
                                                                          
                                                                         
              
   
                                                                            
                                                                              
                                                                              
          
   
static bool
WaitForWALToBecomeAvailable(XLogRecPtr RecPtr, bool randAccess, bool fetching_ckpt, XLogRecPtr tliRecPtr)
{
  static TimestampTz last_fail_time = 0;
  TimestampTz now;
  bool streaming_reply_sent = false;

            
                                                     
     
                                                                        
                                 
                           
                                                                    
                         
                                                                            
     
                                                                           
                     
     
                                                                             
                                                                         
                                                                             
                                          
     
                                                                          
                                                                        
                                                                           
                                                                              
                                                                          
                          
            
     
  if (!InArchiveRecovery)
  {
    currentSource = XLOG_FROM_PG_WAL;
  }
  else if (currentSource == 0 || (!StandbyMode && currentSource == XLOG_FROM_STREAM))
  {
    lastSourceFailed = false;
    currentSource = XLOG_FROM_ARCHIVE;
  }

  for (;;)
  {
    int oldSource = currentSource;

       
                                                                     
                                                                     
                                                                       
                                    
       
    if (lastSourceFailed)
    {
      switch (currentSource)
      {
      case XLOG_FROM_ARCHIVE:
      case XLOG_FROM_PG_WAL:

           
                                                                 
                                                              
                                                                 
                                                        
           
        if (StandbyMode && CheckForStandbyTrigger())
        {
          ShutdownWalRcv();
          return false;
        }

           
                                                                
                       
           
        if (!StandbyMode)
        {
          return false;
        }

           
                                                                 
                                      
           
                                                                  
                                                                  
                                                                 
                                                              
                                                                 
           
        if (PrimaryConnInfo && strcmp(PrimaryConnInfo, "") != 0)
        {
          XLogRecPtr ptr;
          TimeLineID tli;

          if (fetching_ckpt)
          {
            ptr = RedoStartLSN;
            tli = ControlFile->checkPointCopy.ThisTimeLineID;
          }
          else
          {
            ptr = RecPtr;

               
                                                              
                                                            
               
            tli = tliOfPointInHistory(tliRecPtr, expectedTLEs);

            if (curFileTLI > 0 && tli < curFileTLI)
            {
              elog(ERROR, "according to history file, WAL location %X/%X belongs to timeline %u, but previous recovered WAL file came from timeline %u", (uint32)(tliRecPtr >> 32), (uint32)tliRecPtr, tli, curFileTLI);
            }
          }
          curFileTLI = tli;
          RequestXLogStreaming(tli, ptr, PrimaryConnInfo, PrimarySlotName);
          receivedUpto = 0;
        }

           
                                                                
                                                                  
                                          
           
        currentSource = XLOG_FROM_STREAM;
        break;

      case XLOG_FROM_STREAM:

           
                                                             
                                                            
                                                               
                                                                   
                                                               
                                                                 
                                                                 
                                                              
                                                                   
                                                              
                                                                 
                   
           

           
                                                         
                                 
           
        Assert(StandbyMode);

           
                                                                  
                                                                 
                                             
           
        if (WalRcvStreaming())
        {
          ShutdownWalRcv();
        }

           
                                                                  
                                                                
           
        if (recoveryTargetTimeLineGoal == RECOVERY_TARGET_TIMELINE_LATEST)
        {
          if (rescanLatestTimeLine())
          {
            currentSource = XLOG_FROM_ARCHIVE;
            break;
          }
        }

           
                                                           
                                                           
                                                                 
                                                                  
                                                                 
                                               
           
        now = GetCurrentTimestamp();
        if (!TimestampDifferenceExceeds(last_fail_time, now, wal_retrieve_retry_interval))
        {
          long wait_time;

          wait_time = wal_retrieve_retry_interval - TimestampDifferenceMilliseconds(last_fail_time, now);

                                                                
          KnownAssignedTransactionIdsIdleMaintenance();

          (void)WaitLatch(&XLogCtl->recoveryWakeupLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, wait_time, WAIT_EVENT_RECOVERY_WAL_STREAM);
          ResetLatch(&XLogCtl->recoveryWakeupLatch);
          now = GetCurrentTimestamp();

                                                           
          HandleStartupProcInterrupts();
        }
        last_fail_time = now;
        currentSource = XLOG_FROM_ARCHIVE;
        break;

      default:
        elog(ERROR, "unexpected WAL source %d", currentSource);
      }
    }
    else if (currentSource == XLOG_FROM_PG_WAL)
    {
         
                                                                        
                                                                     
                                 
         
      if (InArchiveRecovery)
      {
        currentSource = XLOG_FROM_ARCHIVE;
      }
    }

    if (currentSource != oldSource)
    {
      elog(DEBUG2, "switched WAL source from %s to %s after %s", xlogSourceNames[oldSource], xlogSourceNames[currentSource], lastSourceFailed ? "failure" : "success");
    }

       
                                                                       
               
       
    lastSourceFailed = false;

    switch (currentSource)
    {
    case XLOG_FROM_ARCHIVE:
    case XLOG_FROM_PG_WAL:
         
                                                                
                            
         
      Assert(!WalRcvStreaming());

                                                  
      if (readFile >= 0)
      {
        close(readFile);
        readFile = -1;
      }
                                             
      if (randAccess)
      {
        curFileTLI = 0;
      }

         
                                                                   
                           
         
      readFile = XLogFileReadAnyTLI(readSegNo, DEBUG2, currentSource == XLOG_FROM_ARCHIVE ? XLOG_FROM_ANY : currentSource);
      if (readFile >= 0)
      {
        return true;               
      }

         
                                               
         
      lastSourceFailed = true;
      break;

    case XLOG_FROM_STREAM:
    {
      bool havedata;

         
                                                       
                               
         
      Assert(StandbyMode);

         
                                                
         
      if (!WalRcvStreaming())
      {
        lastSourceFailed = true;
        break;
      }

         
                                                                
         
                                                              
                                                              
                                                             
                                                                
                                                                 
                                                       
                                                             
                                                        
         
      if (RecPtr < receivedUpto)
      {
        havedata = true;
      }
      else
      {
        XLogRecPtr latestChunkStart;

        receivedUpto = GetWalRcvWriteRecPtr(&latestChunkStart, &receiveTLI);
        if (RecPtr < receivedUpto && receiveTLI == curFileTLI)
        {
          havedata = true;
          if (latestChunkStart <= RecPtr)
          {
            XLogReceiptTime = GetCurrentTimestamp();
            SetCurrentChunkStartTime(XLogReceiptTime);
          }
        }
        else
        {
          havedata = false;
        }
      }
      if (havedata)
      {
           
                                                              
                                                             
                                                           
                                                          
                                                               
                                                           
                    
           
                                                        
                                                               
                                                           
                                                           
                                                            
                                                        
                                       
           
        if (readFile < 0)
        {
          if (!expectedTLEs)
          {
            expectedTLEs = readTimeLineHistory(recoveryTargetTLI);
          }
          readFile = XLogFileRead(readSegNo, PANIC, receiveTLI, XLOG_FROM_STREAM, false);
          Assert(readFile >= 0);
        }
        else
        {
                                                        
          readSource = XLOG_FROM_STREAM;
          XLogReceiptSource = XLOG_FROM_STREAM;
          return true;
        }
        break;
      }

         
                                                             
                                                         
         
      if (CheckForStandbyTrigger())
      {
           
                                                               
                                                              
                                                             
                                                            
                                                             
                                                             
                        
           
        lastSourceFailed = true;
        break;
      }

         
                                                               
                                                                
                                                             
                                                     
                      
         
      if (!streaming_reply_sent)
      {
        WalRcvForceReply();
        streaming_reply_sent = true;
      }

                                                                
      KnownAssignedTransactionIdsIdleMaintenance();

         
                                                               
                                                                 
                                       
         
      (void)WaitLatch(&XLogCtl->recoveryWakeupLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 5000L, WAIT_EVENT_RECOVERY_WAL_ALL);
      ResetLatch(&XLogCtl->recoveryWakeupLatch);
      break;
    }

    default:
      elog(ERROR, "unexpected WAL source %d", currentSource);
    }

       
                                                                     
                
       
    HandleStartupProcInterrupts();
  }

  return false;                  
}

   
                                                                          
                                                               
   
                                                                           
                                                                            
                                                                          
                                                                             
                                                                               
                                                                                
                                                                                
                                  
   
                                                                           
                                                                            
                                                                        
                           
   
static int
emode_for_corrupt_record(int emode, XLogRecPtr RecPtr)
{
  static XLogRecPtr lastComplaint = 0;

  if (readSource == XLOG_FROM_PG_WAL && emode == LOG)
  {
    if (RecPtr == lastComplaint)
    {
      emode = DEBUG1;
    }
    else
    {
      lastComplaint = RecPtr;
    }
  }
  return emode;
}

   
                                                                             
                                                                         
   
static bool
CheckForStandbyTrigger(void)
{
  struct stat stat_buf;
  static bool triggered = false;

  if (triggered)
  {
    return true;
  }

  if (IsPromoteTriggered())
  {
       
                                                                          
                                                                    
                                                                          
                                                                       
                                               
       
    if (stat(PROMOTE_SIGNAL_FILE, &stat_buf) == 0)
    {
      unlink(PROMOTE_SIGNAL_FILE);
      unlink(FALLBACK_PROMOTE_SIGNAL_FILE);
      fast_promote = true;
    }
    else if (stat(FALLBACK_PROMOTE_SIGNAL_FILE, &stat_buf) == 0)
    {
      unlink(FALLBACK_PROMOTE_SIGNAL_FILE);
      fast_promote = false;
    }

    ereport(LOG, (errmsg("received promote request")));

    ResetPromoteTriggered();
    triggered = true;
    return true;
  }

  if (PromoteTriggerFile == NULL || strcmp(PromoteTriggerFile, "") == 0)
  {
    return false;
  }

  if (stat(PromoteTriggerFile, &stat_buf) == 0)
  {
    ereport(LOG, (errmsg("promote trigger file found: %s", PromoteTriggerFile)));
    unlink(PromoteTriggerFile);
    triggered = true;
    fast_promote = true;
    return true;
  }
  else if (errno != ENOENT)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat promote trigger file \"%s\": %m", PromoteTriggerFile)));
  }

  return false;
}

   
                                                           
   
void
RemovePromoteSignalFiles(void)
{
  unlink(PROMOTE_SIGNAL_FILE);
  unlink(FALLBACK_PROMOTE_SIGNAL_FILE);
}

   
                                                            
                                                 
   
bool
CheckPromoteSignal(void)
{
  struct stat stat_buf;

  if (stat(PROMOTE_SIGNAL_FILE, &stat_buf) == 0 || stat(FALLBACK_PROMOTE_SIGNAL_FILE, &stat_buf) == 0)
  {
    return true;
  }

  return false;
}

   
                                                                          
                                
   
void
WakeupRecovery(void)
{
  SetLatch(&XLogCtl->recoveryWakeupLatch);
}

   
                                      
   
void
SetWalWriterSleeping(bool sleeping)
{
  SpinLockAcquire(&XLogCtl->info_lck);
  XLogCtl->WalWriterSleeping = sleeping;
  SpinLockRelease(&XLogCtl->info_lck);
}

   
                                                            
   
void
XLogRequestWalReceiverReply(void)
{
  doRequestWalReceiverReply = true;
}
