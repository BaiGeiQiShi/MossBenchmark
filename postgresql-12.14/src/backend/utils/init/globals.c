                                                                            
   
             
                                  
   
                                                                         
                                                                        
   
   
                  
                                      
   
         
                                                                     
                       
   
                                                                            
   
#include "postgres.h"

#include "common/file_perm.h"
#include "libpq/libpq-be.h"
#include "libpq/pqcomm.h"
#include "miscadmin.h"
#include "storage/backendid.h"

ProtocolVersion FrontendProtocol;

volatile sig_atomic_t InterruptPending = false;
volatile sig_atomic_t QueryCancelPending = false;
volatile sig_atomic_t ProcDiePending = false;
volatile sig_atomic_t ClientConnectionLost = false;
volatile sig_atomic_t IdleInTransactionSessionTimeoutPending = false;
volatile sig_atomic_t ConfigReloadPending = false;
volatile uint32 InterruptHoldoffCount = 0;
volatile uint32 QueryCancelHoldoffCount = 0;
volatile uint32 CritSectionCount = 0;

int MyProcPid;
pg_time_t MyStartTime;
TimestampTz MyStartTimestamp;
struct Port *MyProcPort;
int32 MyCancelKey;
int MyPMChildSlot;

   
                                                                              
                                                                         
                                                                      
                                                                               
                                       
   
struct Latch *MyLatch;

   
                                                                               
                                                                             
                                                                               
               
   
char *DataDir = NULL;

   
                                                                             
                                                                        
   
int data_directory_mode = PG_DIR_MODE_OWNER;

char OutputFileName[MAXPGPATH];                            

char my_exec_path[MAXPGPATH];                                 
char pkglib_path[MAXPGPATH];                                  

#ifdef EXEC_BACKEND
char postgres_exec_path[MAXPGPATH];                           

                                                            
#endif

BackendId MyBackendId = InvalidBackendId;

BackendId ParallelMasterBackendId = InvalidBackendId;

Oid MyDatabaseId = InvalidOid;

Oid MyDatabaseTableSpace = InvalidOid;

   
                                                                   
                                                                   
   
char *DatabasePath = NULL;

pid_t PostmasterPid = 0;

   
                                                                              
                                                                    
                                                                       
                                                                         
                                                                         
                                                                           
                                                                  
   
                                                            
   
bool IsPostmasterEnvironment = false;
bool IsUnderPostmaster = false;
bool IsBinaryUpgrade = false;
bool IsBackgroundWorker = false;

bool ExitOnAnyError = false;

int DateStyle = USE_ISO_DATES;
int DateOrder = DATEORDER_MDY;
int IntervalStyle = INTSTYLE_POSTGRES;

bool enableFsync = true;
bool allowSystemTableMods = false;
int work_mem = 1024;
int maintenance_work_mem = 16384;
int max_parallel_maintenance_workers = 2;

   
                                                              
   
                                                                                
                                
   
int NBuffers = 1000;
int MaxConnections = 90;
int max_worker_processes = 8;
int max_parallel_workers = 8;
int MaxBackends = 0;

int VacuumCostPageHit = 1;                                
int VacuumCostPageMiss = 10;
int VacuumCostPageDirty = 20;
int VacuumCostLimit = 200;
double VacuumCostDelay = 0;

int VacuumPageHit = 0;
int VacuumPageMiss = 0;
int VacuumPageDirty = 0;

int VacuumCostBalance = 0;                               
bool VacuumCostActive = false;

double vacuum_cleanup_index_scale_factor;
