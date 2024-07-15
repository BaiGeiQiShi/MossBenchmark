                                                                       
         
   
                                                                 
                                                          
                                                           
   
   
                                                                
                                                  
   
                  
                                  
   
                                                                       
   
#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#include "access/commit_ts.h"
#include "access/gin.h"
#include "access/rmgr.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "commands/async.h"
#include "commands/prepare.h"
#include "commands/tablespace.h"
#include "commands/user.h"
#include "commands/vacuum.h"
#include "commands/variable.h"
#include "commands/trigger.h"
#include "common/string.h"
#include "funcapi.h"
#include "jit/jit.h"
#include "libpq/auth.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "optimizer/cost.h"
#include "optimizer/geqo.h"
#include "optimizer/optimizer.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "parser/parse_expr.h"
#include "parser/parse_type.h"
#include "parser/parser.h"
#include "parser/scansup.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/bgworker_internals.h"
#include "postmaster/bgwriter.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "postmaster/walwriter.h"
#include "replication/logicallauncher.h"
#include "replication/slot.h"
#include "replication/syncrep.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "storage/bufmgr.h"
#include "storage/dsm_impl.h"
#include "storage/standby.h"
#include "storage/fd.h"
#include "storage/large_object.h"
#include "storage/pg_shmem.h"
#include "storage/proc.h"
#include "storage/predicate.h"
#include "tcop/tcopprot.h"
#include "tsearch/ts_cache.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/guc_tables.h"
#include "utils/float.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/pg_lsn.h"
#include "utils/plancache.h"
#include "utils/portal.h"
#include "utils/ps_status.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"
#include "utils/tzparser.h"
#include "utils/varlena.h"
#include "utils/xml.h"

#ifndef PG_KRB_SRVTAB
#define PG_KRB_SRVTAB ""
#endif

#define CONFIG_FILENAME "postgresql.conf"
#define HBA_FILENAME "pg_hba.conf"
#define IDENT_FILENAME "pg_ident.conf"

#ifdef EXEC_BACKEND
#define CONFIG_EXEC_PARAMS "global/config_exec_params"
#define CONFIG_EXEC_PARAMS_NEW "global/config_exec_params.new"
#endif

   
                                                                       
                  
   
#define REALTYPE_PRECISION 17

                                                            
extern bool Log_disconnections;
extern int CommitDelay;
extern int CommitSiblings;
extern char *default_tablespace;
extern char *temp_tablespaces;
extern bool ignore_checksum_failure;
extern bool synchronize_seqscans;

#ifdef TRACE_SYNCSCAN
extern bool trace_syncscan;
#endif
#ifdef DEBUG_BOUNDED_SORT
extern bool optimize_bounded_sort;
#endif

static int GUC_check_errcode_value;

                                             
char *GUC_check_errmsg_string;
char *GUC_check_errdetail_string;
char *GUC_check_errhint_string;

static void
do_serialize(char **destptr, Size *maxbytes, const char *fmt, ...) pg_attribute_printf(3, 4);

static void
set_config_sourcefile(const char *name, char *sourcefile, int sourceline);
static bool
call_bool_check_hook(struct config_bool *conf, bool *newval, void **extra, GucSource source, int elevel);
static bool
call_int_check_hook(struct config_int *conf, int *newval, void **extra, GucSource source, int elevel);
static bool
call_real_check_hook(struct config_real *conf, double *newval, void **extra, GucSource source, int elevel);
static bool
call_string_check_hook(struct config_string *conf, char **newval, void **extra, GucSource source, int elevel);
static bool
call_enum_check_hook(struct config_enum *conf, int *newval, void **extra, GucSource source, int elevel);

static bool
check_log_destination(char **newval, void **extra, GucSource source);
static void
assign_log_destination(const char *newval, void *extra);

static bool
check_wal_consistency_checking(char **newval, void **extra, GucSource source);
static void
assign_wal_consistency_checking(const char *newval, void *extra);

#ifdef HAVE_SYSLOG
static int syslog_facility = LOG_LOCAL0;
#else
static int syslog_facility = 0;
#endif

static void
assign_syslog_facility(int newval, void *extra);
static void
assign_syslog_ident(const char *newval, void *extra);
static void
assign_session_replication_role(int newval, void *extra);
static bool
check_temp_buffers(int *newval, void **extra, GucSource source);
static bool
check_bonjour(bool *newval, void **extra, GucSource source);
static bool
check_ssl(bool *newval, void **extra, GucSource source);
static bool
check_stage_log_stats(bool *newval, void **extra, GucSource source);
static bool
check_log_stats(bool *newval, void **extra, GucSource source);
static bool
check_canonical_path(char **newval, void **extra, GucSource source);
static bool
check_timezone_abbreviations(char **newval, void **extra, GucSource source);
static void
assign_timezone_abbreviations(const char *newval, void *extra);
static void
pg_timezone_abbrev_initialize(void);
static const char *
show_archive_command(void);
static void
assign_tcp_keepalives_idle(int newval, void *extra);
static void
assign_tcp_keepalives_interval(int newval, void *extra);
static void
assign_tcp_keepalives_count(int newval, void *extra);
static void
assign_tcp_user_timeout(int newval, void *extra);
static const char *
show_tcp_keepalives_idle(void);
static const char *
show_tcp_keepalives_interval(void);
static const char *
show_tcp_keepalives_count(void);
static const char *
show_tcp_user_timeout(void);
static bool
check_maxconnections(int *newval, void **extra, GucSource source);
static bool
check_max_worker_processes(int *newval, void **extra, GucSource source);
static bool
check_autovacuum_max_workers(int *newval, void **extra, GucSource source);
static bool
check_max_wal_senders(int *newval, void **extra, GucSource source);
static bool
check_autovacuum_work_mem(int *newval, void **extra, GucSource source);
static bool
check_effective_io_concurrency(int *newval, void **extra, GucSource source);
static void
assign_effective_io_concurrency(int newval, void *extra);
static void
assign_pgstat_temp_directory(const char *newval, void *extra);
static bool
check_application_name(char **newval, void **extra, GucSource source);
static void
assign_application_name(const char *newval, void *extra);
static bool
check_cluster_name(char **newval, void **extra, GucSource source);
static const char *
show_unix_socket_permissions(void);
static const char *
show_log_file_mode(void);
static const char *
show_data_directory_mode(void);
static bool
check_recovery_target_timeline(char **newval, void **extra, GucSource source);
static void
assign_recovery_target_timeline(const char *newval, void *extra);
static bool
check_recovery_target(char **newval, void **extra, GucSource source);
static void
assign_recovery_target(const char *newval, void *extra);
static bool
check_recovery_target_xid(char **newval, void **extra, GucSource source);
static void
assign_recovery_target_xid(const char *newval, void *extra);
static bool
check_recovery_target_time(char **newval, void **extra, GucSource source);
static void
assign_recovery_target_time(const char *newval, void *extra);
static bool
check_recovery_target_name(char **newval, void **extra, GucSource source);
static void
assign_recovery_target_name(const char *newval, void *extra);
static bool
check_recovery_target_lsn(char **newval, void **extra, GucSource source);
static void
assign_recovery_target_lsn(const char *newval, void *extra);
static bool
check_primary_slot_name(char **newval, void **extra, GucSource source);
static bool
check_default_with_oids(bool *newval, void **extra, GucSource source);

                                                                       
static ConfigVariable *
ProcessConfigFileInternal(GucContext context, bool applySettings, int elevel);

   
                                                   
   
                                                      
   

static const struct config_enum_entry bytea_output_options[] = {{"escape", BYTEA_OUTPUT_ESCAPE, false}, {"hex", BYTEA_OUTPUT_HEX, false}, {NULL, 0, false}};

   
                                                                              
                                                                               
                                            
   
static const struct config_enum_entry client_message_level_options[] = {{"debug5", DEBUG5, false}, {"debug4", DEBUG4, false}, {"debug3", DEBUG3, false}, {"debug2", DEBUG2, false}, {"debug1", DEBUG1, false}, {"debug", DEBUG2, true}, {"log", LOG, false}, {"info", INFO, true}, {"notice", NOTICE, false}, {"warning", WARNING, false}, {"error", ERROR, false}, {NULL, 0, false}};

static const struct config_enum_entry server_message_level_options[] = {{"debug5", DEBUG5, false}, {"debug4", DEBUG4, false}, {"debug3", DEBUG3, false}, {"debug2", DEBUG2, false}, {"debug1", DEBUG1, false}, {"debug", DEBUG2, true}, {"info", INFO, false}, {"notice", NOTICE, false}, {"warning", WARNING, false}, {"error", ERROR, false}, {"log", LOG, false}, {"fatal", FATAL, false}, {"panic", PANIC, false}, {NULL, 0, false}};

static const struct config_enum_entry intervalstyle_options[] = {{"postgres", INTSTYLE_POSTGRES, false}, {"postgres_verbose", INTSTYLE_POSTGRES_VERBOSE, false}, {"sql_standard", INTSTYLE_SQL_STANDARD, false}, {"iso_8601", INTSTYLE_ISO_8601, false}, {NULL, 0, false}};

static const struct config_enum_entry log_error_verbosity_options[] = {{"terse", PGERROR_TERSE, false}, {"default", PGERROR_DEFAULT, false}, {"verbose", PGERROR_VERBOSE, false}, {NULL, 0, false}};

static const struct config_enum_entry log_statement_options[] = {{"none", LOGSTMT_NONE, false}, {"ddl", LOGSTMT_DDL, false}, {"mod", LOGSTMT_MOD, false}, {"all", LOGSTMT_ALL, false}, {NULL, 0, false}};

static const struct config_enum_entry isolation_level_options[] = {{"serializable", XACT_SERIALIZABLE, false}, {"repeatable read", XACT_REPEATABLE_READ, false}, {"read committed", XACT_READ_COMMITTED, false}, {"read uncommitted", XACT_READ_UNCOMMITTED, false}, {NULL, 0}};

static const struct config_enum_entry session_replication_role_options[] = {{"origin", SESSION_REPLICATION_ROLE_ORIGIN, false}, {"replica", SESSION_REPLICATION_ROLE_REPLICA, false}, {"local", SESSION_REPLICATION_ROLE_LOCAL, false}, {NULL, 0, false}};

static const struct config_enum_entry syslog_facility_options[] = {
#ifdef HAVE_SYSLOG
    {"local0", LOG_LOCAL0, false}, {"local1", LOG_LOCAL1, false}, {"local2", LOG_LOCAL2, false}, {"local3", LOG_LOCAL3, false}, {"local4", LOG_LOCAL4, false}, {"local5", LOG_LOCAL5, false}, {"local6", LOG_LOCAL6, false}, {"local7", LOG_LOCAL7, false},
#else
    {"none", 0, false},
#endif
    {NULL, 0}};

static const struct config_enum_entry track_function_options[] = {{"none", TRACK_FUNC_OFF, false}, {"pl", TRACK_FUNC_PL, false}, {"all", TRACK_FUNC_ALL, false}, {NULL, 0, false}};

static const struct config_enum_entry xmlbinary_options[] = {{"base64", XMLBINARY_BASE64, false}, {"hex", XMLBINARY_HEX, false}, {NULL, 0, false}};

static const struct config_enum_entry xmloption_options[] = {{"content", XMLOPTION_CONTENT, false}, {"document", XMLOPTION_DOCUMENT, false}, {NULL, 0, false}};

   
                                                                     
                                                     
   
static const struct config_enum_entry backslash_quote_options[] = {{"safe_encoding", BACKSLASH_QUOTE_SAFE_ENCODING, false}, {"on", BACKSLASH_QUOTE_ON, false}, {"off", BACKSLASH_QUOTE_OFF, false}, {"true", BACKSLASH_QUOTE_ON, true}, {"false", BACKSLASH_QUOTE_OFF, true}, {"yes", BACKSLASH_QUOTE_ON, true}, {"no", BACKSLASH_QUOTE_OFF, true}, {"1", BACKSLASH_QUOTE_ON, true}, {"0", BACKSLASH_QUOTE_OFF, true}, {NULL, 0, false}};

   
                                                                 
                                                     
   
static const struct config_enum_entry constraint_exclusion_options[] = {{"partition", CONSTRAINT_EXCLUSION_PARTITION, false}, {"on", CONSTRAINT_EXCLUSION_ON, false}, {"off", CONSTRAINT_EXCLUSION_OFF, false}, {"true", CONSTRAINT_EXCLUSION_ON, true}, {"false", CONSTRAINT_EXCLUSION_OFF, true}, {"yes", CONSTRAINT_EXCLUSION_ON, true}, {"no", CONSTRAINT_EXCLUSION_OFF, true}, {"1", CONSTRAINT_EXCLUSION_ON, true}, {"0", CONSTRAINT_EXCLUSION_OFF, true}, {NULL, 0, false}};

   
                                                                              
                                                                    
   
static const struct config_enum_entry synchronous_commit_options[] = {{"local", SYNCHRONOUS_COMMIT_LOCAL_FLUSH, false}, {"remote_write", SYNCHRONOUS_COMMIT_REMOTE_WRITE, false}, {"remote_apply", SYNCHRONOUS_COMMIT_REMOTE_APPLY, false}, {"on", SYNCHRONOUS_COMMIT_ON, false}, {"off", SYNCHRONOUS_COMMIT_OFF, false}, {"true", SYNCHRONOUS_COMMIT_ON, true}, {"false", SYNCHRONOUS_COMMIT_OFF, true}, {"yes", SYNCHRONOUS_COMMIT_ON, true}, {"no", SYNCHRONOUS_COMMIT_OFF, true}, {"1", SYNCHRONOUS_COMMIT_ON, true}, {"0", SYNCHRONOUS_COMMIT_OFF, true}, {NULL, 0, false}};

   
                                                                             
                               
   
static const struct config_enum_entry huge_pages_options[] = {{"off", HUGE_PAGES_OFF, false}, {"on", HUGE_PAGES_ON, false}, {"try", HUGE_PAGES_TRY, false}, {"true", HUGE_PAGES_ON, true}, {"false", HUGE_PAGES_OFF, true}, {"yes", HUGE_PAGES_ON, true}, {"no", HUGE_PAGES_OFF, true}, {"1", HUGE_PAGES_ON, true}, {"0", HUGE_PAGES_OFF, true}, {NULL, 0, false}};

static const struct config_enum_entry force_parallel_mode_options[] = {{"off", FORCE_PARALLEL_OFF, false}, {"on", FORCE_PARALLEL_ON, false}, {"regress", FORCE_PARALLEL_REGRESS, false}, {"true", FORCE_PARALLEL_ON, true}, {"false", FORCE_PARALLEL_OFF, true}, {"yes", FORCE_PARALLEL_ON, true}, {"no", FORCE_PARALLEL_OFF, true}, {"1", FORCE_PARALLEL_ON, true}, {"0", FORCE_PARALLEL_OFF, true}, {NULL, 0, false}};

static const struct config_enum_entry plan_cache_mode_options[] = {{"auto", PLAN_CACHE_MODE_AUTO, false}, {"force_generic_plan", PLAN_CACHE_MODE_FORCE_GENERIC_PLAN, false}, {"force_custom_plan", PLAN_CACHE_MODE_FORCE_CUSTOM_PLAN, false}, {NULL, 0, false}};

   
                                                                      
                                                                      
                                      
   
static const struct config_enum_entry password_encryption_options[] = {{"md5", PASSWORD_TYPE_MD5, false}, {"scram-sha-256", PASSWORD_TYPE_SCRAM_SHA_256, false}, {"on", PASSWORD_TYPE_MD5, true}, {"true", PASSWORD_TYPE_MD5, true}, {"yes", PASSWORD_TYPE_MD5, true}, {"1", PASSWORD_TYPE_MD5, true}, {NULL, 0, false}};

const struct config_enum_entry ssl_protocol_versions_info[] = {{"", PG_TLS_ANY, false}, {"TLSv1", PG_TLS1_VERSION, false}, {"TLSv1.1", PG_TLS1_1_VERSION, false}, {"TLSv1.2", PG_TLS1_2_VERSION, false}, {"TLSv1.3", PG_TLS1_3_VERSION, false}, {NULL, 0, false}};

static struct config_enum_entry shared_memory_options[] = {
#ifndef WIN32
    {"sysv", SHMEM_TYPE_SYSV, false},
#endif
#ifndef EXEC_BACKEND
    {"mmap", SHMEM_TYPE_MMAP, false},
#endif
#ifdef WIN32
    {"windows", SHMEM_TYPE_WINDOWS, false},
#endif
    {NULL, 0, false}};

   
                                                   
   
extern const struct config_enum_entry wal_level_options[];
extern const struct config_enum_entry archive_mode_options[];
extern const struct config_enum_entry recovery_target_action_options[];
extern const struct config_enum_entry sync_method_options[];
extern const struct config_enum_entry dynamic_shared_memory_options[];

   
                                                           
   
bool log_duration = false;
bool Debug_print_plan = false;
bool Debug_print_parse = false;
bool Debug_print_rewritten = false;
bool Debug_pretty_print = true;

bool log_parser_stats = false;
bool log_planner_stats = false;
bool log_executor_stats = false;
bool log_statement_stats = false;                                    
                                                
bool log_btree_build_stats = false;
char *event_source;

bool row_security;
bool check_function_bodies = true;

   
                                                                               
            
   
bool default_with_oids = false;
bool session_auth_is_superuser;

int log_min_error_statement = ERROR;
int log_min_messages = WARNING;
int client_min_messages = NOTICE;
int log_min_duration_statement = -1;
int log_temp_files = -1;
double log_xact_sample_rate = 0;
int trace_recovery_messages = LOG;

int temp_file_limit = -1;

int num_temp_buffers = 1024;

char *cluster_name = "";
char *ConfigFileName;
char *HbaFileName;
char *IdentFileName;
char *external_pid_file;

char *pgstat_temp_directory;

char *application_name;

int tcp_keepalives_idle;
int tcp_keepalives_interval;
int tcp_keepalives_count;
int tcp_user_timeout;

   
                                                                            
                                                                             
                                                                             
                                                      
   
int ssl_renegotiation_limit;

   
                                                                             
                                                                             
   
int huge_pages;

   
                                                                          
                                                                             
                                        
   
static char *syslog_ident_str;
static double phony_random_seed;
static char *client_encoding_string;
static char *datestyle_string;
static char *locale_collate;
static char *locale_ctype;
static char *server_encoding_string;
static char *server_version_string;
static int server_version_num;
static char *timezone_string;
static char *log_timezone_string;
static char *timezone_abbreviations_string;
static char *data_directory;
static char *session_authorization_string;
static int max_function_args;
static int max_index_keys;
static int max_identifier_length;
static int block_size;
static int segment_size;
static int wal_block_size;
static bool data_checksums;
static bool integer_datetimes;
static bool assert_enabled;
static char *recovery_target_timeline_string;
static char *recovery_target_string;
static char *recovery_target_xid_string;
static char *recovery_target_name_string;
static char *recovery_target_lsn_string;

                                                                    
char *role_string;

   
                                                         
   
                                                       
   
const char *const GucContext_Names[] = {
                       "internal",
                         "postmaster",
                     "sighup",
                         "superuser-backend",
                      "backend",
                    "superuser",
                      "user"};

   
                                                       
   
                                                       
   
const char *const GucSource_Names[] = {
                        "default",
                                "default",
                        "environment variable",
                     "configuration file",
                     "command line",
                       "global",
                         "database",
                     "user",
                              "database user",
                       "client",
                         "override",
                            "interactive",
                     "test",
                        "session"};

   
                                                                    
   
const char *const config_group_names[] = {
                   
    gettext_noop("Ungrouped"),
                        
    gettext_noop("File Locations"),
                   
    gettext_noop("Connections and Authentication"),
                            
    gettext_noop("Connections and Authentication / Connection Settings"),
                        
    gettext_noop("Connections and Authentication / Authentication"),
                       
    gettext_noop("Connections and Authentication / SSL"),
                   
    gettext_noop("Resource Usage"),
                       
    gettext_noop("Resource Usage / Memory"),
                        
    gettext_noop("Resource Usage / Disk"),
                          
    gettext_noop("Resource Usage / Kernel Resources"),
                                
    gettext_noop("Resource Usage / Cost-Based Vacuum Delay"),
                            
    gettext_noop("Resource Usage / Background Writer"),
                                
    gettext_noop("Resource Usage / Asynchronous Behavior"),
             
    gettext_noop("Write-Ahead Log"),
                      
    gettext_noop("Write-Ahead Log / Settings"),
                         
    gettext_noop("Write-Ahead Log / Checkpoints"),
                       
    gettext_noop("Write-Ahead Log / Archiving"),
                              
    gettext_noop("Write-Ahead Log / Archive Recovery"),
                             
    gettext_noop("Write-Ahead Log / Recovery Target"),
                     
    gettext_noop("Replication"),
                             
    gettext_noop("Replication / Sending Servers"),
                            
    gettext_noop("Replication / Master Server"),
                             
    gettext_noop("Replication / Standby Servers"),
                                 
    gettext_noop("Replication / Subscribers"),
                      
    gettext_noop("Query Tuning"),
                             
    gettext_noop("Query Tuning / Planner Method Configuration"),
                           
    gettext_noop("Query Tuning / Planner Cost Constants"),
                           
    gettext_noop("Query Tuning / Genetic Query Optimizer"),
                            
    gettext_noop("Query Tuning / Other Planner Options"),
                 
    gettext_noop("Reporting and Logging"),
                       
    gettext_noop("Reporting and Logging / Where to Log"),
                      
    gettext_noop("Reporting and Logging / When to Log"),
                      
    gettext_noop("Reporting and Logging / What to Log"),
                       
    gettext_noop("Process Title"),
               
    gettext_noop("Statistics"),
                          
    gettext_noop("Statistics / Monitoring"),
                         
    gettext_noop("Statistics / Query and Index Statistics Collector"),
                    
    gettext_noop("Autovacuum"),
                     
    gettext_noop("Client Connection Defaults"),
                               
    gettext_noop("Client Connection Defaults / Statement Behavior"),
                            
    gettext_noop("Client Connection Defaults / Locale and Formatting"),
                             
    gettext_noop("Client Connection Defaults / Shared Library Preloading"),
                           
    gettext_noop("Client Connection Defaults / Other Defaults"),
                         
    gettext_noop("Lock Management"),
                        
    gettext_noop("Version and Platform Compatibility"),
                                 
    gettext_noop("Version and Platform Compatibility / Previous PostgreSQL Versions"),
                               
    gettext_noop("Version and Platform Compatibility / Other Platforms and Clients"),
                        
    gettext_noop("Error Handling"),
                        
    gettext_noop("Preset Options"),
                        
    gettext_noop("Customized Options"),
                           
    gettext_noop("Developer Options"),
                                                            
    NULL};

   
                                                               
   
                                                       
   
const char *const config_type_names[] = {
                   "bool",
                  "integer",
                   "real",
                     "string",
                   "enum"};

   
                           
   
                                                                           
                                                                            
                 
   
                                                                
                                                                         
                                         
   
                                                                             
                                                                            
                                                        
   
#define MAX_UNIT_LEN 3                                               

typedef struct
{
  char unit[MAX_UNIT_LEN + 1];                                    
                                          
  int base_unit;                                 
  double multiplier;                                                        
} unit_conversion;

                                                                         
#if BLCKSZ < 1024 || BLCKSZ > (1024 * 1024)
#error BLCKSZ must be between 1KB and 1MB
#endif
#if XLOG_BLCKSZ < 1024 || XLOG_BLCKSZ > (1024 * 1024)
#error XLOG_BLCKSZ must be between 1KB and 1MB
#endif

static const char *memory_units_hint = gettext_noop("Valid units for this parameter are \"B\", \"kB\", \"MB\", \"GB\", and \"TB\".");

static const unit_conversion memory_unit_conversion_table[] = {
    {"TB", GUC_UNIT_BYTE, 1024.0 * 1024.0 * 1024.0 * 1024.0}, {"GB", GUC_UNIT_BYTE, 1024.0 * 1024.0 * 1024.0}, {"MB", GUC_UNIT_BYTE, 1024.0 * 1024.0}, {"kB", GUC_UNIT_BYTE, 1024.0}, {"B", GUC_UNIT_BYTE, 1.0},

    {"TB", GUC_UNIT_KB, 1024.0 * 1024.0 * 1024.0}, {"GB", GUC_UNIT_KB, 1024.0 * 1024.0}, {"MB", GUC_UNIT_KB, 1024.0}, {"kB", GUC_UNIT_KB, 1.0}, {"B", GUC_UNIT_KB, 1.0 / 1024.0},

    {"TB", GUC_UNIT_MB, 1024.0 * 1024.0}, {"GB", GUC_UNIT_MB, 1024.0}, {"MB", GUC_UNIT_MB, 1.0}, {"kB", GUC_UNIT_MB, 1.0 / 1024.0}, {"B", GUC_UNIT_MB, 1.0 / (1024.0 * 1024.0)},

    {"TB", GUC_UNIT_BLOCKS, (1024.0 * 1024.0 * 1024.0) / (BLCKSZ / 1024)}, {"GB", GUC_UNIT_BLOCKS, (1024.0 * 1024.0) / (BLCKSZ / 1024)}, {"MB", GUC_UNIT_BLOCKS, 1024.0 / (BLCKSZ / 1024)}, {"kB", GUC_UNIT_BLOCKS, 1.0 / (BLCKSZ / 1024)}, {"B", GUC_UNIT_BLOCKS, 1.0 / BLCKSZ},

    {"TB", GUC_UNIT_XBLOCKS, (1024.0 * 1024.0 * 1024.0) / (XLOG_BLCKSZ / 1024)}, {"GB", GUC_UNIT_XBLOCKS, (1024.0 * 1024.0) / (XLOG_BLCKSZ / 1024)}, {"MB", GUC_UNIT_XBLOCKS, 1024.0 / (XLOG_BLCKSZ / 1024)}, {"kB", GUC_UNIT_XBLOCKS, 1.0 / (XLOG_BLCKSZ / 1024)}, {"B", GUC_UNIT_XBLOCKS, 1.0 / XLOG_BLCKSZ},

    {""}                          
};

static const char *time_units_hint = gettext_noop("Valid units for this parameter are \"us\", \"ms\", \"s\", \"min\", \"h\", and \"d\".");

static const unit_conversion time_unit_conversion_table[] = {
    {"d", GUC_UNIT_MS, 1000 * 60 * 60 * 24}, {"h", GUC_UNIT_MS, 1000 * 60 * 60}, {"min", GUC_UNIT_MS, 1000 * 60}, {"s", GUC_UNIT_MS, 1000}, {"ms", GUC_UNIT_MS, 1}, {"us", GUC_UNIT_MS, 1.0 / 1000},

    {"d", GUC_UNIT_S, 60 * 60 * 24}, {"h", GUC_UNIT_S, 60 * 60}, {"min", GUC_UNIT_S, 60}, {"s", GUC_UNIT_S, 1}, {"ms", GUC_UNIT_S, 1.0 / 1000}, {"us", GUC_UNIT_S, 1.0 / (1000 * 1000)},

    {"d", GUC_UNIT_MIN, 60 * 24}, {"h", GUC_UNIT_MIN, 60}, {"min", GUC_UNIT_MIN, 1}, {"s", GUC_UNIT_MIN, 1.0 / 60}, {"ms", GUC_UNIT_MIN, 1.0 / (1000 * 60)}, {"us", GUC_UNIT_MIN, 1.0 / (1000 * 1000 * 60)},

    {""}                          
};

   
                          
   
                                                       
   
                     
   
                                                                    
                         
   
                                                                      
              
   
                                                                    
                       
   
                          
   
                                                                  
                  
   
                                                                     
   
                                                              
                                                                  
   

                                         

static struct config_bool ConfigureNamesBool[] = {{{"enable_seqscan", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of sequential-scan plans."), NULL, GUC_EXPLAIN}, &enable_seqscan, true, NULL, NULL, NULL}, {{"enable_indexscan", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of index-scan plans."), NULL, GUC_EXPLAIN}, &enable_indexscan, true, NULL, NULL, NULL}, {{"enable_indexonlyscan", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of index-only-scan plans."), NULL, GUC_EXPLAIN}, &enable_indexonlyscan, true, NULL, NULL, NULL}, {{"enable_bitmapscan", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of bitmap-scan plans."), NULL, GUC_EXPLAIN}, &enable_bitmapscan, true, NULL, NULL, NULL}, {{"enable_tidscan", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of TID scan plans."), NULL, GUC_EXPLAIN}, &enable_tidscan, true, NULL, NULL, NULL},
    {{"enable_sort", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of explicit sort steps."), NULL, GUC_EXPLAIN}, &enable_sort, true, NULL, NULL, NULL}, {{"enable_hashagg", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of hashed aggregation plans."), NULL, GUC_EXPLAIN}, &enable_hashagg, true, NULL, NULL, NULL}, {{"enable_material", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of materialization."), NULL, GUC_EXPLAIN}, &enable_material, true, NULL, NULL, NULL}, {{"enable_nestloop", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of nested-loop join plans."), NULL, GUC_EXPLAIN}, &enable_nestloop, true, NULL, NULL, NULL}, {{"enable_mergejoin", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of merge join plans."), NULL, GUC_EXPLAIN}, &enable_mergejoin, true, NULL, NULL, NULL},
    {{"enable_hashjoin", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of hash join plans."), NULL, GUC_EXPLAIN}, &enable_hashjoin, true, NULL, NULL, NULL}, {{"enable_gathermerge", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of gather merge plans."), NULL, GUC_EXPLAIN}, &enable_gathermerge, true, NULL, NULL, NULL}, {{"enable_partitionwise_join", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables partitionwise join."), NULL, GUC_EXPLAIN}, &enable_partitionwise_join, false, NULL, NULL, NULL}, {{"enable_partitionwise_aggregate", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables partitionwise aggregation and grouping."), NULL, GUC_EXPLAIN}, &enable_partitionwise_aggregate, false, NULL, NULL, NULL}, {{"enable_parallel_append", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of parallel append plans."), NULL, GUC_EXPLAIN}, &enable_parallel_append, true, NULL, NULL, NULL},
    {{"enable_parallel_hash", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables the planner's use of parallel hash plans."), NULL, GUC_EXPLAIN}, &enable_parallel_hash, true, NULL, NULL, NULL},
    {{"enable_partition_pruning", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enables plan-time and run-time partition pruning."),
         gettext_noop("Allows the query planner and executor to compare partition "
                      "bounds to conditions in the query to determine which "
                      "partitions must be scanned."),
         GUC_EXPLAIN},
        &enable_partition_pruning, true, NULL, NULL, NULL},
    {{"geqo", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("Enables genetic query optimization."),
         gettext_noop("This algorithm attempts to do planning without "
                      "exhaustive searching."),
         GUC_EXPLAIN},
        &enable_geqo, true, NULL, NULL, NULL},
    {                                                               
        {"is_superuser", PGC_INTERNAL, UNGROUPED, gettext_noop("Shows whether the current user is a superuser."), NULL, GUC_REPORT | GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &session_auth_is_superuser, false, NULL, NULL, NULL},
    {{"bonjour", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Enables advertising the server via Bonjour."), NULL}, &enable_bonjour, false, check_bonjour, NULL, NULL}, {{"track_commit_timestamp", PGC_POSTMASTER, REPLICATION, gettext_noop("Collects transaction commit time."), NULL}, &track_commit_timestamp, false, NULL, NULL, NULL}, {{"ssl", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Enables SSL connections."), NULL}, &EnableSSL, false, check_ssl, NULL, NULL}, {{"ssl_passphrase_command_supports_reload", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Also use ssl_passphrase_command during server reload."), NULL}, &ssl_passphrase_command_supports_reload, false, NULL, NULL, NULL}, {{"ssl_prefer_server_ciphers", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Give priority to server ciphersuite order."), NULL}, &SSLPreferServerCiphers, true, NULL, NULL, NULL},
    {{"fsync", PGC_SIGHUP, WAL_SETTINGS, gettext_noop("Forces synchronization of updates to disk."),
         gettext_noop("The server will use the fsync() system call in several places to make "
                      "sure that updates are physically written to disk. This insures "
                      "that a database cluster will recover to a consistent state after "
                      "an operating system or hardware crash.")},
        &enableFsync, true, NULL, NULL, NULL},
    {{"ignore_checksum_failure", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Continues processing after a checksum failure."),
         gettext_noop("Detection of a checksum failure normally causes PostgreSQL to "
                      "report an error, aborting the current transaction. Setting "
                      "ignore_checksum_failure to true causes the system to ignore the failure "
                      "(but still report a warning), and continue processing. This "
                      "behavior could cause crashes or other serious problems. Only "
                      "has an effect if checksums are enabled."),
         GUC_NOT_IN_SAMPLE},
        &ignore_checksum_failure, false, NULL, NULL, NULL},
    {{"zero_damaged_pages", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Continues processing past damaged page headers."),
         gettext_noop("Detection of a damaged page header normally causes PostgreSQL to "
                      "report an error, aborting the current transaction. Setting "
                      "zero_damaged_pages to true causes the system to instead report a "
                      "warning, zero out the damaged page, and continue processing. This "
                      "behavior will destroy data, namely all the rows on the damaged page."),
         GUC_NOT_IN_SAMPLE},
        &zero_damaged_pages, false, NULL, NULL, NULL},
    {{"full_page_writes", PGC_SIGHUP, WAL_SETTINGS, gettext_noop("Writes full pages to WAL when first modified after a checkpoint."),
         gettext_noop("A page write in process during an operating system crash might be "
                      "only partially written to disk.  During recovery, the row changes "
                      "stored in WAL are not enough to recover.  This option writes "
                      "pages when first modified after a checkpoint to WAL so full recovery "
                      "is possible.")},
        &fullPageWrites, true, NULL, NULL, NULL},

    {{"wal_log_hints", PGC_POSTMASTER, WAL_SETTINGS, gettext_noop("Writes full pages to WAL when first modified after a checkpoint, even for a non-critical modification."), NULL}, &wal_log_hints, false, NULL, NULL, NULL},

    {{"wal_compression", PGC_SUSET, WAL_SETTINGS, gettext_noop("Compresses full-page writes written in WAL file."), NULL}, &wal_compression, false, NULL, NULL, NULL},

    {{"wal_init_zero", PGC_SUSET, WAL_SETTINGS, gettext_noop("Writes zeroes to new WAL files before first use."), NULL}, &wal_init_zero, true, NULL, NULL, NULL},

    {{"wal_recycle", PGC_SUSET, WAL_SETTINGS, gettext_noop("Recycles WAL files by renaming them."), NULL}, &wal_recycle, true, NULL, NULL, NULL},

    {{"log_checkpoints", PGC_SIGHUP, LOGGING_WHAT, gettext_noop("Logs each checkpoint."), NULL}, &log_checkpoints, false, NULL, NULL, NULL}, {{"log_connections", PGC_SU_BACKEND, LOGGING_WHAT, gettext_noop("Logs each successful connection."), NULL}, &Log_connections, false, NULL, NULL, NULL}, {{"log_disconnections", PGC_SU_BACKEND, LOGGING_WHAT, gettext_noop("Logs end of a session, including duration."), NULL}, &Log_disconnections, false, NULL, NULL, NULL}, {{"log_replication_commands", PGC_SUSET, LOGGING_WHAT, gettext_noop("Logs each replication command."), NULL}, &log_replication_commands, false, NULL, NULL, NULL},
    {{"debug_assertions", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows whether the running server has assertion checks enabled."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &assert_enabled,
#ifdef USE_ASSERT_CHECKING
        true,
#else
        false,
#endif
        NULL, NULL, NULL},

    {{"exit_on_error", PGC_USERSET, ERROR_HANDLING_OPTIONS, gettext_noop("Terminate session on any error."), NULL}, &ExitOnAnyError, false, NULL, NULL, NULL}, {{"restart_after_crash", PGC_SIGHUP, ERROR_HANDLING_OPTIONS, gettext_noop("Reinitialize server after backend crash."), NULL}, &restart_after_crash, true, NULL, NULL, NULL},

    {{"log_duration", PGC_SUSET, LOGGING_WHAT, gettext_noop("Logs the duration of each completed SQL statement."), NULL}, &log_duration, false, NULL, NULL, NULL}, {{"debug_print_parse", PGC_USERSET, LOGGING_WHAT, gettext_noop("Logs each query's parse tree."), NULL}, &Debug_print_parse, false, NULL, NULL, NULL}, {{"debug_print_rewritten", PGC_USERSET, LOGGING_WHAT, gettext_noop("Logs each query's rewritten parse tree."), NULL}, &Debug_print_rewritten, false, NULL, NULL, NULL}, {{"debug_print_plan", PGC_USERSET, LOGGING_WHAT, gettext_noop("Logs each query's execution plan."), NULL}, &Debug_print_plan, false, NULL, NULL, NULL}, {{"debug_pretty_print", PGC_USERSET, LOGGING_WHAT, gettext_noop("Indents parse and plan tree displays."), NULL}, &Debug_pretty_print, true, NULL, NULL, NULL}, {{"log_parser_stats", PGC_SUSET, STATS_MONITORING, gettext_noop("Writes parser performance statistics to the server log."), NULL}, &log_parser_stats, false, check_stage_log_stats, NULL, NULL},
    {{"log_planner_stats", PGC_SUSET, STATS_MONITORING, gettext_noop("Writes planner performance statistics to the server log."), NULL}, &log_planner_stats, false, check_stage_log_stats, NULL, NULL}, {{"log_executor_stats", PGC_SUSET, STATS_MONITORING, gettext_noop("Writes executor performance statistics to the server log."), NULL}, &log_executor_stats, false, check_stage_log_stats, NULL, NULL}, {{"log_statement_stats", PGC_SUSET, STATS_MONITORING, gettext_noop("Writes cumulative performance statistics to the server log."), NULL}, &log_statement_stats, false, check_log_stats, NULL, NULL},
#ifdef BTREE_BUILD_STATS
    {{"log_btree_build_stats", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Logs system resource usage statistics (memory and CPU) on various B-tree operations."), NULL, GUC_NOT_IN_SAMPLE}, &log_btree_build_stats, false, NULL, NULL, NULL},
#endif

    {{"track_activities", PGC_SUSET, STATS_COLLECTOR, gettext_noop("Collects information about executing commands."),
         gettext_noop("Enables the collection of information on the currently "
                      "executing command of each session, along with "
                      "the time at which that command began execution.")},
        &pgstat_track_activities, true, NULL, NULL, NULL},
    {{"track_counts", PGC_SUSET, STATS_COLLECTOR, gettext_noop("Collects statistics on database activity."), NULL}, &pgstat_track_counts, true, NULL, NULL, NULL}, {{"track_io_timing", PGC_SUSET, STATS_COLLECTOR, gettext_noop("Collects timing statistics for database I/O activity."), NULL}, &track_io_timing, false, NULL, NULL, NULL},

    {{"update_process_title", PGC_SUSET, PROCESS_TITLE, gettext_noop("Updates the process title to show the active SQL command."), gettext_noop("Enables updating of the process title every time a new SQL command is received by the server.")}, &update_process_title,
#ifdef WIN32
        false,
#else
        true,
#endif
        NULL, NULL, NULL},

    {{"autovacuum", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Starts the autovacuum subprocess."), NULL}, &autovacuum_start_daemon, true, NULL, NULL, NULL},

    {{"trace_notify", PGC_USERSET, DEVELOPER_OPTIONS, gettext_noop("Generates debugging output for LISTEN and NOTIFY."), NULL, GUC_NOT_IN_SAMPLE}, &Trace_notify, false, NULL, NULL, NULL},

#ifdef LOCK_DEBUG
    {{"trace_locks", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Emits information about lock usage."), NULL, GUC_NOT_IN_SAMPLE}, &Trace_locks, false, NULL, NULL, NULL}, {{"trace_userlocks", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Emits information about user lock usage."), NULL, GUC_NOT_IN_SAMPLE}, &Trace_userlocks, false, NULL, NULL, NULL}, {{"trace_lwlocks", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Emits information about lightweight lock usage."), NULL, GUC_NOT_IN_SAMPLE}, &Trace_lwlocks, false, NULL, NULL, NULL}, {{"debug_deadlocks", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Dumps information about all current locks when a deadlock timeout occurs."), NULL, GUC_NOT_IN_SAMPLE}, &Debug_deadlocks, false, NULL, NULL, NULL},
#endif

    {{"log_lock_waits", PGC_SUSET, LOGGING_WHAT, gettext_noop("Logs long lock waits."), NULL}, &log_lock_waits, false, NULL, NULL, NULL},

    {{"log_hostname", PGC_SIGHUP, LOGGING_WHAT, gettext_noop("Logs the host name in the connection logs."),
         gettext_noop("By default, connection logs only show the IP address "
                      "of the connecting host. If you want them to show the host name you "
                      "can turn this on, but depending on your host name resolution "
                      "setup it might impose a non-negligible performance penalty.")},
        &log_hostname, false, NULL, NULL, NULL},
    {{"transform_null_equals", PGC_USERSET, COMPAT_OPTIONS_CLIENT, gettext_noop("Treats \"expr=NULL\" as \"expr IS NULL\"."),
         gettext_noop("When turned on, expressions of the form expr = NULL "
                      "(or NULL = expr) are treated as expr IS NULL, that is, they "
                      "return true if expr evaluates to the null value, and false "
                      "otherwise. The correct behavior of expr = NULL is to always "
                      "return null (unknown).")},
        &Transform_null_equals, false, NULL, NULL, NULL},
    {{"db_user_namespace", PGC_SIGHUP, CONN_AUTH_AUTH, gettext_noop("Enables per-database user names."), NULL}, &Db_user_namespace, false, NULL, NULL, NULL}, {{"default_transaction_read_only", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the default read-only status of new transactions."), NULL}, &DefaultXactReadOnly, false, NULL, NULL, NULL}, {{"transaction_read_only", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the current transaction's read-only status."), NULL, GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &XactReadOnly, false, check_transaction_read_only, NULL, NULL}, {{"default_transaction_deferrable", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the default deferrable status of new transactions."), NULL}, &DefaultXactDeferrable, false, NULL, NULL, NULL},
    {{"transaction_deferrable", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Whether to defer a read-only serializable transaction until it can be executed with no possible serialization failures."), NULL, GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &XactDeferrable, false, check_transaction_deferrable, NULL, NULL}, {{"row_security", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Enable row security."), gettext_noop("When enabled, row security will be applied to all users.")}, &row_security, true, NULL, NULL, NULL}, {{"check_function_bodies", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Check function bodies during CREATE FUNCTION."), NULL}, &check_function_bodies, true, NULL, NULL, NULL},
    {{"array_nulls", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("Enable input of NULL elements in arrays."),
         gettext_noop("When turned on, unquoted NULL in an array input "
                      "value means a null value; "
                      "otherwise it is taken literally.")},
        &Array_nulls, true, NULL, NULL, NULL},

       
                                                                             
                                                                          
                                                      
       
    {{"default_with_oids", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("WITH OIDS is no longer supported; this can only be false."), NULL, GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE}, &default_with_oids, false, check_default_with_oids, NULL, NULL}, {{"logging_collector", PGC_POSTMASTER, LOGGING_WHERE, gettext_noop("Start a subprocess to capture stderr output and/or csvlogs into log files."), NULL}, &Logging_collector, false, NULL, NULL, NULL}, {{"log_truncate_on_rotation", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Truncate existing log files of same name during log rotation."), NULL}, &Log_truncate_on_rotation, false, NULL, NULL, NULL},

#ifdef TRACE_SORT
    {{"trace_sort", PGC_USERSET, DEVELOPER_OPTIONS, gettext_noop("Emit information about resource usage in sorting."), NULL, GUC_NOT_IN_SAMPLE}, &trace_sort, false, NULL, NULL, NULL},
#endif

#ifdef TRACE_SYNCSCAN
                                                                      
    {{"trace_syncscan", PGC_USERSET, DEVELOPER_OPTIONS, gettext_noop("Generate debugging output for synchronized scanning."), NULL, GUC_NOT_IN_SAMPLE}, &trace_syncscan, false, NULL, NULL, NULL},
#endif

#ifdef DEBUG_BOUNDED_SORT
                                                                      
    {{"optimize_bounded_sort", PGC_USERSET, QUERY_TUNING_METHOD, gettext_noop("Enable bounded sorting using heap sort."), NULL, GUC_NOT_IN_SAMPLE | GUC_EXPLAIN}, &optimize_bounded_sort, true, NULL, NULL, NULL},
#endif

#ifdef WAL_DEBUG
    {{"wal_debug", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Emit WAL-related debugging output."), NULL, GUC_NOT_IN_SAMPLE}, &XLOG_DEBUG, false, NULL, NULL, NULL},
#endif

    {{"integer_datetimes", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Datetimes are integer based."), NULL, GUC_REPORT | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &integer_datetimes, true, NULL, NULL, NULL},

    {{"krb_caseins_users", PGC_SIGHUP, CONN_AUTH_AUTH, gettext_noop("Sets whether Kerberos and GSSAPI user names should be treated as case-insensitive."), NULL}, &pg_krb_caseins_users, false, NULL, NULL, NULL},

    {{"escape_string_warning", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("Warn about backslash escapes in ordinary string literals."), NULL}, &escape_string_warning, true, NULL, NULL, NULL},

    {{"standard_conforming_strings", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("Causes '...' strings to treat backslashes literally."), NULL, GUC_REPORT}, &standard_conforming_strings, true, NULL, NULL, NULL},

    {{"synchronize_seqscans", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("Enable synchronized sequential scans."), NULL}, &synchronize_seqscans, true, NULL, NULL, NULL},

    {{"recovery_target_inclusive", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Sets whether to include or exclude transaction with recovery target."), NULL}, &recoveryTargetInclusive, true, NULL, NULL, NULL},

    {{"hot_standby", PGC_POSTMASTER, REPLICATION_STANDBY, gettext_noop("Allows connections and queries during recovery."), NULL}, &EnableHotStandby, true, NULL, NULL, NULL},

    {{"hot_standby_feedback", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Allows feedback from a hot standby to the primary that will avoid query conflicts."), NULL}, &hot_standby_feedback, false, NULL, NULL, NULL},

    {{"allow_system_table_mods", PGC_POSTMASTER, DEVELOPER_OPTIONS, gettext_noop("Allows modifications of the structure of system tables."), NULL, GUC_NOT_IN_SAMPLE}, &allowSystemTableMods, false, NULL, NULL, NULL},

    {{"ignore_system_indexes", PGC_BACKEND, DEVELOPER_OPTIONS, gettext_noop("Disables reading from system indexes."),
         gettext_noop("It does not prevent updating the indexes, so it is safe "
                      "to use.  The worst consequence is slowness."),
         GUC_NOT_IN_SAMPLE},
        &IgnoreSystemIndexes, false, NULL, NULL, NULL},

    {{"allow_in_place_tablespaces", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Allows tablespaces directly inside pg_tblspc, for testing."), NULL, GUC_NOT_IN_SAMPLE}, &allow_in_place_tablespaces, false, NULL, NULL, NULL},

    {{"lo_compat_privileges", PGC_SUSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("Enables backward compatibility mode for privilege checks on large objects."),
         gettext_noop("Skips privilege checks when reading or modifying large objects, "
                      "for compatibility with PostgreSQL releases prior to 9.0.")},
        &lo_compat_privileges, false, NULL, NULL, NULL},

    {{
         "operator_precedence_warning",
         PGC_USERSET,
         COMPAT_OPTIONS_PREVIOUS,
         gettext_noop("Emit a warning for constructs that changed meaning since PostgreSQL 9.4."),
         NULL,
     },
        &operator_precedence_warning, false, NULL, NULL, NULL},

    {{
         "quote_all_identifiers",
         PGC_USERSET,
         COMPAT_OPTIONS_PREVIOUS,
         gettext_noop("When generating SQL fragments, quote all identifiers."),
         NULL,
     },
        &quote_all_identifiers, false, NULL, NULL, NULL},

    {{"data_checksums", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows whether data checksums are turned on for this cluster."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &data_checksums, false, NULL, NULL, NULL},

    {{"syslog_sequence_numbers", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Add sequence number to syslog messages to avoid duplicate suppression."), NULL}, &syslog_sequence_numbers, true, NULL, NULL, NULL},

    {{"syslog_split_messages", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Split messages sent to syslog by lines and to fit into 1024 bytes."), NULL}, &syslog_split_messages, true, NULL, NULL, NULL},

    {{"parallel_leader_participation", PGC_USERSET, RESOURCES_ASYNCHRONOUS, gettext_noop("Controls whether Gather and Gather Merge also run subplans."), gettext_noop("Should gather nodes also run subplans, or just gather tuples?"), GUC_EXPLAIN}, &parallel_leader_participation, true, NULL, NULL, NULL},

    {{"jit", PGC_USERSET, QUERY_TUNING_OTHER, gettext_noop("Allow JIT compilation."), NULL, GUC_EXPLAIN}, &jit_enabled, true, NULL, NULL, NULL},

    {{"jit_debugging_support", PGC_SU_BACKEND, DEVELOPER_OPTIONS, gettext_noop("Register JIT compiled function with debugger."), NULL, GUC_NOT_IN_SAMPLE}, &jit_debugging_support, false,

           
                                                                              
                                                                       
                         
           
        NULL, NULL, NULL},

    {{"jit_dump_bitcode", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Write out LLVM bitcode to facilitate JIT debugging."), NULL, GUC_NOT_IN_SAMPLE}, &jit_dump_bitcode, false, NULL, NULL, NULL},

    {{"jit_expressions", PGC_USERSET, DEVELOPER_OPTIONS, gettext_noop("Allow JIT compilation of expressions."), NULL, GUC_NOT_IN_SAMPLE}, &jit_expressions, true, NULL, NULL, NULL},

    {{"jit_profiling_support", PGC_SU_BACKEND, DEVELOPER_OPTIONS, gettext_noop("Register JIT compiled function with perf profiler."), NULL, GUC_NOT_IN_SAMPLE}, &jit_profiling_support, false,

           
                                                                              
                                                                       
                         
           
        NULL, NULL, NULL},

    {{"jit_tuple_deforming", PGC_USERSET, DEVELOPER_OPTIONS, gettext_noop("Allow JIT compilation of tuple deforming."), NULL, GUC_NOT_IN_SAMPLE}, &jit_tuple_deforming, true, NULL, NULL, NULL},

    {{
         "data_sync_retry",
         PGC_POSTMASTER,
         ERROR_HANDLING_OPTIONS,
         gettext_noop("Whether to continue running after a failure to sync data files."),
     },
        &data_sync_retry, false, NULL, NULL, NULL},

                            
    {{NULL, 0, 0, NULL, NULL}, NULL, false, NULL, NULL, NULL}};

static struct config_int ConfigureNamesInt[] = {{{"archive_timeout", PGC_SIGHUP, WAL_ARCHIVING,
                                                     gettext_noop("Forces a switch to the next WAL file if a "
                                                                  "new file has not been started within N seconds."),
                                                     NULL, GUC_UNIT_S},
                                                    &XLogArchiveTimeout, 0, 0, INT_MAX / 2, NULL, NULL, NULL},
    {{"post_auth_delay", PGC_BACKEND, DEVELOPER_OPTIONS, gettext_noop("Waits N seconds on connection startup after authentication."), gettext_noop("This allows attaching a debugger to the process."), GUC_NOT_IN_SAMPLE | GUC_UNIT_S}, &PostAuthDelay, 0, 0, INT_MAX / 1000000, NULL, NULL, NULL},
    {{"default_statistics_target", PGC_USERSET, QUERY_TUNING_OTHER, gettext_noop("Sets the default statistics target."),
         gettext_noop("This applies to table columns that have not had a "
                      "column-specific target set via ALTER TABLE SET STATISTICS.")},
        &default_statistics_target, 100, 1, 10000, NULL, NULL, NULL},
    {{"from_collapse_limit", PGC_USERSET, QUERY_TUNING_OTHER,
         gettext_noop("Sets the FROM-list size beyond which subqueries "
                      "are not collapsed."),
         gettext_noop("The planner will merge subqueries into upper "
                      "queries if the resulting FROM list would have no more than "
                      "this many items."),
         GUC_EXPLAIN},
        &from_collapse_limit, 8, 1, INT_MAX, NULL, NULL, NULL},
    {{"join_collapse_limit", PGC_USERSET, QUERY_TUNING_OTHER,
         gettext_noop("Sets the FROM-list size beyond which JOIN "
                      "constructs are not flattened."),
         gettext_noop("The planner will flatten explicit JOIN "
                      "constructs into lists of FROM items whenever a "
                      "list of no more than this many items would result."),
         GUC_EXPLAIN},
        &join_collapse_limit, 8, 1, INT_MAX, NULL, NULL, NULL},
    {{"geqo_threshold", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("Sets the threshold of FROM items beyond which GEQO is used."), NULL, GUC_EXPLAIN}, &geqo_threshold, 12, 2, INT_MAX, NULL, NULL, NULL}, {{"geqo_effort", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("GEQO: effort is used to set the default for other GEQO parameters."), NULL, GUC_EXPLAIN}, &Geqo_effort, DEFAULT_GEQO_EFFORT, MIN_GEQO_EFFORT, MAX_GEQO_EFFORT, NULL, NULL, NULL}, {{"geqo_pool_size", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("GEQO: number of individuals in the population."), gettext_noop("Zero selects a suitable default value."), GUC_EXPLAIN}, &Geqo_pool_size, 0, 0, INT_MAX, NULL, NULL, NULL}, {{"geqo_generations", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("GEQO: number of iterations of the algorithm."), gettext_noop("Zero selects a suitable default value."), GUC_EXPLAIN}, &Geqo_generations, 0, 0, INT_MAX, NULL, NULL, NULL},

    {                                                              
        {"deadlock_timeout", PGC_SUSET, LOCK_MANAGEMENT, gettext_noop("Sets the time to wait on a lock before checking for deadlock."), NULL, GUC_UNIT_MS}, &DeadlockTimeout, 1000, 1, INT_MAX, NULL, NULL, NULL},

    {{"max_standby_archive_delay", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Sets the maximum delay before canceling queries when a hot standby server is processing archived WAL data."), NULL, GUC_UNIT_MS}, &max_standby_archive_delay, 30 * 1000, -1, INT_MAX, NULL, NULL, NULL},

    {{"max_standby_streaming_delay", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Sets the maximum delay before canceling queries when a hot standby server is processing streamed WAL data."), NULL, GUC_UNIT_MS}, &max_standby_streaming_delay, 30 * 1000, -1, INT_MAX, NULL, NULL, NULL},

    {{"recovery_min_apply_delay", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Sets the minimum delay for applying changes during recovery."), NULL, GUC_UNIT_MS}, &recovery_min_apply_delay, 0, 0, INT_MAX, NULL, NULL, NULL},

    {{"wal_receiver_status_interval", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Sets the maximum interval between WAL receiver status reports to the sending server."), NULL, GUC_UNIT_S}, &wal_receiver_status_interval, 10, 0, INT_MAX / 1000, NULL, NULL, NULL},

    {{"wal_receiver_timeout", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Sets the maximum wait time to receive data from the sending server."), NULL, GUC_UNIT_MS}, &wal_receiver_timeout, 60 * 1000, 0, INT_MAX, NULL, NULL, NULL},

    {{"max_connections", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the maximum number of concurrent connections."), NULL}, &MaxConnections, 100, 1, MAX_BACKENDS, check_maxconnections, NULL, NULL},

    {                         
        {"superuser_reserved_connections", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the number of connection slots reserved for superusers."), NULL}, &ReservedBackends, 3, 0, MAX_BACKENDS, NULL, NULL, NULL},

       
                                                                         
                                                                         
       
    {{"shared_buffers", PGC_POSTMASTER, RESOURCES_MEM, gettext_noop("Sets the number of shared memory buffers used by the server."), NULL, GUC_UNIT_BLOCKS}, &NBuffers, 1024, 16, INT_MAX / 2, NULL, NULL, NULL},

    {{"temp_buffers", PGC_USERSET, RESOURCES_MEM, gettext_noop("Sets the maximum number of temporary buffers used by each session."), NULL, GUC_UNIT_BLOCKS | GUC_EXPLAIN}, &num_temp_buffers, 1024, 100, INT_MAX / 2, check_temp_buffers, NULL, NULL},

    {{"port", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the TCP port the server listens on."), NULL}, &PostPortNumber, DEF_PGPORT, 1, 65535, NULL, NULL, NULL},

    {{"unix_socket_permissions", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the access permissions of the Unix-domain socket."),
         gettext_noop("Unix-domain sockets use the usual Unix file system "
                      "permission set. The parameter value is expected "
                      "to be a numeric mode specification in the form "
                      "accepted by the chmod and umask system calls. "
                      "(To use the customary octal format the number must "
                      "start with a 0 (zero).)")},
        &Unix_socket_permissions, 0777, 0000, 0777, NULL, NULL, show_unix_socket_permissions},

    {{"log_file_mode", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Sets the file permissions for log files."),
         gettext_noop("The parameter value is expected "
                      "to be a numeric mode specification in the form "
                      "accepted by the chmod and umask system calls. "
                      "(To use the customary octal format the number must "
                      "start with a 0 (zero).)")},
        &Log_file_mode, 0600, 0000, 0777, NULL, NULL, show_log_file_mode},

    {{"data_directory_mode", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Mode of the data directory."),
         gettext_noop("The parameter value is a numeric mode specification "
                      "in the form accepted by the chmod and umask system "
                      "calls. (To use the customary octal format the number "
                      "must start with a 0 (zero).)"),
         GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE},
        &data_directory_mode, 0700, 0000, 0777, NULL, NULL, show_data_directory_mode},

    {{"work_mem", PGC_USERSET, RESOURCES_MEM, gettext_noop("Sets the maximum memory to be used for query workspaces."),
         gettext_noop("This much memory can be used by each internal "
                      "sort operation and hash table before switching to "
                      "temporary disk files."),
         GUC_UNIT_KB | GUC_EXPLAIN},
        &work_mem, 4096, 64, MAX_KILOBYTES, NULL, NULL, NULL},

    {{"maintenance_work_mem", PGC_USERSET, RESOURCES_MEM, gettext_noop("Sets the maximum memory to be used for maintenance operations."), gettext_noop("This includes operations such as VACUUM and CREATE INDEX."), GUC_UNIT_KB}, &maintenance_work_mem, 65536, 1024, MAX_KILOBYTES, NULL, NULL, NULL},

       
                                                                           
                                                                              
                                                                        
       
    {{"max_stack_depth", PGC_SUSET, RESOURCES_MEM, gettext_noop("Sets the maximum stack depth, in kilobytes."), NULL, GUC_UNIT_KB}, &max_stack_depth, 100, 100, MAX_KILOBYTES, check_max_stack_depth, assign_max_stack_depth, NULL},

    {{"temp_file_limit", PGC_SUSET, RESOURCES_DISK, gettext_noop("Limits the total size of all temporary files used by each process."), gettext_noop("-1 means no limit."), GUC_UNIT_KB}, &temp_file_limit, -1, -1, INT_MAX, NULL, NULL, NULL},

    {{"vacuum_cost_page_hit", PGC_USERSET, RESOURCES_VACUUM_DELAY, gettext_noop("Vacuum cost for a page found in the buffer cache."), NULL}, &VacuumCostPageHit, 1, 0, 10000, NULL, NULL, NULL},

    {{"vacuum_cost_page_miss", PGC_USERSET, RESOURCES_VACUUM_DELAY, gettext_noop("Vacuum cost for a page not found in the buffer cache."), NULL}, &VacuumCostPageMiss, 10, 0, 10000, NULL, NULL, NULL},

    {{"vacuum_cost_page_dirty", PGC_USERSET, RESOURCES_VACUUM_DELAY, gettext_noop("Vacuum cost for a page dirtied by vacuum."), NULL}, &VacuumCostPageDirty, 20, 0, 10000, NULL, NULL, NULL},

    {{"vacuum_cost_limit", PGC_USERSET, RESOURCES_VACUUM_DELAY, gettext_noop("Vacuum cost amount available before napping."), NULL}, &VacuumCostLimit, 200, 1, 10000, NULL, NULL, NULL},

    {{"autovacuum_vacuum_cost_limit", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Vacuum cost amount available before napping, for autovacuum."), NULL}, &autovacuum_vac_cost_limit, -1, -1, 10000, NULL, NULL, NULL},

    {{"max_files_per_process", PGC_POSTMASTER, RESOURCES_KERNEL, gettext_noop("Sets the maximum number of simultaneously open files for each server process."), NULL}, &max_files_per_process, 1000, 25, INT_MAX, NULL, NULL, NULL},

       
                                                                         
       
    {{"max_prepared_transactions", PGC_POSTMASTER, RESOURCES_MEM, gettext_noop("Sets the maximum number of simultaneously prepared transactions."), NULL}, &max_prepared_xacts, 0, 0, MAX_BACKENDS, NULL, NULL, NULL},

#ifdef LOCK_DEBUG
    {{"trace_lock_oidmin", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Sets the minimum OID of tables for tracking locks."), gettext_noop("Is used to avoid output on system tables."), GUC_NOT_IN_SAMPLE}, &Trace_lock_oidmin, FirstNormalObjectId, 0, INT_MAX, NULL, NULL, NULL}, {{"trace_lock_table", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Sets the OID of the table with unconditionally lock tracing."), NULL, GUC_NOT_IN_SAMPLE}, &Trace_lock_table, 0, 0, INT_MAX, NULL, NULL, NULL},
#endif

    {{"statement_timeout", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the maximum allowed duration of any statement."), gettext_noop("A value of 0 turns off the timeout."), GUC_UNIT_MS}, &StatementTimeout, 0, 0, INT_MAX, NULL, NULL, NULL},

    {{"lock_timeout", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the maximum allowed duration of any wait for a lock."), gettext_noop("A value of 0 turns off the timeout."), GUC_UNIT_MS}, &LockTimeout, 0, 0, INT_MAX, NULL, NULL, NULL},

    {{"idle_in_transaction_session_timeout", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the maximum allowed duration of any idling transaction."), gettext_noop("A value of 0 turns off the timeout."), GUC_UNIT_MS}, &IdleInTransactionSessionTimeout, 0, 0, INT_MAX, NULL, NULL, NULL},

    {{"vacuum_freeze_min_age", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Minimum age at which VACUUM should freeze a table row."), NULL}, &vacuum_freeze_min_age, 50000000, 0, 1000000000, NULL, NULL, NULL},

    {{"vacuum_freeze_table_age", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Age at which VACUUM should scan whole table to freeze tuples."), NULL}, &vacuum_freeze_table_age, 150000000, 0, 2000000000, NULL, NULL, NULL},

    {{"vacuum_multixact_freeze_min_age", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Minimum age at which VACUUM should freeze a MultiXactId in a table row."), NULL}, &vacuum_multixact_freeze_min_age, 5000000, 0, 1000000000, NULL, NULL, NULL},

    {{"vacuum_multixact_freeze_table_age", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Multixact age at which VACUUM should scan whole table to freeze tuples."), NULL}, &vacuum_multixact_freeze_table_age, 150000000, 0, 2000000000, NULL, NULL, NULL},

    {{"vacuum_defer_cleanup_age", PGC_SIGHUP, REPLICATION_MASTER, gettext_noop("Number of transactions by which VACUUM and HOT cleanup should be deferred, if any."), NULL}, &vacuum_defer_cleanup_age, 0, 0, 1000000, NULL, NULL, NULL},

       
                                                                         
       
    {{"max_locks_per_transaction", PGC_POSTMASTER, LOCK_MANAGEMENT, gettext_noop("Sets the maximum number of locks per transaction."),
         gettext_noop("The shared lock table is sized on the assumption that "
                      "at most max_locks_per_transaction * max_connections distinct "
                      "objects will need to be locked at any one time.")},
        &max_locks_per_xact, 64, 10, INT_MAX, NULL, NULL, NULL},

    {{"max_pred_locks_per_transaction", PGC_POSTMASTER, LOCK_MANAGEMENT, gettext_noop("Sets the maximum number of predicate locks per transaction."),
         gettext_noop("The shared predicate lock table is sized on the assumption that "
                      "at most max_pred_locks_per_transaction * max_connections distinct "
                      "objects will need to be locked at any one time.")},
        &max_predicate_locks_per_xact, 64, 10, INT_MAX, NULL, NULL, NULL},

    {{"max_pred_locks_per_relation", PGC_SIGHUP, LOCK_MANAGEMENT, gettext_noop("Sets the maximum number of predicate-locked pages and tuples per relation."),
         gettext_noop("If more than this total of pages and tuples in the same relation are locked "
                      "by a connection, those locks are replaced by a relation-level lock.")},
        &max_predicate_locks_per_relation, -2, INT_MIN, INT_MAX, NULL, NULL, NULL},

    {{"max_pred_locks_per_page", PGC_SIGHUP, LOCK_MANAGEMENT, gettext_noop("Sets the maximum number of predicate-locked tuples per page."),
         gettext_noop("If more than this number of tuples on the same page are locked "
                      "by a connection, those locks are replaced by a page-level lock.")},
        &max_predicate_locks_per_page, 2, 0, INT_MAX, NULL, NULL, NULL},

    {{"authentication_timeout", PGC_SIGHUP, CONN_AUTH_AUTH, gettext_noop("Sets the maximum allowed time to complete client authentication."), NULL, GUC_UNIT_S}, &AuthenticationTimeout, 60, 1, 600, NULL, NULL, NULL},

    {                         
        {"pre_auth_delay", PGC_SIGHUP, DEVELOPER_OPTIONS, gettext_noop("Waits N seconds on connection startup before authentication."), gettext_noop("This allows attaching a debugger to the process."), GUC_NOT_IN_SAMPLE | GUC_UNIT_S}, &PreAuthDelay, 0, 0, 60, NULL, NULL, NULL},

    {{"wal_keep_segments", PGC_SIGHUP, REPLICATION_SENDING, gettext_noop("Sets the number of WAL files held for standby servers."), NULL}, &wal_keep_segments, 0, 0, INT_MAX, NULL, NULL, NULL},

    {{"min_wal_size", PGC_SIGHUP, WAL_CHECKPOINTS, gettext_noop("Sets the minimum size to shrink the WAL to."), NULL, GUC_UNIT_MB}, &min_wal_size_mb, DEFAULT_MIN_WAL_SEGS *(DEFAULT_XLOG_SEG_SIZE / (1024 * 1024)), 2, MAX_KILOBYTES, NULL, NULL, NULL},

    {{"max_wal_size", PGC_SIGHUP, WAL_CHECKPOINTS, gettext_noop("Sets the WAL size that triggers a checkpoint."), NULL, GUC_UNIT_MB}, &max_wal_size_mb, DEFAULT_MAX_WAL_SEGS *(DEFAULT_XLOG_SEG_SIZE / (1024 * 1024)), 2, MAX_KILOBYTES, NULL, assign_max_wal_size, NULL},

    {{"checkpoint_timeout", PGC_SIGHUP, WAL_CHECKPOINTS, gettext_noop("Sets the maximum time between automatic WAL checkpoints."), NULL, GUC_UNIT_S}, &CheckPointTimeout, 300, 30, 86400, NULL, NULL, NULL},

    {{"checkpoint_warning", PGC_SIGHUP, WAL_CHECKPOINTS,
         gettext_noop("Enables warnings if checkpoint segments are filled more "
                      "frequently than this."),
         gettext_noop("Write a message to the server log if checkpoints "
                      "caused by the filling of checkpoint segment files happens more "
                      "frequently than this number of seconds. Zero turns off the warning."),
         GUC_UNIT_S},
        &CheckPointWarning, 30, 0, INT_MAX, NULL, NULL, NULL},

    {{"checkpoint_flush_after", PGC_SIGHUP, WAL_CHECKPOINTS, gettext_noop("Number of pages after which previously performed writes are flushed to disk."), NULL, GUC_UNIT_BLOCKS}, &checkpoint_flush_after, DEFAULT_CHECKPOINT_FLUSH_AFTER, 0, WRITEBACK_MAX_PENDING_FLUSHES, NULL, NULL, NULL},

    {{"wal_buffers", PGC_POSTMASTER, WAL_SETTINGS, gettext_noop("Sets the number of disk-page buffers in shared memory for WAL."), NULL, GUC_UNIT_XBLOCKS}, &XLOGbuffers, -1, -1, (INT_MAX / XLOG_BLCKSZ), check_wal_buffers, NULL, NULL},

    {{"wal_writer_delay", PGC_SIGHUP, WAL_SETTINGS, gettext_noop("Time between WAL flushes performed in the WAL writer."), NULL, GUC_UNIT_MS}, &WalWriterDelay, 200, 1, 10000, NULL, NULL, NULL},

    {{"wal_writer_flush_after", PGC_SIGHUP, WAL_SETTINGS, gettext_noop("Amount of WAL written out by WAL writer that triggers a flush."), NULL, GUC_UNIT_XBLOCKS}, &WalWriterFlushAfter, (1024 * 1024) / XLOG_BLCKSZ, 0, INT_MAX, NULL, NULL, NULL},

    {{"max_wal_senders", PGC_POSTMASTER, REPLICATION_SENDING, gettext_noop("Sets the maximum number of simultaneously running WAL sender processes."), NULL}, &max_wal_senders, 10, 0, MAX_BACKENDS, check_max_wal_senders, NULL, NULL},

    {                         
        {"max_replication_slots", PGC_POSTMASTER, REPLICATION_SENDING, gettext_noop("Sets the maximum number of simultaneously defined replication slots."), NULL}, &max_replication_slots, 10, 0, MAX_BACKENDS           , NULL, NULL, NULL},

    {{"wal_sender_timeout", PGC_USERSET, REPLICATION_SENDING, gettext_noop("Sets the maximum time to wait for WAL replication."), NULL, GUC_UNIT_MS}, &wal_sender_timeout, 60 * 1000, 0, INT_MAX, NULL, NULL, NULL},

    {{
         "commit_delay", PGC_SUSET, WAL_SETTINGS,
         gettext_noop("Sets the delay in microseconds between transaction commit and "
                      "flushing WAL to disk."),
         NULL
                                                                              
     },
        &CommitDelay, 0, 0, 100000, NULL, NULL, NULL},

    {{"commit_siblings", PGC_USERSET, WAL_SETTINGS,
         gettext_noop("Sets the minimum concurrent open transactions before performing "
                      "commit_delay."),
         NULL},
        &CommitSiblings, 5, 0, 1000, NULL, NULL, NULL},

    {{"extra_float_digits", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the number of digits displayed for floating-point values."),
         gettext_noop("This affects real, double precision, and geometric data types. "
                      "A zero or negative parameter value is added to the standard "
                      "number of digits (FLT_DIG or DBL_DIG as appropriate). "
                      "Any value greater than zero selects precise output mode.")},
        &extra_float_digits, 1, -15, 3, NULL, NULL, NULL},

    {{"log_min_duration_statement", PGC_SUSET, LOGGING_WHEN,
         gettext_noop("Sets the minimum execution time above which "
                      "statements will be logged."),
         gettext_noop("Zero prints all queries. -1 turns this feature off."), GUC_UNIT_MS},
        &log_min_duration_statement, -1, -1, INT_MAX, NULL, NULL, NULL},

    {{"log_autovacuum_min_duration", PGC_SIGHUP, LOGGING_WHAT,
         gettext_noop("Sets the minimum execution time above which "
                      "autovacuum actions will be logged."),
         gettext_noop("Zero prints all actions. -1 turns autovacuum logging off."), GUC_UNIT_MS},
        &Log_autovacuum_min_duration, -1, -1, INT_MAX, NULL, NULL, NULL},

    {{"bgwriter_delay", PGC_SIGHUP, RESOURCES_BGWRITER, gettext_noop("Background writer sleep time between rounds."), NULL, GUC_UNIT_MS}, &BgWriterDelay, 200, 10, 10000, NULL, NULL, NULL},

    {{"bgwriter_lru_maxpages", PGC_SIGHUP, RESOURCES_BGWRITER, gettext_noop("Background writer maximum number of LRU pages to flush per round."), NULL}, &bgwriter_lru_maxpages, 100, 0, INT_MAX / 2,                                         
        NULL, NULL, NULL},

    {{"bgwriter_flush_after", PGC_SIGHUP, RESOURCES_BGWRITER, gettext_noop("Number of pages after which previously performed writes are flushed to disk."), NULL, GUC_UNIT_BLOCKS}, &bgwriter_flush_after, DEFAULT_BGWRITER_FLUSH_AFTER, 0, WRITEBACK_MAX_PENDING_FLUSHES, NULL, NULL, NULL},

    {{"effective_io_concurrency", PGC_USERSET, RESOURCES_ASYNCHRONOUS, gettext_noop("Number of simultaneous requests that can be handled efficiently by the disk subsystem."), gettext_noop("For RAID arrays, this should be approximately the number of drive spindles in the array."), GUC_EXPLAIN}, &effective_io_concurrency,
#ifdef USE_PREFETCH
        1,
#else
        0,
#endif
        0, MAX_IO_CONCURRENCY, check_effective_io_concurrency, assign_effective_io_concurrency, NULL},

    {{"backend_flush_after", PGC_USERSET, RESOURCES_ASYNCHRONOUS, gettext_noop("Number of pages after which previously performed writes are flushed to disk."), NULL, GUC_UNIT_BLOCKS}, &backend_flush_after, DEFAULT_BACKEND_FLUSH_AFTER, 0, WRITEBACK_MAX_PENDING_FLUSHES, NULL, NULL, NULL},

    {{
         "max_worker_processes",
         PGC_POSTMASTER,
         RESOURCES_ASYNCHRONOUS,
         gettext_noop("Maximum number of concurrent worker processes."),
         NULL,
     },
        &max_worker_processes, 8, 0, MAX_BACKENDS, check_max_worker_processes, NULL, NULL},

    {{
         "max_logical_replication_workers",
         PGC_POSTMASTER,
         REPLICATION_SUBSCRIBERS,
         gettext_noop("Maximum number of logical replication worker processes."),
         NULL,
     },
        &max_logical_replication_workers, 4, 0, MAX_BACKENDS, NULL, NULL, NULL},

    {{
         "max_sync_workers_per_subscription",
         PGC_SIGHUP,
         REPLICATION_SUBSCRIBERS,
         gettext_noop("Maximum number of table synchronization workers per subscription."),
         NULL,
     },
        &max_sync_workers_per_subscription, 2, 0, MAX_BACKENDS, NULL, NULL, NULL},

    {{"log_rotation_age", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Automatic log file rotation will occur after N minutes."), NULL, GUC_UNIT_MIN}, &Log_RotationAge, HOURS_PER_DAY *MINS_PER_HOUR, 0, INT_MAX / SECS_PER_MINUTE, NULL, NULL, NULL},

    {{"log_rotation_size", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Automatic log file rotation will occur after N kilobytes."), NULL, GUC_UNIT_KB}, &Log_RotationSize, 10 * 1024, 0, INT_MAX / 1024, NULL, NULL, NULL},

    {{"max_function_args", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the maximum number of function arguments."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &max_function_args, FUNC_MAX_ARGS, FUNC_MAX_ARGS, FUNC_MAX_ARGS, NULL, NULL, NULL},

    {{"max_index_keys", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the maximum number of index keys."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &max_index_keys, INDEX_MAX_KEYS, INDEX_MAX_KEYS, INDEX_MAX_KEYS, NULL, NULL, NULL},

    {{"max_identifier_length", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the maximum identifier length."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &max_identifier_length, NAMEDATALEN - 1, NAMEDATALEN - 1, NAMEDATALEN - 1, NULL, NULL, NULL},

    {{"block_size", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the size of a disk block."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &block_size, BLCKSZ, BLCKSZ, BLCKSZ, NULL, NULL, NULL},

    {{"segment_size", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the number of pages per disk file."), NULL, GUC_UNIT_BLOCKS | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &segment_size, RELSEG_SIZE, RELSEG_SIZE, RELSEG_SIZE, NULL, NULL, NULL},

    {{"wal_block_size", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the block size in the write ahead log."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &wal_block_size, XLOG_BLCKSZ, XLOG_BLCKSZ, XLOG_BLCKSZ, NULL, NULL, NULL},

    {{"wal_retrieve_retry_interval", PGC_SIGHUP, REPLICATION_STANDBY,
         gettext_noop("Sets the time to wait before retrying to retrieve WAL "
                      "after a failed attempt."),
         NULL, GUC_UNIT_MS},
        &wal_retrieve_retry_interval, 5000, 1, INT_MAX, NULL, NULL, NULL},

    {{"wal_segment_size", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the size of write ahead log segments."), NULL, GUC_UNIT_BYTE | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &wal_segment_size, DEFAULT_XLOG_SEG_SIZE, WalSegMinSize, WalSegMaxSize, NULL, NULL, NULL},

    {{"autovacuum_naptime", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Time to sleep between autovacuum runs."), NULL, GUC_UNIT_S}, &autovacuum_naptime, 60, 1, INT_MAX / 1000, NULL, NULL, NULL}, {{"autovacuum_vacuum_threshold", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Minimum number of tuple updates or deletes prior to vacuum."), NULL}, &autovacuum_vac_thresh, 50, 0, INT_MAX, NULL, NULL, NULL}, {{"autovacuum_analyze_threshold", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Minimum number of tuple inserts, updates, or deletes prior to analyze."), NULL}, &autovacuum_anl_thresh, 50, 0, INT_MAX, NULL, NULL, NULL},
    {                                                                
        {"autovacuum_freeze_max_age", PGC_POSTMASTER, AUTOVACUUM, gettext_noop("Age at which to autovacuum a table to prevent transaction ID wraparound."), NULL}, &autovacuum_freeze_max_age,
                                                                 
        200000000, 100000, 2000000000, NULL, NULL, NULL},
    {                                                                   
        {"autovacuum_multixact_freeze_max_age", PGC_POSTMASTER, AUTOVACUUM, gettext_noop("Multixact age at which to autovacuum a table to prevent multixact wraparound."), NULL}, &autovacuum_multixact_freeze_max_age, 400000000, 10000, 2000000000, NULL, NULL, NULL},
    {                         
        {"autovacuum_max_workers", PGC_POSTMASTER, AUTOVACUUM, gettext_noop("Sets the maximum number of simultaneously running autovacuum worker processes."), NULL}, &autovacuum_max_workers, 3, 1, MAX_BACKENDS, check_autovacuum_max_workers, NULL, NULL},

    {{"max_parallel_maintenance_workers", PGC_USERSET, RESOURCES_ASYNCHRONOUS, gettext_noop("Sets the maximum number of parallel processes per maintenance operation."), NULL}, &max_parallel_maintenance_workers, 2, 0, 1024, NULL, NULL, NULL},

    {{"max_parallel_workers_per_gather", PGC_USERSET, RESOURCES_ASYNCHRONOUS, gettext_noop("Sets the maximum number of parallel processes per executor node."), NULL, GUC_EXPLAIN}, &max_parallel_workers_per_gather, 2, 0, MAX_PARALLEL_WORKER_LIMIT, NULL, NULL, NULL},

    {{"max_parallel_workers", PGC_USERSET, RESOURCES_ASYNCHRONOUS, gettext_noop("Sets the maximum number of parallel workers that can be active at one time."), NULL, GUC_EXPLAIN}, &max_parallel_workers, 8, 0, MAX_PARALLEL_WORKER_LIMIT, NULL, NULL, NULL},

    {{"autovacuum_work_mem", PGC_SIGHUP, RESOURCES_MEM, gettext_noop("Sets the maximum memory to be used by each autovacuum worker process."), NULL, GUC_UNIT_KB}, &autovacuum_work_mem, -1, -1, MAX_KILOBYTES, check_autovacuum_work_mem, NULL, NULL},

    {{"old_snapshot_threshold", PGC_POSTMASTER, RESOURCES_ASYNCHRONOUS, gettext_noop("Time before a snapshot is too old to read pages changed after the snapshot was taken."), gettext_noop("A value of -1 disables this feature."), GUC_UNIT_MIN}, &old_snapshot_threshold, -1, -1, MINS_PER_HOUR *HOURS_PER_DAY * 60, NULL, NULL, NULL},

    {{"tcp_keepalives_idle", PGC_USERSET, CLIENT_CONN_OTHER, gettext_noop("Time between issuing TCP keepalives."), gettext_noop("A value of 0 uses the system default."), GUC_UNIT_S}, &tcp_keepalives_idle, 0, 0, INT_MAX, NULL, assign_tcp_keepalives_idle, show_tcp_keepalives_idle},

    {{"tcp_keepalives_interval", PGC_USERSET, CLIENT_CONN_OTHER, gettext_noop("Time between TCP keepalive retransmits."), gettext_noop("A value of 0 uses the system default."), GUC_UNIT_S}, &tcp_keepalives_interval, 0, 0, INT_MAX, NULL, assign_tcp_keepalives_interval, show_tcp_keepalives_interval},

    {{
         "ssl_renegotiation_limit",
         PGC_USERSET,
         CONN_AUTH_SSL,
         gettext_noop("SSL renegotiation is no longer supported; this can only be 0."),
         NULL,
         GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE,
     },
        &ssl_renegotiation_limit, 0, 0, 0, NULL, NULL, NULL},

    {{
         "tcp_keepalives_count",
         PGC_USERSET,
         CLIENT_CONN_OTHER,
         gettext_noop("Maximum number of TCP keepalive retransmits."),
         gettext_noop("This controls the number of consecutive keepalive retransmits that can be "
                      "lost before a connection is considered dead. A value of 0 uses the "
                      "system default."),
     },
        &tcp_keepalives_count, 0, 0, INT_MAX, NULL, assign_tcp_keepalives_count, show_tcp_keepalives_count},

    {{"gin_fuzzy_search_limit", PGC_USERSET, CLIENT_CONN_OTHER, gettext_noop("Sets the maximum allowed result for exact search by GIN."), NULL, 0}, &GinFuzzySearchLimit, 0, 0, INT_MAX, NULL, NULL, NULL},

    {{
         "effective_cache_size",
         PGC_USERSET,
         QUERY_TUNING_COST,
         gettext_noop("Sets the planner's assumption about the total size of the data caches."),
         gettext_noop("That is, the total size of the caches (kernel cache and shared buffers) used for PostgreSQL data files. "
                      "This is measured in disk pages, which are normally 8 kB each."),
         GUC_UNIT_BLOCKS | GUC_EXPLAIN,
     },
        &effective_cache_size, DEFAULT_EFFECTIVE_CACHE_SIZE, 1, INT_MAX, NULL, NULL, NULL},

    {{
         "min_parallel_table_scan_size",
         PGC_USERSET,
         QUERY_TUNING_COST,
         gettext_noop("Sets the minimum amount of table data for a parallel scan."),
         gettext_noop("If the planner estimates that it will read a number of table pages too small to reach this limit, a parallel scan will not be considered."),
         GUC_UNIT_BLOCKS | GUC_EXPLAIN,
     },
        &min_parallel_table_scan_size, (8 * 1024 * 1024) / BLCKSZ, 0, INT_MAX / 3, NULL, NULL, NULL},

    {{
         "min_parallel_index_scan_size",
         PGC_USERSET,
         QUERY_TUNING_COST,
         gettext_noop("Sets the minimum amount of index data for a parallel scan."),
         gettext_noop("If the planner estimates that it will read a number of index pages too small to reach this limit, a parallel scan will not be considered."),
         GUC_UNIT_BLOCKS | GUC_EXPLAIN,
     },
        &min_parallel_index_scan_size, (512 * 1024) / BLCKSZ, 0, INT_MAX / 3, NULL, NULL, NULL},

    {                                     
        {"server_version_num", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the server version as an integer."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &server_version_num, PG_VERSION_NUM, PG_VERSION_NUM, PG_VERSION_NUM, NULL, NULL, NULL},

    {{"log_temp_files", PGC_SUSET, LOGGING_WHAT, gettext_noop("Log the use of temporary files larger than this number of kilobytes."), gettext_noop("Zero logs all files. The default is -1 (turning this feature off)."), GUC_UNIT_KB}, &log_temp_files, -1, -1, INT_MAX, NULL, NULL, NULL},

    {{"track_activity_query_size", PGC_POSTMASTER, RESOURCES_MEM, gettext_noop("Sets the size reserved for pg_stat_activity.query, in bytes."), NULL, GUC_UNIT_BYTE}, &pgstat_track_activity_query_size, 1024, 100, 102400, NULL, NULL, NULL},

    {{"gin_pending_list_limit", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the maximum size of the pending list for GIN index."), NULL, GUC_UNIT_KB}, &gin_pending_list_limit, 4096, 64, MAX_KILOBYTES, NULL, NULL, NULL},

    {{"tcp_user_timeout", PGC_USERSET, CLIENT_CONN_OTHER, gettext_noop("TCP user timeout."), gettext_noop("A value of 0 uses the system default."), GUC_UNIT_MS}, &tcp_user_timeout, 0, 0, INT_MAX, NULL, assign_tcp_user_timeout, show_tcp_user_timeout},

                            
    {{NULL, 0, 0, NULL, NULL}, NULL, 0, 0, 0, NULL, NULL, NULL}};

static struct config_real ConfigureNamesReal[] = {{{"seq_page_cost", PGC_USERSET, QUERY_TUNING_COST,
                                                       gettext_noop("Sets the planner's estimate of the cost of a "
                                                                    "sequentially fetched disk page."),
                                                       NULL, GUC_EXPLAIN},
                                                      &seq_page_cost, DEFAULT_SEQ_PAGE_COST, 0, DBL_MAX, NULL, NULL, NULL},
    {{"random_page_cost", PGC_USERSET, QUERY_TUNING_COST,
         gettext_noop("Sets the planner's estimate of the cost of a "
                      "nonsequentially fetched disk page."),
         NULL, GUC_EXPLAIN},
        &random_page_cost, DEFAULT_RANDOM_PAGE_COST, 0, DBL_MAX, NULL, NULL, NULL},
    {{"cpu_tuple_cost", PGC_USERSET, QUERY_TUNING_COST,
         gettext_noop("Sets the planner's estimate of the cost of "
                      "processing each tuple (row)."),
         NULL, GUC_EXPLAIN},
        &cpu_tuple_cost, DEFAULT_CPU_TUPLE_COST, 0, DBL_MAX, NULL, NULL, NULL},
    {{"cpu_index_tuple_cost", PGC_USERSET, QUERY_TUNING_COST,
         gettext_noop("Sets the planner's estimate of the cost of "
                      "processing each index entry during an index scan."),
         NULL, GUC_EXPLAIN},
        &cpu_index_tuple_cost, DEFAULT_CPU_INDEX_TUPLE_COST, 0, DBL_MAX, NULL, NULL, NULL},
    {{"cpu_operator_cost", PGC_USERSET, QUERY_TUNING_COST,
         gettext_noop("Sets the planner's estimate of the cost of "
                      "processing each operator or function call."),
         NULL, GUC_EXPLAIN},
        &cpu_operator_cost, DEFAULT_CPU_OPERATOR_COST, 0, DBL_MAX, NULL, NULL, NULL},
    {{"parallel_tuple_cost", PGC_USERSET, QUERY_TUNING_COST,
         gettext_noop("Sets the planner's estimate of the cost of "
                      "passing each tuple (row) from worker to master backend."),
         NULL, GUC_EXPLAIN},
        &parallel_tuple_cost, DEFAULT_PARALLEL_TUPLE_COST, 0, DBL_MAX, NULL, NULL, NULL},
    {{"parallel_setup_cost", PGC_USERSET, QUERY_TUNING_COST,
         gettext_noop("Sets the planner's estimate of the cost of "
                      "starting up worker processes for parallel query."),
         NULL, GUC_EXPLAIN},
        &parallel_setup_cost, DEFAULT_PARALLEL_SETUP_COST, 0, DBL_MAX, NULL, NULL, NULL},

    {{"jit_above_cost", PGC_USERSET, QUERY_TUNING_COST, gettext_noop("Perform JIT compilation if query is more expensive."), gettext_noop("-1 disables JIT compilation."), GUC_EXPLAIN}, &jit_above_cost, 100000, -1, DBL_MAX, NULL, NULL, NULL},

    {{"jit_optimize_above_cost", PGC_USERSET, QUERY_TUNING_COST, gettext_noop("Optimize JITed functions if query is more expensive."), gettext_noop("-1 disables optimization."), GUC_EXPLAIN}, &jit_optimize_above_cost, 500000, -1, DBL_MAX, NULL, NULL, NULL},

    {{"jit_inline_above_cost", PGC_USERSET, QUERY_TUNING_COST, gettext_noop("Perform JIT inlining if query is more expensive."), gettext_noop("-1 disables inlining."), GUC_EXPLAIN}, &jit_inline_above_cost, 500000, -1, DBL_MAX, NULL, NULL, NULL},

    {{"cursor_tuple_fraction", PGC_USERSET, QUERY_TUNING_OTHER,
         gettext_noop("Sets the planner's estimate of the fraction of "
                      "a cursor's rows that will be retrieved."),
         NULL, GUC_EXPLAIN},
        &cursor_tuple_fraction, DEFAULT_CURSOR_TUPLE_FRACTION, 0.0, 1.0, NULL, NULL, NULL},

    {{"geqo_selection_bias", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("GEQO: selective pressure within the population."), NULL, GUC_EXPLAIN}, &Geqo_selection_bias, DEFAULT_GEQO_SELECTION_BIAS, MIN_GEQO_SELECTION_BIAS, MAX_GEQO_SELECTION_BIAS, NULL, NULL, NULL}, {{"geqo_seed", PGC_USERSET, QUERY_TUNING_GEQO, gettext_noop("GEQO: seed for random path selection."), NULL, GUC_EXPLAIN}, &Geqo_seed, 0.0, 0.0, 1.0, NULL, NULL, NULL},

    {{"bgwriter_lru_multiplier", PGC_SIGHUP, RESOURCES_BGWRITER, gettext_noop("Multiple of the average buffer usage to free per round."), NULL}, &bgwriter_lru_multiplier, 2.0, 0.0, 10.0, NULL, NULL, NULL},

    {{"seed", PGC_USERSET, UNGROUPED, gettext_noop("Sets the seed for random-number generation."), NULL, GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &phony_random_seed, 0.0, -1.0, 1.0, check_random_seed, assign_random_seed, show_random_seed},

    {{"vacuum_cost_delay", PGC_USERSET, RESOURCES_VACUUM_DELAY, gettext_noop("Vacuum cost delay in milliseconds."), NULL, GUC_UNIT_MS}, &VacuumCostDelay, 0, 0, 100, NULL, NULL, NULL},

    {{"autovacuum_vacuum_cost_delay", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Vacuum cost delay in milliseconds, for autovacuum."), NULL, GUC_UNIT_MS}, &autovacuum_vac_cost_delay, 2, -1, 100, NULL, NULL, NULL},

    {{"autovacuum_vacuum_scale_factor", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Number of tuple updates or deletes prior to vacuum as a fraction of reltuples."), NULL}, &autovacuum_vac_scale, 0.2, 0.0, 100.0, NULL, NULL, NULL}, {{"autovacuum_analyze_scale_factor", PGC_SIGHUP, AUTOVACUUM, gettext_noop("Number of tuple inserts, updates, or deletes prior to analyze as a fraction of reltuples."), NULL}, &autovacuum_anl_scale, 0.1, 0.0, 100.0, NULL, NULL, NULL},

    {{"checkpoint_completion_target", PGC_SIGHUP, WAL_CHECKPOINTS, gettext_noop("Time spent flushing dirty buffers during checkpoint, as fraction of checkpoint interval."), NULL}, &CheckPointCompletionTarget, 0.5, 0.0, 1.0, NULL, assign_checkpoint_completion_target, NULL},

    {{"vacuum_cleanup_index_scale_factor", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Number of tuple inserts prior to index cleanup as a fraction of reltuples."), NULL}, &vacuum_cleanup_index_scale_factor, 0.1, 0.0, 1e10, NULL, NULL, NULL},

    {{"log_transaction_sample_rate", PGC_SUSET, LOGGING_WHEN, gettext_noop("Set the fraction of transactions to log for new transactions."),
         gettext_noop("Logs all statements from a fraction of transactions. "
                      "Use a value between 0.0 (never log) and 1.0 (log all "
                      "statements for all transactions).")},
        &log_xact_sample_rate, 0.0, 0.0, 1.0, NULL, NULL, NULL},

                            
    {{NULL, 0, 0, NULL, NULL}, NULL, 0.0, 0.0, 0.0, NULL, NULL, NULL}};

static struct config_string ConfigureNamesString[] = {{{"archive_command", PGC_SIGHUP, WAL_ARCHIVING, gettext_noop("Sets the shell command that will be called to archive a WAL file."), NULL}, &XLogArchiveCommand, "", NULL, NULL, show_archive_command},

    {{"restore_command", PGC_POSTMASTER, WAL_ARCHIVE_RECOVERY, gettext_noop("Sets the shell command that will retrieve an archived WAL file."), NULL}, &recoveryRestoreCommand, "", NULL, NULL, NULL},

    {{"archive_cleanup_command", PGC_SIGHUP, WAL_ARCHIVE_RECOVERY, gettext_noop("Sets the shell command that will be executed at every restart point."), NULL}, &archiveCleanupCommand, "", NULL, NULL, NULL},

    {{"recovery_end_command", PGC_SIGHUP, WAL_ARCHIVE_RECOVERY, gettext_noop("Sets the shell command that will be executed once at the end of recovery."), NULL}, &recoveryEndCommand, "", NULL, NULL, NULL},

    {{"recovery_target_timeline", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Specifies the timeline to recover into."), NULL}, &recovery_target_timeline_string, "latest", check_recovery_target_timeline, assign_recovery_target_timeline, NULL},

    {{"recovery_target", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Set to \"immediate\" to end recovery as soon as a consistent state is reached."), NULL}, &recovery_target_string, "", check_recovery_target, assign_recovery_target, NULL}, {{"recovery_target_xid", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Sets the transaction ID up to which recovery will proceed."), NULL}, &recovery_target_xid_string, "", check_recovery_target_xid, assign_recovery_target_xid, NULL}, {{"recovery_target_time", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Sets the time stamp up to which recovery will proceed."), NULL}, &recovery_target_time_string, "", check_recovery_target_time, assign_recovery_target_time, NULL}, {{"recovery_target_name", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Sets the named restore point up to which recovery will proceed."), NULL}, &recovery_target_name_string, "", check_recovery_target_name, assign_recovery_target_name, NULL},
    {{"recovery_target_lsn", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Sets the LSN of the write-ahead log location up to which recovery will proceed."), NULL}, &recovery_target_lsn_string, "", check_recovery_target_lsn, assign_recovery_target_lsn, NULL},

    {{"promote_trigger_file", PGC_SIGHUP, REPLICATION_STANDBY, gettext_noop("Specifies a file name whose presence ends recovery in the standby."), NULL}, &PromoteTriggerFile, "", NULL, NULL, NULL},

    {{"primary_conninfo", PGC_POSTMASTER, REPLICATION_STANDBY, gettext_noop("Sets the connection string to be used to connect to the sending server."), NULL, GUC_SUPERUSER_ONLY}, &PrimaryConnInfo, "", NULL, NULL, NULL},

    {{"primary_slot_name", PGC_POSTMASTER, REPLICATION_STANDBY, gettext_noop("Sets the name of the replication slot to use on the sending server."), NULL}, &PrimarySlotName, "", check_primary_slot_name, NULL, NULL},

    {{"client_encoding", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the client's character set encoding."), NULL, GUC_IS_NAME | GUC_REPORT}, &client_encoding_string, "SQL_ASCII", check_client_encoding, assign_client_encoding, NULL},

    {{"log_line_prefix", PGC_SIGHUP, LOGGING_WHAT, gettext_noop("Controls information prefixed to each log line."), gettext_noop("If blank, no prefix is used.")}, &Log_line_prefix, "%m [%p] ", NULL, NULL, NULL},

    {{"log_timezone", PGC_SIGHUP, LOGGING_WHAT, gettext_noop("Sets the time zone to use in log messages."), NULL}, &log_timezone_string, "GMT", check_log_timezone, assign_log_timezone, show_log_timezone},

    {{"DateStyle", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the display format for date and time values."),
         gettext_noop("Also controls interpretation of ambiguous "
                      "date inputs."),
         GUC_LIST_INPUT | GUC_REPORT},
        &datestyle_string, "ISO, MDY", check_datestyle, assign_datestyle, NULL},

    {{"default_table_access_method", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the default table access method for new tables."), NULL, GUC_IS_NAME}, &default_table_access_method, DEFAULT_TABLE_ACCESS_METHOD, check_default_table_access_method, NULL, NULL},

    {{"default_tablespace", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the default tablespace to create tables and indexes in."), gettext_noop("An empty string selects the database's default tablespace."), GUC_IS_NAME}, &default_tablespace, "", check_default_tablespace, NULL, NULL},

    {{"temp_tablespaces", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the tablespace(s) to use for temporary tables and sort files."), NULL, GUC_LIST_INPUT | GUC_LIST_QUOTE}, &temp_tablespaces, "", check_temp_tablespaces, assign_temp_tablespaces, NULL},

    {{"dynamic_library_path", PGC_SUSET, CLIENT_CONN_OTHER, gettext_noop("Sets the path for dynamically loadable modules."),
         gettext_noop("If a dynamically loadable module needs to be opened and "
                      "the specified name does not have a directory component (i.e., the "
                      "name does not contain a slash), the system will search this path for "
                      "the specified file."),
         GUC_SUPERUSER_ONLY},
        &Dynamic_library_path, "$libdir", NULL, NULL, NULL},

    {{"krb_server_keyfile", PGC_SIGHUP, CONN_AUTH_AUTH, gettext_noop("Sets the location of the Kerberos server key file."), NULL, GUC_SUPERUSER_ONLY}, &pg_krb_server_keyfile, PG_KRB_SRVTAB, NULL, NULL, NULL},

    {{"bonjour_name", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the Bonjour service name."), NULL}, &bonjour_name, "", NULL, NULL, NULL},

                                                                    

    {{"lc_collate", PGC_INTERNAL, CLIENT_CONN_LOCALE, gettext_noop("Shows the collation order locale."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &locale_collate, "C", NULL, NULL, NULL},

    {{"lc_ctype", PGC_INTERNAL, CLIENT_CONN_LOCALE, gettext_noop("Shows the character classification and case conversion locale."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &locale_ctype, "C", NULL, NULL, NULL},

    {{"lc_messages", PGC_SUSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the language in which messages are displayed."), NULL}, &locale_messages, "", check_locale_messages, assign_locale_messages, NULL},

    {{"lc_monetary", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the locale for formatting monetary amounts."), NULL}, &locale_monetary, "C", check_locale_monetary, assign_locale_monetary, NULL},

    {{"lc_numeric", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the locale for formatting numbers."), NULL}, &locale_numeric, "C", check_locale_numeric, assign_locale_numeric, NULL},

    {{"lc_time", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the locale for formatting date and time values."), NULL}, &locale_time, "C", check_locale_time, assign_locale_time, NULL},

    {{"session_preload_libraries", PGC_SUSET, CLIENT_CONN_PRELOAD, gettext_noop("Lists shared libraries to preload into each backend."), NULL, GUC_LIST_INPUT | GUC_LIST_QUOTE | GUC_SUPERUSER_ONLY}, &session_preload_libraries_string, "", NULL, NULL, NULL},

    {{"shared_preload_libraries", PGC_POSTMASTER, CLIENT_CONN_PRELOAD, gettext_noop("Lists shared libraries to preload into server."), NULL, GUC_LIST_INPUT | GUC_LIST_QUOTE | GUC_SUPERUSER_ONLY}, &shared_preload_libraries_string, "", NULL, NULL, NULL},

    {{"local_preload_libraries", PGC_USERSET, CLIENT_CONN_PRELOAD, gettext_noop("Lists unprivileged shared libraries to preload into each backend."), NULL, GUC_LIST_INPUT | GUC_LIST_QUOTE}, &local_preload_libraries_string, "", NULL, NULL, NULL},

    {{"search_path", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the schema search order for names that are not schema-qualified."), NULL, GUC_LIST_INPUT | GUC_LIST_QUOTE | GUC_EXPLAIN}, &namespace_search_path, "\"$user\", public", check_search_path, assign_search_path, NULL},

    {                                     
        {"server_encoding", PGC_INTERNAL, CLIENT_CONN_LOCALE, gettext_noop("Sets the server (database) character set encoding."), NULL, GUC_IS_NAME | GUC_REPORT | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &server_encoding_string, "SQL_ASCII", NULL, NULL, NULL},

    {                                     
        {"server_version", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Shows the server version."), NULL, GUC_REPORT | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &server_version_string, PG_VERSION, NULL, NULL, NULL},

    {                                              
        {"role", PGC_USERSET, UNGROUPED, gettext_noop("Sets the current role."), NULL, GUC_IS_NAME | GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE | GUC_NOT_WHILE_SEC_REST}, &role_string, "none", check_role, assign_role, show_role},

    {                                                               
        {"session_authorization", PGC_USERSET, UNGROUPED, gettext_noop("Sets the session user name."), NULL, GUC_IS_NAME | GUC_REPORT | GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE | GUC_NOT_WHILE_SEC_REST}, &session_authorization_string, NULL, check_session_authorization, assign_session_authorization, NULL},

    {{"log_destination", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Sets the destination for server log output."),
         gettext_noop("Valid values are combinations of \"stderr\", "
                      "\"syslog\", \"csvlog\", and \"eventlog\", "
                      "depending on the platform."),
         GUC_LIST_INPUT},
        &Log_destination_string, "stderr", check_log_destination, assign_log_destination, NULL},
    {{"log_directory", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Sets the destination directory for log files."),
         gettext_noop("Can be specified as relative to the data directory "
                      "or as absolute path."),
         GUC_SUPERUSER_ONLY},
        &Log_directory, "log", check_canonical_path, NULL, NULL},
    {{"log_filename", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Sets the file name pattern for log files."), NULL, GUC_SUPERUSER_ONLY}, &Log_filename, "postgresql-%Y-%m-%d_%H%M%S.log", NULL, NULL, NULL},

    {{"syslog_ident", PGC_SIGHUP, LOGGING_WHERE,
         gettext_noop("Sets the program name used to identify PostgreSQL "
                      "messages in syslog."),
         NULL},
        &syslog_ident_str, "postgres", NULL, assign_syslog_ident, NULL},

    {{"event_source", PGC_POSTMASTER, LOGGING_WHERE,
         gettext_noop("Sets the application name used to identify "
                      "PostgreSQL messages in the event log."),
         NULL},
        &event_source, DEFAULT_EVENT_SOURCE, NULL, NULL, NULL},

    {{"TimeZone", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the time zone for displaying and interpreting time stamps."), NULL, GUC_REPORT}, &timezone_string, "GMT", check_timezone, assign_timezone, show_timezone}, {{"timezone_abbreviations", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Selects a file of time zone abbreviations."), NULL}, &timezone_abbreviations_string, NULL, check_timezone_abbreviations, assign_timezone_abbreviations, NULL},

    {{"unix_socket_group", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the owning group of the Unix-domain socket."),
         gettext_noop("The owning user of the socket is always the user "
                      "that starts the server.")},
        &Unix_socket_group, "", NULL, NULL, NULL},

    {{"unix_socket_directories", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the directories where Unix-domain sockets will be created."), NULL, GUC_SUPERUSER_ONLY}, &Unix_socket_directories,
#ifdef HAVE_UNIX_SOCKETS
        DEFAULT_PGSOCKET_DIR,
#else
        "",
#endif
        NULL, NULL, NULL},

    {{"listen_addresses", PGC_POSTMASTER, CONN_AUTH_SETTINGS, gettext_noop("Sets the host name or IP address(es) to listen to."), NULL, GUC_LIST_INPUT}, &ListenAddresses, "localhost", NULL, NULL, NULL},

    {   
                                                                            
                           
        
        {"data_directory", PGC_POSTMASTER, FILE_LOCATIONS, gettext_noop("Sets the server's data directory."), NULL, GUC_SUPERUSER_ONLY | GUC_DISALLOW_IN_AUTO_FILE}, &data_directory, NULL, NULL, NULL, NULL},

    {{"config_file", PGC_POSTMASTER, FILE_LOCATIONS, gettext_noop("Sets the server's main configuration file."), NULL, GUC_DISALLOW_IN_FILE | GUC_SUPERUSER_ONLY}, &ConfigFileName, NULL, NULL, NULL, NULL},

    {{"hba_file", PGC_POSTMASTER, FILE_LOCATIONS, gettext_noop("Sets the server's \"hba\" configuration file."), NULL, GUC_SUPERUSER_ONLY}, &HbaFileName, NULL, NULL, NULL, NULL},

    {{"ident_file", PGC_POSTMASTER, FILE_LOCATIONS, gettext_noop("Sets the server's \"ident\" configuration file."), NULL, GUC_SUPERUSER_ONLY}, &IdentFileName, NULL, NULL, NULL, NULL},

    {{"external_pid_file", PGC_POSTMASTER, FILE_LOCATIONS, gettext_noop("Writes the postmaster PID to the specified file."), NULL, GUC_SUPERUSER_ONLY}, &external_pid_file, NULL, check_canonical_path, NULL, NULL},

    {{"ssl_library", PGC_INTERNAL, PRESET_OPTIONS, gettext_noop("Name of the SSL library."), NULL, GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &ssl_library,
#ifdef USE_SSL
        "OpenSSL",
#else
        "",
#endif
        NULL, NULL, NULL},

    {{"ssl_cert_file", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Location of the SSL server certificate file."), NULL}, &ssl_cert_file, "server.crt", NULL, NULL, NULL},

    {{"ssl_key_file", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Location of the SSL server private key file."), NULL}, &ssl_key_file, "server.key", NULL, NULL, NULL},

    {{"ssl_ca_file", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Location of the SSL certificate authority file."), NULL}, &ssl_ca_file, "", NULL, NULL, NULL},

    {{"ssl_crl_file", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Location of the SSL certificate revocation list file."), NULL}, &ssl_crl_file, "", NULL, NULL, NULL},

    {{"stats_temp_directory", PGC_SIGHUP, STATS_COLLECTOR, gettext_noop("Writes temporary statistics files to the specified directory."), NULL, GUC_SUPERUSER_ONLY}, &pgstat_temp_directory, PG_STAT_TMP_DIR, check_canonical_path, assign_pgstat_temp_directory, NULL},

    {{"synchronous_standby_names", PGC_SIGHUP, REPLICATION_MASTER, gettext_noop("Number of synchronous standbys and list of names of potential synchronous ones."), NULL, GUC_LIST_INPUT}, &SyncRepStandbyNames, "", check_synchronous_standby_names, assign_synchronous_standby_names, NULL},

    {{"default_text_search_config", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets default text search configuration."), NULL}, &TSCurrentConfig, "pg_catalog.simple", check_TSCurrentConfig, assign_TSCurrentConfig, NULL},

    {{"ssl_ciphers", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Sets the list of allowed SSL ciphers."), NULL, GUC_SUPERUSER_ONLY}, &SSLCipherSuites,
#ifdef USE_SSL
        "HIGH:MEDIUM:+3DES:!aNULL",
#else
        "none",
#endif
        NULL, NULL, NULL},

    {{"ssl_ecdh_curve", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Sets the curve to use for ECDH."), NULL, GUC_SUPERUSER_ONLY}, &SSLECDHCurve,
#ifdef USE_SSL
        "prime256v1",
#else
        "none",
#endif
        NULL, NULL, NULL},

    {{"ssl_dh_params_file", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Location of the SSL DH parameters file."), NULL, GUC_SUPERUSER_ONLY}, &ssl_dh_params_file, "", NULL, NULL, NULL},

    {{"ssl_passphrase_command", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Command to obtain passphrases for SSL."), NULL}, &ssl_passphrase_command, "", NULL, NULL, NULL},

    {{"application_name", PGC_USERSET, LOGGING_WHAT, gettext_noop("Sets the application name to be reported in statistics and logs."), NULL, GUC_IS_NAME | GUC_REPORT | GUC_NOT_IN_SAMPLE}, &application_name, "", check_application_name, assign_application_name, NULL},

    {{"cluster_name", PGC_POSTMASTER, PROCESS_TITLE, gettext_noop("Sets the name of the cluster, which is included in the process title."), NULL, GUC_IS_NAME}, &cluster_name, "", check_cluster_name, NULL, NULL},

    {{"wal_consistency_checking", PGC_SUSET, DEVELOPER_OPTIONS, gettext_noop("Sets the WAL resource managers for which WAL consistency checks are done."), gettext_noop("Full-page images will be logged for all data blocks and cross-checked against the results of WAL replay."), GUC_LIST_INPUT | GUC_NOT_IN_SAMPLE}, &wal_consistency_checking_string, "", check_wal_consistency_checking, assign_wal_consistency_checking, NULL},

    {{"jit_provider", PGC_POSTMASTER, CLIENT_CONN_PRELOAD, gettext_noop("JIT provider to use."), NULL, GUC_SUPERUSER_ONLY}, &jit_provider, "llvmjit", NULL, NULL, NULL},

                            
    {{NULL, 0, 0, NULL, NULL}, NULL, NULL, NULL, NULL, NULL}};

static struct config_enum ConfigureNamesEnum[] = {{{"backslash_quote", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS, gettext_noop("Sets whether \"\\'\" is allowed in string literals."), NULL}, &backslash_quote, BACKSLASH_QUOTE_SAFE_ENCODING, backslash_quote_options, NULL, NULL, NULL},

    {{"bytea_output", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the output format for bytea."), NULL}, &bytea_output, BYTEA_OUTPUT_HEX, bytea_output_options, NULL, NULL, NULL},

    {{"client_min_messages", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the message levels that are sent to the client."),
         gettext_noop("Each level includes all the levels that follow it. The later"
                      " the level, the fewer messages are sent.")},
        &client_min_messages, NOTICE, client_message_level_options, NULL, NULL, NULL},

    {{"constraint_exclusion", PGC_USERSET, QUERY_TUNING_OTHER, gettext_noop("Enables the planner to use constraints to optimize queries."),
         gettext_noop("Table scans will be skipped if their constraints"
                      " guarantee that no rows match the query."),
         GUC_EXPLAIN},
        &constraint_exclusion, CONSTRAINT_EXCLUSION_PARTITION, constraint_exclusion_options, NULL, NULL, NULL},

    {{"default_transaction_isolation", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the transaction isolation level of each new transaction."), NULL}, &DefaultXactIsoLevel, XACT_READ_COMMITTED, isolation_level_options, NULL, NULL, NULL},

    {{"transaction_isolation", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the current transaction's isolation level."), NULL, GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE}, &XactIsoLevel, XACT_READ_COMMITTED, isolation_level_options, check_XactIsoLevel, NULL, NULL},

    {{"IntervalStyle", PGC_USERSET, CLIENT_CONN_LOCALE, gettext_noop("Sets the display format for interval values."), NULL, GUC_REPORT}, &IntervalStyle, INTSTYLE_POSTGRES, intervalstyle_options, NULL, NULL, NULL},

    {{"log_error_verbosity", PGC_SUSET, LOGGING_WHAT, gettext_noop("Sets the verbosity of logged messages."), NULL}, &Log_error_verbosity, PGERROR_DEFAULT, log_error_verbosity_options, NULL, NULL, NULL},

    {{"log_min_messages", PGC_SUSET, LOGGING_WHEN, gettext_noop("Sets the message levels that are logged."),
         gettext_noop("Each level includes all the levels that follow it. The later"
                      " the level, the fewer messages are sent.")},
        &log_min_messages, WARNING, server_message_level_options, NULL, NULL, NULL},

    {{"log_min_error_statement", PGC_SUSET, LOGGING_WHEN, gettext_noop("Causes all statements generating error at or above this level to be logged."),
         gettext_noop("Each level includes all the levels that follow it. The later"
                      " the level, the fewer messages are sent.")},
        &log_min_error_statement, ERROR, server_message_level_options, NULL, NULL, NULL},

    {{"log_statement", PGC_SUSET, LOGGING_WHAT, gettext_noop("Sets the type of statements logged."), NULL}, &log_statement, LOGSTMT_NONE, log_statement_options, NULL, NULL, NULL},

    {{"syslog_facility", PGC_SIGHUP, LOGGING_WHERE, gettext_noop("Sets the syslog \"facility\" to be used when syslog enabled."), NULL}, &syslog_facility,
#ifdef HAVE_SYSLOG
        LOG_LOCAL0,
#else
        0,
#endif
        syslog_facility_options, NULL, assign_syslog_facility, NULL},

    {{"session_replication_role", PGC_SUSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets the session's behavior for triggers and rewrite rules."), NULL}, &SessionReplicationRole, SESSION_REPLICATION_ROLE_ORIGIN, session_replication_role_options, NULL, assign_session_replication_role, NULL},

    {{"synchronous_commit", PGC_USERSET, WAL_SETTINGS, gettext_noop("Sets the current transaction's synchronization level."), NULL}, &synchronous_commit, SYNCHRONOUS_COMMIT_ON, synchronous_commit_options, NULL, assign_synchronous_commit, NULL},

    {{"archive_mode", PGC_POSTMASTER, WAL_ARCHIVING, gettext_noop("Allows archiving of WAL files using archive_command."), NULL}, &XLogArchiveMode, ARCHIVE_MODE_OFF, archive_mode_options, NULL, NULL, NULL},

    {{"recovery_target_action", PGC_POSTMASTER, WAL_RECOVERY_TARGET, gettext_noop("Sets the action to perform upon reaching the recovery target."), NULL}, &recoveryTargetAction, RECOVERY_TARGET_ACTION_PAUSE, recovery_target_action_options, NULL, NULL, NULL},

    {{"trace_recovery_messages", PGC_SIGHUP, DEVELOPER_OPTIONS, gettext_noop("Enables logging of recovery-related debugging information."),
         gettext_noop("Each level includes all the levels that follow it. The later"
                      " the level, the fewer messages are sent.")},
        &trace_recovery_messages,

           
                                                                            
                                                                    
           
        LOG, client_message_level_options, NULL, NULL, NULL},

    {{"track_functions", PGC_SUSET, STATS_COLLECTOR, gettext_noop("Collects function-level statistics on database activity."), NULL}, &pgstat_track_functions, TRACK_FUNC_OFF, track_function_options, NULL, NULL, NULL},

    {{"wal_level", PGC_POSTMASTER, WAL_SETTINGS, gettext_noop("Set the level of information written to the WAL."), NULL}, &wal_level, WAL_LEVEL_REPLICA, wal_level_options, NULL, NULL, NULL},

    {{"dynamic_shared_memory_type", PGC_POSTMASTER, RESOURCES_MEM, gettext_noop("Selects the dynamic shared memory implementation used."), NULL}, &dynamic_shared_memory_type, DEFAULT_DYNAMIC_SHARED_MEMORY_TYPE, dynamic_shared_memory_options, NULL, NULL, NULL},

    {{"shared_memory_type", PGC_POSTMASTER, RESOURCES_MEM, gettext_noop("Selects the shared memory implementation used for the main shared memory region."), NULL}, &shared_memory_type, DEFAULT_SHARED_MEMORY_TYPE, shared_memory_options, NULL, NULL, NULL},

    {{"wal_sync_method", PGC_SIGHUP, WAL_SETTINGS, gettext_noop("Selects the method used for forcing WAL updates to disk."), NULL}, &sync_method, DEFAULT_SYNC_METHOD, sync_method_options, NULL, assign_xlog_sync_method, NULL},

    {{"xmlbinary", PGC_USERSET, CLIENT_CONN_STATEMENT, gettext_noop("Sets how binary values are to be encoded in XML."), NULL}, &xmlbinary, XMLBINARY_BASE64, xmlbinary_options, NULL, NULL, NULL},

    {{"xmloption", PGC_USERSET, CLIENT_CONN_STATEMENT,
         gettext_noop("Sets whether XML data in implicit parsing and serialization "
                      "operations is to be considered as documents or content fragments."),
         NULL},
        &xmloption, XMLOPTION_CONTENT, xmloption_options, NULL, NULL, NULL},

    {{"huge_pages", PGC_POSTMASTER, RESOURCES_MEM, gettext_noop("Use of huge pages on Linux or Windows."), NULL}, &huge_pages, HUGE_PAGES_TRY, huge_pages_options, NULL, NULL, NULL},

    {{"force_parallel_mode", PGC_USERSET, QUERY_TUNING_OTHER, gettext_noop("Forces use of parallel query facilities."), gettext_noop("If possible, run query using a parallel worker and with parallel restrictions."), GUC_EXPLAIN}, &force_parallel_mode, FORCE_PARALLEL_OFF, force_parallel_mode_options, NULL, NULL, NULL},

    {{"password_encryption", PGC_USERSET, CONN_AUTH_AUTH, gettext_noop("Chooses the algorithm for encrypting passwords."), NULL}, &Password_encryption, PASSWORD_TYPE_MD5, password_encryption_options, NULL, NULL, NULL},

    {{"plan_cache_mode", PGC_USERSET, QUERY_TUNING_OTHER, gettext_noop("Controls the planner's selection of custom or generic plan."),
         gettext_noop("Prepared statements can have custom and generic plans, and the planner "
                      "will attempt to choose which is better.  This can be set to override "
                      "the default behavior."),
         GUC_EXPLAIN},
        &plan_cache_mode, PLAN_CACHE_MODE_AUTO, plan_cache_mode_options, NULL, NULL, NULL},

    {{"ssl_min_protocol_version", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Sets the minimum SSL/TLS protocol version to use."), NULL, GUC_SUPERUSER_ONLY}, &ssl_min_protocol_version, PG_TLS1_VERSION, ssl_protocol_versions_info + 1,                             
        NULL, NULL, NULL},

    {{"ssl_max_protocol_version", PGC_SIGHUP, CONN_AUTH_SSL, gettext_noop("Sets the maximum SSL/TLS protocol version to use."), NULL, GUC_SUPERUSER_ONLY}, &ssl_max_protocol_version, PG_TLS_ANY, ssl_protocol_versions_info, NULL, NULL, NULL},

                            
    {{NULL, 0, 0, NULL, NULL}, NULL, 0, NULL, NULL, NULL, NULL}};

                                       

   
                                                                            
                                                                           
                                                                           
                         
   
static const char *const map_old_guc_names[] = {"sort_mem", "work_mem", "vacuum_mem", "maintenance_work_mem", NULL};

   
                                                                         
   
static struct config_generic **guc_variables;

                                                         
static int num_guc_variables;

                     
static int size_guc_variables;

static bool guc_dirty;                                           

static bool reporting_enabled;                                

static int GUCNestLevel = 0;                                 

static int
guc_var_compare(const void *a, const void *b);
static int
guc_name_compare(const char *namea, const char *nameb);
static void
InitializeGUCOptionsFromEnvironment(void);
static void
InitializeOneGUCOption(struct config_generic *gconf);
static void
push_old_value(struct config_generic *gconf, GucAction action);
static void
ReportGUCOption(struct config_generic *record);
static void
reapply_stacked_values(struct config_generic *variable, struct config_string *pHolder, GucStack *stack, const char *curvalue, GucContext curscontext, GucSource cursource);
static void
ShowGUCConfigOption(const char *name, DestReceiver *dest);
static void
ShowAllGUCConfig(DestReceiver *dest);
static char *
_ShowOption(struct config_generic *record, bool use_units);
static bool
validate_option_array_item(const char *name, const char *value, bool skipIfNoPermissions);
static void
write_auto_conf_file(int fd, const char *filename, ConfigVariable *head_p);
static void
replace_auto_config_value(ConfigVariable **head_p, ConfigVariable **tail_p, const char *name, const char *value);

   
                                                                
   
static void *
guc_malloc(int elevel, size_t size)
{
  void *data;

                                              
  if (size == 0)
  {
    size = 1;
  }
  data = malloc(size);
  if (data == NULL)
  {
    ereport(elevel, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }
  return data;
}

static void *
guc_realloc(int elevel, void *old, size_t size)
{
  void *data;

                                                     
  if (old == NULL && size == 0)
  {
    size = 1;
  }
  data = realloc(old, size);
  if (data == NULL)
  {
    ereport(elevel, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }
  return data;
}

static char *
guc_strdup(int elevel, const char *src)
{
  char *data;

  data = strdup(src);
  if (data == NULL)
  {
    ereport(elevel, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }
  return data;
}

   
                                                                     
   
static bool
string_field_used(struct config_string *conf, char *strval)
{
  GucStack *stack;

  if (strval == *(conf->variable) || strval == conf->reset_val || strval == conf->boot_val)
  {
    return true;
  }
  for (stack = conf->gen.stack; stack; stack = stack->prev)
  {
    if (strval == stack->prior.val.stringval || strval == stack->masked.val.stringval)
    {
      return true;
    }
  }
  return false;
}

   
                                                                          
                                                                             
            
   
static void
set_string_field(struct config_string *conf, char **field, char *newval)
{
  char *oldval = *field;

                         
  *field = newval;

                                                                    
  if (oldval && !string_field_used(conf, oldval))
  {
    free(oldval);
  }
}

   
                                                                         
   
static bool
extra_field_used(struct config_generic *gconf, void *extra)
{
  GucStack *stack;

  if (extra == gconf->extra)
  {
    return true;
  }
  switch (gconf->vartype)
  {
  case PGC_BOOL:
    if (extra == ((struct config_bool *)gconf)->reset_extra)
    {
      return true;
    }
    break;
  case PGC_INT:
    if (extra == ((struct config_int *)gconf)->reset_extra)
    {
      return true;
    }
    break;
  case PGC_REAL:
    if (extra == ((struct config_real *)gconf)->reset_extra)
    {
      return true;
    }
    break;
  case PGC_STRING:
    if (extra == ((struct config_string *)gconf)->reset_extra)
    {
      return true;
    }
    break;
  case PGC_ENUM:
    if (extra == ((struct config_enum *)gconf)->reset_extra)
    {
      return true;
    }
    break;
  }
  for (stack = gconf->stack; stack; stack = stack->prev)
  {
    if (extra == stack->prior.extra || extra == stack->masked.extra)
    {
      return true;
    }
  }

  return false;
}

   
                                                                            
                                                                             
            
   
static void
set_extra_field(struct config_generic *gconf, void **field, void *newval)
{
  void *oldval = *field;

                         
  *field = newval;

                                                                    
  if (oldval && !extra_field_used(gconf, oldval))
  {
    free(oldval);
  }
}

   
                                                                     
                                                                      
   
                                                                   
                                                                           
   
static void
set_stack_value(struct config_generic *gconf, config_var_value *val)
{
  switch (gconf->vartype)
  {
  case PGC_BOOL:
    val->val.boolval = *((struct config_bool *)gconf)->variable;
    break;
  case PGC_INT:
    val->val.intval = *((struct config_int *)gconf)->variable;
    break;
  case PGC_REAL:
    val->val.realval = *((struct config_real *)gconf)->variable;
    break;
  case PGC_STRING:
    set_string_field((struct config_string *)gconf, &(val->val.stringval), *((struct config_string *)gconf)->variable);
    break;
  case PGC_ENUM:
    val->val.enumval = *((struct config_enum *)gconf)->variable;
    break;
  }
  set_extra_field(gconf, &(val->extra), gconf->extra);
}

   
                                                                     
                                                                      
   
static void
discard_stack_value(struct config_generic *gconf, config_var_value *val)
{
  switch (gconf->vartype)
  {
  case PGC_BOOL:
  case PGC_INT:
  case PGC_REAL:
  case PGC_ENUM:
                                
    break;
  case PGC_STRING:
    set_string_field((struct config_string *)gconf, &(val->val.stringval), NULL);
    break;
  }
  set_extra_field(gconf, &(val->extra), NULL);
}

   
                                                                          
   
struct config_generic **
get_guc_variables(void)
{
  return guc_variables;
}

   
                                                                  
                                                                       
                                             
   
void
build_guc_variables(void)
{
  int size_vars;
  int num_vars = 0;
  struct config_generic **guc_vars;
  int i;

  for (i = 0; ConfigureNamesBool[i].gen.name; i++)
  {
    struct config_bool *conf = &ConfigureNamesBool[i];

                                                                         
    conf->gen.vartype = PGC_BOOL;
    num_vars++;
  }

  for (i = 0; ConfigureNamesInt[i].gen.name; i++)
  {
    struct config_int *conf = &ConfigureNamesInt[i];

    conf->gen.vartype = PGC_INT;
    num_vars++;
  }

  for (i = 0; ConfigureNamesReal[i].gen.name; i++)
  {
    struct config_real *conf = &ConfigureNamesReal[i];

    conf->gen.vartype = PGC_REAL;
    num_vars++;
  }

  for (i = 0; ConfigureNamesString[i].gen.name; i++)
  {
    struct config_string *conf = &ConfigureNamesString[i];

    conf->gen.vartype = PGC_STRING;
    num_vars++;
  }

  for (i = 0; ConfigureNamesEnum[i].gen.name; i++)
  {
    struct config_enum *conf = &ConfigureNamesEnum[i];

    conf->gen.vartype = PGC_ENUM;
    num_vars++;
  }

     
                                 
     
  size_vars = num_vars + num_vars / 4;

  guc_vars = (struct config_generic **)guc_malloc(FATAL, size_vars * sizeof(struct config_generic *));

  num_vars = 0;

  for (i = 0; ConfigureNamesBool[i].gen.name; i++)
  {
    guc_vars[num_vars++] = &ConfigureNamesBool[i].gen;
  }

  for (i = 0; ConfigureNamesInt[i].gen.name; i++)
  {
    guc_vars[num_vars++] = &ConfigureNamesInt[i].gen;
  }

  for (i = 0; ConfigureNamesReal[i].gen.name; i++)
  {
    guc_vars[num_vars++] = &ConfigureNamesReal[i].gen;
  }

  for (i = 0; ConfigureNamesString[i].gen.name; i++)
  {
    guc_vars[num_vars++] = &ConfigureNamesString[i].gen;
  }

  for (i = 0; ConfigureNamesEnum[i].gen.name; i++)
  {
    guc_vars[num_vars++] = &ConfigureNamesEnum[i].gen;
  }

  if (guc_variables)
  {
    free(guc_variables);
  }
  guc_variables = guc_vars;
  num_guc_variables = num_vars;
  size_guc_variables = size_vars;
  qsort((void *)guc_variables, num_guc_variables, sizeof(struct config_generic *), guc_var_compare);
}

   
                                                              
                               
   
static bool
add_guc_variable(struct config_generic *var, int elevel)
{
  if (num_guc_variables + 1 >= size_guc_variables)
  {
       
                                  
       
    int size_vars = size_guc_variables + size_guc_variables / 4;
    struct config_generic **guc_vars;

    if (size_vars == 0)
    {
      size_vars = 100;
      guc_vars = (struct config_generic **)guc_malloc(elevel, size_vars * sizeof(struct config_generic *));
    }
    else
    {
      guc_vars = (struct config_generic **)guc_realloc(elevel, guc_variables, size_vars * sizeof(struct config_generic *));
    }

    if (guc_vars == NULL)
    {
      return false;                    
    }

    guc_variables = guc_vars;
    size_guc_variables = size_vars;
  }
  guc_variables[num_guc_variables++] = var;
  qsort((void *)guc_variables, num_guc_variables, sizeof(struct config_generic *), guc_var_compare);
  return true;
}

   
                                                                     
   
static struct config_generic *
add_placeholder_variable(const char *name, int elevel)
{
  size_t sz = sizeof(struct config_string) + sizeof(char *);
  struct config_string *var;
  struct config_generic *gen;

  var = (struct config_string *)guc_malloc(elevel, sz);
  if (var == NULL)
  {
    return NULL;
  }
  memset(var, 0, sz);
  gen = &var->gen;

  gen->name = guc_strdup(elevel, name);
  if (gen->name == NULL)
  {
    free(var);
    return NULL;
  }

  gen->context = PGC_USERSET;
  gen->group = CUSTOM_OPTIONS;
  gen->short_desc = "GUC placeholder variable";
  gen->flags = GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_CUSTOM_PLACEHOLDER;
  gen->vartype = PGC_STRING;

     
                                                                      
                                                                          
                                                
     
  var->variable = (char **)(var + 1);

  if (!add_guc_variable((struct config_generic *)var, elevel))
  {
    free(unconstify(char *, gen->name));
    free(var);
    return NULL;
  }

  return gen;
}

   
                                                                       
                                                                     
                                                                
   
static struct config_generic *
find_option(const char *name, bool create_placeholders, int elevel)
{
  const char **key = &name;
  struct config_generic **res;
  int i;

  Assert(name);

     
                                                                             
                                                
     
  res = (struct config_generic **)bsearch((void *)&key, (void *)guc_variables, num_guc_variables, sizeof(struct config_generic *), guc_var_compare);
  if (res)
  {
    return *res;
  }

     
                                                                             
                                                                             
                   
     
  for (i = 0; map_old_guc_names[i] != NULL; i += 2)
  {
    if (guc_name_compare(name, map_old_guc_names[i]) == 0)
    {
      return find_option(map_old_guc_names[i + 1], false, elevel);
    }
  }

  if (create_placeholders)
  {
       
                                                                     
       
    if (strchr(name, GUC_QUALIFIER_SEPARATOR) != NULL)
    {
      return add_placeholder_variable(name, elevel);
    }
  }

                    
  return NULL;
}

   
                                                              
   
static int
guc_var_compare(const void *a, const void *b)
{
  const struct config_generic *confa = *(struct config_generic *const *)a;
  const struct config_generic *confb = *(struct config_generic *const *)b;

  return guc_name_compare(confa->name, confb->name);
}

   
                                              
   
static int
guc_name_compare(const char *namea, const char *nameb)
{
     
                                                                           
                                                                             
                                                  
     
  while (*namea && *nameb)
  {
    char cha = *namea++;
    char chb = *nameb++;

    if (cha >= 'A' && cha <= 'Z')
    {
      cha += 'a' - 'A';
    }
    if (chb >= 'A' && chb <= 'Z')
    {
      chb += 'a' - 'A';
    }
    if (cha != chb)
    {
      return cha - chb;
    }
  }
  if (*namea)
  {
    return 1;                  
  }
  if (*nameb)
  {
    return -1;                  
  }
  return 0;
}

   
                                                  
   
                                                                       
                                    
   
void
InitializeGUCOptions(void)
{
  int i;

     
                                                                            
                                                                    
     
  pg_timezone_initialize();

     
                                              
     
  build_guc_variables();

     
                                                                        
                              
     
  for (i = 0; i < num_guc_variables; i++)
  {
    InitializeOneGUCOption(guc_variables[i]);
  }

  guc_dirty = false;

  reporting_enabled = false;

     
                                                                
                              
     
  SetConfigOption("transaction_isolation", "read committed", PGC_POSTMASTER, PGC_S_OVERRIDE);
  SetConfigOption("transaction_read_only", "no", PGC_POSTMASTER, PGC_S_OVERRIDE);
  SetConfigOption("transaction_deferrable", "no", PGC_POSTMASTER, PGC_S_OVERRIDE);

     
                                                                           
                                                     
     
  InitializeGUCOptionsFromEnvironment();
}

   
                                                                      
   
                                                                             
                                                                     
                                                                    
                                                                        
                                              
   
static void
InitializeGUCOptionsFromEnvironment(void)
{
  char *env;
  long stack_rlimit;

  env = getenv("PGPORT");
  if (env != NULL)
  {
    SetConfigOption("port", env, PGC_POSTMASTER, PGC_S_ENV_VAR);
  }

  env = getenv("PGDATESTYLE");
  if (env != NULL)
  {
    SetConfigOption("datestyle", env, PGC_POSTMASTER, PGC_S_ENV_VAR);
  }

  env = getenv("PGCLIENTENCODING");
  if (env != NULL)
  {
    SetConfigOption("client_encoding", env, PGC_POSTMASTER, PGC_S_ENV_VAR);
  }

     
                                                                          
                                                                             
                                                                           
     
  stack_rlimit = get_stack_depth_rlimit();
  if (stack_rlimit > 0)
  {
    long new_limit = (stack_rlimit - STACK_DEPTH_SLOP) / 1024L;

    if (new_limit > 100)
    {
      char limbuf[16];

      new_limit = Min(new_limit, 2048);
      sprintf(limbuf, "%ld", new_limit);
      SetConfigOption("max_stack_depth", limbuf, PGC_POSTMASTER, PGC_S_ENV_VAR);
    }
  }
}

   
                                                                  
   
                                                                              
                                                                           
   
static void
InitializeOneGUCOption(struct config_generic *gconf)
{
  gconf->status = 0;
  gconf->source = PGC_S_DEFAULT;
  gconf->reset_source = PGC_S_DEFAULT;
  gconf->scontext = PGC_INTERNAL;
  gconf->reset_scontext = PGC_INTERNAL;
  gconf->stack = NULL;
  gconf->extra = NULL;
  gconf->sourcefile = NULL;
  gconf->sourceline = 0;

  switch (gconf->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *conf = (struct config_bool *)gconf;
    bool newval = conf->boot_val;
    void *extra = NULL;

    if (!call_bool_check_hook(conf, &newval, &extra, PGC_S_DEFAULT, LOG))
    {
      elog(FATAL, "failed to initialize %s to %d", conf->gen.name, (int)newval);
    }
    if (conf->assign_hook)
    {
      conf->assign_hook(newval, extra);
    }
    *conf->variable = conf->reset_val = newval;
    conf->gen.extra = conf->reset_extra = extra;
    break;
  }
  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)gconf;
    int newval = conf->boot_val;
    void *extra = NULL;

    Assert(newval >= conf->min);
    Assert(newval <= conf->max);
    if (!call_int_check_hook(conf, &newval, &extra, PGC_S_DEFAULT, LOG))
    {
      elog(FATAL, "failed to initialize %s to %d", conf->gen.name, newval);
    }
    if (conf->assign_hook)
    {
      conf->assign_hook(newval, extra);
    }
    *conf->variable = conf->reset_val = newval;
    conf->gen.extra = conf->reset_extra = extra;
    break;
  }
  case PGC_REAL:
  {
    struct config_real *conf = (struct config_real *)gconf;
    double newval = conf->boot_val;
    void *extra = NULL;

    Assert(newval >= conf->min);
    Assert(newval <= conf->max);
    if (!call_real_check_hook(conf, &newval, &extra, PGC_S_DEFAULT, LOG))
    {
      elog(FATAL, "failed to initialize %s to %g", conf->gen.name, newval);
    }
    if (conf->assign_hook)
    {
      conf->assign_hook(newval, extra);
    }
    *conf->variable = conf->reset_val = newval;
    conf->gen.extra = conf->reset_extra = extra;
    break;
  }
  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)gconf;
    char *newval;
    void *extra = NULL;

                                                    
    if (conf->boot_val != NULL)
    {
      newval = guc_strdup(FATAL, conf->boot_val);
    }
    else
    {
      newval = NULL;
    }

    if (!call_string_check_hook(conf, &newval, &extra, PGC_S_DEFAULT, LOG))
    {
      elog(FATAL, "failed to initialize %s to \"%s\"", conf->gen.name, newval ? newval : "");
    }
    if (conf->assign_hook)
    {
      conf->assign_hook(newval, extra);
    }
    *conf->variable = conf->reset_val = newval;
    conf->gen.extra = conf->reset_extra = extra;
    break;
  }
  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)gconf;
    int newval = conf->boot_val;
    void *extra = NULL;

    if (!call_enum_check_hook(conf, &newval, &extra, PGC_S_DEFAULT, LOG))
    {
      elog(FATAL, "failed to initialize %s to %d", conf->gen.name, newval);
    }
    if (conf->assign_hook)
    {
      conf->assign_hook(newval, extra);
    }
    *conf->variable = conf->reset_val = newval;
    conf->gen.extra = conf->reset_extra = extra;
    break;
  }
  }
}

   
                                                                     
                                           
   
                                                          
                                                                     
                                                
   
                                                                        
                                
   
bool
SelectConfigFiles(const char *userDoption, const char *progname)
{
  char *configdir;
  char *fname;
  struct stat stat_buf;

                                                   
  if (userDoption)
  {
    configdir = make_absolute_path(userDoption);
  }
  else
  {
    configdir = make_absolute_path(getenv("PGDATA"));
  }

  if (configdir && stat(configdir, &stat_buf) != 0)
  {
    write_stderr("%s: could not access directory \"%s\": %s\n", progname, configdir, strerror(errno));
    if (errno == ENOENT)
    {
      write_stderr("Run initdb or pg_basebackup to initialize a PostgreSQL data directory.\n");
    }
    return false;
  }

     
                                                                      
                                                                            
                                                                           
                                      
     
  if (ConfigFileName)
  {
    fname = make_absolute_path(ConfigFileName);
  }
  else if (configdir)
  {
    fname = guc_malloc(FATAL, strlen(configdir) + strlen(CONFIG_FILENAME) + 2);
    sprintf(fname, "%s/%s", configdir, CONFIG_FILENAME);
  }
  else
  {
    write_stderr("%s does not know where to find the server configuration file.\n"
                 "You must specify the --config-file or -D invocation "
                 "option or set the PGDATA environment variable.\n",
        progname);
    return false;
  }

     
                                                                           
                                   
     
  SetConfigOption("config_file", fname, PGC_POSTMASTER, PGC_S_OVERRIDE);
  free(fname);

     
                                                  
     
  if (stat(ConfigFileName, &stat_buf) != 0)
  {
    write_stderr("%s: could not access the server configuration file \"%s\": %s\n", progname, ConfigFileName, strerror(errno));
    free(configdir);
    return false;
  }

     
                                                                         
                                                                            
                                                                  
     
  ProcessConfigFile(PGC_POSTMASTER);

     
                                                                           
                                                
     
                                                                           
              
     
  if (data_directory)
  {
    SetDataDir(data_directory);
  }
  else if (configdir)
  {
    SetDataDir(configdir);
  }
  else
  {
    write_stderr("%s does not know where to find the database system data.\n"
                 "This can be specified as \"data_directory\" in \"%s\", "
                 "or by the -D invocation option, or by the "
                 "PGDATA environment variable.\n",
        progname, ConfigFileName);
    return false;
  }

     
                                                                           
                                                                          
                                                                           
                                                                           
                                                                             
                          
     
  SetConfigOption("data_directory", DataDir, PGC_POSTMASTER, PGC_S_OVERRIDE);

     
                                                                          
                                                                          
                                                                            
                                         
     
  ProcessConfigFile(PGC_POSTMASTER);

     
                                                                             
                                                                             
                                                                         
                                                                           
                                  
     
  pg_timezone_abbrev_initialize();

     
                                                                          
     
  if (HbaFileName)
  {
    fname = make_absolute_path(HbaFileName);
  }
  else if (configdir)
  {
    fname = guc_malloc(FATAL, strlen(configdir) + strlen(HBA_FILENAME) + 2);
    sprintf(fname, "%s/%s", configdir, HBA_FILENAME);
  }
  else
  {
    write_stderr("%s does not know where to find the \"hba\" configuration file.\n"
                 "This can be specified as \"hba_file\" in \"%s\", "
                 "or by the -D invocation option, or by the "
                 "PGDATA environment variable.\n",
        progname, ConfigFileName);
    return false;
  }
  SetConfigOption("hba_file", fname, PGC_POSTMASTER, PGC_S_OVERRIDE);
  free(fname);

     
                                 
     
  if (IdentFileName)
  {
    fname = make_absolute_path(IdentFileName);
  }
  else if (configdir)
  {
    fname = guc_malloc(FATAL, strlen(configdir) + strlen(IDENT_FILENAME) + 2);
    sprintf(fname, "%s/%s", configdir, IDENT_FILENAME);
  }
  else
  {
    write_stderr("%s does not know where to find the \"ident\" configuration file.\n"
                 "This can be specified as \"ident_file\" in \"%s\", "
                 "or by the -D invocation option, or by the "
                 "PGDATA environment variable.\n",
        progname, ConfigFileName);
    return false;
  }
  SetConfigOption("ident_file", fname, PGC_POSTMASTER, PGC_S_OVERRIDE);
  free(fname);

  free(configdir);

  return true;
}

   
                                                                          
   
void
ResetAllOptions(void)
{
  int i;

  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *gconf = guc_variables[i];

                                         
    if (gconf->context != PGC_SUSET && gconf->context != PGC_USERSET)
    {
      continue;
    }
                                                         
    if (gconf->flags & GUC_NO_RESET_ALL)
    {
      continue;
    }
                                        
    if (gconf->source <= PGC_S_OVERRIDE)
    {
      continue;
    }

                                                     
    push_old_value(gconf, GUC_ACTION_SET);

    switch (gconf->vartype)
    {
    case PGC_BOOL:
    {
      struct config_bool *conf = (struct config_bool *)gconf;

      if (conf->assign_hook)
      {
        conf->assign_hook(conf->reset_val, conf->reset_extra);
      }
      *conf->variable = conf->reset_val;
      set_extra_field(&conf->gen, &conf->gen.extra, conf->reset_extra);
      break;
    }
    case PGC_INT:
    {
      struct config_int *conf = (struct config_int *)gconf;

      if (conf->assign_hook)
      {
        conf->assign_hook(conf->reset_val, conf->reset_extra);
      }
      *conf->variable = conf->reset_val;
      set_extra_field(&conf->gen, &conf->gen.extra, conf->reset_extra);
      break;
    }
    case PGC_REAL:
    {
      struct config_real *conf = (struct config_real *)gconf;

      if (conf->assign_hook)
      {
        conf->assign_hook(conf->reset_val, conf->reset_extra);
      }
      *conf->variable = conf->reset_val;
      set_extra_field(&conf->gen, &conf->gen.extra, conf->reset_extra);
      break;
    }
    case PGC_STRING:
    {
      struct config_string *conf = (struct config_string *)gconf;

      if (conf->assign_hook)
      {
        conf->assign_hook(conf->reset_val, conf->reset_extra);
      }
      set_string_field(conf, conf->variable, conf->reset_val);
      set_extra_field(&conf->gen, &conf->gen.extra, conf->reset_extra);
      break;
    }
    case PGC_ENUM:
    {
      struct config_enum *conf = (struct config_enum *)gconf;

      if (conf->assign_hook)
      {
        conf->assign_hook(conf->reset_val, conf->reset_extra);
      }
      *conf->variable = conf->reset_val;
      set_extra_field(&conf->gen, &conf->gen.extra, conf->reset_extra);
      break;
    }
    }

    gconf->source = gconf->reset_source;
    gconf->scontext = gconf->reset_scontext;

    if (gconf->flags & GUC_REPORT)
    {
      ReportGUCOption(gconf);
    }
  }
}

   
                  
                                                                           
   
static void
push_old_value(struct config_generic *gconf, GucAction action)
{
  GucStack *stack;

                                                    
  if (GUCNestLevel == 0)
  {
    return;
  }

                                                                   
  stack = gconf->stack;
  if (stack && stack->nest_level >= GUCNestLevel)
  {
                                               
    Assert(stack->nest_level == GUCNestLevel);
    switch (action)
    {
    case GUC_ACTION_SET:
                                                             
      if (stack->state == GUC_SET_LOCAL)
      {
                                           
        discard_stack_value(gconf, &stack->masked);
      }
      stack->state = GUC_SET;
      break;
    case GUC_ACTION_LOCAL:
      if (stack->state == GUC_SET)
      {
                                                             
        stack->masked_scontext = gconf->scontext;
        set_stack_value(gconf, &stack->masked);
        stack->state = GUC_SET_LOCAL;
      }
                                                        
      break;
    case GUC_ACTION_SAVE:
                                                         
      Assert(stack->state == GUC_SAVE);
      break;
    }
    Assert(guc_dirty);                          
    return;
  }

     
                            
     
                                                                            
     
  stack = (GucStack *)MemoryContextAllocZero(TopTransactionContext, sizeof(GucStack));

  stack->prev = gconf->stack;
  stack->nest_level = GUCNestLevel;
  switch (action)
  {
  case GUC_ACTION_SET:
    stack->state = GUC_SET;
    break;
  case GUC_ACTION_LOCAL:
    stack->state = GUC_LOCAL;
    break;
  case GUC_ACTION_SAVE:
    stack->state = GUC_SAVE;
    break;
  }
  stack->source = gconf->source;
  stack->scontext = gconf->scontext;
  set_stack_value(gconf, &stack->prior);

  gconf->stack = stack;

                                                
  guc_dirty = true;
}

   
                                                
   
void
AtStart_GUC(void)
{
     
                                                                            
                                                                          
                                                           
     
  if (GUCNestLevel != 0)
  {
    elog(WARNING, "GUC nest level = %d at transaction start", GUCNestLevel);
  }
  GUCNestLevel = 1;
}

   
                                                                               
                                                                           
                                                                     
                                                                                
   
int
NewGUCNestLevel(void)
{
  return ++GUCNestLevel;
}

   
                                                                          
                                                                          
                                                                           
                                                                    
                                                                          
                                                                             
   
void
AtEOXact_GUC(bool isCommit, int nestLevel)
{
  bool still_dirty;
  int i;

     
                                                                             
                                                                  
                            
     
  Assert(nestLevel > 0 && (nestLevel <= GUCNestLevel || (nestLevel == GUCNestLevel + 1 && !isCommit)));

                                                           
  if (!guc_dirty)
  {
    GUCNestLevel = nestLevel - 1;
    return;
  }

  still_dirty = false;
  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *gconf = guc_variables[i];
    GucStack *stack;

       
                                                                           
                                                                          
                                                                          
                                                                         
                                                           
       
    while ((stack = gconf->stack) != NULL && stack->nest_level >= nestLevel)
    {
      GucStack *prev = stack->prev;
      bool restorePrior = false;
      bool restoreMasked = false;
      bool changed;

         
                                                                  
                                                                     
                                                                     
                                                                        
         
      if (!isCommit)                                           
      {
        restorePrior = true;
      }
      else if (stack->state == GUC_SAVE)
      {
        restorePrior = true;
      }
      else if (stack->nest_level == 1)
      {
                                
        if (stack->state == GUC_SET_LOCAL)
        {
          restoreMasked = true;
        }
        else if (stack->state == GUC_SET)
        {
                                                
          discard_stack_value(gconf, &stack->prior);
        }
        else                        
        {
          restorePrior = true;
        }
      }
      else if (prev == NULL || prev->nest_level < stack->nest_level - 1)
      {
                                                       
        stack->nest_level--;
        continue;
      }
      else
      {
           
                                                                       
                                   
           
        switch (stack->state)
        {
        case GUC_SAVE:
          Assert(false);                     
          break;

        case GUC_SET:
                                             
          discard_stack_value(gconf, &stack->prior);
          if (prev->state == GUC_SET_LOCAL)
          {
            discard_stack_value(gconf, &prev->masked);
          }
          prev->state = GUC_SET;
          break;

        case GUC_LOCAL:
          if (prev->state == GUC_SET)
          {
                                     
            prev->masked_scontext = stack->scontext;
            prev->masked = stack->prior;
            prev->state = GUC_SET_LOCAL;
          }
          else
          {
                                                   
            discard_stack_value(gconf, &stack->prior);
          }
          break;

        case GUC_SET_LOCAL:
                                                          
          discard_stack_value(gconf, &stack->prior);
                                          
          prev->masked_scontext = stack->masked_scontext;
          if (prev->state == GUC_SET_LOCAL)
          {
            discard_stack_value(gconf, &prev->masked);
          }
          prev->masked = stack->masked;
          prev->state = GUC_SET_LOCAL;
          break;
        }
      }

      changed = false;

      if (restorePrior || restoreMasked)
      {
                                                                  
        config_var_value newvalue;
        GucSource newsource;
        GucContext newscontext;

        if (restoreMasked)
        {
          newvalue = stack->masked;
          newsource = PGC_S_SESSION;
          newscontext = stack->masked_scontext;
        }
        else
        {
          newvalue = stack->prior;
          newsource = stack->source;
          newscontext = stack->scontext;
        }

        switch (gconf->vartype)
        {
        case PGC_BOOL:
        {
          struct config_bool *conf = (struct config_bool *)gconf;
          bool newval = newvalue.val.boolval;
          void *newextra = newvalue.extra;

          if (*conf->variable != newval || conf->gen.extra != newextra)
          {
            if (conf->assign_hook)
            {
              conf->assign_hook(newval, newextra);
            }
            *conf->variable = newval;
            set_extra_field(&conf->gen, &conf->gen.extra, newextra);
            changed = true;
          }
          break;
        }
        case PGC_INT:
        {
          struct config_int *conf = (struct config_int *)gconf;
          int newval = newvalue.val.intval;
          void *newextra = newvalue.extra;

          if (*conf->variable != newval || conf->gen.extra != newextra)
          {
            if (conf->assign_hook)
            {
              conf->assign_hook(newval, newextra);
            }
            *conf->variable = newval;
            set_extra_field(&conf->gen, &conf->gen.extra, newextra);
            changed = true;
          }
          break;
        }
        case PGC_REAL:
        {
          struct config_real *conf = (struct config_real *)gconf;
          double newval = newvalue.val.realval;
          void *newextra = newvalue.extra;

          if (*conf->variable != newval || conf->gen.extra != newextra)
          {
            if (conf->assign_hook)
            {
              conf->assign_hook(newval, newextra);
            }
            *conf->variable = newval;
            set_extra_field(&conf->gen, &conf->gen.extra, newextra);
            changed = true;
          }
          break;
        }
        case PGC_STRING:
        {
          struct config_string *conf = (struct config_string *)gconf;
          char *newval = newvalue.val.stringval;
          void *newextra = newvalue.extra;

          if (*conf->variable != newval || conf->gen.extra != newextra)
          {
            if (conf->assign_hook)
            {
              conf->assign_hook(newval, newextra);
            }
            set_string_field(conf, conf->variable, newval);
            set_extra_field(&conf->gen, &conf->gen.extra, newextra);
            changed = true;
          }

             
                                                            
                                                             
                                                         
                             
             
          set_string_field(conf, &stack->prior.val.stringval, NULL);
          set_string_field(conf, &stack->masked.val.stringval, NULL);
          break;
        }
        case PGC_ENUM:
        {
          struct config_enum *conf = (struct config_enum *)gconf;
          int newval = newvalue.val.enumval;
          void *newextra = newvalue.extra;

          if (*conf->variable != newval || conf->gen.extra != newextra)
          {
            if (conf->assign_hook)
            {
              conf->assign_hook(newval, newextra);
            }
            *conf->variable = newval;
            set_extra_field(&conf->gen, &conf->gen.extra, newextra);
            changed = true;
          }
          break;
        }
        }

           
                                                             
           
        set_extra_field(gconf, &(stack->prior.extra), NULL);
        set_extra_field(gconf, &(stack->masked.extra), NULL);

                                            
        gconf->source = newsource;
        gconf->scontext = newscontext;
      }

                                          
      gconf->stack = prev;
      pfree(stack);

                                             
      if (changed && (gconf->flags & GUC_REPORT))
      {
        ReportGUCOption(gconf);
      }
    }                                

    if (stack != NULL)
    {
      still_dirty = true;
    }
  }

                                                                       
  guc_dirty = still_dirty;

                            
  GUCNestLevel = nestLevel - 1;
}

   
                                                                           
                                                      
   
void
BeginReportingGUCOptions(void)
{
  int i;

     
                                                                             
                   
     
  if (whereToSendOutput != DestRemote || PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    return;
  }

  reporting_enabled = true;

                                                        
  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *conf = guc_variables[i];

    if (conf->flags & GUC_REPORT)
    {
      ReportGUCOption(conf);
    }
  }
}

   
                                                                      
   
static void
ReportGUCOption(struct config_generic *record)
{
  if (reporting_enabled && (record->flags & GUC_REPORT))
  {
    char *val = _ShowOption(record, false);
    StringInfoData msgbuf;

    pq_beginmessage(&msgbuf, 'S');
    pq_sendstring(&msgbuf, record->name);
    pq_sendstring(&msgbuf, val);
    pq_endmessage(&msgbuf);

    pfree(val);
  }
}

   
                                                                           
                                                                            
                                                                      
                                                 
                                                                              
                               
   
                                                                       
   
static bool
convert_to_base_unit(double value, const char *unit, int base_unit, double *base_value)
{
  char unitstr[MAX_UNIT_LEN + 1];
  int unitlen;
  const unit_conversion *table;
  int i;

                                                       
  unitlen = 0;
  while (*unit != '\0' && !isspace((unsigned char)*unit) && unitlen < MAX_UNIT_LEN)
  {
    unitstr[unitlen++] = *(unit++);
  }
  unitstr[unitlen] = '\0';
                                   
  while (isspace((unsigned char)*unit))
  {
    unit++;
  }
  if (*unit != '\0')
  {
    return false;                                         
  }

                                        
  if (base_unit & GUC_UNIT_MEMORY)
  {
    table = memory_unit_conversion_table;
  }
  else
  {
    table = time_unit_conversion_table;
  }

  for (i = 0; *table[i].unit; i++)
  {
    if (base_unit == table[i].base_unit && strcmp(unitstr, table[i].unit) == 0)
    {
      double cvalue = value * table[i].multiplier;

         
                                                                        
                                                                        
                 
         
      if (*table[i + 1].unit && base_unit == table[i + 1].base_unit)
      {
        cvalue = rint(cvalue / table[i + 1].multiplier) * table[i + 1].multiplier;
      }

      *base_value = cvalue;
      return true;
    }
  }
  return false;
}

   
                                                                        
   
                                                                               
                                                                               
                                                             
   
static void
convert_int_from_base_unit(int64 base_value, int base_unit, int64 *value, const char **unit)
{
  const unit_conversion *table;
  int i;

  *unit = NULL;

  if (base_unit & GUC_UNIT_MEMORY)
  {
    table = memory_unit_conversion_table;
  }
  else
  {
    table = time_unit_conversion_table;
  }

  for (i = 0; *table[i].unit; i++)
  {
    if (base_unit == table[i].base_unit)
    {
         
                                                                        
                                                                         
                                        
         
      if (table[i].multiplier <= 1.0 || base_value % (int64)table[i].multiplier == 0)
      {
        *value = (int64)rint(base_value / table[i].multiplier);
        *unit = table[i].unit;
        break;
      }
    }
  }

  Assert(*unit != NULL);
}

   
                                                                              
   
                                                                       
                                                               
   
static void
convert_real_from_base_unit(double base_value, int base_unit, double *value, const char **unit)
{
  const unit_conversion *table;
  int i;

  *unit = NULL;

  if (base_unit & GUC_UNIT_MEMORY)
  {
    table = memory_unit_conversion_table;
  }
  else
  {
    table = time_unit_conversion_table;
  }

  for (i = 0; *table[i].unit; i++)
  {
    if (base_unit == table[i].base_unit)
    {
         
                                                                       
                                                                
         
                                                                        
                                                                    
                                                                      
                                                                      
                                                                       
         
      *value = base_value / table[i].multiplier;
      *unit = table[i].unit;
      if (*value > 0 && fabs((rint(*value) / *value) - 1.0) <= 1e-8)
      {
        break;
      }
    }
  }

  Assert(*unit != NULL);
}

   
                                                                     
                                       
   
static const char *
get_config_unit_name(int flags)
{
  switch (flags & (GUC_UNIT_MEMORY | GUC_UNIT_TIME))
  {
  case 0:
    return NULL;                       
  case GUC_UNIT_BYTE:
    return "B";
  case GUC_UNIT_KB:
    return "kB";
  case GUC_UNIT_MB:
    return "MB";
  case GUC_UNIT_BLOCKS:
  {
    static char bbuf[8];

                                          
    if (bbuf[0] == '\0')
    {
      snprintf(bbuf, sizeof(bbuf), "%dkB", BLCKSZ / 1024);
    }
    return bbuf;
  }
  case GUC_UNIT_XBLOCKS:
  {
    static char xbuf[8];

                                          
    if (xbuf[0] == '\0')
    {
      snprintf(xbuf, sizeof(xbuf), "%dkB", XLOG_BLCKSZ / 1024);
    }
    return xbuf;
  }
  case GUC_UNIT_MS:
    return "ms";
  case GUC_UNIT_S:
    return "s";
  case GUC_UNIT_MIN:
    return "min";
  default:
    elog(ERROR, "unrecognized GUC units value: %d", flags & (GUC_UNIT_MEMORY | GUC_UNIT_TIME));
    return NULL;
  }
}

   
                                                                   
                                                                           
                                                                          
                                                                             
                      
   
                                                       
                                                                
                                                                      
                                              
   
bool
parse_int(const char *value, int *result, int flags, const char **hintmsg)
{
     
                                                                        
                                    
     
  double val;
  char *endptr;

                                                               
  if (result)
  {
    *result = 0;
  }
  if (hintmsg)
  {
    *hintmsg = NULL;
  }

     
                                                                       
                                                                           
                                                                             
                                                                         
                                                              
     
  errno = 0;
  val = strtol(value, &endptr, 0);
  if (*endptr == '.' || *endptr == 'e' || *endptr == 'E' || errno == ERANGE)
  {
    errno = 0;
    val = strtod(value, &endptr);
  }

  if (endptr == value || errno == ERANGE)
  {
    return false;                              
  }

                                                           
  if (isnan(val))
  {
    return false;                                          
  }

                                                
  while (isspace((unsigned char)*endptr))
  {
    endptr++;
  }

                            
  if (*endptr != '\0')
  {
    if ((flags & GUC_UNIT) == 0)
    {
      return false;                                          
    }

    if (!convert_to_base_unit(val, endptr, (flags & GUC_UNIT), &val))
    {
                                                                       
      if (hintmsg)
      {
        if (flags & GUC_UNIT_MEMORY)
        {
          *hintmsg = memory_units_hint;
        }
        else
        {
          *hintmsg = time_units_hint;
        }
      }
      return false;
    }
  }

                                             
  val = rint(val);

  if (val > INT_MAX || val < INT_MIN)
  {
    if (hintmsg)
    {
      *hintmsg = gettext_noop("Value exceeds integer range.");
    }
    return false;
  }

  if (result)
  {
    *result = (int)val;
  }
  return true;
}

   
                                                                      
                                                                             
                      
   
                                                       
                                                                
                                                                      
                                              
   
bool
parse_real(const char *value, double *result, int flags, const char **hintmsg)
{
  double val;
  char *endptr;

                                                               
  if (result)
  {
    *result = 0;
  }
  if (hintmsg)
  {
    *hintmsg = NULL;
  }

  errno = 0;
  val = strtod(value, &endptr);

  if (endptr == value || errno == ERANGE)
  {
    return false;                              
  }

                                                            
  if (isnan(val))
  {
    return false;                                          
  }

                                                
  while (isspace((unsigned char)*endptr))
  {
    endptr++;
  }

                            
  if (*endptr != '\0')
  {
    if ((flags & GUC_UNIT) == 0)
    {
      return false;                                          
    }

    if (!convert_to_base_unit(val, endptr, (flags & GUC_UNIT), &val))
    {
                                                                       
      if (hintmsg)
      {
        if (flags & GUC_UNIT_MEMORY)
        {
          *hintmsg = memory_units_hint;
        }
        else
        {
          *hintmsg = time_units_hint;
        }
      }
      return false;
    }
  }

  if (result)
  {
    *result = val;
  }
  return true;
}

   
                                                               
                                                                 
                                                   
   
                                                           
                               
   
const char *
config_enum_lookup_by_value(struct config_enum *record, int val)
{
  const struct config_enum_entry *entry;

  for (entry = record->options; entry && entry->name; entry++)
  {
    if (entry->val == val)
    {
      return entry->name;
    }
  }

  elog(ERROR, "could not find enum option %d for %s", val, record->gen.name);
  return NULL;                       
}

   
                                                              
                       
                                                                  
                                                                 
   
bool
config_enum_lookup_by_name(struct config_enum *record, const char *value, int *retval)
{
  const struct config_enum_entry *entry;

  for (entry = record->options; entry && entry->name; entry++)
  {
    if (pg_strcasecmp(value, entry->name) == 0)
    {
      *retval = entry->val;
      return true;
    }
  }

  *retval = 0;
  return false;
}

   
                                                                 
                                                  
                                                                   
                                                                
   
static char *
config_enum_get_options(struct config_enum *record, const char *prefix, const char *suffix, const char *separator)
{
  const struct config_enum_entry *entry;
  StringInfoData retstr;
  int seplen;

  initStringInfo(&retstr);
  appendStringInfoString(&retstr, prefix);

  seplen = strlen(separator);
  for (entry = record->options; entry && entry->name; entry++)
  {
    if (!entry->hidden)
    {
      appendStringInfoString(&retstr, entry->name);
      appendBinaryStringInfo(&retstr, separator, seplen);
    }
  }

     
                                                                          
                                                                            
                                                                          
                                                                    
                              
     
  if (retstr.len >= seplen)
  {
                                 
    retstr.data[retstr.len - seplen] = '\0';
    retstr.len -= seplen;
  }

  appendStringInfoString(&retstr, suffix);

  return retstr.data;
}

   
                                                                       
              
   
                                                                             
                                                         
   
                                      
                                                           
                                      
                                                                  
                                             
                                                                  
                                                                          
                                              
   
                                                                          
   
static bool
parse_and_validate_value(struct config_generic *record, const char *name, const char *value, GucSource source, int elevel, union config_var_val *newval, void **newextra)
{
  switch (record->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *conf = (struct config_bool *)record;

    if (!parse_bool(value, &newval->boolval))
    {
      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("parameter \"%s\" requires a Boolean value", name)));
      return false;
    }

    if (!call_bool_check_hook(conf, &newval->boolval, newextra, source, elevel))
    {
      return false;
    }
  }
  break;
  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)record;
    const char *hintmsg;

    if (!parse_int(value, &newval->intval, conf->gen.flags, &hintmsg))
    {
      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for parameter \"%s\": \"%s\"", name, value), hintmsg ? errhint("%s", _(hintmsg)) : 0));
      return false;
    }

    if (newval->intval < conf->min || newval->intval > conf->max)
    {
      const char *unit = get_config_unit_name(conf->gen.flags);

      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("%d%s%s is outside the valid range for parameter \"%s\" (%d .. %d)", newval->intval, unit ? " " : "", unit ? unit : "", name, conf->min, conf->max)));
      return false;
    }

    if (!call_int_check_hook(conf, &newval->intval, newextra, source, elevel))
    {
      return false;
    }
  }
  break;
  case PGC_REAL:
  {
    struct config_real *conf = (struct config_real *)record;
    const char *hintmsg;

    if (!parse_real(value, &newval->realval, conf->gen.flags, &hintmsg))
    {
      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for parameter \"%s\": \"%s\"", name, value), hintmsg ? errhint("%s", _(hintmsg)) : 0));
      return false;
    }

    if (newval->realval < conf->min || newval->realval > conf->max)
    {
      const char *unit = get_config_unit_name(conf->gen.flags);

      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("%g%s%s is outside the valid range for parameter \"%s\" (%g .. %g)", newval->realval, unit ? " " : "", unit ? unit : "", name, conf->min, conf->max)));
      return false;
    }

    if (!call_real_check_hook(conf, &newval->realval, newextra, source, elevel))
    {
      return false;
    }
  }
  break;
  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)record;

       
                                                                
                         
       
    newval->stringval = guc_strdup(elevel, value);
    if (newval->stringval == NULL)
    {
      return false;
    }

       
                                                             
                                  
       
    if (conf->gen.flags & GUC_IS_NAME)
    {
      truncate_identifier(newval->stringval, strlen(newval->stringval), true);
    }

    if (!call_string_check_hook(conf, &newval->stringval, newextra, source, elevel))
    {
      free(newval->stringval);
      newval->stringval = NULL;
      return false;
    }
  }
  break;
  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)record;

    if (!config_enum_lookup_by_name(conf, value, &newval->enumval))
    {
      char *hintmsg;

      hintmsg = config_enum_get_options(conf, "Available values: ", ".", ", ");

      ereport(elevel, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for parameter \"%s\": \"%s\"", name, value), hintmsg ? errhint("%s", _(hintmsg)) : 0));

      if (hintmsg)
      {
        pfree(hintmsg);
      }
      return false;
    }

    if (!call_enum_check_hook(conf, &newval->enumval, newextra, source, elevel))
    {
      return false;
    }
  }
  break;
  }

  return true;
}

   
                                      
   
                                                                       
                                                                          
                                                                            
                                 
   
                                                                       
                                                                           
   
                                                                              
                                                                                
   
                                                                     
                                       
   
                                                                            
                                                                          
                                                                            
   
                 
                                                        
                                                    
                                                                             
   
                                                                     
                                                                           
                                                                           
                                                                         
                                                                        
                                       
   
                                                       
   
int
set_config_option(const char *name, const char *value, GucContext context, GucSource source, GucAction action, bool changeVal, int elevel, bool is_reload)
{
  struct config_generic *record;
  union config_var_val newval_union;
  void *newextra = NULL;
  bool prohibitValueChange = false;
  bool makeDefault;

  if (elevel == 0)
  {
    if (source == PGC_S_DEFAULT || source == PGC_S_FILE)
    {
         
                                                                        
                                              
         
      elevel = IsUnderPostmaster ? DEBUG3 : LOG;
    }
    else if (source == PGC_S_GLOBAL || source == PGC_S_DATABASE || source == PGC_S_USER || source == PGC_S_DATABASE_USER)
    {
      elevel = WARNING;
    }
    else
    {
      elevel = ERROR;
    }
  }

     
                                                                         
                                                                          
                                                                            
                                                                          
                                                          
     
                                                                       
     
  if (IsInParallelMode() && changeVal && action != GUC_ACTION_SAVE)
  {
    ereport(elevel, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot set parameters during a parallel operation")));
  }

  record = find_option(name, true, elevel);
  if (record == NULL)
  {
    ereport(elevel, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
    return 0;
  }

     
                                                                            
            
     
  switch (record->context)
  {
  case PGC_INTERNAL:
    if (context != PGC_INTERNAL)
    {
      ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed", name)));
      return 0;
    }
    break;
  case PGC_POSTMASTER:
    if (context == PGC_SIGHUP)
    {
         
                                                          
                                                                     
                                                                 
                                                               
                                                                     
                                                                    
                                   
         
      prohibitValueChange = true;
    }
    else if (context != PGC_POSTMASTER)
    {
      ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", name)));
      return 0;
    }
    break;
  case PGC_SIGHUP:
    if (context != PGC_SIGHUP && context != PGC_POSTMASTER)
    {
      ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed now", name)));
      return 0;
    }

       
                                                                       
                                                                   
                                                                
                                            
       
    break;
  case PGC_SU_BACKEND:
                                                              
    if (context == PGC_BACKEND)
    {
      ereport(elevel, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to set parameter \"%s\"", name)));
      return 0;
    }
                                                         
                     
  case PGC_BACKEND:
    if (context == PGC_SIGHUP)
    {
         
                                                                    
                                                                 
                                                 
                                                                   
                                                                  
                                                             
         
                                                                     
                                                               
                                                            
                                                                 
                                                                     
                                                                 
                                                                  
                                                                  
                  
         
      if (IsUnderPostmaster && !is_reload)
      {
        return -1;
      }
    }
    else if (context != PGC_POSTMASTER && context != PGC_BACKEND && context != PGC_SU_BACKEND && source != PGC_S_CLIENT)
    {
      ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be set after connection start", name)));
      return 0;
    }
    break;
  case PGC_SUSET:
    if (context == PGC_USERSET || context == PGC_BACKEND)
    {
      ereport(elevel, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to set parameter \"%s\"", name)));
      return 0;
    }
    break;
  case PGC_USERSET:
                     
    break;
  }

     
                                                                        
                                                                             
                                                                           
                                                            
     
                                                                            
                                                                             
                                                                            
     
                                                                       
                                                                       
                                                                            
                                                                            
                                                                             
                                                                        
                                                                            
     
  if (record->flags & GUC_NOT_WHILE_SEC_REST)
  {
    if (InLocalUserIdChange())
    {
         
                                                                         
                      
         
      ereport(elevel, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("cannot set parameter \"%s\" within security-definer function", name)));
      return 0;
    }
    if (InSecurityRestrictedOperation())
    {
      ereport(elevel, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("cannot set parameter \"%s\" within security-restricted operation", name)));
      return 0;
    }
  }

     
                                                                     
                                                                          
                                                                        
                           
     
  makeDefault = changeVal && (source <= PGC_S_OVERRIDE) && ((value != NULL) || source == PGC_S_DEFAULT);

     
                                                                         
                                                                        
                                                                           
                                                                          
                                                                        
     
  if (record->source > source)
  {
    if (changeVal && !makeDefault)
    {
      elog(DEBUG3, "\"%s\": setting ignored because previous source is higher priority", name);
      return -1;
    }
    changeVal = false;
  }

     
                                      
     
  switch (record->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *conf = (struct config_bool *)record;

#define newval (newval_union.boolval)

    if (value)
    {
      if (!parse_and_validate_value(record, name, value, source, elevel, &newval_union, &newextra))
      {
        return 0;
      }
    }
    else if (source == PGC_S_DEFAULT)
    {
      newval = conf->boot_val;
      if (!call_bool_check_hook(conf, &newval, &newextra, source, elevel))
      {
        return 0;
      }
    }
    else
    {
      newval = conf->reset_val;
      newextra = conf->reset_extra;
      source = conf->gen.reset_source;
      context = conf->gen.reset_scontext;
    }

    if (prohibitValueChange)
    {
                                                     
      if (newextra && !extra_field_used(&conf->gen, newextra))
      {
        free(newextra);
      }

      if (*conf->variable != newval)
      {
        record->status |= GUC_PENDING_RESTART;
        ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", name)));
        return 0;
      }
      record->status &= ~GUC_PENDING_RESTART;
      return -1;
    }

    if (changeVal)
    {
                                                       
      if (!makeDefault)
      {
        push_old_value(&conf->gen, action);
      }

      if (conf->assign_hook)
      {
        conf->assign_hook(newval, newextra);
      }
      *conf->variable = newval;
      set_extra_field(&conf->gen, &conf->gen.extra, newextra);
      conf->gen.source = source;
      conf->gen.scontext = context;
    }
    if (makeDefault)
    {
      GucStack *stack;

      if (conf->gen.reset_source <= source)
      {
        conf->reset_val = newval;
        set_extra_field(&conf->gen, &conf->reset_extra, newextra);
        conf->gen.reset_source = source;
        conf->gen.reset_scontext = context;
      }
      for (stack = conf->gen.stack; stack; stack = stack->prev)
      {
        if (stack->source <= source)
        {
          stack->prior.val.boolval = newval;
          set_extra_field(&conf->gen, &stack->prior.extra, newextra);
          stack->source = source;
          stack->scontext = context;
        }
      }
    }

                                                     
    if (newextra && !extra_field_used(&conf->gen, newextra))
    {
      free(newextra);
    }
    break;

#undef newval
  }

  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)record;

#define newval (newval_union.intval)

    if (value)
    {
      if (!parse_and_validate_value(record, name, value, source, elevel, &newval_union, &newextra))
      {
        return 0;
      }
    }
    else if (source == PGC_S_DEFAULT)
    {
      newval = conf->boot_val;
      if (!call_int_check_hook(conf, &newval, &newextra, source, elevel))
      {
        return 0;
      }
    }
    else
    {
      newval = conf->reset_val;
      newextra = conf->reset_extra;
      source = conf->gen.reset_source;
      context = conf->gen.reset_scontext;
    }

    if (prohibitValueChange)
    {
                                                     
      if (newextra && !extra_field_used(&conf->gen, newextra))
      {
        free(newextra);
      }

      if (*conf->variable != newval)
      {
        record->status |= GUC_PENDING_RESTART;
        ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", name)));
        return 0;
      }
      record->status &= ~GUC_PENDING_RESTART;
      return -1;
    }

    if (changeVal)
    {
                                                       
      if (!makeDefault)
      {
        push_old_value(&conf->gen, action);
      }

      if (conf->assign_hook)
      {
        conf->assign_hook(newval, newextra);
      }
      *conf->variable = newval;
      set_extra_field(&conf->gen, &conf->gen.extra, newextra);
      conf->gen.source = source;
      conf->gen.scontext = context;
    }
    if (makeDefault)
    {
      GucStack *stack;

      if (conf->gen.reset_source <= source)
      {
        conf->reset_val = newval;
        set_extra_field(&conf->gen, &conf->reset_extra, newextra);
        conf->gen.reset_source = source;
        conf->gen.reset_scontext = context;
      }
      for (stack = conf->gen.stack; stack; stack = stack->prev)
      {
        if (stack->source <= source)
        {
          stack->prior.val.intval = newval;
          set_extra_field(&conf->gen, &stack->prior.extra, newextra);
          stack->source = source;
          stack->scontext = context;
        }
      }
    }

                                                     
    if (newextra && !extra_field_used(&conf->gen, newextra))
    {
      free(newextra);
    }
    break;

#undef newval
  }

  case PGC_REAL:
  {
    struct config_real *conf = (struct config_real *)record;

#define newval (newval_union.realval)

    if (value)
    {
      if (!parse_and_validate_value(record, name, value, source, elevel, &newval_union, &newextra))
      {
        return 0;
      }
    }
    else if (source == PGC_S_DEFAULT)
    {
      newval = conf->boot_val;
      if (!call_real_check_hook(conf, &newval, &newextra, source, elevel))
      {
        return 0;
      }
    }
    else
    {
      newval = conf->reset_val;
      newextra = conf->reset_extra;
      source = conf->gen.reset_source;
      context = conf->gen.reset_scontext;
    }

    if (prohibitValueChange)
    {
                                                     
      if (newextra && !extra_field_used(&conf->gen, newextra))
      {
        free(newextra);
      }

      if (*conf->variable != newval)
      {
        record->status |= GUC_PENDING_RESTART;
        ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", name)));
        return 0;
      }
      record->status &= ~GUC_PENDING_RESTART;
      return -1;
    }

    if (changeVal)
    {
                                                       
      if (!makeDefault)
      {
        push_old_value(&conf->gen, action);
      }

      if (conf->assign_hook)
      {
        conf->assign_hook(newval, newextra);
      }
      *conf->variable = newval;
      set_extra_field(&conf->gen, &conf->gen.extra, newextra);
      conf->gen.source = source;
      conf->gen.scontext = context;
    }
    if (makeDefault)
    {
      GucStack *stack;

      if (conf->gen.reset_source <= source)
      {
        conf->reset_val = newval;
        set_extra_field(&conf->gen, &conf->reset_extra, newextra);
        conf->gen.reset_source = source;
        conf->gen.reset_scontext = context;
      }
      for (stack = conf->gen.stack; stack; stack = stack->prev)
      {
        if (stack->source <= source)
        {
          stack->prior.val.realval = newval;
          set_extra_field(&conf->gen, &stack->prior.extra, newextra);
          stack->source = source;
          stack->scontext = context;
        }
      }
    }

                                                     
    if (newextra && !extra_field_used(&conf->gen, newextra))
    {
      free(newextra);
    }
    break;

#undef newval
  }

  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)record;

#define newval (newval_union.stringval)

    if (value)
    {
      if (!parse_and_validate_value(record, name, value, source, elevel, &newval_union, &newextra))
      {
        return 0;
      }
    }
    else if (source == PGC_S_DEFAULT)
    {
                                                      
      if (conf->boot_val != NULL)
      {
        newval = guc_strdup(elevel, conf->boot_val);
        if (newval == NULL)
        {
          return 0;
        }
      }
      else
      {
        newval = NULL;
      }

      if (!call_string_check_hook(conf, &newval, &newextra, source, elevel))
      {
        free(newval);
        return 0;
      }
    }
    else
    {
         
                                                             
                         
         
      newval = conf->reset_val;
      newextra = conf->reset_extra;
      source = conf->gen.reset_source;
      context = conf->gen.reset_scontext;
    }

    if (prohibitValueChange)
    {
      bool newval_different;

                                                                
      newval_different = (*conf->variable == NULL || newval == NULL || strcmp(*conf->variable, newval) != 0);

                                                 
      if (newval && !string_field_used(conf, newval))
      {
        free(newval);
      }
                                                     
      if (newextra && !extra_field_used(&conf->gen, newextra))
      {
        free(newextra);
      }

      if (newval_different)
      {
        record->status |= GUC_PENDING_RESTART;
        ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", name)));
        return 0;
      }
      record->status &= ~GUC_PENDING_RESTART;
      return -1;
    }

    if (changeVal)
    {
                                                       
      if (!makeDefault)
      {
        push_old_value(&conf->gen, action);
      }

      if (conf->assign_hook)
      {
        conf->assign_hook(newval, newextra);
      }
      set_string_field(conf, conf->variable, newval);
      set_extra_field(&conf->gen, &conf->gen.extra, newextra);
      conf->gen.source = source;
      conf->gen.scontext = context;
    }

    if (makeDefault)
    {
      GucStack *stack;

      if (conf->gen.reset_source <= source)
      {
        set_string_field(conf, &conf->reset_val, newval);
        set_extra_field(&conf->gen, &conf->reset_extra, newextra);
        conf->gen.reset_source = source;
        conf->gen.reset_scontext = context;
      }
      for (stack = conf->gen.stack; stack; stack = stack->prev)
      {
        if (stack->source <= source)
        {
          set_string_field(conf, &stack->prior.val.stringval, newval);
          set_extra_field(&conf->gen, &stack->prior.extra, newextra);
          stack->source = source;
          stack->scontext = context;
        }
      }
    }

                                                   
    if (newval && !string_field_used(conf, newval))
    {
      free(newval);
    }
                                                     
    if (newextra && !extra_field_used(&conf->gen, newextra))
    {
      free(newextra);
    }
    break;

#undef newval
  }

  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)record;

#define newval (newval_union.enumval)

    if (value)
    {
      if (!parse_and_validate_value(record, name, value, source, elevel, &newval_union, &newextra))
      {
        return 0;
      }
    }
    else if (source == PGC_S_DEFAULT)
    {
      newval = conf->boot_val;
      if (!call_enum_check_hook(conf, &newval, &newextra, source, elevel))
      {
        return 0;
      }
    }
    else
    {
      newval = conf->reset_val;
      newextra = conf->reset_extra;
      source = conf->gen.reset_source;
      context = conf->gen.reset_scontext;
    }

    if (prohibitValueChange)
    {
                                                     
      if (newextra && !extra_field_used(&conf->gen, newextra))
      {
        free(newextra);
      }

      if (*conf->variable != newval)
      {
        record->status |= GUC_PENDING_RESTART;
        ereport(elevel, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed without restarting the server", name)));
        return 0;
      }
      record->status &= ~GUC_PENDING_RESTART;
      return -1;
    }

    if (changeVal)
    {
                                                       
      if (!makeDefault)
      {
        push_old_value(&conf->gen, action);
      }

      if (conf->assign_hook)
      {
        conf->assign_hook(newval, newextra);
      }
      *conf->variable = newval;
      set_extra_field(&conf->gen, &conf->gen.extra, newextra);
      conf->gen.source = source;
      conf->gen.scontext = context;
    }
    if (makeDefault)
    {
      GucStack *stack;

      if (conf->gen.reset_source <= source)
      {
        conf->reset_val = newval;
        set_extra_field(&conf->gen, &conf->reset_extra, newextra);
        conf->gen.reset_source = source;
        conf->gen.reset_scontext = context;
      }
      for (stack = conf->gen.stack; stack; stack = stack->prev)
      {
        if (stack->source <= source)
        {
          stack->prior.val.enumval = newval;
          set_extra_field(&conf->gen, &stack->prior.extra, newextra);
          stack->source = source;
          stack->scontext = context;
        }
      }
    }

                                                     
    if (newextra && !extra_field_used(&conf->gen, newextra))
    {
      free(newextra);
    }
    break;

#undef newval
  }
  }

  if (changeVal && (record->flags & GUC_REPORT))
  {
    ReportGUCOption(record);
  }

  return changeVal ? 1 : -1;
}

   
                                                                         
   
static void
set_config_sourcefile(const char *name, char *sourcefile, int sourceline)
{
  struct config_generic *record;
  int elevel;

     
                                                                          
                                    
     
  elevel = IsUnderPostmaster ? DEBUG3 : LOG;

  record = find_option(name, true, elevel);
                         
  if (record == NULL)
  {
    elog(ERROR, "unrecognized configuration parameter \"%s\"", name);
  }

  sourcefile = guc_strdup(elevel, sourcefile);
  if (record->sourcefile)
  {
    free(record->sourcefile);
  }
  record->sourcefile = sourcefile;
  record->sourceline = sourceline;
}

   
                                           
   
                                                                          
                                                                              
                                             
   
                                                                      
                            
   
void
SetConfigOption(const char *name, const char *value, GucContext context, GucSource source)
{
  (void)set_config_option(name, value, context, source, GUC_ACTION_SET, true, 0, false);
}

   
                                                              
   
                                                                             
                                                                            
                                                
   
                                                                            
                                                                       
                                                                        
   
                                                                     
                                                                 
   
const char *
GetConfigOption(const char *name, bool missing_ok, bool restrict_privileged)
{
  struct config_generic *record;
  static char buffer[256];

  record = find_option(name, false, ERROR);
  if (record == NULL)
  {
    if (missing_ok)
    {
      return NULL;
    }
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
  }
  if (restrict_privileged && (record->flags & GUC_SUPERUSER_ONLY) && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or a member of pg_read_all_settings to examine \"%s\"", name)));
  }

  switch (record->vartype)
  {
  case PGC_BOOL:
    return *((struct config_bool *)record)->variable ? "on" : "off";

  case PGC_INT:
    snprintf(buffer, sizeof(buffer), "%d", *((struct config_int *)record)->variable);
    return buffer;

  case PGC_REAL:
    snprintf(buffer, sizeof(buffer), "%g", *((struct config_real *)record)->variable);
    return buffer;

  case PGC_STRING:
    return *((struct config_string *)record)->variable;

  case PGC_ENUM:
    return config_enum_lookup_by_value((struct config_enum *)record, *((struct config_enum *)record)->variable);
  }
  return NULL;
}

   
                                                         
   
                                                                     
                                                                           
                                                              
   
const char *
GetConfigOptionResetString(const char *name)
{
  struct config_generic *record;
  static char buffer[256];

  record = find_option(name, false, ERROR);
  if (record == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
  }
  if ((record->flags & GUC_SUPERUSER_ONLY) && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or a member of pg_read_all_settings to examine \"%s\"", name)));
  }

  switch (record->vartype)
  {
  case PGC_BOOL:
    return ((struct config_bool *)record)->reset_val ? "on" : "off";

  case PGC_INT:
    snprintf(buffer, sizeof(buffer), "%d", ((struct config_int *)record)->reset_val);
    return buffer;

  case PGC_REAL:
    snprintf(buffer, sizeof(buffer), "%g", ((struct config_real *)record)->reset_val);
    return buffer;

  case PGC_STRING:
    return ((struct config_string *)record)->reset_val;

  case PGC_ENUM:
    return config_enum_lookup_by_value((struct config_enum *)record, ((struct config_enum *)record)->reset_val);
  }
  return NULL;
}

   
                                                       
   
                                                                
                                                
   
int
GetConfigOptionFlags(const char *name, bool missing_ok)
{
  struct config_generic *record;

  record = find_option(name, false, WARNING);
  if (record == NULL)
  {
    if (missing_ok)
    {
      return 0;
    }
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
  }
  return record->flags;
}

   
                             
                                                              
                                                           
   
                                                                         
                                    
   
                                                                           
                      
   
static char *
flatten_set_variable_args(const char *name, List *args)
{
  struct config_generic *record;
  int flags;
  StringInfoData buf;
  ListCell *l;

                                 
  if (args == NIL)
  {
    return NULL;
  }

     
                                                                       
                                                                           
     
  record = find_option(name, false, WARNING);
  if (record)
  {
    flags = record->flags;
  }
  else
  {
    flags = 0;
  }

                                                    
  if ((flags & GUC_LIST_INPUT) == 0 && list_length(args) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("SET %s takes only one argument", name)));
  }

  initStringInfo(&buf);

     
                                                                          
                                                                             
                          
     
  foreach (l, args)
  {
    Node *arg = (Node *)lfirst(l);
    char *val;
    TypeName *typeName = NULL;
    A_Const *con;

    if (l != list_head(args))
    {
      appendStringInfoString(&buf, ", ");
    }

    if (IsA(arg, TypeCast))
    {
      TypeCast *tc = (TypeCast *)arg;

      arg = tc->arg;
      typeName = tc->typeName;
    }

    if (!IsA(arg, A_Const))
    {
      elog(ERROR, "unrecognized node type: %d", (int)nodeTag(arg));
    }
    con = (A_Const *)arg;

    switch (nodeTag(&con->val))
    {
    case T_Integer:
      appendStringInfo(&buf, "%d", intVal(&con->val));
      break;
    case T_Float:
                                                    
      appendStringInfoString(&buf, strVal(&con->val));
      break;
    case T_String:
      val = strVal(&con->val);
      if (typeName != NULL)
      {
           
                                                                  
                                                                   
                           
           
        Oid typoid;
        int32 typmod;
        Datum interval;
        char *intervalout;

        typenameTypeIdAndMod(NULL, typeName, &typoid, &typmod);
        Assert(typoid == INTERVALOID);

        interval = DirectFunctionCall3(interval_in, CStringGetDatum(val), ObjectIdGetDatum(InvalidOid), Int32GetDatum(typmod));

        intervalout = DatumGetCString(DirectFunctionCall1(interval_out, interval));
        appendStringInfo(&buf, "INTERVAL '%s'", intervalout);
      }
      else
      {
           
                                                                
                                                      
           
        if (flags & GUC_LIST_QUOTE)
        {
          appendStringInfoString(&buf, quote_identifier(val));
        }
        else
        {
          appendStringInfoString(&buf, val);
        }
      }
      break;
    default:
      elog(ERROR, "unrecognized node type: %d", (int)nodeTag(&con->val));
      break;
    }
  }

  return buf.data;
}

   
                                                                       
                                                                        
                               
   
static void
write_auto_conf_file(int fd, const char *filename, ConfigVariable *head)
{
  StringInfoData buf;
  ConfigVariable *item;

  initStringInfo(&buf);

                                                   
  appendStringInfoString(&buf, "# Do not edit this file manually!\n");
  appendStringInfoString(&buf, "# It will be overwritten by the ALTER SYSTEM command.\n");

  errno = 0;
  if (write(fd, buf.data, buf.len) != buf.len)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", filename)));
  }

                                                       
  for (item = head; item != NULL; item = item->next)
  {
    char *escaped;

    resetStringInfo(&buf);

    appendStringInfoString(&buf, item->name);
    appendStringInfoString(&buf, " = '");

    escaped = escape_single_quotes_ascii(item->value);
    if (!escaped)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
    appendStringInfoString(&buf, escaped);
    free(escaped);

    appendStringInfoString(&buf, "'\n");

    errno = 0;
    if (write(fd, buf.data, buf.len) != buf.len)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", filename)));
    }
  }

                                                           
  if (pg_fsync(fd) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", filename)));
  }

  pfree(buf.data);
}

   
                                                                        
                                                                      
   
static void
replace_auto_config_value(ConfigVariable **head_p, ConfigVariable **tail_p, const char *name, const char *value)
{
  ConfigVariable *item, *next, *prev = NULL;

     
                                                                            
                                                                           
              
     
  for (item = *head_p; item != NULL; item = next)
  {
    next = item->next;
    if (guc_name_compare(item->name, name) == 0)
    {
                                    
      if (prev)
      {
        prev->next = next;
      }
      else
      {
        *head_p = next;
      }
      if (next == NULL)
      {
        *tail_p = prev;
      }

      pfree(item->name);
      pfree(item->value);
      pfree(item->filename);
      pfree(item);
    }
    else
    {
      prev = item;
    }
  }

                                         
  if (value == NULL)
  {
    return;
  }

                              
  item = palloc(sizeof *item);
  item->name = pstrdup(name);
  item->value = pstrdup(value);
  item->errmsg = NULL;
  item->filename = pstrdup("");                               
  item->sourceline = 0;
  item->ignore = false;
  item->applied = false;
  item->next = NULL;

  if (*head_p == NULL)
  {
    *head_p = item;
  }
  else
  {
    (*tail_p)->next = item;
  }
  *tail_p = item;
}

   
                                   
   
                                                                            
                                                                             
                                                                  
   
                                                                     
   
                                                        
                                                     
   
void
AlterSystemSetConfigFile(AlterSystemStmt *altersysstmt)
{
  char *name;
  char *value;
  bool resetall = false;
  ConfigVariable *head = NULL;
  ConfigVariable *tail = NULL;
  volatile int Tmpfd;
  char AutoConfFileName[MAXPGPATH];
  char AutoConfTmpFileName[MAXPGPATH];

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser to execute ALTER SYSTEM command"))));
  }

     
                                 
     
  name = altersysstmt->setstmt->name;

  switch (altersysstmt->setstmt->kind)
  {
  case VAR_SET_VALUE:
    value = ExtractSetVariableArgs(altersysstmt->setstmt);
    break;

  case VAR_SET_DEFAULT:
  case VAR_RESET:
    value = NULL;
    break;

  case VAR_RESET_ALL:
    value = NULL;
    resetall = true;
    break;

  default:
    elog(ERROR, "unrecognized alter system stmt type: %d", altersysstmt->setstmt->kind);
    break;
  }

     
                                                                   
     
  if (!resetall)
  {
    struct config_generic *record;

    record = find_option(name, false, ERROR);
    if (record == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
    }

       
                                                                          
                                            
       
    if ((record->context == PGC_INTERNAL) || (record->flags & GUC_DISALLOW_IN_FILE) || (record->flags & GUC_DISALLOW_IN_AUTO_FILE))
    {
      ereport(ERROR, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), errmsg("parameter \"%s\" cannot be changed", name)));
    }

       
                                                       
       
    if (value)
    {
      union config_var_val newval;
      void *newextra = NULL;

                                                                  
      if (!parse_and_validate_value(record, name, value, PGC_S_FILE, ERROR, &newval, &newextra))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for parameter \"%s\": \"%s\"", name, value)));
      }

      if (record->vartype == PGC_STRING && newval.stringval != NULL)
      {
        free(newval.stringval);
      }
      if (newextra)
      {
        free(newextra);
      }

         
                                                                     
                                                                       
                          
         
      if (strchr(value, '\n'))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("parameter value for ALTER SYSTEM must not contain a newline")));
      }
    }
  }

     
                                                                             
                                                                            
     
  snprintf(AutoConfFileName, sizeof(AutoConfFileName), "%s", PG_AUTOCONF_FILENAME);
  snprintf(AutoConfTmpFileName, sizeof(AutoConfTmpFileName), "%s.%s", AutoConfFileName, "tmp");

     
                                                                         
                                                                          
                                    
     
  LWLockAcquire(AutoFileLock, LW_EXCLUSIVE);

     
                                                                           
                                                    
     
  if (!resetall)
  {
    struct stat st;

    if (stat(AutoConfFileName, &st) == 0)
    {
                                              
      FILE *infile;

      infile = AllocateFile(AutoConfFileName, "r");
      if (infile == NULL)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", AutoConfFileName)));
      }

                    
      if (!ParseConfigFp(infile, AutoConfFileName, 0, LOG, &head, &tail))
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not parse contents of file \"%s\"", AutoConfFileName)));
      }

      FreeFile(infile);
    }

       
                                                                        
                    
       
    replace_auto_config_value(&head, &tail, name, value);
  }

     
                                                                           
                                           
     
                                                                             
                            
     
  Tmpfd = BasicOpenFile(AutoConfTmpFileName, O_CREAT | O_RDWR | O_TRUNC);
  if (Tmpfd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", AutoConfTmpFileName)));
  }

     
                                                                           
                                                                          
     
  PG_TRY();
  {
                                                               
    write_auto_conf_file(Tmpfd, AutoConfTmpFileName, head);

                                                                  
    close(Tmpfd);
    Tmpfd = -1;

       
                                                                           
                                                                    
                
       
    durable_rename(AutoConfTmpFileName, AutoConfFileName, ERROR);
  }
  PG_CATCH();
  {
                                                                    
    if (Tmpfd >= 0)
    {
      close(Tmpfd);
    }

                                      
    (void)unlink(AutoConfTmpFileName);

    PG_RE_THROW();
  }
  PG_END_TRY();

  FreeConfigVariables(head);

  LWLockRelease(AutoFileLock);
}

   
               
   
void
ExecSetVariableStmt(VariableSetStmt *stmt, bool isTopLevel)
{
  GucAction action = stmt->is_local ? GUC_ACTION_LOCAL : GUC_ACTION_SET;

     
                                                                       
                                                         
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot set parameters during a parallel operation")));
  }

  switch (stmt->kind)
  {
  case VAR_SET_VALUE:
  case VAR_SET_CURRENT:
    if (stmt->is_local)
    {
      WarnNoTransactionBlock(isTopLevel, "SET LOCAL");
    }
    (void)set_config_option(stmt->name, ExtractSetVariableArgs(stmt), (superuser() ? PGC_SUSET : PGC_USERSET), PGC_S_SESSION, action, true, 0, false);
    break;
  case VAR_SET_MULTI:

       
                                                               
                                                                    
                                                                     
                                                                   
                                    
       
    if (strcmp(stmt->name, "TRANSACTION") == 0)
    {
      ListCell *head;

      WarnNoTransactionBlock(isTopLevel, "SET TRANSACTION");

      foreach (head, stmt->args)
      {
        DefElem *item = (DefElem *)lfirst(head);

        if (strcmp(item->defname, "transaction_isolation") == 0)
        {
          SetPGVariable("transaction_isolation", list_make1(item->arg), stmt->is_local);
        }
        else if (strcmp(item->defname, "transaction_read_only") == 0)
        {
          SetPGVariable("transaction_read_only", list_make1(item->arg), stmt->is_local);
        }
        else if (strcmp(item->defname, "transaction_deferrable") == 0)
        {
          SetPGVariable("transaction_deferrable", list_make1(item->arg), stmt->is_local);
        }
        else
        {
          elog(ERROR, "unexpected SET TRANSACTION element: %s", item->defname);
        }
      }
    }
    else if (strcmp(stmt->name, "SESSION CHARACTERISTICS") == 0)
    {
      ListCell *head;

      foreach (head, stmt->args)
      {
        DefElem *item = (DefElem *)lfirst(head);

        if (strcmp(item->defname, "transaction_isolation") == 0)
        {
          SetPGVariable("default_transaction_isolation", list_make1(item->arg), stmt->is_local);
        }
        else if (strcmp(item->defname, "transaction_read_only") == 0)
        {
          SetPGVariable("default_transaction_read_only", list_make1(item->arg), stmt->is_local);
        }
        else if (strcmp(item->defname, "transaction_deferrable") == 0)
        {
          SetPGVariable("default_transaction_deferrable", list_make1(item->arg), stmt->is_local);
        }
        else
        {
          elog(ERROR, "unexpected SET SESSION element: %s", item->defname);
        }
      }
    }
    else if (strcmp(stmt->name, "TRANSACTION SNAPSHOT") == 0)
    {
      A_Const *con = linitial_node(A_Const, stmt->args);

      if (stmt->is_local)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("SET LOCAL TRANSACTION SNAPSHOT is not implemented")));
      }

      WarnNoTransactionBlock(isTopLevel, "SET TRANSACTION");
      Assert(nodeTag(&con->val) == T_String);
      ImportSnapshot(strVal(&con->val));
    }
    else
    {
      elog(ERROR, "unexpected SET MULTI element: %s", stmt->name);
    }
    break;
  case VAR_SET_DEFAULT:
    if (stmt->is_local)
    {
      WarnNoTransactionBlock(isTopLevel, "SET LOCAL");
    }
                      
  case VAR_RESET:
    if (strcmp(stmt->name, "transaction_isolation") == 0)
    {
      WarnNoTransactionBlock(isTopLevel, "RESET TRANSACTION");
    }

    (void)set_config_option(stmt->name, NULL, (superuser() ? PGC_SUSET : PGC_USERSET), PGC_S_SESSION, action, true, 0, false);
    break;
  case VAR_RESET_ALL:
    ResetAllOptions();
    break;
  }
}

   
                                                                         
                           
   
                                                               
   
char *
ExtractSetVariableArgs(VariableSetStmt *stmt)
{
  switch (stmt->kind)
  {
  case VAR_SET_VALUE:
    return flatten_set_variable_args(stmt->name, stmt->args);
  case VAR_SET_CURRENT:
    return GetConfigOptionByName(stmt->name, NULL, false);
  default:
    return NULL;
  }
}

   
                                                                          
   
                                                                              
                                                                    
   
void
SetPGVariable(const char *name, List *args, bool is_local)
{
  char *argstring = flatten_set_variable_args(name, args);

                                                                   
  (void)set_config_option(name, argstring, (superuser() ? PGC_SUSET : PGC_USERSET), PGC_S_SESSION, is_local ? GUC_ACTION_LOCAL : GUC_ACTION_SET, true, 0, false);
}

   
                                                   
   
Datum
set_config_by_name(PG_FUNCTION_ARGS)
{
  char *name;
  char *value;
  char *new_value;
  bool is_local;

  if (PG_ARGISNULL(0))
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("SET requires parameter name")));
  }

                                 
  name = TextDatumGetCString(PG_GETARG_DATUM(0));

                                                                
  if (PG_ARGISNULL(1))
  {
    value = NULL;
  }
  else
  {
    value = TextDatumGetCString(PG_GETARG_DATUM(1));
  }

     
                                                                           
             
     
  if (PG_ARGISNULL(2))
  {
    is_local = false;
  }
  else
  {
    is_local = PG_GETARG_BOOL(2);
  }

                                                                   
  (void)set_config_option(name, value, (superuser() ? PGC_SUSET : PGC_USERSET), PGC_S_SESSION, is_local ? GUC_ACTION_LOCAL : GUC_ACTION_SET, true, 0, false);

                                 
  new_value = GetConfigOptionByName(name, NULL, false);

                                     
  PG_RETURN_TEXT_P(cstring_to_text(new_value));
}

   
                                                                     
                                                            
   
static struct config_generic *
init_custom_variable(const char *name, const char *short_desc, const char *long_desc, GucContext context, int flags, enum config_type type, size_t sz)
{
  struct config_generic *gen;

     
                                                                            
                                                                          
                                                                             
                                                                             
                                            
     
  if (context == PGC_POSTMASTER && !process_shared_preload_libraries_in_progress)
  {
    elog(FATAL, "cannot create PGC_POSTMASTER variables after startup");
  }

     
                                                                         
                                                                           
                                                                          
                                                            
     
  if (flags & GUC_LIST_QUOTE)
  {
    elog(FATAL, "extensions cannot define GUC_LIST_QUOTE variables");
  }

     
                                                                   
                                                                          
                                                                            
                                                    
     
  if (context == PGC_USERSET && (strcmp(name, "pljava.classpath") == 0 || strcmp(name, "pljava.vmoptions") == 0))
  {
    context = PGC_SUSET;
  }

  gen = (struct config_generic *)guc_malloc(ERROR, sz);
  memset(gen, 0, sz);

  gen->name = guc_strdup(ERROR, name);
  gen->context = context;
  gen->group = CUSTOM_OPTIONS;
  gen->short_desc = short_desc;
  gen->long_desc = long_desc;
  gen->flags = flags;
  gen->vartype = type;

  return gen;
}

   
                                                                       
                                                                    
   
static void
define_custom_variable(struct config_generic *variable)
{
  const char *name = variable->name;
  const char **nameAddr = &name;
  struct config_string *pHolder;
  struct config_generic **res;

     
                                                    
     
  res = (struct config_generic **)bsearch((void *)&nameAddr, (void *)guc_variables, num_guc_variables, sizeof(struct config_generic *), guc_var_compare);
  if (res == NULL)
  {
       
                                                                       
                                                        
       
    InitializeOneGUCOption(variable);
    add_guc_variable(variable, ERROR);
    return;
  }

     
                                  
     
  if (((*res)->flags & GUC_CUSTOM_PLACEHOLDER) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("attempt to redefine parameter \"%s\"", name)));
  }

  Assert((*res)->vartype == PGC_STRING);
  pHolder = (struct config_string *)(*res);

     
                                                                         
                                                                            
                                    
     
  InitializeOneGUCOption(variable);

     
                                                                            
                  
     
  *res = variable;

     
                                                                      
                                                                             
                                                                      
     
                                                                           
                                                                        
                                                                             
                                                               
     

                                           
  if (pHolder->reset_val)
  {
    (void)set_config_option(name, pHolder->reset_val, pHolder->gen.reset_scontext, pHolder->gen.reset_source, GUC_ACTION_SET, true, WARNING, false);
  }
                                                          
  Assert(variable->stack == NULL);

                                                                             
  reapply_stacked_values(variable, pHolder, pHolder->gen.stack, *(pHolder->variable), pHolder->gen.scontext, pHolder->gen.source);

                                                            
  if (pHolder->gen.sourcefile)
  {
    set_config_sourcefile(name, pHolder->gen.sourcefile, pHolder->gen.sourceline);
  }

     
                                                                          
                                                                            
                                                                           
                                                
     
  set_string_field(pHolder, pHolder->variable, NULL);
  set_string_field(pHolder, &pHolder->reset_val, NULL);

  free(pHolder);
}

   
                                                                             
   
                                                                              
                                                                           
                                       
   
static void
reapply_stacked_values(struct config_generic *variable, struct config_string *pHolder, GucStack *stack, const char *curvalue, GucContext curscontext, GucSource cursource)
{
  const char *name = variable->name;
  GucStack *oldvarstack = variable->stack;

  if (stack != NULL)
  {
                                                                         
    reapply_stacked_values(variable, pHolder, stack->prev, stack->prior.val.stringval, stack->scontext, stack->source);

                                              
    switch (stack->state)
    {
    case GUC_SAVE:
      (void)set_config_option(name, curvalue, curscontext, cursource, GUC_ACTION_SAVE, true, WARNING, false);
      break;

    case GUC_SET:
      (void)set_config_option(name, curvalue, curscontext, cursource, GUC_ACTION_SET, true, WARNING, false);
      break;

    case GUC_LOCAL:
      (void)set_config_option(name, curvalue, curscontext, cursource, GUC_ACTION_LOCAL, true, WARNING, false);
      break;

    case GUC_SET_LOCAL:
                                                
      (void)set_config_option(name, stack->masked.val.stringval, stack->masked_scontext, PGC_S_SESSION, GUC_ACTION_SET, true, WARNING, false);
                                                 
      (void)set_config_option(name, curvalue, curscontext, cursource, GUC_ACTION_LOCAL, true, WARNING, false);
      break;
    }

                                                                      
    if (variable->stack != oldvarstack)
    {
      variable->stack->nest_level = stack->nest_level;
    }
  }
  else
  {
       
                                                                        
                                                                      
                                                                         
                                                                          
                                                                          
               
       
    if (curvalue != pHolder->reset_val || curscontext != pHolder->gen.reset_scontext || cursource != pHolder->gen.reset_source)
    {
      (void)set_config_option(name, curvalue, curscontext, cursource, GUC_ACTION_SET, true, WARNING, false);
      variable->stack = NULL;
    }
  }
}

void
DefineCustomBoolVariable(const char *name, const char *short_desc, const char *long_desc, bool *valueAddr, bool bootValue, GucContext context, int flags, GucBoolCheckHook check_hook, GucBoolAssignHook assign_hook, GucShowHook show_hook)
{
  struct config_bool *var;

  var = (struct config_bool *)init_custom_variable(name, short_desc, long_desc, context, flags, PGC_BOOL, sizeof(struct config_bool));
  var->variable = valueAddr;
  var->boot_val = bootValue;
  var->reset_val = bootValue;
  var->check_hook = check_hook;
  var->assign_hook = assign_hook;
  var->show_hook = show_hook;
  define_custom_variable(&var->gen);
}

void
DefineCustomIntVariable(const char *name, const char *short_desc, const char *long_desc, int *valueAddr, int bootValue, int minValue, int maxValue, GucContext context, int flags, GucIntCheckHook check_hook, GucIntAssignHook assign_hook, GucShowHook show_hook)
{
  struct config_int *var;

  var = (struct config_int *)init_custom_variable(name, short_desc, long_desc, context, flags, PGC_INT, sizeof(struct config_int));
  var->variable = valueAddr;
  var->boot_val = bootValue;
  var->reset_val = bootValue;
  var->min = minValue;
  var->max = maxValue;
  var->check_hook = check_hook;
  var->assign_hook = assign_hook;
  var->show_hook = show_hook;
  define_custom_variable(&var->gen);
}

void
DefineCustomRealVariable(const char *name, const char *short_desc, const char *long_desc, double *valueAddr, double bootValue, double minValue, double maxValue, GucContext context, int flags, GucRealCheckHook check_hook, GucRealAssignHook assign_hook, GucShowHook show_hook)
{
  struct config_real *var;

  var = (struct config_real *)init_custom_variable(name, short_desc, long_desc, context, flags, PGC_REAL, sizeof(struct config_real));
  var->variable = valueAddr;
  var->boot_val = bootValue;
  var->reset_val = bootValue;
  var->min = minValue;
  var->max = maxValue;
  var->check_hook = check_hook;
  var->assign_hook = assign_hook;
  var->show_hook = show_hook;
  define_custom_variable(&var->gen);
}

void
DefineCustomStringVariable(const char *name, const char *short_desc, const char *long_desc, char **valueAddr, const char *bootValue, GucContext context, int flags, GucStringCheckHook check_hook, GucStringAssignHook assign_hook, GucShowHook show_hook)
{
  struct config_string *var;

  var = (struct config_string *)init_custom_variable(name, short_desc, long_desc, context, flags, PGC_STRING, sizeof(struct config_string));
  var->variable = valueAddr;
  var->boot_val = bootValue;
  var->check_hook = check_hook;
  var->assign_hook = assign_hook;
  var->show_hook = show_hook;
  define_custom_variable(&var->gen);
}

void
DefineCustomEnumVariable(const char *name, const char *short_desc, const char *long_desc, int *valueAddr, int bootValue, const struct config_enum_entry *options, GucContext context, int flags, GucEnumCheckHook check_hook, GucEnumAssignHook assign_hook, GucShowHook show_hook)
{
  struct config_enum *var;

  var = (struct config_enum *)init_custom_variable(name, short_desc, long_desc, context, flags, PGC_ENUM, sizeof(struct config_enum));
  var->variable = valueAddr;
  var->boot_val = bootValue;
  var->reset_val = bootValue;
  var->options = options;
  var->check_hook = check_hook;
  var->assign_hook = assign_hook;
  var->show_hook = show_hook;
  define_custom_variable(&var->gen);
}

void
EmitWarningsOnPlaceholders(const char *className)
{
  int classLen = strlen(className);
  int i;

  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *var = guc_variables[i];

    if ((var->flags & GUC_CUSTOM_PLACEHOLDER) != 0 && strncmp(className, var->name, classLen) == 0 && var->name[classLen] == GUC_QUALIFIER_SEPARATOR)
    {
      ereport(WARNING, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", var->name)));
    }
  }
}

   
                
   
void
GetPGVariable(const char *name, DestReceiver *dest)
{
  if (guc_name_compare(name, "all") == 0)
  {
    ShowAllGUCConfig(dest);
  }
  else
  {
    ShowGUCConfigOption(name, dest);
  }
}

TupleDesc
GetPGVariableResultDesc(const char *name)
{
  TupleDesc tupdesc;

  if (guc_name_compare(name, "all") == 0)
  {
                                                                 
    tupdesc = CreateTemplateTupleDesc(3);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "setting", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "description", TEXTOID, -1, 0);
  }
  else
  {
    const char *varname;

                                            
    (void)GetConfigOptionByName(name, &varname, false);

                                                                   
    tupdesc = CreateTemplateTupleDesc(1);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, varname, TEXTOID, -1, 0);
  }
  return tupdesc;
}

   
                
   
static void
ShowGUCConfigOption(const char *name, DestReceiver *dest)
{
  TupOutputState *tstate;
  TupleDesc tupdesc;
  const char *varname;
  char *value;

                                                    
  value = GetConfigOptionByName(name, &varname, false);

                                                                 
  tupdesc = CreateTemplateTupleDesc(1);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)1, varname, TEXTOID, -1, 0);

                                        
  tstate = begin_tup_output_tupdesc(dest, tupdesc, &TTSOpsVirtual);

               
  do_text_output_oneline(tstate, value);

  end_tup_output(tstate);
}

   
                    
   
static void
ShowAllGUCConfig(DestReceiver *dest)
{
  int i;
  TupOutputState *tstate;
  TupleDesc tupdesc;
  Datum values[3];
  bool isnull[3] = {false, false, false};

                                                               
  tupdesc = CreateTemplateTupleDesc(3);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)1, "name", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)2, "setting", TEXTOID, -1, 0);
  TupleDescInitBuiltinEntry(tupdesc, (AttrNumber)3, "description", TEXTOID, -1, 0);

                                        
  tstate = begin_tup_output_tupdesc(dest, tupdesc, &TTSOpsVirtual);

  for (i = 0; i < num_guc_variables; i++)
  {
    struct config_generic *conf = guc_variables[i];
    char *setting;

    if ((conf->flags & GUC_NO_SHOW_ALL) || ((conf->flags & GUC_SUPERUSER_ONLY) && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS)))
    {
      continue;
    }

                                    
    values[0] = PointerGetDatum(cstring_to_text(conf->name));

    setting = _ShowOption(conf, true);
    if (setting)
    {
      values[1] = PointerGetDatum(cstring_to_text(setting));
      isnull[1] = false;
    }
    else
    {
      values[1] = PointerGetDatum(NULL);
      isnull[1] = true;
    }

    if (conf->short_desc)
    {
      values[2] = PointerGetDatum(cstring_to_text(conf->short_desc));
      isnull[2] = false;
    }
    else
    {
      values[2] = PointerGetDatum(NULL);
      isnull[2] = true;
    }

                         
    do_tup_output(tstate, values, isnull);

                  
    pfree(DatumGetPointer(values[0]));
    if (setting)
    {
      pfree(setting);
      pfree(DatumGetPointer(values[1]));
    }
    if (conf->short_desc)
    {
      pfree(DatumGetPointer(values[2]));
    }
  }

  end_tup_output(tstate);
}

   
                                                               
   
                                                                               
                                                       
   
struct config_generic **
get_explain_guc_options(int *num)
{
  struct config_generic **result;

  *num = 0;

     
                                                                            
                                                            
     
  result = palloc(sizeof(struct config_generic *) * num_guc_variables);

  for (int i = 0; i < num_guc_variables; i++)
  {
    bool modified;
    struct config_generic *conf = guc_variables[i];

                                                                
    if (!(conf->flags & GUC_EXPLAIN))
    {
      continue;
    }

                                                         
    if ((conf->flags & GUC_NO_SHOW_ALL) || ((conf->flags & GUC_SUPERUSER_ONLY) && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS)))
    {
      continue;
    }

                                                                       
    modified = false;

    switch (conf->vartype)
    {
    case PGC_BOOL:
    {
      struct config_bool *lconf = (struct config_bool *)conf;

      modified = (lconf->boot_val != *(lconf->variable));
    }
    break;

    case PGC_INT:
    {
      struct config_int *lconf = (struct config_int *)conf;

      modified = (lconf->boot_val != *(lconf->variable));
    }
    break;

    case PGC_REAL:
    {
      struct config_real *lconf = (struct config_real *)conf;

      modified = (lconf->boot_val != *(lconf->variable));
    }
    break;

    case PGC_STRING:
    {
      struct config_string *lconf = (struct config_string *)conf;

      modified = (strcmp(lconf->boot_val, *(lconf->variable)) != 0);
    }
    break;

    case PGC_ENUM:
    {
      struct config_enum *lconf = (struct config_enum *)conf;

      modified = (lconf->boot_val != *(lconf->variable));
    }
    break;

    default:
      elog(ERROR, "unexpected GUC type: %d", conf->vartype);
    }

    if (!modified)
    {
      continue;
    }

                       
    result[*num] = conf;
    *num = *num + 1;
  }

  return result;
}

   
                                                                          
                                                                              
                                                                              
   
char *
GetConfigOptionByName(const char *name, const char **varname, bool missing_ok)
{
  struct config_generic *record;

  record = find_option(name, false, ERROR);
  if (record == NULL)
  {
    if (missing_ok)
    {
      if (varname)
      {
        *varname = NULL;
      }
      return NULL;
    }

    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
  }

  if ((record->flags & GUC_SUPERUSER_ONLY) && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or a member of pg_read_all_settings to examine \"%s\"", name)));
  }

  if (varname)
  {
    *varname = record->name;
  }

  return _ShowOption(record, true);
}

   
                                                                             
                                            
   
void
GetConfigOptionByNum(int varnum, const char **values, bool *noshow)
{
  char buffer[256];
  struct config_generic *conf;

                                             
  Assert((varnum >= 0) && (varnum < num_guc_variables));

  conf = guc_variables[varnum];

  if (noshow)
  {
    if ((conf->flags & GUC_NO_SHOW_ALL) || ((conf->flags & GUC_SUPERUSER_ONLY) && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS)))
    {
      *noshow = true;
    }
    else
    {
      *noshow = false;
    }
  }

                                        

            
  values[0] = conf->name;

                                                                        
  values[1] = _ShowOption(conf, false);

                                   
  values[2] = get_config_unit_name(conf->flags);

             
  values[3] = _(config_group_names[conf->group]);

                  
  values[4] = conf->short_desc != NULL ? _(conf->short_desc) : NULL;

                  
  values[5] = conf->long_desc != NULL ? _(conf->long_desc) : NULL;

               
  values[6] = GucContext_Names[conf->context];

               
  values[7] = config_type_names[conf->vartype];

              
  values[8] = GucSource_Names[conf->source];

                                            
  switch (conf->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *lconf = (struct config_bool *)conf;

                 
    values[9] = NULL;

                 
    values[10] = NULL;

                  
    values[11] = NULL;

                  
    values[12] = pstrdup(lconf->boot_val ? "on" : "off");

                   
    values[13] = pstrdup(lconf->reset_val ? "on" : "off");
  }
  break;

  case PGC_INT:
  {
    struct config_int *lconf = (struct config_int *)conf;

                 
    snprintf(buffer, sizeof(buffer), "%d", lconf->min);
    values[9] = pstrdup(buffer);

                 
    snprintf(buffer, sizeof(buffer), "%d", lconf->max);
    values[10] = pstrdup(buffer);

                  
    values[11] = NULL;

                  
    snprintf(buffer, sizeof(buffer), "%d", lconf->boot_val);
    values[12] = pstrdup(buffer);

                   
    snprintf(buffer, sizeof(buffer), "%d", lconf->reset_val);
    values[13] = pstrdup(buffer);
  }
  break;

  case PGC_REAL:
  {
    struct config_real *lconf = (struct config_real *)conf;

                 
    snprintf(buffer, sizeof(buffer), "%g", lconf->min);
    values[9] = pstrdup(buffer);

                 
    snprintf(buffer, sizeof(buffer), "%g", lconf->max);
    values[10] = pstrdup(buffer);

                  
    values[11] = NULL;

                  
    snprintf(buffer, sizeof(buffer), "%g", lconf->boot_val);
    values[12] = pstrdup(buffer);

                   
    snprintf(buffer, sizeof(buffer), "%g", lconf->reset_val);
    values[13] = pstrdup(buffer);
  }
  break;

  case PGC_STRING:
  {
    struct config_string *lconf = (struct config_string *)conf;

                 
    values[9] = NULL;

                 
    values[10] = NULL;

                  
    values[11] = NULL;

                  
    if (lconf->boot_val == NULL)
    {
      values[12] = NULL;
    }
    else
    {
      values[12] = pstrdup(lconf->boot_val);
    }

                   
    if (lconf->reset_val == NULL)
    {
      values[13] = NULL;
    }
    else
    {
      values[13] = pstrdup(lconf->reset_val);
    }
  }
  break;

  case PGC_ENUM:
  {
    struct config_enum *lconf = (struct config_enum *)conf;

                 
    values[9] = NULL;

                 
    values[10] = NULL;

                  

       
                                                         
                  
       
    values[11] = config_enum_get_options((struct config_enum *)conf, "{\"", "\"}", "\",\"");

                  
    values[12] = pstrdup(config_enum_lookup_by_value(lconf, lconf->boot_val));

                   
    values[13] = pstrdup(config_enum_lookup_by_value(lconf, lconf->reset_val));
  }
  break;

  default:
  {
       
                                                                 
       

                 
    values[9] = NULL;

                 
    values[10] = NULL;

                  
    values[11] = NULL;

                  
    values[12] = NULL;

                   
    values[13] = NULL;
  }
  break;
  }

     
                                                                          
                                                                 
                                      
     
  if (conf->source == PGC_S_FILE && is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_SETTINGS))
  {
    values[14] = conf->sourcefile;
    snprintf(buffer, sizeof(buffer), "%d", conf->sourceline);
    values[15] = pstrdup(buffer);
  }
  else
  {
    values[14] = NULL;
    values[15] = NULL;
  }

  values[16] = (conf->status & GUC_PENDING_RESTART) ? "t" : "f";
}

   
                                            
   
int
GetNumConfigOptions(void)
{
  return num_guc_variables;
}

   
                                                                    
               
   
Datum
show_config_by_name(PG_FUNCTION_ARGS)
{
  char *varname = TextDatumGetCString(PG_GETARG_DATUM(0));
  char *varval;

                     
  varval = GetConfigOptionByName(varname, NULL, false);

                       
  PG_RETURN_TEXT_P(cstring_to_text(varval));
}

   
                                                                               
                                                                             
                          
   
Datum
show_config_by_name_missing_ok(PG_FUNCTION_ARGS)
{
  char *varname = TextDatumGetCString(PG_GETARG_DATUM(0));
  bool missing_ok = PG_GETARG_BOOL(1);
  char *varval;

                     
  varval = GetConfigOptionByName(varname, NULL, missing_ok);

                                       
  if (varval == NULL)
  {
    PG_RETURN_NULL();
  }

                       
  PG_RETURN_TEXT_P(cstring_to_text(varval));
}

   
                                                                    
                     
   
#define NUM_PG_SETTINGS_ATTS 17

Datum
show_all_settings(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  TupleDesc tupdesc;
  int call_cntr;
  int max_calls;
  AttInMetadata *attinmeta;
  MemoryContext oldcontext;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

       
                                                                         
                                
       
    tupdesc = CreateTemplateTupleDesc(NUM_PG_SETTINGS_ATTS);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "setting", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "unit", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)4, "category", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)5, "short_desc", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)6, "extra_desc", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)7, "context", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)8, "vartype", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)9, "source", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)10, "min_val", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)11, "max_val", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)12, "enumvals", TEXTARRAYOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)13, "boot_val", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)14, "reset_val", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)15, "sourcefile", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)16, "sourceline", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)17, "pending_restart", BOOLOID, -1, 0);

       
                                                                           
                 
       
    attinmeta = TupleDescGetAttInMetadata(tupdesc);
    funcctx->attinmeta = attinmeta;

                                               
    funcctx->max_calls = GetNumConfigOptions();

    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();

  call_cntr = funcctx->call_cntr;
  max_calls = funcctx->max_calls;
  attinmeta = funcctx->attinmeta;

  if (call_cntr < max_calls)                                         
  {
    char *values[NUM_PG_SETTINGS_ATTS];
    bool noshow;
    HeapTuple tuple;
    Datum result;

       
                                                        
       
    do
    {
      GetConfigOptionByNum(call_cntr, (const char **)values, &noshow);
      if (noshow)
      {
                                                              
        call_cntr = ++funcctx->call_cntr;

                                                   
        if (call_cntr >= max_calls)
        {
          SRF_RETURN_DONE(funcctx);
        }
      }
    } while (noshow);

                       
    tuple = BuildTupleFromCStrings(attinmeta, values);

                                     
    result = HeapTupleGetDatum(tuple);

    SRF_RETURN_NEXT(funcctx, result);
  }
  else
  {
                                       
    SRF_RETURN_DONE(funcctx);
  }
}

   
                          
   
                                                                        
                                                                               
                                                                              
                                                                              
                                                                          
                                        
   
                                                                          
                                                                          
                       
   
Datum
show_all_file_settings(PG_FUNCTION_ARGS)
{
#define NUM_PG_FILE_SETTINGS_ATTS 7
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  ConfigVariable *conf;
  int seqno;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not "
                                                                   "allowed in this context")));
  }

                                                                
  conf = ProcessConfigFileInternal(PGC_SIGHUP, false, DEBUG3);

                                                                            
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

                                                    
  tupdesc = CreateTemplateTupleDesc(NUM_PG_FILE_SETTINGS_ATTS);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "sourcefile", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "sourceline", INT4OID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)3, "seqno", INT4OID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)4, "name", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)5, "setting", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)6, "applied", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)7, "error", TEXTOID, -1, 0);

                                                   
  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

                                                   
  MemoryContextSwitchTo(oldcontext);

                                                   
  for (seqno = 1; conf != NULL; conf = conf->next, seqno++)
  {
    Datum values[NUM_PG_FILE_SETTINGS_ATTS];
    bool nulls[NUM_PG_FILE_SETTINGS_ATTS];

    memset(values, 0, sizeof(values));
    memset(nulls, 0, sizeof(nulls));

                    
    if (conf->filename)
    {
      values[0] = PointerGetDatum(cstring_to_text(conf->filename));
    }
    else
    {
      nulls[0] = true;
    }

                                                      
    if (conf->filename)
    {
      values[1] = Int32GetDatum(conf->sourceline);
    }
    else
    {
      nulls[1] = true;
    }

               
    values[2] = Int32GetDatum(seqno);

              
    if (conf->name)
    {
      values[3] = PointerGetDatum(cstring_to_text(conf->name));
    }
    else
    {
      nulls[3] = true;
    }

                 
    if (conf->value)
    {
      values[4] = PointerGetDatum(cstring_to_text(conf->value));
    }
    else
    {
      nulls[4] = true;
    }

                 
    values[5] = BoolGetDatum(conf->applied);

               
    if (conf->errmsg)
    {
      values[6] = PointerGetDatum(cstring_to_text(conf->errmsg));
    }
    else
    {
      nulls[6] = true;
    }

                                   
    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

static char *
_ShowOption(struct config_generic *record, bool use_units)
{
  char buffer[256];
  const char *val;

  switch (record->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *conf = (struct config_bool *)record;

    if (conf->show_hook)
    {
      val = conf->show_hook();
    }
    else
    {
      val = *conf->variable ? "on" : "off";
    }
  }
  break;

  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)record;

    if (conf->show_hook)
    {
      val = conf->show_hook();
    }
    else
    {
         
                                                          
                     
         
      int64 result = *conf->variable;
      const char *unit;

      if (use_units && result > 0 && (record->flags & GUC_UNIT))
      {
        convert_int_from_base_unit(result, record->flags & GUC_UNIT, &result, &unit);
      }
      else
      {
        unit = "";
      }

      snprintf(buffer, sizeof(buffer), INT64_FORMAT "%s", result, unit);
      val = buffer;
    }
  }
  break;

  case PGC_REAL:
  {
    struct config_real *conf = (struct config_real *)record;

    if (conf->show_hook)
    {
      val = conf->show_hook();
    }
    else
    {
      double result = *conf->variable;
      const char *unit;

      if (use_units && result > 0 && (record->flags & GUC_UNIT))
      {
        convert_real_from_base_unit(result, record->flags & GUC_UNIT, &result, &unit);
      }
      else
      {
        unit = "";
      }

      snprintf(buffer, sizeof(buffer), "%g%s", result, unit);
      val = buffer;
    }
  }
  break;

  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)record;

    if (conf->show_hook)
    {
      val = conf->show_hook();
    }
    else if (*conf->variable && **conf->variable)
    {
      val = *conf->variable;
    }
    else
    {
      val = "";
    }
  }
  break;

  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)record;

    if (conf->show_hook)
    {
      val = conf->show_hook();
    }
    else
    {
      val = config_enum_lookup_by_value(conf, *conf->variable);
    }
  }
  break;

  default:
                                     
    val = "???";
    break;
  }

  return pstrdup(val);
}

#ifdef EXEC_BACKEND

   
                                                                     
                                                              
   
                                           
                                            
                                                                 
                                 
                             
                               
   
static void
write_one_nondefault_variable(FILE *fp, struct config_generic *gconf)
{
  if (gconf->source == PGC_S_DEFAULT)
  {
    return;
  }

  fprintf(fp, "%s", gconf->name);
  fputc(0, fp);

  switch (gconf->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *conf = (struct config_bool *)gconf;

    if (*conf->variable)
    {
      fprintf(fp, "true");
    }
    else
    {
      fprintf(fp, "false");
    }
  }
  break;

  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)gconf;

    fprintf(fp, "%d", *conf->variable);
  }
  break;

  case PGC_REAL:
  {
    struct config_real *conf = (struct config_real *)gconf;

    fprintf(fp, "%.17g", *conf->variable);
  }
  break;

  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)gconf;

    fprintf(fp, "%s", *conf->variable);
  }
  break;

  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)gconf;

    fprintf(fp, "%s", config_enum_lookup_by_value(conf, *conf->variable));
  }
  break;
  }

  fputc(0, fp);

  if (gconf->sourcefile)
  {
    fprintf(fp, "%s", gconf->sourcefile);
  }
  fputc(0, fp);

  fwrite(&gconf->sourceline, 1, sizeof(gconf->sourceline), fp);
  fwrite(&gconf->source, 1, sizeof(gconf->source), fp);
  fwrite(&gconf->scontext, 1, sizeof(gconf->scontext), fp);
}

void
write_nondefault_variables(GucContext context)
{
  int elevel;
  FILE *fp;
  int i;

  Assert(context == PGC_POSTMASTER || context == PGC_SIGHUP);

  elevel = (context == PGC_SIGHUP) ? LOG : ERROR;

     
               
     
  fp = AllocateFile(CONFIG_EXEC_PARAMS_NEW, "w");
  if (!fp)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", CONFIG_EXEC_PARAMS_NEW)));
    return;
  }

  for (i = 0; i < num_guc_variables; i++)
  {
    write_one_nondefault_variable(fp, guc_variables[i]);
  }

  if (FreeFile(fp))
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", CONFIG_EXEC_PARAMS_NEW)));
    return;
  }

     
                                                                          
                          
     
  rename(CONFIG_EXEC_PARAMS_NEW, CONFIG_EXEC_PARAMS);
}

   
                                              
   
                                       
   
static char *
read_string_with_null(FILE *fp)
{
  int i = 0, ch, maxlen = 256;
  char *str = NULL;

  do
  {
    if ((ch = fgetc(fp)) == EOF)
    {
      if (i == 0)
      {
        return NULL;
      }
      else
      {
        elog(FATAL, "invalid format of exec config params file");
      }
    }
    if (i == 0)
    {
      str = guc_malloc(FATAL, maxlen);
    }
    else if (i == maxlen)
    {
      str = guc_realloc(FATAL, str, maxlen *= 2);
    }
    str[i++] = ch;
  } while (ch != 0);

  return str;
}

   
                                                                    
             
   
void
read_nondefault_variables(void)
{
  FILE *fp;
  char *varname, *varvalue, *varsourcefile;
  int varsourceline;
  GucSource varsource;
  GucContext varscontext;

     
                                                                             
                         
     
  Assert(IsInitProcessingMode());

     
               
     
  fp = AllocateFile(CONFIG_EXEC_PARAMS, "r");
  if (!fp)
  {
                                
    if (errno != ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not read from file \"%s\": %m", CONFIG_EXEC_PARAMS)));
    }
    return;
  }

  for (;;)
  {
    struct config_generic *record;

    if ((varname = read_string_with_null(fp)) == NULL)
    {
      break;
    }

    if ((record = find_option(varname, true, FATAL)) == NULL)
    {
      elog(FATAL, "failed to locate variable \"%s\" in exec config params file", varname);
    }

    if ((varvalue = read_string_with_null(fp)) == NULL)
    {
      elog(FATAL, "invalid format of exec config params file");
    }
    if ((varsourcefile = read_string_with_null(fp)) == NULL)
    {
      elog(FATAL, "invalid format of exec config params file");
    }
    if (fread(&varsourceline, 1, sizeof(varsourceline), fp) != sizeof(varsourceline))
    {
      elog(FATAL, "invalid format of exec config params file");
    }
    if (fread(&varsource, 1, sizeof(varsource), fp) != sizeof(varsource))
    {
      elog(FATAL, "invalid format of exec config params file");
    }
    if (fread(&varscontext, 1, sizeof(varscontext), fp) != sizeof(varscontext))
    {
      elog(FATAL, "invalid format of exec config params file");
    }

    (void)set_config_option(varname, varvalue, varscontext, varsource, GUC_ACTION_SET, true, 0, true);
    if (varsourcefile[0])
    {
      set_config_sourcefile(varname, varsourcefile, varsourceline);
    }

    free(varname);
    free(varvalue);
    free(varsourcefile);
  }

  FreeFile(fp);
}
#endif                   

   
                    
                                                                              
                                                                               
                                              
   
                                                                          
                                                                              
                                                                             
                                                                               
                                               
   
                                                                           
                                                                        
                                                                              
                                                                            
                                                                
   
                                                                            
                                                                            
                                                                          
                                
   
static bool
can_skip_gucvar(struct config_generic *gconf)
{
  return gconf->context == PGC_POSTMASTER || gconf->context == PGC_INTERNAL || gconf->source == PGC_S_DEFAULT || strcmp(gconf->name, "role") == 0;
}

   
                           
                                                             
   
                                                      
   
static Size
estimate_variable_size(struct config_generic *gconf)
{
  Size size;
  Size valsize = 0;

  if (can_skip_gucvar(gconf))
  {
    return 0;
  }

                                      
  size = strlen(gconf->name) + 1;

                                                        
  switch (gconf->vartype)
  {
  case PGC_BOOL:
  {
    valsize = 5;                                           
  }
  break;

  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)gconf;

       
                                                            
                                                                 
                                                                  
                                  
       
    if (Abs(*conf->variable) < 1000)
    {
      valsize = 3 + 1;
    }
    else
    {
      valsize = 10 + 1;
    }
  }
  break;

  case PGC_REAL:
  {
       
                                                                
                                                            
                                                              
                                 
       
    valsize = 1 + 1 + 1 + REALTYPE_PRECISION + 5;
  }
  break;

  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)gconf;

       
                                                                
                                                           
                                                         
       
    if (*conf->variable)
    {
      valsize = strlen(*conf->variable);
    }
    else
    {
      valsize = 0;
    }
  }
  break;

  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)gconf;

    valsize = strlen(config_enum_lookup_by_value(conf, *conf->variable));
  }
  break;
  }

                                                       
  size = add_size(size, valsize + 1);

  if (gconf->sourcefile)
  {
    size = add_size(size, strlen(gconf->sourcefile));
  }

                                                            
  size = add_size(size, 1);

                                               
  if (gconf->sourcefile && gconf->sourcefile[0])
  {
    size = add_size(size, sizeof(gconf->sourceline));
  }

  size = add_size(size, sizeof(gconf->source));
  size = add_size(size, sizeof(gconf->scontext));

  return size;
}

   
                          
                                                                          
   
Size
EstimateGUCStateSpace(void)
{
  Size size;
  int i;

                                                                
  size = sizeof(Size);

                                                     
  for (i = 0; i < num_guc_variables; i++)
  {
    size = add_size(size, estimate_variable_size(guc_variables[i]));
  }

  return size;
}

   
                 
                                                                      
                                                                           
                                                             
   
static void
do_serialize(char **destptr, Size *maxbytes, const char *fmt, ...)
{
  va_list vargs;
  int n;

  if (*maxbytes <= 0)
  {
    elog(ERROR, "not enough space to serialize GUC state");
  }

  va_start(vargs, fmt);
  n = vsnprintf(*destptr, *maxbytes, fmt, vargs);
  va_end(vargs);

  if (n < 0)
  {
                                                          
    elog(ERROR, "vsnprintf failed: %m with format string \"%s\"", fmt);
  }
  if (n >= *maxbytes)
  {
                                               
    elog(ERROR, "not enough space to serialize GUC state");
  }

                                                      
  *destptr += n + 1;
  *maxbytes -= n + 1;
}

                                           
static void
do_serialize_binary(char **destptr, Size *maxbytes, void *val, Size valsize)
{
  if (valsize > *maxbytes)
  {
    elog(ERROR, "not enough space to serialize GUC state");
  }

  memcpy(*destptr, val, valsize);
  *destptr += valsize;
  *maxbytes -= valsize;
}

   
                       
                                                                           
   
static void
serialize_variable(char **destptr, Size *maxbytes, struct config_generic *gconf)
{
  if (can_skip_gucvar(gconf))
  {
    return;
  }

  do_serialize(destptr, maxbytes, "%s", gconf->name);

  switch (gconf->vartype)
  {
  case PGC_BOOL:
  {
    struct config_bool *conf = (struct config_bool *)gconf;

    do_serialize(destptr, maxbytes, (*conf->variable ? "true" : "false"));
  }
  break;

  case PGC_INT:
  {
    struct config_int *conf = (struct config_int *)gconf;

    do_serialize(destptr, maxbytes, "%d", *conf->variable);
  }
  break;

  case PGC_REAL:
  {
    struct config_real *conf = (struct config_real *)gconf;

    do_serialize(destptr, maxbytes, "%.*e", REALTYPE_PRECISION, *conf->variable);
  }
  break;

  case PGC_STRING:
  {
    struct config_string *conf = (struct config_string *)gconf;

                                                                 
    do_serialize(destptr, maxbytes, "%s", *conf->variable ? *conf->variable : "");
  }
  break;

  case PGC_ENUM:
  {
    struct config_enum *conf = (struct config_enum *)gconf;

    do_serialize(destptr, maxbytes, "%s", config_enum_lookup_by_value(conf, *conf->variable));
  }
  break;
  }

  do_serialize(destptr, maxbytes, "%s", (gconf->sourcefile ? gconf->sourcefile : ""));

  if (gconf->sourcefile && gconf->sourcefile[0])
  {
    do_serialize_binary(destptr, maxbytes, &gconf->sourceline, sizeof(gconf->sourceline));
  }

  do_serialize_binary(destptr, maxbytes, &gconf->source, sizeof(gconf->source));
  do_serialize_binary(destptr, maxbytes, &gconf->scontext, sizeof(gconf->scontext));
}

   
                      
                                                                           
   
void
SerializeGUCState(Size maxsize, char *start_address)
{
  char *curptr;
  Size actual_size;
  Size bytes_left;
  int i;

                                                                 
  Assert(maxsize > sizeof(actual_size));
  curptr = start_address + sizeof(actual_size);
  bytes_left = maxsize - sizeof(actual_size);

  for (i = 0; i < num_guc_variables; i++)
  {
    serialize_variable(&curptr, &bytes_left, guc_variables[i]);
  }

                                                                      
  actual_size = maxsize - bytes_left - sizeof(actual_size);
  memcpy(start_address, &actual_size, sizeof(actual_size));
}

   
                  
                                                                            
                                                                               
                            
   
static char *
read_gucstate(char **srcptr, char *srcend)
{
  char *retptr = *srcptr;
  char *ptr;

  if (*srcptr >= srcend)
  {
    elog(ERROR, "incomplete GUC state");
  }

                                                    
  for (ptr = *srcptr; ptr < srcend && *ptr != '\0'; ptr++)
    ;

  if (ptr >= srcend)
  {
    elog(ERROR, "could not find null terminator in GUC state");
  }

                                                                      
  *srcptr = ptr + 1;

  return retptr;
}

                                                              
static void
read_gucstate_binary(char **srcptr, char *srcend, void *dest, Size size)
{
  if (*srcptr + size > srcend)
  {
    elog(ERROR, "incomplete GUC state");
  }

  memcpy(dest, *srcptr, size);
  *srcptr += size;
}

   
                                                                           
                                                     
   
static void
guc_restore_error_context_callback(void *arg)
{
  char **error_context_name_and_value = (char **)arg;

  if (error_context_name_and_value)
  {
    errcontext("while setting parameter \"%s\" to \"%s\"", error_context_name_and_value[0], error_context_name_and_value[1]);
  }
}

   
                    
                                                                              
                                   
   
void
RestoreGUCState(void *gucstate)
{
  char *varname, *varvalue, *varsourcefile;
  int varsourceline;
  GucSource varsource;
  GucContext varscontext;
  char *srcptr = (char *)gucstate;
  char *srcend;
  Size len;
  int i;
  ErrorContextCallback error_context_callback;

                                         
  for (i = 0; i < num_guc_variables; i++)
  {
    if (!can_skip_gucvar(guc_variables[i]))
    {
      InitializeOneGUCOption(guc_variables[i]);
    }
  }

                                                       
  memcpy(&len, gucstate, sizeof(len));

  srcptr += sizeof(len);
  srcend = srcptr + len;

                                                                            
  error_context_callback.callback = guc_restore_error_context_callback;
  error_context_callback.previous = error_context_stack;
  error_context_callback.arg = NULL;
  error_context_stack = &error_context_callback;

  while (srcptr < srcend)
  {
    int result;
    char *error_context_name_and_value[2];

    varname = read_gucstate(&srcptr, srcend);
    varvalue = read_gucstate(&srcptr, srcend);
    varsourcefile = read_gucstate(&srcptr, srcend);
    if (varsourcefile[0])
    {
      read_gucstate_binary(&srcptr, srcend, &varsourceline, sizeof(varsourceline));
    }
    else
    {
      varsourceline = 0;
    }
    read_gucstate_binary(&srcptr, srcend, &varsource, sizeof(varsource));
    read_gucstate_binary(&srcptr, srcend, &varscontext, sizeof(varscontext));

    error_context_name_and_value[0] = varname;
    error_context_name_and_value[1] = varvalue;
    error_context_callback.arg = &error_context_name_and_value[0];
    result = set_config_option(varname, varvalue, varscontext, varsource, GUC_ACTION_SET, true, ERROR, true);
    if (result <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("parameter \"%s\" could not be set", varname)));
    }
    if (varsourcefile[0])
    {
      set_config_sourcefile(varname, varsourcefile, varsourceline);
    }
    error_context_callback.arg = NULL;
  }

  error_context_stack = error_context_callback.previous;
}

   
                                                               
                                                                      
                                                                      
                                                                     
                                                                
   
void
ParseLongOption(const char *string, char **name, char **value)
{
  size_t equal_pos;
  char *cp;

  AssertArg(string);
  AssertArg(name);
  AssertArg(value);

  equal_pos = strcspn(string, "=");

  if (string[equal_pos] == '=')
  {
    *name = guc_malloc(FATAL, equal_pos + 1);
    strlcpy(*name, string, equal_pos + 1);

    *value = guc_strdup(FATAL, &string[equal_pos + 1]);
  }
  else
  {
                                 
    *name = guc_strdup(FATAL, string);
    *value = NULL;
  }

  for (cp = *name; *cp; cp++)
  {
    if (*cp == '-')
    {
      *cp = '_';
    }
  }
}

   
                                                             
                                                                              
   
                                                                       
   
void
ProcessGUCArray(ArrayType *array, GucContext context, GucSource source, GucAction action)
{
  int i;

  Assert(array != NULL);
  Assert(ARR_ELEMTYPE(array) == TEXTOID);
  Assert(ARR_NDIM(array) == 1);
  Assert(ARR_LBOUND(array)[0] == 1);

  for (i = 1; i <= ARR_DIMS(array)[0]; i++)
  {
    Datum d;
    bool isnull;
    char *s;
    char *name;
    char *value;
    char *namecopy;
    char *valuecopy;

    d = array_ref(array, 1, &i, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */, &isnull);

    if (isnull)
    {
      continue;
    }

    s = TextDatumGetCString(d);

    ParseLongOption(s, &name, &value);
    if (!value)
    {
      ereport(WARNING, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("could not parse setting for parameter \"%s\"", name)));
      free(name);
      continue;
    }

                                                                    
    namecopy = pstrdup(name);
    free(name);
    valuecopy = pstrdup(value);
    free(value);

    (void)set_config_option(namecopy, valuecopy, context, source, action, true, 0, false);

    pfree(namecopy);
    pfree(valuecopy);
    pfree(s);
  }
}

   
                                                                     
                                                
   
ArrayType *
GUCArrayAdd(ArrayType *array, const char *name, const char *value)
{
  struct config_generic *record;
  Datum datum;
  char *newval;
  ArrayType *a;

  Assert(name);
  Assert(value);

                                                               
  (void)validate_option_array_item(name, value, false);

                                                                        
  record = find_option(name, false, WARNING);
  if (record)
  {
    name = record->name;
  }

                                
  newval = psprintf("%s=%s", name, value);
  datum = CStringGetTextDatum(newval);

  if (array)
  {
    int index;
    bool isnull;
    int i;

    Assert(ARR_ELEMTYPE(array) == TEXTOID);
    Assert(ARR_NDIM(array) == 1);
    Assert(ARR_LBOUND(array)[0] == 1);

    index = ARR_DIMS(array)[0] + 1;                    

    for (i = 1; i <= ARR_DIMS(array)[0]; i++)
    {
      Datum d;
      char *current;

      d = array_ref(array, 1, &i, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */, &isnull);
      if (isnull)
      {
        continue;
      }
      current = TextDatumGetCString(d);

                                                        
      if (strncmp(current, newval, strlen(name) + 1) == 0)
      {
        index = i;
        break;
      }
    }

    a = array_set(array, 1, &index, datum, false, -1                    , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */);
  }
  else
  {
    a = construct_array(&datum, 1, TEXTOID, -1, false, 'i');
  }

  return a;
}

   
                                                                          
                                                                           
                                         
   
ArrayType *
GUCArrayDelete(ArrayType *array, const char *name)
{
  struct config_generic *record;
  ArrayType *newarray;
  int i;
  int index;

  Assert(name);

                                                               
  (void)validate_option_array_item(name, NULL, false);

                                                                        
  record = find_option(name, false, WARNING);
  if (record)
  {
    name = record->name;
  }

                                                                 
  if (!array)
  {
    return NULL;
  }

  newarray = NULL;
  index = 1;

  for (i = 1; i <= ARR_DIMS(array)[0]; i++)
  {
    Datum d;
    char *val;
    bool isnull;

    d = array_ref(array, 1, &i, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */, &isnull);
    if (isnull)
    {
      continue;
    }
    val = TextDatumGetCString(d);

                                                     
    if (strncmp(val, name, strlen(name)) == 0 && val[strlen(name)] == '=')
    {
      continue;
    }

                                         
    if (newarray)
    {
      newarray = array_set(newarray, 1, &index, d, false, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */);
    }
    else
    {
      newarray = construct_array(&d, 1, TEXTOID, -1, false, 'i');
    }

    index++;
  }

  return newarray;
}

   
                                                                      
                                                                      
                              
   
ArrayType *
GUCArrayReset(ArrayType *array)
{
  ArrayType *newarray;
  int i;
  int index;

                                                 
  if (!array)
  {
    return NULL;
  }

                                                                   
  if (superuser())
  {
    return NULL;
  }

  newarray = NULL;
  index = 1;

  for (i = 1; i <= ARR_DIMS(array)[0]; i++)
  {
    Datum d;
    char *val;
    char *eqsgn;
    bool isnull;

    d = array_ref(array, 1, &i, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */, &isnull);
    if (isnull)
    {
      continue;
    }
    val = TextDatumGetCString(d);

    eqsgn = strchr(val, '=');
    *eqsgn = '\0';

                                                 
    if (validate_option_array_item(val, NULL, true))
    {
      continue;
    }

                                         
    if (newarray)
    {
      newarray = array_set(newarray, 1, &index, d, false, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */);
    }
    else
    {
      newarray = construct_array(&d, 1, TEXTOID, -1, false, 'i');
    }

    index++;
    pfree(val);
  }

  return newarray;
}

   
                                                                    
   
                                                                           
                                                                             
                                                          
   
                                                                              
                                                                             
                        
   
static bool
validate_option_array_item(const char *name, const char *value, bool skipIfNoPermissions)

{
  struct config_generic *gconf;

     
                                        
     
                                                                    
                                                                          
                                   
     
                                                                             
                                                                         
                                                                           
                                                                     
                                                        
                                                     
     
                                                                            
                                                                     
     
  gconf = find_option(name, true, WARNING);
  if (!gconf)
  {
                                                 
    if (skipIfNoPermissions)
    {
      return false;
    }
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unrecognized configuration parameter \"%s\"", name)));
  }

  if (gconf->flags & GUC_CUSTOM_PLACEHOLDER)
  {
       
                                                                           
                            
       
    if (superuser())
    {
      return true;
    }
    if (skipIfNoPermissions)
    {
      return false;
    }
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to set parameter \"%s\"", name)));
  }

                                                                      
  if (gconf->context == PGC_USERSET)
            ;
  else if (gconf->context == PGC_SUSET && superuser())
            ;
  else if (skipIfNoPermissions)
  {
    return false;
  }
                                                                            

                                                   
  (void)set_config_option(name, value, superuser() ? PGC_SUSET : PGC_USERSET, PGC_S_TEST, GUC_ACTION_SET, false, 0, false);

  return true;
}

   
                                                          
                                                                     
   
                                                                            
                                                                            
                                        
   
void
GUC_check_errcode(int sqlerrcode)
{
  GUC_check_errcode_value = sqlerrcode;
}

   
                                                                    
                                                                         
                                            
   

static bool
call_bool_check_hook(struct config_bool *conf, bool *newval, void **extra, GucSource source, int elevel)
{
                                
  if (!conf->check_hook)
  {
    return true;
  }

                                                 
  GUC_check_errcode_value = ERRCODE_INVALID_PARAMETER_VALUE;
  GUC_check_errmsg_string = NULL;
  GUC_check_errdetail_string = NULL;
  GUC_check_errhint_string = NULL;

  if (!conf->check_hook(newval, extra, source))
  {
    ereport(elevel, (errcode(GUC_check_errcode_value), GUC_check_errmsg_string ? errmsg_internal("%s", GUC_check_errmsg_string) : errmsg("invalid value for parameter \"%s\": %d", conf->gen.name, (int)*newval), GUC_check_errdetail_string ? errdetail_internal("%s", GUC_check_errdetail_string) : 0, GUC_check_errhint_string ? errhint("%s", GUC_check_errhint_string) : 0));
                                                   
    FlushErrorState();
    return false;
  }

  return true;
}

static bool
call_int_check_hook(struct config_int *conf, int *newval, void **extra, GucSource source, int elevel)
{
                                
  if (!conf->check_hook)
  {
    return true;
  }

                                                 
  GUC_check_errcode_value = ERRCODE_INVALID_PARAMETER_VALUE;
  GUC_check_errmsg_string = NULL;
  GUC_check_errdetail_string = NULL;
  GUC_check_errhint_string = NULL;

  if (!conf->check_hook(newval, extra, source))
  {
    ereport(elevel, (errcode(GUC_check_errcode_value), GUC_check_errmsg_string ? errmsg_internal("%s", GUC_check_errmsg_string) : errmsg("invalid value for parameter \"%s\": %d", conf->gen.name, *newval), GUC_check_errdetail_string ? errdetail_internal("%s", GUC_check_errdetail_string) : 0, GUC_check_errhint_string ? errhint("%s", GUC_check_errhint_string) : 0));
                                                   
    FlushErrorState();
    return false;
  }

  return true;
}

static bool
call_real_check_hook(struct config_real *conf, double *newval, void **extra, GucSource source, int elevel)
{
                                
  if (!conf->check_hook)
  {
    return true;
  }

                                                 
  GUC_check_errcode_value = ERRCODE_INVALID_PARAMETER_VALUE;
  GUC_check_errmsg_string = NULL;
  GUC_check_errdetail_string = NULL;
  GUC_check_errhint_string = NULL;

  if (!conf->check_hook(newval, extra, source))
  {
    ereport(elevel, (errcode(GUC_check_errcode_value), GUC_check_errmsg_string ? errmsg_internal("%s", GUC_check_errmsg_string) : errmsg("invalid value for parameter \"%s\": %g", conf->gen.name, *newval), GUC_check_errdetail_string ? errdetail_internal("%s", GUC_check_errdetail_string) : 0, GUC_check_errhint_string ? errhint("%s", GUC_check_errhint_string) : 0));
                                                   
    FlushErrorState();
    return false;
  }

  return true;
}

static bool
call_string_check_hook(struct config_string *conf, char **newval, void **extra, GucSource source, int elevel)
{
  volatile bool result = true;

                                
  if (!conf->check_hook)
  {
    return true;
  }

     
                                                                    
                                                                          
                                     
     
  PG_TRY();
  {
                                                   
    GUC_check_errcode_value = ERRCODE_INVALID_PARAMETER_VALUE;
    GUC_check_errmsg_string = NULL;
    GUC_check_errdetail_string = NULL;
    GUC_check_errhint_string = NULL;

    if (!conf->check_hook(newval, extra, source))
    {
      ereport(elevel, (errcode(GUC_check_errcode_value), GUC_check_errmsg_string ? errmsg_internal("%s", GUC_check_errmsg_string) : errmsg("invalid value for parameter \"%s\": \"%s\"", conf->gen.name, *newval ? *newval : ""), GUC_check_errdetail_string ? errdetail_internal("%s", GUC_check_errdetail_string) : 0, GUC_check_errhint_string ? errhint("%s", GUC_check_errhint_string) : 0));
                                                     
      FlushErrorState();
      result = false;
    }
  }
  PG_CATCH();
  {
    free(*newval);
    PG_RE_THROW();
  }
  PG_END_TRY();

  return result;
}

static bool
call_enum_check_hook(struct config_enum *conf, int *newval, void **extra, GucSource source, int elevel)
{
                                
  if (!conf->check_hook)
  {
    return true;
  }

                                                 
  GUC_check_errcode_value = ERRCODE_INVALID_PARAMETER_VALUE;
  GUC_check_errmsg_string = NULL;
  GUC_check_errdetail_string = NULL;
  GUC_check_errhint_string = NULL;

  if (!conf->check_hook(newval, extra, source))
  {
    ereport(elevel, (errcode(GUC_check_errcode_value), GUC_check_errmsg_string ? errmsg_internal("%s", GUC_check_errmsg_string) : errmsg("invalid value for parameter \"%s\": \"%s\"", conf->gen.name, config_enum_lookup_by_value(conf, *newval)), GUC_check_errdetail_string ? errdetail_internal("%s", GUC_check_errdetail_string) : 0, GUC_check_errhint_string ? errhint("%s", GUC_check_errhint_string) : 0));
                                                   
    FlushErrorState();
    return false;
  }

  return true;
}

   
                                                     
   

static bool
check_wal_consistency_checking(char **newval, void **extra, GucSource source)
{
  char *rawstring;
  List *elemlist;
  ListCell *l;
  bool newwalconsistency[RM_MAX_ID + 1];

                            
  MemSet(newwalconsistency, 0, (RM_MAX_ID + 1) * sizeof(bool));

                                        
  rawstring = pstrdup(*newval);

                                             
  if (!SplitIdentifierString(rawstring, ',', &elemlist))
  {
                              
    GUC_check_errdetail("List syntax is invalid.");
    pfree(rawstring);
    list_free(elemlist);
    return false;
  }

  foreach (l, elemlist)
  {
    char *tok = (char *)lfirst(l);
    bool found = false;
    RmgrId rmid;

                          
    if (pg_strcasecmp(tok, "all") == 0)
    {
      for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
      {
        if (RmgrTable[rmid].rm_mask != NULL)
        {
          newwalconsistency[rmid] = true;
        }
      }
      found = true;
    }
    else
    {
         
                                                                 
                  
         
      for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
      {
        if (pg_strcasecmp(tok, RmgrTable[rmid].rm_name) == 0 && RmgrTable[rmid].rm_mask != NULL)
        {
          newwalconsistency[rmid] = true;
          found = true;
        }
      }
    }

                                                                       
    if (!found)
    {
      GUC_check_errdetail("Unrecognized key word: \"%s\".", tok);
      pfree(rawstring);
      list_free(elemlist);
      return false;
    }
  }

  pfree(rawstring);
  list_free(elemlist);

                        
  *extra = guc_malloc(ERROR, (RM_MAX_ID + 1) * sizeof(bool));
  memcpy(*extra, newwalconsistency, (RM_MAX_ID + 1) * sizeof(bool));
  return true;
}

static void
assign_wal_consistency_checking(const char *newval, void *extra)
{
  wal_consistency_checking = (bool *)extra;
}

static bool
check_log_destination(char **newval, void **extra, GucSource source)
{
  char *rawstring;
  List *elemlist;
  ListCell *l;
  int newlogdest = 0;
  int *myextra;

                                        
  rawstring = pstrdup(*newval);

                                             
  if (!SplitIdentifierString(rawstring, ',', &elemlist))
  {
                              
    GUC_check_errdetail("List syntax is invalid.");
    pfree(rawstring);
    list_free(elemlist);
    return false;
  }

  foreach (l, elemlist)
  {
    char *tok = (char *)lfirst(l);

    if (pg_strcasecmp(tok, "stderr") == 0)
    {
      newlogdest |= LOG_DESTINATION_STDERR;
    }
    else if (pg_strcasecmp(tok, "csvlog") == 0)
    {
      newlogdest |= LOG_DESTINATION_CSVLOG;
    }
#ifdef HAVE_SYSLOG
    else if (pg_strcasecmp(tok, "syslog") == 0)
    {
      newlogdest |= LOG_DESTINATION_SYSLOG;
    }
#endif
#ifdef WIN32
    else if (pg_strcasecmp(tok, "eventlog") == 0)
    {
      newlogdest |= LOG_DESTINATION_EVENTLOG;
    }
#endif
    else
    {
      GUC_check_errdetail("Unrecognized key word: \"%s\".", tok);
      pfree(rawstring);
      list_free(elemlist);
      return false;
    }
  }

  pfree(rawstring);
  list_free(elemlist);

  myextra = (int *)guc_malloc(ERROR, sizeof(int));
  *myextra = newlogdest;
  *extra = (void *)myextra;

  return true;
}

static void
assign_log_destination(const char *newval, void *extra)
{
  Log_destination = *((int *)extra);
}

static void
assign_syslog_facility(int newval, void *extra)
{
#ifdef HAVE_SYSLOG
  set_syslog_parameters(syslog_ident_str ? syslog_ident_str : "postgres", newval);
#endif
                                              
}

static void
assign_syslog_ident(const char *newval, void *extra)
{
#ifdef HAVE_SYSLOG
  set_syslog_parameters(newval, syslog_facility);
#endif
                                                                          
}

static void
assign_session_replication_role(int newval, void *extra)
{
     
                                                                         
                          
     
  if (SessionReplicationRole != newval)
  {
    ResetPlanCache();
  }
}

static bool
check_temp_buffers(int *newval, void **extra, GucSource source)
{
     
                                                                             
                                                     
     
  if (source != PGC_S_TEST && NLocBuffer && NLocBuffer != *newval)
  {
    GUC_check_errdetail("\"temp_buffers\" cannot be changed after any temporary tables have been accessed in the session.");
    return false;
  }
  return true;
}

static bool
check_bonjour(bool *newval, void **extra, GucSource source)
{
#ifndef USE_BONJOUR
  if (*newval)
  {
    GUC_check_errmsg("Bonjour is not supported by this build");
    return false;
  }
#endif
  return true;
}

static bool
check_ssl(bool *newval, void **extra, GucSource source)
{
#ifndef USE_SSL
  if (*newval)
  {
    GUC_check_errmsg("SSL is not supported by this build");
    return false;
  }
#endif
  return true;
}

static bool
check_stage_log_stats(bool *newval, void **extra, GucSource source)
{
  if (*newval && log_statement_stats)
  {
    GUC_check_errdetail("Cannot enable parameter when \"log_statement_stats\" is true.");
    return false;
  }
  return true;
}

static bool
check_log_stats(bool *newval, void **extra, GucSource source)
{
  if (*newval && (log_parser_stats || log_planner_stats || log_executor_stats))
  {
    GUC_check_errdetail("Cannot enable \"log_statement_stats\" when "
                        "\"log_parser_stats\", \"log_planner_stats\", "
                        "or \"log_executor_stats\" is true.");
    return false;
  }
  return true;
}

static bool
check_canonical_path(char **newval, void **extra, GucSource source)
{
     
                                                                           
                                                                          
                            
     
  if (*newval)
  {
    canonicalize_path(*newval);
  }
  return true;
}

static bool
check_timezone_abbreviations(char **newval, void **extra, GucSource source)
{
     
                                                                          
                                                                           
                                                                      
                                                                      
                                                                          
                                                                        
                                                                   
                                                                             
                                 
     
  if (*newval == NULL)
  {
    Assert(source == PGC_S_DEFAULT);
    return true;
  }

                                                                    
  *extra = load_tzoffsets(*newval);

                                                                          
  if (!*extra)
  {
    return false;
  }

  return true;
}

static void
assign_timezone_abbreviations(const char *newval, void *extra)
{
                                                   
  if (!extra)
  {
    return;
  }

  InstallTimeZoneAbbrevs((TimeZoneAbbrevTable *)extra);
}

   
                                                                           
   
                                                                   
                                                                     
                                                                     
   
                                                                           
                                                          
   
static void
pg_timezone_abbrev_initialize(void)
{
  SetConfigOption("timezone_abbreviations", "Default", PGC_POSTMASTER, PGC_S_DYNAMIC_DEFAULT);
}

static const char *
show_archive_command(void)
{
  if (XLogArchivingActive())
  {
    return XLogArchiveCommand;
  }
  else
  {
    return "(disabled)";
  }
}

static void
assign_tcp_keepalives_idle(int newval, void *extra)
{
     
                                                                            
                                                                            
                                                                   
                                                                        
                                         
     
                                                                             
                                                                          
                                            
     
  (void)pq_setkeepalivesidle(newval, MyProcPort);
}

static const char *
show_tcp_keepalives_idle(void)
{
                                                  
  static char nbuf[16];

  snprintf(nbuf, sizeof(nbuf), "%d", pq_getkeepalivesidle(MyProcPort));
  return nbuf;
}

static void
assign_tcp_keepalives_interval(int newval, void *extra)
{
                                                  
  (void)pq_setkeepalivesinterval(newval, MyProcPort);
}

static const char *
show_tcp_keepalives_interval(void)
{
                                                  
  static char nbuf[16];

  snprintf(nbuf, sizeof(nbuf), "%d", pq_getkeepalivesinterval(MyProcPort));
  return nbuf;
}

static void
assign_tcp_keepalives_count(int newval, void *extra)
{
                                                  
  (void)pq_setkeepalivescount(newval, MyProcPort);
}

static const char *
show_tcp_keepalives_count(void)
{
                                                  
  static char nbuf[16];

  snprintf(nbuf, sizeof(nbuf), "%d", pq_getkeepalivescount(MyProcPort));
  return nbuf;
}

static void
assign_tcp_user_timeout(int newval, void *extra)
{
                                                  
  (void)pq_settcpusertimeout(newval, MyProcPort);
}

static const char *
show_tcp_user_timeout(void)
{
                                                  
  static char nbuf[16];

  snprintf(nbuf, sizeof(nbuf), "%d", pq_gettcpusertimeout(MyProcPort));
  return nbuf;
}

static bool
check_maxconnections(int *newval, void **extra, GucSource source)
{
  if (*newval + autovacuum_max_workers + 1 + max_worker_processes + max_wal_senders > MAX_BACKENDS)
  {
    return false;
  }
  return true;
}

static bool
check_autovacuum_max_workers(int *newval, void **extra, GucSource source)
{
  if (MaxConnections + *newval + 1 + max_worker_processes + max_wal_senders > MAX_BACKENDS)
  {
    return false;
  }
  return true;
}

static bool
check_max_wal_senders(int *newval, void **extra, GucSource source)
{
  if (MaxConnections + autovacuum_max_workers + 1 + max_worker_processes + *newval > MAX_BACKENDS)
  {
    return false;
  }
  return true;
}

static bool
check_autovacuum_work_mem(int *newval, void **extra, GucSource source)
{
     
                            
     
                                                                           
                                                           
     
  if (*newval == -1)
  {
    return true;
  }

     
                                                          
                                                                            
           
     
  if (*newval < 1024)
  {
    *newval = 1024;
  }

  return true;
}

static bool
check_max_worker_processes(int *newval, void **extra, GucSource source)
{
  if (MaxConnections + autovacuum_max_workers + 1 + *newval + max_wal_senders > MAX_BACKENDS)
  {
    return false;
  }
  return true;
}

static bool
check_effective_io_concurrency(int *newval, void **extra, GucSource source)
{
#ifdef USE_PREFETCH
  double new_prefetch_pages;

  if (ComputeIoConcurrency(*newval, &new_prefetch_pages))
  {
    int *myextra = (int *)guc_malloc(ERROR, sizeof(int));

    *myextra = (int)rint(new_prefetch_pages);
    *extra = (void *)myextra;

    return true;
  }
  else
  {
    return false;
  }
#else
  if (*newval != 0)
  {
    GUC_check_errdetail("effective_io_concurrency must be set to 0 on platforms that lack posix_fadvise().");
    return false;
  }
  return true;
#endif                   
}

static void
assign_effective_io_concurrency(int newval, void *extra)
{
#ifdef USE_PREFETCH
  target_prefetch_pages = *((int *)extra);
#endif                   
}

static void
assign_pgstat_temp_directory(const char *newval, void *extra)
{
                                                                
  char *dname;
  char *tname;
  char *fname;

                 
  dname = guc_malloc(ERROR, strlen(newval) + 1);                  
  sprintf(dname, "%s", newval);

                    
  tname = guc_malloc(ERROR, strlen(newval) + 12);                  
  sprintf(tname, "%s/global.tmp", newval);
  fname = guc_malloc(ERROR, strlen(newval) + 13);                   
  sprintf(fname, "%s/global.stat", newval);

  if (pgstat_stat_directory)
  {
    free(pgstat_stat_directory);
  }
  pgstat_stat_directory = dname;
  if (pgstat_stat_tmpname)
  {
    free(pgstat_stat_tmpname);
  }
  pgstat_stat_tmpname = tname;
  if (pgstat_stat_filename)
  {
    free(pgstat_stat_filename);
  }
  pgstat_stat_filename = fname;
}

static bool
check_application_name(char **newval, void **extra, GucSource source)
{
                                                            
  pg_clean_ascii(*newval);

  return true;
}

static void
assign_application_name(const char *newval, void *extra)
{
                                        
  pgstat_report_appname(newval);
}

static bool
check_cluster_name(char **newval, void **extra, GucSource source)
{
                                                        
  pg_clean_ascii(*newval);

  return true;
}

static const char *
show_unix_socket_permissions(void)
{
  static char buf[12];

  snprintf(buf, sizeof(buf), "%04o", Unix_socket_permissions);
  return buf;
}

static const char *
show_log_file_mode(void)
{
  static char buf[12];

  snprintf(buf, sizeof(buf), "%04o", Log_file_mode);
  return buf;
}

static const char *
show_data_directory_mode(void)
{
  static char buf[12];

  snprintf(buf, sizeof(buf), "%04o", data_directory_mode);
  return buf;
}

static bool
check_recovery_target_timeline(char **newval, void **extra, GucSource source)
{
  RecoveryTargetTimeLineGoal rttg;
  RecoveryTargetTimeLineGoal *myextra;

  if (strcmp(*newval, "current") == 0)
  {
    rttg = RECOVERY_TARGET_TIMELINE_CONTROLFILE;
  }
  else if (strcmp(*newval, "latest") == 0)
  {
    rttg = RECOVERY_TARGET_TIMELINE_LATEST;
  }
  else
  {
    rttg = RECOVERY_TARGET_TIMELINE_NUMERIC;

    errno = 0;
    strtoul(*newval, NULL, 0);
    if (errno == EINVAL || errno == ERANGE)
    {
      GUC_check_errdetail("recovery_target_timeline is not a valid number.");
      return false;
    }
  }

  myextra = (RecoveryTargetTimeLineGoal *)guc_malloc(ERROR, sizeof(RecoveryTargetTimeLineGoal));
  *myextra = rttg;
  *extra = (void *)myextra;

  return true;
}

static void
assign_recovery_target_timeline(const char *newval, void *extra)
{
  recoveryTargetTimeLineGoal = *((RecoveryTargetTimeLineGoal *)extra);
  if (recoveryTargetTimeLineGoal == RECOVERY_TARGET_TIMELINE_NUMERIC)
  {
    recoveryTargetTLIRequested = (TimeLineID)strtoul(newval, NULL, 0);
  }
  else
  {
    recoveryTargetTLIRequested = 0;
  }
}

   
                                                                               
                                                                               
                                                                          
                                                                            
                                                                              
                                                                              
                                                                               
                                                                         
           
   

static void
pg_attribute_noreturn() error_multiple_recovery_targets(void)
{
  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("multiple recovery targets specified"), errdetail("At most one of recovery_target, recovery_target_lsn, recovery_target_name, recovery_target_time, recovery_target_xid may be set.")));
}

static bool
check_recovery_target(char **newval, void **extra, GucSource source)
{
  if (strcmp(*newval, "immediate") != 0 && strcmp(*newval, "") != 0)
  {
    GUC_check_errdetail("The only allowed value is \"immediate\".");
    return false;
  }
  return true;
}

static void
assign_recovery_target(const char *newval, void *extra)
{
  if (recoveryTarget != RECOVERY_TARGET_UNSET && recoveryTarget != RECOVERY_TARGET_IMMEDIATE)
  {
    error_multiple_recovery_targets();
  }

  if (newval && strcmp(newval, "") != 0)
  {
    recoveryTarget = RECOVERY_TARGET_IMMEDIATE;
  }
  else
  {
    recoveryTarget = RECOVERY_TARGET_UNSET;
  }
}

static bool
check_recovery_target_xid(char **newval, void **extra, GucSource source)
{
  if (strcmp(*newval, "") != 0)
  {
    TransactionId xid;
    TransactionId *myextra;

    errno = 0;
    xid = (TransactionId)pg_strtouint64(*newval, NULL, 0);
    if (errno == EINVAL || errno == ERANGE)
    {
      return false;
    }

    myextra = (TransactionId *)guc_malloc(ERROR, sizeof(TransactionId));
    *myextra = xid;
    *extra = (void *)myextra;
  }
  return true;
}

static void
assign_recovery_target_xid(const char *newval, void *extra)
{
  if (recoveryTarget != RECOVERY_TARGET_UNSET && recoveryTarget != RECOVERY_TARGET_XID)
  {
    error_multiple_recovery_targets();
  }

  if (newval && strcmp(newval, "") != 0)
  {
    recoveryTarget = RECOVERY_TARGET_XID;
    recoveryTargetXid = *((TransactionId *)extra);
  }
  else
  {
    recoveryTarget = RECOVERY_TARGET_UNSET;
  }
}

   
                                                                           
                                                                           
                                                                               
                                                                            
                                              
   
static bool
check_recovery_target_time(char **newval, void **extra, GucSource source)
{
  if (strcmp(*newval, "") != 0)
  {
                                    
    if (strcmp(*newval, "now") == 0 || strcmp(*newval, "today") == 0 || strcmp(*newval, "tomorrow") == 0 || strcmp(*newval, "yesterday") == 0)
    {
      return false;
    }

       
                                                         
       
    {
      char *str = *newval;
      fsec_t fsec;
      struct pg_tm tt, *tm = &tt;
      int tz;
      int dtype;
      int nf;
      int dterr;
      char *field[MAXDATEFIELDS];
      int ftype[MAXDATEFIELDS];
      char workbuf[MAXDATELEN + MAXDATEFIELDS];
      TimestampTz timestamp;

      dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
      if (dterr == 0)
      {
        dterr = DecodeDateTime(field, ftype, nf, &dtype, tm, &fsec, &tz);
      }
      if (dterr != 0)
      {
        return false;
      }
      if (dtype != DTK_DATE)
      {
        return false;
      }

      if (tm2timestamp(tm, fsec, &tz, &timestamp) != 0)
      {
        GUC_check_errdetail("timestamp out of range: \"%s\"", str);
        return false;
      }
    }
  }
  return true;
}

static void
assign_recovery_target_time(const char *newval, void *extra)
{
  if (recoveryTarget != RECOVERY_TARGET_UNSET && recoveryTarget != RECOVERY_TARGET_TIME)
  {
    error_multiple_recovery_targets();
  }

  if (newval && strcmp(newval, "") != 0)
  {
    recoveryTarget = RECOVERY_TARGET_TIME;
  }
  else
  {
    recoveryTarget = RECOVERY_TARGET_UNSET;
  }
}

static bool
check_recovery_target_name(char **newval, void **extra, GucSource source)
{
                                        
  if (strlen(*newval) >= MAXFNAMELEN)
  {
    GUC_check_errdetail("%s is too long (maximum %d characters).", "recovery_target_name", MAXFNAMELEN - 1);
    return false;
  }
  return true;
}

static void
assign_recovery_target_name(const char *newval, void *extra)
{
  if (recoveryTarget != RECOVERY_TARGET_UNSET && recoveryTarget != RECOVERY_TARGET_NAME)
  {
    error_multiple_recovery_targets();
  }

  if (newval && strcmp(newval, "") != 0)
  {
    recoveryTarget = RECOVERY_TARGET_NAME;
    recoveryTargetName = newval;
  }
  else
  {
    recoveryTarget = RECOVERY_TARGET_UNSET;
  }
}

static bool
check_recovery_target_lsn(char **newval, void **extra, GucSource source)
{
  if (strcmp(*newval, "") != 0)
  {
    XLogRecPtr lsn;
    XLogRecPtr *myextra;
    bool have_error = false;

    lsn = pg_lsn_in_internal(*newval, &have_error);
    if (have_error)
    {
      return false;
    }

    myextra = (XLogRecPtr *)guc_malloc(ERROR, sizeof(XLogRecPtr));
    *myextra = lsn;
    *extra = (void *)myextra;
  }
  return true;
}

static void
assign_recovery_target_lsn(const char *newval, void *extra)
{
  if (recoveryTarget != RECOVERY_TARGET_UNSET && recoveryTarget != RECOVERY_TARGET_LSN)
  {
    error_multiple_recovery_targets();
  }

  if (newval && strcmp(newval, "") != 0)
  {
    recoveryTarget = RECOVERY_TARGET_LSN;
    recoveryTargetLSN = *((XLogRecPtr *)extra);
  }
  else
  {
    recoveryTarget = RECOVERY_TARGET_UNSET;
  }
}

static bool
check_primary_slot_name(char **newval, void **extra, GucSource source)
{
  if (*newval && strcmp(*newval, "") != 0 && !ReplicationSlotValidateName(*newval, WARNING))
  {
    return false;
  }

  return true;
}

static bool
check_default_with_oids(bool *newval, void **extra, GucSource source)
{
  if (*newval)
  {
                                                       
    GUC_check_errcode(ERRCODE_FEATURE_NOT_SUPPORTED);
    GUC_check_errmsg("tables declared WITH OIDS are not supported");

    return false;
  }

  return true;
}

#include "guc-file.c"
