                                                                            
   
                                                   
   
                                                                     
                                                               
                                                                      
   
                                                                         
                                                                      
                                                                     
                                                                      
                        
   
                                                                        
                                                                      
                                                                      
                              
   
                                                                           
                                                                        
                                                                       
   
                                                                           
                                                                         
                                                                       
                                                                         
                                                                        
                                
   
   
         
                                                                         
   
                                                                            
                                                                         
                                                                         
   
                                                                    
   
                                                                         
                                                                        
   
                           
   
                                                                            
   

#include "postgres_fe.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#ifdef HAVE_SHM_OPEN
#include "sys/mman.h"
#endif

#include "access/xlog_internal.h"
#include "catalog/pg_authid_d.h"
#include "catalog/pg_class_d.h"                         
#include "catalog/pg_collation_d.h"
#include "common/file_perm.h"
#include "common/file_utils.h"
#include "common/logging.h"
#include "common/restricted_token.h"
#include "common/username.h"
#include "fe_utils/string_utils.h"
#include "getaddrinfo.h"
#include "getopt_long.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"

                                                                               
extern const char *
select_default_timezone(const char *share_path);

static const char *const auth_methods_host[] = {"trust", "reject", "scram-sha-256", "md5", "password", "ident", "radius",
#ifdef ENABLE_GSS
    "gss",
#endif
#ifdef ENABLE_SSPI
    "sspi",
#endif
#ifdef USE_PAM
    "pam", "pam ",
#endif
#ifdef USE_BSD_AUTH
    "bsd",
#endif
#ifdef USE_LDAP
    "ldap",
#endif
#ifdef USE_SSL
    "cert",
#endif
    NULL};
static const char *const auth_methods_local[] = {"trust", "reject", "scram-sha-256", "md5", "password", "peer", "radius",
#ifdef USE_PAM
    "pam", "pam ",
#endif
#ifdef USE_BSD_AUTH
    "bsd",
#endif
#ifdef USE_LDAP
    "ldap",
#endif
    NULL};

   
                                                  
   
static char *share_path = NULL;

                                          
static char *pg_data = NULL;
static char *encoding = NULL;
static char *locale = NULL;
static char *lc_collate = NULL;
static char *lc_ctype = NULL;
static char *lc_monetary = NULL;
static char *lc_numeric = NULL;
static char *lc_time = NULL;
static char *lc_messages = NULL;
static const char *default_text_search_config = NULL;
static char *username = NULL;
static bool pwprompt = false;
static char *pwfilename = NULL;
static char *superuser_password = NULL;
static const char *authmethodhost = NULL;
static const char *authmethodlocal = NULL;
static bool debug = false;
static bool noclean = false;
static bool do_sync = true;
static bool sync_only = false;
static bool show_setting = false;
static bool data_checksums = false;
static char *xlog_dir = NULL;
static char *str_wal_segment_size_mb = NULL;
static int wal_segment_size_mb;

                   
static const char *progname;
static int encodingid;
static char *bki_file;
static char *desc_file;
static char *shdesc_file;
static char *hba_file;
static char *ident_file;
static char *conf_file;
static char *dictionary_file;
static char *info_schema_file;
static char *features_file;
static char *system_views_file;
static bool success = false;
static bool made_new_pgdata = false;
static bool found_existing_pgdata = false;
static bool made_new_xlogdir = false;
static bool found_existing_xlogdir = false;
static char infoversion[100];
static bool caught_signal = false;
static bool output_failed = false;
static int output_errno = 0;
static char *pgdata_native;

              
static int n_connections = 10;
static int n_buffers = 50;
static const char *dynamic_shared_memory_type = NULL;
static const char *default_timezone = NULL;

   
                                               
   
#define AUTHTRUST_WARNING                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "# CAUTION: Configuring the system for local \"trust\" authentication\n"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "# allows any local user to connect as any PostgreSQL user, including\n"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "# the database superuser.  If you do not trust all your local users,\n"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "# use another authentication method.\n"
static bool authwarning = false;

   
                                                        
   
                                                                          
                                                                           
   
                                                                            
                                                                        
                                
   
static const char *boot_options = "-F";
static const char *backend_options = "--single -F -O -j -c search_path=pg_catalog -c exit_on_error=true";

static const char *const subdirs[] = {"global", "pg_wal/archive_status", "pg_commit_ts", "pg_dynshmem", "pg_notify", "pg_serial", "pg_snapshots", "pg_subtrans", "pg_twophase", "pg_multixact", "pg_multixact/members", "pg_multixact/offsets", "base", "base/1", "pg_replslot", "pg_tblspc", "pg_stat", "pg_stat_tmp", "pg_xact", "pg_logical", "pg_logical/snapshots", "pg_logical/mappings"};

                                       
static char bin_path[MAXPGPATH];
static char backend_exec[MAXPGPATH];

static char **
replace_token(char **lines, const char *token, const char *replacement);

#ifndef HAVE_UNIX_SOCKETS
static char **
filter_lines_with_token(char **lines, const char *token);
#endif
static char **
readfile(const char *path);
static void
writefile(char *path, char **lines);
static FILE *
popen_check(const char *command, const char *mode);
static char *
get_id(void);
static int
get_encoding_id(const char *encoding_name);
static void
set_input(char **dest, const char *filename);
static void
check_input(char *path);
static void
write_version_file(const char *extrapath);
static void
set_null_conf(void);
static void
test_config_settings(void);
static void
setup_config(void);
static void
bootstrap_template1(void);
static void
setup_auth(FILE *cmdfd);
static void
get_su_pwd(void);
static void
setup_depend(FILE *cmdfd);
static void
setup_sysviews(FILE *cmdfd);
static void
setup_description(FILE *cmdfd);
static void
setup_collation(FILE *cmdfd);
static void
setup_dictionary(FILE *cmdfd);
static void
setup_privileges(FILE *cmdfd);
static void
set_info_version(void);
static void
setup_schema(FILE *cmdfd);
static void
load_plpgsql(FILE *cmdfd);
static void
vacuum_db(FILE *cmdfd);
static void
make_template0(FILE *cmdfd);
static void
make_postgres(FILE *cmdfd);
static void
trapsig(int signum);
static void
check_ok(void);
static char *
escape_quotes(const char *src);
static char *
escape_quotes_bki(const char *src);
static int
locale_date_order(const char *locale);
static void
check_locale_name(int category, const char *locale, char **canonname);
static bool
check_locale_encoding(const char *locale, int encoding);
static void
setlocales(void);
static void
usage(const char *progname);
void
setup_pgdata(void);
void
setup_bin_paths(const char *argv0);
void
setup_data_file_paths(void);
void
setup_locale_encoding(void);
void
setup_signals(void);
void
setup_text_search(void);
void
create_data_directory(void);
void
create_xlog_or_symlink(void);
void
warn_on_mount_point(int error);
void
initialize_data_directory(void);

   
                                        
   
#define PG_CMD_DECL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  char cmd[MAXPGPATH];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  FILE *cmdfd

#define PG_CMD_OPEN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    cmdfd = popen_check(cmd, "w");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    if (cmdfd == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      exit(1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

#define PG_CMD_CLOSE                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (pclose_check(cmdfd))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      exit(1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

#define PG_CMD_PUTS(line)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (fputs(line, cmdfd) < 0 || fflush(cmdfd) < 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
      output_failed = true, output_errno = errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define PG_CMD_PRINTF1(fmt, arg1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (fprintf(cmdfd, fmt, arg1) < 0 || fflush(cmdfd) < 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      output_failed = true, output_errno = errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define PG_CMD_PRINTF2(fmt, arg1, arg2)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (fprintf(cmdfd, fmt, arg1, arg2) < 0 || fflush(cmdfd) < 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      output_failed = true, output_errno = errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define PG_CMD_PRINTF3(fmt, arg1, arg2, arg3)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (fprintf(cmdfd, fmt, arg1, arg2, arg3) < 0 || fflush(cmdfd) < 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      output_failed = true, output_errno = errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

   
                                                                      
                                           
   
static char *
escape_quotes(const char *src)
{
  char *result = escape_single_quotes_ascii(src);

  if (!result)
  {
    pg_log_error("out of memory");
    exit(1);
  }
  return result;
}

   
                                                          
                                                             
                                                             
                                                           
                                                                
                                                                
                                                  
   
static char *
escape_quotes_bki(const char *src)
{
  char *result;
  char *data = escape_quotes(src);
  char *resultp;
  char *datap;
  int nquotes = 0;

                                   
  datap = data;
  while ((datap = strchr(datap, '"')) != NULL)
  {
    nquotes++;
    datap++;
  }

  result = (char *)pg_malloc(strlen(data) + 3 + nquotes * 3);
  resultp = result;
  *resultp++ = '"';
  for (datap = data; *datap; datap++)
  {
    if (*datap == '"')
    {
      strcpy(resultp, "\\042");
      resultp += 4;
    }
    else
    {
      *resultp++ = *datap;
    }
  }
  *resultp++ = '"';
  *resultp = '\0';

  free(data);
  return result;
}

   
                                                                         
                                          
   
                                                                    
                                  
   
static char **
replace_token(char **lines, const char *token, const char *replacement)
{
  int numlines = 1;
  int i;
  char **result;
  int toklen, replen, diff;

  for (i = 0; lines[i]; i++)
  {
    numlines++;
  }

  result = (char **)pg_malloc(numlines * sizeof(char *));

  toklen = strlen(token);
  replen = strlen(replacement);
  diff = replen - toklen;

  for (i = 0; i < numlines; i++)
  {
    char *where;
    char *newline;
    int pre;

                                                       
    if (lines[i] == NULL || (where = strstr(lines[i], token)) == NULL)
    {
      result[i] = lines[i];
      continue;
    }

                                                             

    newline = (char *)pg_malloc(strlen(lines[i]) + diff + 1);

    pre = where - lines[i];

    memcpy(newline, lines[i], pre);

    memcpy(newline + pre, replacement, replen);

    strcpy(newline + pre + replen, lines[i] + pre + toklen);

    result[i] = newline;
  }

  return result;
}

   
                                                           
   
                                
   
#ifndef HAVE_UNIX_SOCKETS
static char **
filter_lines_with_token(char **lines, const char *token)
{
  int numlines = 1;
  int i, src, dst;
  char **result;

  for (i = 0; lines[i]; i++)
  {
    numlines++;
  }

  result = (char **)pg_malloc(numlines * sizeof(char *));

  for (src = 0, dst = 0; src < numlines; src++)
  {
    if (lines[src] == NULL || strstr(lines[src], token) == NULL)
    {
      result[dst++] = lines[src];
    }
  }

  return result;
}
#endif

   
                                  
   
static char **
readfile(const char *path)
{
  FILE *infile;
  int maxlength = 1, linelen = 0;
  int nlines = 0;
  int n;
  char **result;
  char *buffer;
  int c;

  if ((infile = fopen(path, "r")) == NULL)
  {
    pg_log_error("could not open file \"%s\" for reading: %m", path);
    exit(1);
  }

                                                                    

  while ((c = fgetc(infile)) != EOF)
  {
    linelen++;
    if (c == '\n')
    {
      nlines++;
      if (linelen > maxlength)
      {
        maxlength = linelen;
      }
      linelen = 0;
    }
  }

                                                             
  if (linelen)
  {
    nlines++;
  }
  if (linelen > maxlength)
  {
    maxlength = linelen;
  }

                                             
  result = (char **)pg_malloc((nlines + 1) * sizeof(char *));
  buffer = (char *)pg_malloc(maxlength + 1);

                                                  
  rewind(infile);
  n = 0;
  while (fgets(buffer, maxlength + 1, infile) != NULL && n < nlines)
  {
    result[n++] = pg_strdup(buffer);
  }

  fclose(infile);
  free(buffer);
  result[n] = NULL;

  return result;
}

   
                                     
   
                                                                         
                                                                             
   
static void
writefile(char *path, char **lines)
{
  FILE *out_file;
  char **line;

  if ((out_file = fopen(path, "w")) == NULL)
  {
    pg_log_error("could not open file \"%s\" for writing: %m", path);
    exit(1);
  }
  for (line = lines; *line != NULL; line++)
  {
    if (fputs(*line, out_file) < 0)
    {
      pg_log_error("could not write file \"%s\": %m", path);
      exit(1);
    }
    free(*line);
  }
  if (fclose(out_file))
  {
    pg_log_error("could not write file \"%s\": %m", path);
    exit(1);
  }
}

   
                                                   
   
static FILE *
popen_check(const char *command, const char *mode)
{
  FILE *cmdfd;

  fflush(stdout);
  fflush(stderr);
  errno = 0;
  cmdfd = popen(command, mode);
  if (cmdfd == NULL)
  {
    pg_log_error("could not execute command \"%s\": %m", command);
  }
  return cmdfd;
}

   
                                            
                                                  
   
static void
cleanup_directories_atexit(void)
{
  if (success)
  {
    return;
  }

  if (!noclean)
  {
    if (made_new_pgdata)
    {
      pg_log_info("removing data directory \"%s\"", pg_data);
      if (!rmtree(pg_data, true))
      {
        pg_log_error("failed to remove data directory");
      }
    }
    else if (found_existing_pgdata)
    {
      pg_log_info("removing contents of data directory \"%s\"", pg_data);
      if (!rmtree(pg_data, false))
      {
        pg_log_error("failed to remove contents of data directory");
      }
    }

    if (made_new_xlogdir)
    {
      pg_log_info("removing WAL directory \"%s\"", xlog_dir);
      if (!rmtree(xlog_dir, true))
      {
        pg_log_error("failed to remove WAL directory");
      }
    }
    else if (found_existing_xlogdir)
    {
      pg_log_info("removing contents of WAL directory \"%s\"", xlog_dir);
      if (!rmtree(xlog_dir, false))
      {
        pg_log_error("failed to remove contents of WAL directory");
      }
    }
                                                    
  }
  else
  {
    if (made_new_pgdata || found_existing_pgdata)
    {
      pg_log_info("data directory \"%s\" not removed at user's request", pg_data);
    }

    if (made_new_xlogdir || found_existing_xlogdir)
    {
      pg_log_info("WAL directory \"%s\" not removed at user's request", xlog_dir);
    }
  }
}

   
                         
   
                                   
   
static char *
get_id(void)
{
  const char *username;

#ifndef WIN32
  if (geteuid() == 0)                      
  {
    pg_log_error("cannot be run as root");
    fprintf(stderr, _("Please log in (using, e.g., \"su\") as the (unprivileged) user that will\n"
                      "own the server process.\n"));
    exit(1);
  }
#endif

  username = get_user_name_or_exit(progname);

  return pg_strdup(username);
}

static char *
encodingid_to_string(int enc)
{
  char result[20];

  sprintf(result, "%d", enc);
  return pg_strdup(result);
}

   
                                                 
   
static int
get_encoding_id(const char *encoding_name)
{
  int enc;

  if (encoding_name && *encoding_name)
  {
    if ((enc = pg_valid_server_encoding(encoding_name)) >= 0)
    {
      return enc;
    }
  }
  pg_log_error("\"%s\" is not a valid server encoding name", encoding_name ? encoding_name : "(null)");
  exit(1);
}

   
                                                                       
                                                                       
   
struct tsearch_config_match
{
  const char *tsconfname;
  const char *langname;
};

static const struct tsearch_config_match tsearch_config_languages[] = {
    {"arabic", "ar"}, {"arabic", "Arabic"}, {"danish", "da"}, {"danish", "Danish"}, {"dutch", "nl"}, {"dutch", "Dutch"}, {"english", "C"}, {"english", "POSIX"}, {"english", "en"}, {"english", "English"}, {"finnish", "fi"}, {"finnish", "Finnish"}, {"french", "fr"}, {"french", "French"}, {"german", "de"}, {"german", "German"}, {"hungarian", "hu"}, {"hungarian", "Hungarian"}, {"indonesian", "id"}, {"indonesian", "Indonesian"}, {"irish", "ga"}, {"irish", "Irish"}, {"italian", "it"}, {"italian", "Italian"}, {"lithuanian", "lt"}, {"lithuanian", "Lithuanian"}, {"nepali", "ne"}, {"nepali", "Nepali"}, {"norwegian", "no"}, {"norwegian", "Norwegian"}, {"portuguese", "pt"}, {"portuguese", "Portuguese"}, {"romanian", "ro"}, {"russian", "ru"}, {"russian", "Russian"}, {"spanish", "es"}, {"spanish", "Spanish"}, {"swedish", "sv"}, {"swedish", "Swedish"}, {"tamil", "ta"}, {"tamil", "Tamil"}, {"turkish", "tr"}, {"turkish", "Turkish"}, {NULL, NULL}                 
};

   
                                                                          
                                  
   
static const char *
find_matching_ts_config(const char *lc_type)
{
  int i;
  char *langname, *ptr;

     
                                                                          
                                                                     
                                   
     
                                                                           
                                                                            
                                                        
     
                                                    
     
  if (lc_type == NULL)
  {
    langname = pg_strdup("");
  }
  else
  {
    ptr = langname = pg_strdup(lc_type);
    while (*ptr && *ptr != '_' && *ptr != '-' && *ptr != '.' && *ptr != '@')
    {
      ptr++;
    }
    *ptr = '\0';
  }

  for (i = 0; tsearch_config_languages[i].tsconfname; i++)
  {
    if (pg_strcasecmp(tsearch_config_languages[i].langname, langname) == 0)
    {
      free(langname);
      return tsearch_config_languages[i].tsconfname;
    }
  }

  free(langname);
  return NULL;
}

   
                                                              
   
static void
set_input(char **dest, const char *filename)
{
  *dest = psprintf("%s/%s", share_path, filename);
}

   
                                      
   
static void
check_input(char *path)
{
  struct stat statbuf;

  if (stat(path, &statbuf) != 0)
  {
    if (errno == ENOENT)
    {
      pg_log_error("file \"%s\" does not exist", path);
      fprintf(stderr, _("This might mean you have a corrupted installation or identified\n"
                        "the wrong directory with the invocation option -L.\n"));
    }
    else
    {
      pg_log_error("could not access file \"%s\": %m", path);
      fprintf(stderr, _("This might mean you have a corrupted installation or identified\n"
                        "the wrong directory with the invocation option -L.\n"));
    }
    exit(1);
  }
  if (!S_ISREG(statbuf.st_mode))
  {
    pg_log_error("file \"%s\" is not a regular file", path);
    fprintf(stderr, _("This might mean you have a corrupted installation or identified\n"
                      "the wrong directory with the invocation option -L.\n"));
    exit(1);
  }
}

   
                                                                      
                            
   
static void
write_version_file(const char *extrapath)
{
  FILE *version_file;
  char *path;

  if (extrapath == NULL)
  {
    path = psprintf("%s/PG_VERSION", pg_data);
  }
  else
  {
    path = psprintf("%s/%s/PG_VERSION", pg_data, extrapath);
  }

  if ((version_file = fopen(path, PG_BINARY_W)) == NULL)
  {
    pg_log_error("could not open file \"%s\" for writing: %m", path);
    exit(1);
  }
  if (fprintf(version_file, "%s\n", PG_MAJORVERSION) < 0 || fclose(version_file))
  {
    pg_log_error("could not write file \"%s\": %m", path);
    exit(1);
  }
  free(path);
}

   
                                                                            
                  
   
static void
set_null_conf(void)
{
  FILE *conf_file;
  char *path;

  path = psprintf("%s/postgresql.conf", pg_data);
  conf_file = fopen(path, PG_BINARY_W);
  if (conf_file == NULL)
  {
    pg_log_error("could not open file \"%s\" for writing: %m", path);
    exit(1);
  }
  if (fclose(conf_file))
  {
    pg_log_error("could not write file \"%s\": %m", path);
    exit(1);
  }
  free(path);
}

   
                                                                          
                                                                         
                                                                          
                                                                        
                                                                         
                                                                            
                                                                          
                                                                        
                   
   
static const char *
choose_dsm_implementation(void)
{
#ifdef HAVE_SHM_OPEN
  int ntries = 10;

                                                                            
  srandom((unsigned int)(getpid() ^ time(NULL)));

  while (ntries > 0)
  {
    uint32 handle;
    char name[64];
    int fd;

    handle = random();
    snprintf(name, 64, "/PostgreSQL.%u", handle);
    if ((fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0600)) != -1)
    {
      close(fd);
      shm_unlink(name);
      return "posix";
    }
    if (errno != EEXIST)
    {
      break;
    }
    --ntries;
  }
#endif

#ifdef WIN32
  return "windows";
#else
  return "sysv";
#endif
}

   
                                               
   
                                                                 
   
static void
test_config_settings(void)
{
     
                                                                       
                                                                 
     
#define MIN_BUFS_FOR_CONNS(nconns) ((nconns) * 10)

  static const int trial_conns[] = {100, 50, 40, 30, 20};
  static const int trial_bufs[] = {16384, 8192, 4096, 3584, 3072, 2560, 2048, 1536, 1000, 900, 800, 700, 600, 500, 400, 300, 200, 100, 50};

  char cmd[MAXPGPATH];
  const int connslen = sizeof(trial_conns) / sizeof(int);
  const int bufslen = sizeof(trial_bufs) / sizeof(int);
  int i, status, test_conns, test_buffs, ok_buffers = 0;

     
                                                                           
                                                        
     
  printf(_("selecting dynamic shared memory implementation ... "));
  fflush(stdout);
  dynamic_shared_memory_type = choose_dsm_implementation();
  printf("%s\n", dynamic_shared_memory_type);

     
                                                                             
                                           
     
  printf(_("selecting default max_connections ... "));
  fflush(stdout);

  for (i = 0; i < connslen; i++)
  {
    test_conns = trial_conns[i];
    test_buffs = MIN_BUFS_FOR_CONNS(test_conns);

    snprintf(cmd, sizeof(cmd),
        "\"%s\" --boot -x0 %s "
        "-c max_connections=%d "
        "-c shared_buffers=%d "
        "-c dynamic_shared_memory_type=%s "
        "< \"%s\" > \"%s\" 2>&1",
        backend_exec, boot_options, test_conns, test_buffs, dynamic_shared_memory_type, DEVNULL, DEVNULL);
    status = system(cmd);
    if (status == 0)
    {
      ok_buffers = test_buffs;
      break;
    }
  }
  if (i >= connslen)
  {
    i = connslen - 1;
  }
  n_connections = trial_conns[i];

  printf("%d\n", n_connections);

  printf(_("selecting default shared_buffers ... "));
  fflush(stdout);

  for (i = 0; i < bufslen; i++)
  {
                                                          
    test_buffs = (trial_bufs[i] * 8192) / BLCKSZ;
    if (test_buffs <= ok_buffers)
    {
      test_buffs = ok_buffers;
      break;
    }

    snprintf(cmd, sizeof(cmd),
        "\"%s\" --boot -x0 %s "
        "-c max_connections=%d "
        "-c shared_buffers=%d "
        "-c dynamic_shared_memory_type=%s "
        "< \"%s\" > \"%s\" 2>&1",
        backend_exec, boot_options, n_connections, test_buffs, dynamic_shared_memory_type, DEVNULL, DEVNULL);
    status = system(cmd);
    if (status == 0)
    {
      break;
    }
  }
  n_buffers = test_buffs;

  if ((n_buffers * (BLCKSZ / 1024)) % 1024 == 0)
  {
    printf("%dMB\n", (n_buffers * (BLCKSZ / 1024)) / 1024);
  }
  else
  {
    printf("%dkB\n", n_buffers * (BLCKSZ / 1024));
  }

  printf(_("selecting default time zone ... "));
  fflush(stdout);
  default_timezone = select_default_timezone(share_path);
  printf("%s\n", default_timezone ? default_timezone : "GMT");
}

   
                                                        
   
static char *
pretty_wal_size(int segment_count)
{
  int sz = wal_segment_size_mb * segment_count;
  char *result = pg_malloc(14);

  if ((sz % 1024) == 0)
  {
    snprintf(result, 14, "%dGB", sz / 1024);
  }
  else
  {
    snprintf(result, 14, "%dMB", sz);
  }

  return result;
}

   
                               
   
static void
setup_config(void)
{
  char **conflines;
  char repltok[MAXPGPATH];
  char path[MAXPGPATH];
  char *autoconflines[3];

  fputs(_("creating configuration files ... "), stdout);
  fflush(stdout);

                       

  conflines = readfile(conf_file);

  snprintf(repltok, sizeof(repltok), "max_connections = %d", n_connections);
  conflines = replace_token(conflines, "#max_connections = 100", repltok);

  if ((n_buffers * (BLCKSZ / 1024)) % 1024 == 0)
  {
    snprintf(repltok, sizeof(repltok), "shared_buffers = %dMB", (n_buffers * (BLCKSZ / 1024)) / 1024);
  }
  else
  {
    snprintf(repltok, sizeof(repltok), "shared_buffers = %dkB", n_buffers * (BLCKSZ / 1024));
  }
  conflines = replace_token(conflines, "#shared_buffers = 32MB", repltok);

#ifdef HAVE_UNIX_SOCKETS
  snprintf(repltok, sizeof(repltok), "#unix_socket_directories = '%s'", DEFAULT_PGSOCKET_DIR);
#else
  snprintf(repltok, sizeof(repltok), "#unix_socket_directories = ''");
#endif
  conflines = replace_token(conflines, "#unix_socket_directories = '/tmp'", repltok);

#if DEF_PGPORT != 5432
  snprintf(repltok, sizeof(repltok), "#port = %d", DEF_PGPORT);
  conflines = replace_token(conflines, "#port = 5432", repltok);
#endif

                                                 
  snprintf(repltok, sizeof(repltok), "min_wal_size = %s", pretty_wal_size(DEFAULT_MIN_WAL_SEGS));
  conflines = replace_token(conflines, "#min_wal_size = 80MB", repltok);

  snprintf(repltok, sizeof(repltok), "max_wal_size = %s", pretty_wal_size(DEFAULT_MAX_WAL_SEGS));
  conflines = replace_token(conflines, "#max_wal_size = 1GB", repltok);

  snprintf(repltok, sizeof(repltok), "lc_messages = '%s'", escape_quotes(lc_messages));
  conflines = replace_token(conflines, "#lc_messages = 'C'", repltok);

  snprintf(repltok, sizeof(repltok), "lc_monetary = '%s'", escape_quotes(lc_monetary));
  conflines = replace_token(conflines, "#lc_monetary = 'C'", repltok);

  snprintf(repltok, sizeof(repltok), "lc_numeric = '%s'", escape_quotes(lc_numeric));
  conflines = replace_token(conflines, "#lc_numeric = 'C'", repltok);

  snprintf(repltok, sizeof(repltok), "lc_time = '%s'", escape_quotes(lc_time));
  conflines = replace_token(conflines, "#lc_time = 'C'", repltok);

  switch (locale_date_order(lc_time))
  {
  case DATEORDER_YMD:
    strcpy(repltok, "datestyle = 'iso, ymd'");
    break;
  case DATEORDER_DMY:
    strcpy(repltok, "datestyle = 'iso, dmy'");
    break;
  case DATEORDER_MDY:
  default:
    strcpy(repltok, "datestyle = 'iso, mdy'");
    break;
  }
  conflines = replace_token(conflines, "#datestyle = 'iso, mdy'", repltok);

  snprintf(repltok, sizeof(repltok), "default_text_search_config = 'pg_catalog.%s'", escape_quotes(default_text_search_config));
  conflines = replace_token(conflines, "#default_text_search_config = 'pg_catalog.simple'", repltok);

  if (default_timezone)
  {
    snprintf(repltok, sizeof(repltok), "timezone = '%s'", escape_quotes(default_timezone));
    conflines = replace_token(conflines, "#timezone = 'GMT'", repltok);
    snprintf(repltok, sizeof(repltok), "log_timezone = '%s'", escape_quotes(default_timezone));
    conflines = replace_token(conflines, "#log_timezone = 'GMT'", repltok);
  }

  snprintf(repltok, sizeof(repltok), "dynamic_shared_memory_type = %s", dynamic_shared_memory_type);
  conflines = replace_token(conflines, "#dynamic_shared_memory_type = posix", repltok);

#if DEFAULT_BACKEND_FLUSH_AFTER > 0
  snprintf(repltok, sizeof(repltok), "#backend_flush_after = %dkB", DEFAULT_BACKEND_FLUSH_AFTER * (BLCKSZ / 1024));
  conflines = replace_token(conflines, "#backend_flush_after = 0", repltok);
#endif

#if DEFAULT_BGWRITER_FLUSH_AFTER > 0
  snprintf(repltok, sizeof(repltok), "#bgwriter_flush_after = %dkB", DEFAULT_BGWRITER_FLUSH_AFTER * (BLCKSZ / 1024));
  conflines = replace_token(conflines, "#bgwriter_flush_after = 0", repltok);
#endif

#if DEFAULT_CHECKPOINT_FLUSH_AFTER > 0
  snprintf(repltok, sizeof(repltok), "#checkpoint_flush_after = %dkB", DEFAULT_CHECKPOINT_FLUSH_AFTER * (BLCKSZ / 1024));
  conflines = replace_token(conflines, "#checkpoint_flush_after = 0", repltok);
#endif

#ifndef USE_PREFETCH
  conflines = replace_token(conflines, "#effective_io_concurrency = 1", "#effective_io_concurrency = 0");
#endif

#ifdef WIN32
  conflines = replace_token(conflines, "#update_process_title = on", "#update_process_title = off");
#endif

  if (strcmp(authmethodlocal, "scram-sha-256") == 0 || strcmp(authmethodhost, "scram-sha-256") == 0)
  {
    conflines = replace_token(conflines, "#password_encryption = md5", "password_encryption = scram-sha-256");
  }

     
                                                                             
                                                                            
                                                                   
                
     
  if (pg_dir_create_mode == PG_DIR_MODE_GROUP)
  {
    conflines = replace_token(conflines, "#log_file_mode = 0600", "log_file_mode = 0640");
  }

  snprintf(path, sizeof(path), "%s/postgresql.conf", pg_data);

  writefile(path, conflines);
  if (chmod(path, pg_file_create_mode) != 0)
  {
    pg_log_error("could not change permissions of \"%s\": %m", path);
    exit(1);
  }

     
                                                                        
                                                                            
                                                                            
                
     
  autoconflines[0] = pg_strdup("# Do not edit this file manually!\n");
  autoconflines[1] = pg_strdup("# It will be overwritten by the ALTER SYSTEM command.\n");
  autoconflines[2] = NULL;

  sprintf(path, "%s/postgresql.auto.conf", pg_data);

  writefile(path, autoconflines);
  if (chmod(path, pg_file_create_mode) != 0)
  {
    pg_log_error("could not change permissions of \"%s\": %m", path);
    exit(1);
  }

  free(conflines);

                   

  conflines = readfile(hba_file);

#ifndef HAVE_UNIX_SOCKETS
  conflines = filter_lines_with_token(conflines, "@remove-line-for-nolocal@");
#else
  conflines = replace_token(conflines, "@remove-line-for-nolocal@", "");
#endif

#ifdef HAVE_IPV6

     
                                                                        
                                                                       
                                                                            
                                                                           
                                       
     
  {
    struct addrinfo *gai_result;
    struct addrinfo hints;
    int err = 0;

#ifdef WIN32
                                                            
    WSADATA wsaData;

    err = WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

                                                              
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    if (err != 0 || getaddrinfo("::1", NULL, &hints, &gai_result) != 0)
    {
      conflines = replace_token(conflines, "host    all             all             ::1", "#host    all             all             ::1");
      conflines = replace_token(conflines, "host    replication     all             ::1", "#host    replication     all             ::1");
    }
  }
#else                  
                                                                       
  conflines = replace_token(conflines, "host    all             all             ::1", "#host    all             all             ::1");
  conflines = replace_token(conflines, "host    replication     all             ::1", "#host    replication     all             ::1");
#endif                

                                              
  conflines = replace_token(conflines, "@authmethodhost@", authmethodhost);
  conflines = replace_token(conflines, "@authmethodlocal@", authmethodlocal);

  conflines = replace_token(conflines, "@authcomment@", (strcmp(authmethodlocal, "trust") == 0 || strcmp(authmethodhost, "trust") == 0) ? AUTHTRUST_WARNING : "");

  snprintf(path, sizeof(path), "%s/pg_hba.conf", pg_data);

  writefile(path, conflines);
  if (chmod(path, pg_file_create_mode) != 0)
  {
    pg_log_error("could not change permissions of \"%s\": %m", path);
    exit(1);
  }

  free(conflines);

                     

  conflines = readfile(ident_file);

  snprintf(path, sizeof(path), "%s/pg_ident.conf", pg_data);

  writefile(path, conflines);
  if (chmod(path, pg_file_create_mode) != 0)
  {
    pg_log_error("could not change permissions of \"%s\": %m", path);
    exit(1);
  }

  free(conflines);

  check_ok();
}

   
                                                            
   
static void
bootstrap_template1(void)
{
  PG_CMD_DECL;
  char **line;
  char **bki_lines;
  char headerline[MAXPGPATH];
  char buf[64];

  printf(_("running bootstrap script ... "));
  fflush(stdout);

  bki_lines = readfile(bki_file);

                                                              

  snprintf(headerline, sizeof(headerline), "# PostgreSQL %s\n", PG_MAJORVERSION);

  if (strcmp(headerline, *bki_lines) != 0)
  {
    pg_log_error("input file \"%s\" does not belong to PostgreSQL %s", bki_file, PG_VERSION);
    fprintf(stderr, _("Check your installation or specify the correct path "
                      "using the option -L.\n"));
    exit(1);
  }

                                                           

  sprintf(buf, "%d", NAMEDATALEN);
  bki_lines = replace_token(bki_lines, "NAMEDATALEN", buf);

  sprintf(buf, "%d", (int)sizeof(Pointer));
  bki_lines = replace_token(bki_lines, "SIZEOF_POINTER", buf);

  bki_lines = replace_token(bki_lines, "ALIGNOF_POINTER", (sizeof(Pointer) == 4) ? "i" : "d");

  bki_lines = replace_token(bki_lines, "FLOAT4PASSBYVAL", FLOAT4PASSBYVAL ? "true" : "false");

  bki_lines = replace_token(bki_lines, "FLOAT8PASSBYVAL", FLOAT8PASSBYVAL ? "true" : "false");

  bki_lines = replace_token(bki_lines, "POSTGRES", escape_quotes_bki(username));

  bki_lines = replace_token(bki_lines, "ENCODING", encodingid_to_string(encodingid));

  bki_lines = replace_token(bki_lines, "LC_COLLATE", escape_quotes_bki(lc_collate));

  bki_lines = replace_token(bki_lines, "LC_CTYPE", escape_quotes_bki(lc_ctype));

     
                                                   
     
                                                                          
                                                                
     
  snprintf(cmd, sizeof(cmd), "LC_COLLATE=%s", lc_collate);
  putenv(pg_strdup(cmd));

  snprintf(cmd, sizeof(cmd), "LC_CTYPE=%s", lc_ctype);
  putenv(pg_strdup(cmd));

  unsetenv("LC_ALL");

                                                                   
  unsetenv("PGCLIENTENCODING");

  snprintf(cmd, sizeof(cmd), "\"%s\" --boot -x1 -X %u %s %s %s", backend_exec, wal_segment_size_mb * (1024 * 1024), data_checksums ? "-k" : "", boot_options, debug ? "-d 5" : "");

  PG_CMD_OPEN;

  for (line = bki_lines; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
    free(*line);
  }

  PG_CMD_CLOSE;

  free(bki_lines);

  check_ok();
}

   
                                    
   
static void
setup_auth(FILE *cmdfd)
{
  const char *const *line;
  static const char *const pg_authid_setup[] = {   
                                                                                                                   
                                                                                              
                                                   
      "REVOKE ALL on pg_authid FROM public;\n\n", NULL};

  for (line = pg_authid_setup; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
  }

  if (superuser_password)
  {
    PG_CMD_PRINTF2("ALTER USER \"%s\" WITH PASSWORD E'%s';\n\n", username, escape_quotes(superuser_password));
  }
}

   
                                          
   
static void
get_su_pwd(void)
{
  char pwd1[100];
  char pwd2[100];

  if (pwprompt)
  {
       
                                   
       
    printf("\n");
    fflush(stdout);
    simple_prompt("Enter new superuser password: ", pwd1, sizeof(pwd1), false);
    simple_prompt("Enter it again: ", pwd2, sizeof(pwd2), false);
    if (strcmp(pwd1, pwd2) != 0)
    {
      fprintf(stderr, _("Passwords didn't match.\n"));
      exit(1);
    }
  }
  else
  {
       
                               
       
                                                                       
                                                                        
                                                                         
                
       
    FILE *pwf = fopen(pwfilename, "r");
    int i;

    if (!pwf)
    {
      pg_log_error("could not open file \"%s\" for reading: %m", pwfilename);
      exit(1);
    }
    if (!fgets(pwd1, sizeof(pwd1), pwf))
    {
      if (ferror(pwf))
      {
        pg_log_error("could not read password from file \"%s\": %m", pwfilename);
      }
      else
      {
        pg_log_error("password file \"%s\" is empty", pwfilename);
      }
      exit(1);
    }
    fclose(pwf);

    i = strlen(pwd1);
    while (i > 0 && (pwd1[i - 1] == '\r' || pwd1[i - 1] == '\n'))
    {
      pwd1[--i] = '\0';
    }
  }

  superuser_password = pg_strdup(pwd1);
}

   
                    
   
static void
setup_depend(FILE *cmdfd)
{
  const char *const *line;
  static const char *const pg_depend_setup[] = {   
                                                                                                                    
                                                                                                                   
                                                                                                                     
                                                                                                                 
                                                                            
                                                   
                                                                                                         
                                                   
                                                                                                                 
                                                           
                                                   
                                                                                                                    
                                                   
                                                                                                                  
                                                                                                               
                                                                
                                                   
                                                                                                                      
                                                                                               
                                                   
      "DELETE FROM pg_depend;\n\n", "VACUUM pg_depend;\n\n", "DELETE FROM pg_shdepend;\n\n", "VACUUM pg_shdepend;\n\n",

      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_class;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_proc;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_type;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_cast;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_constraint;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_conversion;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_attrdef;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_language;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_operator;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_opclass;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_opfamily;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_am;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_amop;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_amproc;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_rewrite;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_trigger;\n\n",

         
                                                                
         
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_namespace "
      "    WHERE nspname LIKE 'pg%';\n\n",

      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_ts_parser;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_ts_dict;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_ts_template;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_ts_config;\n\n",
      "INSERT INTO pg_depend SELECT 0,0,0, tableoid,oid,0, 'p' "
      " FROM pg_collation;\n\n",
      "INSERT INTO pg_shdepend SELECT 0,0,0,0, tableoid,oid, 'p' "
      " FROM pg_authid;\n\n",
      NULL};

  for (line = pg_depend_setup; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
  }
}

   
                       
   
static void
setup_sysviews(FILE *cmdfd)
{
  char **line;
  char **sysviews_setup;

  sysviews_setup = readfile(system_views_file);

  for (line = sysviews_setup; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
    free(*line);
  }

  PG_CMD_PUTS("\n\n");

  free(sysviews_setup);
}

   
                         
   
static void
setup_description(FILE *cmdfd)
{
  PG_CMD_PUTS("CREATE TEMP TABLE tmp_pg_description ( "
              "	objoid oid, "
              "	classname name, "
              "	objsubid int4, "
              "	description text);\n\n");

  PG_CMD_PRINTF1("COPY tmp_pg_description FROM E'%s';\n\n", escape_quotes(desc_file));

  PG_CMD_PUTS("INSERT INTO pg_description "
              " SELECT t.objoid, c.oid, t.objsubid, t.description "
              "  FROM tmp_pg_description t, pg_class c "
              "    WHERE c.relname = t.classname;\n\n");

  PG_CMD_PUTS("CREATE TEMP TABLE tmp_pg_shdescription ( "
              " objoid oid, "
              " classname name, "
              " description text);\n\n");

  PG_CMD_PRINTF1("COPY tmp_pg_shdescription FROM E'%s';\n\n", escape_quotes(shdesc_file));

  PG_CMD_PUTS("INSERT INTO pg_shdescription "
              " SELECT t.objoid, c.oid, t.description "
              "  FROM tmp_pg_shdescription t, pg_class c "
              "   WHERE c.relname = t.classname;\n\n");

                                                                         
  PG_CMD_PUTS("WITH funcdescs AS ( "
              "SELECT p.oid as p_oid, o.oid as o_oid, oprname "
              "FROM pg_proc p JOIN pg_operator o ON oprcode = p.oid ) "
              "INSERT INTO pg_description "
              "  SELECT p_oid, 'pg_proc'::regclass, 0, "
              "    'implementation of ' || oprname || ' operator' "
              "  FROM funcdescs "
              "  WHERE NOT EXISTS (SELECT 1 FROM pg_description "
              "   WHERE objoid = p_oid AND classoid = 'pg_proc'::regclass) "
              "  AND NOT EXISTS (SELECT 1 FROM pg_description "
              "   WHERE objoid = o_oid AND classoid = 'pg_operator'::regclass"
              "         AND description LIKE 'deprecated%');\n\n");

     
                                                                             
                                               
     
  PG_CMD_PUTS("DROP TABLE tmp_pg_description;\n\n");
  PG_CMD_PUTS("DROP TABLE tmp_pg_shdescription;\n\n");
}

   
                         
   
static void
setup_collation(FILE *cmdfd)
{
     
                                                                            
                                                                         
                                                            
     
  PG_CMD_PRINTF3("INSERT INTO pg_collation (oid, collname, collnamespace, collowner, collprovider, collisdeterministic, collencoding, collcollate, collctype)"
                 "VALUES (pg_nextoid('pg_catalog.pg_collation', 'oid', 'pg_catalog.pg_collation_oid_index'), 'ucs_basic', 'pg_catalog'::regnamespace, %u, '%c', true, %d, 'C', 'C');\n\n",
      BOOTSTRAP_SUPERUSERID, COLLPROVIDER_LIBC, PG_UTF8);

                                                                     
  PG_CMD_PUTS("SELECT pg_import_system_collations('pg_catalog');\n\n");
}

   
                                               
   
static void
setup_dictionary(FILE *cmdfd)
{
  char **line;
  char **conv_lines;

  conv_lines = readfile(dictionary_file);
  for (line = conv_lines; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
    free(*line);
  }

  PG_CMD_PUTS("\n\n");

  free(conv_lines);
}

   
                     
   
                                                                            
                                                                      
                       
   
                                                                    
                                                                      
                   
   
                                                                       
                                                                      
                                                                    
                                      
   
                                                                          
                                              
   
static void
setup_privileges(FILE *cmdfd)
{
  char **line;
  char **priv_lines;
  static char *privileges_setup[] = {"UPDATE pg_class "
                                     "  SET relacl = (SELECT array_agg(a.acl) FROM "
                                     " (SELECT E'=r/\"$POSTGRES_SUPERUSERNAME\"' as acl "
                                     "  UNION SELECT unnest(pg_catalog.acldefault("
                                     "    CASE WHEN relkind = " CppAsString2(RELKIND_SEQUENCE) " THEN 's' "
                                                                                               "         ELSE 'r' END::\"char\"," CppAsString2(BOOTSTRAP_SUPERUSERID) "::oid))"
                                                                                                                                                                      " ) as a) "
                                                                                                                                                                      "  WHERE relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_VIEW) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_SEQUENCE) ")"
                                                                                                                                                                                                                                                                                                                                   "  AND relacl IS NULL;\n\n",
      "GRANT USAGE ON SCHEMA pg_catalog TO PUBLIC;\n\n", "GRANT CREATE, USAGE ON SCHEMA public TO PUBLIC;\n\n", "REVOKE ALL ON pg_largeobject FROM PUBLIC;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE relname = 'pg_class'),"
      "        0,"
      "        relacl,"
      "        'i'"
      "    FROM"
      "        pg_class"
      "    WHERE"
      "        relacl IS NOT NULL"
      "        AND relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_VIEW) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_SEQUENCE) ");\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        pg_class.oid,"
      "        (SELECT oid FROM pg_class WHERE relname = 'pg_class'),"
      "        pg_attribute.attnum,"
      "        pg_attribute.attacl,"
      "        'i'"
      "    FROM"
      "        pg_class"
      "        JOIN pg_attribute ON (pg_class.oid = pg_attribute.attrelid)"
      "    WHERE"
      "        pg_attribute.attacl IS NOT NULL"
      "        AND pg_class.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_VIEW) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_SEQUENCE) ");\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE relname = 'pg_proc'),"
      "        0,"
      "        proacl,"
      "        'i'"
      "    FROM"
      "        pg_proc"
      "    WHERE"
      "        proacl IS NOT NULL;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE relname = 'pg_type'),"
      "        0,"
      "        typacl,"
      "        'i'"
      "    FROM"
      "        pg_type"
      "    WHERE"
      "        typacl IS NOT NULL;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE relname = 'pg_language'),"
      "        0,"
      "        lanacl,"
      "        'i'"
      "    FROM"
      "        pg_language"
      "    WHERE"
      "        lanacl IS NOT NULL;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE "
      "		  relname = 'pg_largeobject_metadata'),"
      "        0,"
      "        lomacl,"
      "        'i'"
      "    FROM"
      "        pg_largeobject_metadata"
      "    WHERE"
      "        lomacl IS NOT NULL;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE relname = 'pg_namespace'),"
      "        0,"
      "        nspacl,"
      "        'i'"
      "    FROM"
      "        pg_namespace"
      "    WHERE"
      "        nspacl IS NOT NULL;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class WHERE "
      "		  relname = 'pg_foreign_data_wrapper'),"
      "        0,"
      "        fdwacl,"
      "        'i'"
      "    FROM"
      "        pg_foreign_data_wrapper"
      "    WHERE"
      "        fdwacl IS NOT NULL;\n\n",
      "INSERT INTO pg_init_privs "
      "  (objoid, classoid, objsubid, initprivs, privtype)"
      "    SELECT"
      "        oid,"
      "        (SELECT oid FROM pg_class "
      "		  WHERE relname = 'pg_foreign_server'),"
      "        0,"
      "        srvacl,"
      "        'i'"
      "    FROM"
      "        pg_foreign_server"
      "    WHERE"
      "        srvacl IS NOT NULL;\n\n",
      NULL};

  priv_lines = replace_token(privileges_setup, "$POSTGRES_SUPERUSERNAME", escape_quotes(username));
  for (line = priv_lines; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
  }
}

   
                                                                          
                   
   
static void
set_info_version(void)
{
  char *letterversion;
  long major = 0, minor = 0, micro = 0;
  char *endptr;
  char *vstr = pg_strdup(PG_VERSION);
  char *ptr;

  ptr = vstr + (strlen(vstr) - 1);
  while (ptr != vstr && (*ptr < '0' || *ptr > '9'))
  {
    ptr--;
  }
  letterversion = ptr + 1;
  major = strtol(vstr, &endptr, 10);
  if (*endptr)
  {
    minor = strtol(endptr + 1, &endptr, 10);
  }
  if (*endptr)
  {
    micro = strtol(endptr + 1, &endptr, 10);
  }
  snprintf(infoversion, sizeof(infoversion), "%02ld.%02ld.%04ld%s", major, minor, micro, letterversion);
}

   
                                                    
   
static void
setup_schema(FILE *cmdfd)
{
  char **line;
  char **lines;

  lines = readfile(info_schema_file);

  for (line = lines; *line != NULL; line++)
  {
    PG_CMD_PUTS(*line);
    free(*line);
  }

  PG_CMD_PUTS("\n\n");

  free(lines);

  PG_CMD_PRINTF1("UPDATE information_schema.sql_implementation_info "
                 "  SET character_value = '%s' "
                 "  WHERE implementation_info_name = 'DBMS VERSION';\n\n",
      infoversion);

  PG_CMD_PRINTF1("COPY information_schema.sql_features "
                 "  (feature_id, feature_name, sub_feature_id, "
                 "  sub_feature_name, is_supported, comments) "
                 " FROM E'%s';\n\n",
      escape_quotes(features_file));
}

   
                                      
   
static void
load_plpgsql(FILE *cmdfd)
{
  PG_CMD_PUTS("CREATE EXTENSION plpgsql;\n\n");
}

   
                                    
   
static void
vacuum_db(FILE *cmdfd)
{
                                                               
  PG_CMD_PUTS("ANALYZE;\n\nVACUUM FREEZE;\n\n");
}

   
                               
   
static void
make_template0(FILE *cmdfd)
{
  const char *const *line;
  static const char *const template0_setup[] = {"CREATE DATABASE template0 IS_TEMPLATE = true ALLOW_CONNECTIONS = false;\n\n",

         
                                                             
         
      "UPDATE pg_database SET datlastsysoid = "
      "    (SELECT oid FROM pg_database "
      "    WHERE datname = 'template0');\n\n",

         
                                                                      
                                                                            
                    
         
      "REVOKE CREATE,TEMPORARY ON DATABASE template1 FROM public;\n\n", "REVOKE CREATE,TEMPORARY ON DATABASE template0 FROM public;\n\n",

      "COMMENT ON DATABASE template0 IS 'unmodifiable empty database';\n\n",

         
                                                             
         
      "VACUUM pg_database;\n\n", NULL};

  for (line = template0_setup; *line; line++)
  {
    PG_CMD_PUTS(*line);
  }
}

   
                              
   
static void
make_postgres(FILE *cmdfd)
{
  const char *const *line;
  static const char *const postgres_setup[] = {"CREATE DATABASE postgres;\n\n", "COMMENT ON DATABASE postgres IS 'default administrative connection database';\n\n", NULL};

  for (line = postgres_setup; *line; line++)
  {
    PG_CMD_PUTS(*line);
  }
}

   
                                              
   
                               
                                                                      
                                                                            
                                                                           
                               
   
                                                                             
                    
   
                                                                    
                                                                      
                                                                          
                                                                         
                                                                             
                                                               
   
                                                                               
                                              
   
static void
trapsig(int signum)
{
                                                                 
  pqsignal(signum, trapsig);
  caught_signal = true;
}

   
                                                        
   
static void
check_ok(void)
{
  if (caught_signal)
  {
    printf(_("caught signal\n"));
    fflush(stdout);
    exit(1);
  }
  else if (output_failed)
  {
    printf(_("could not write to child process: %s\n"), strerror(output_errno));
    fflush(stdout);
    exit(1);
  }
  else
  {
                        
    printf(_("ok\n"));
    fflush(stdout);
  }
}

                                                                   
static inline size_t
my_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
  return strftime(s, max, fmt, tm);
}

   
                                           
   
static int
locale_date_order(const char *locale)
{
  struct tm testtime;
  char buf[128];
  char *posD;
  char *posM;
  char *posY;
  char *save;
  size_t res;
  int result;

  result = DATEORDER_MDY;              

  save = setlocale(LC_TIME, NULL);
  if (!save)
  {
    return result;
  }
  save = pg_strdup(save);

  setlocale(LC_TIME, locale);

  memset(&testtime, 0, sizeof(testtime));
  testtime.tm_mday = 22;
  testtime.tm_mon = 10;                                          
  testtime.tm_year = 133;           

  res = my_strftime(buf, sizeof(buf), "%x", &testtime);

  setlocale(LC_TIME, save);
  free(save);

  if (res == 0)
  {
    return result;
  }

  posM = strstr(buf, "11");
  posD = strstr(buf, "22");
  posY = strstr(buf, "33");

  if (!posM || !posD || !posY)
  {
    return result;
  }

  if (posY < posM && posM < posD)
  {
    result = DATEORDER_YMD;
  }
  else if (posD < posM)
  {
    result = DATEORDER_DMY;
  }
  else
  {
    result = DATEORDER_MDY;
  }

  return result;
}

   
                                                             
   
                                                                            
                                                                               
                                                                      
                                                                              
                                                                           
                                               
   
                                                           
   
static void
check_locale_name(int category, const char *locale, char **canonname)
{
  char *save;
  char *res;

  if (canonname)
  {
    *canonname = NULL;                         
  }

  save = setlocale(category, NULL);
  if (!save)
  {
    pg_log_error("setlocale() failed");
    exit(1);
  }

                                                                          
  save = pg_strdup(save);

                            
  if (!locale)
  {
    locale = "";
  }

                                                               
  res = setlocale(category, locale);

                                         
  if (res && canonname)
  {
    *canonname = pg_strdup(res);
  }

                          
  if (!setlocale(category, save))
  {
    pg_log_error("failed to restore old locale \"%s\"", save);
    exit(1);
  }
  free(save);

                                       
  if (res == NULL)
  {
    if (*locale)
    {
      pg_log_error("invalid locale name \"%s\"", locale);
    }
    else
    {
         
                                                                       
                                                                       
                                                                         
                                                                     
                                                                       
                                                                     
         
      pg_log_error("invalid locale settings; check LANG and LC_* environment variables");
    }
    exit(1);
  }
}

   
                                                                            
   
                                                                          
   
static bool
check_locale_encoding(const char *locale, int user_enc)
{
  int locale_enc;

  locale_enc = pg_get_encoding_from_locale(locale, true);

                                                         
  if (!(locale_enc == user_enc || locale_enc == PG_SQL_ASCII || locale_enc == -1 ||
#ifdef WIN32
          user_enc == PG_UTF8 ||
#endif
          user_enc == PG_SQL_ASCII))
  {
    pg_log_error("encoding mismatch");
    fprintf(stderr,
        _("The encoding you selected (%s) and the encoding that the\n"
          "selected locale uses (%s) do not match.  This would lead to\n"
          "misbehavior in various character string processing functions.\n"
          "Rerun %s and either do not specify an encoding explicitly,\n"
          "or choose a matching combination.\n"),
        pg_encoding_to_char(user_enc), pg_encoding_to_char(locale_enc), progname);
    return false;
  }
  return true;
}

   
                               
   
                                                                              
   
static void
setlocales(void)
{
  char *canonname;

                                                     

  if (locale)
  {
    if (!lc_ctype)
    {
      lc_ctype = locale;
    }
    if (!lc_collate)
    {
      lc_collate = locale;
    }
    if (!lc_numeric)
    {
      lc_numeric = locale;
    }
    if (!lc_time)
    {
      lc_time = locale;
    }
    if (!lc_monetary)
    {
      lc_monetary = locale;
    }
    if (!lc_messages)
    {
      lc_messages = locale;
    }
  }

     
                                                                       
                         
     

  check_locale_name(LC_CTYPE, lc_ctype, &canonname);
  lc_ctype = canonname;
  check_locale_name(LC_COLLATE, lc_collate, &canonname);
  lc_collate = canonname;
  check_locale_name(LC_NUMERIC, lc_numeric, &canonname);
  lc_numeric = canonname;
  check_locale_name(LC_TIME, lc_time, &canonname);
  lc_time = canonname;
  check_locale_name(LC_MONETARY, lc_monetary, &canonname);
  lc_monetary = canonname;
#if defined(LC_MESSAGES) && !defined(WIN32)
  check_locale_name(LC_MESSAGES, lc_messages, &canonname);
  lc_messages = canonname;
#else
                                                                   
  check_locale_name(LC_CTYPE, lc_messages, &canonname);
  lc_messages = canonname;
#endif
}

   
                   
   
static void
usage(const char *progname)
{
  printf(_("%s initializes a PostgreSQL database cluster.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]... [DATADIR]\n"), progname);
  printf(_("\nOptions:\n"));
  printf(_("  -A, --auth=METHOD         default authentication method for local connections\n"));
  printf(_("      --auth-host=METHOD    default authentication method for local TCP/IP connections\n"));
  printf(_("      --auth-local=METHOD   default authentication method for local-socket connections\n"));
  printf(_(" [-D, --pgdata=]DATADIR     location for this database cluster\n"));
  printf(_("  -E, --encoding=ENCODING   set default encoding for new databases\n"));
  printf(_("  -g, --allow-group-access  allow group read/execute on data directory\n"));
  printf(_("      --locale=LOCALE       set default locale for new databases\n"));
  printf(_("      --lc-collate=, --lc-ctype=, --lc-messages=LOCALE\n"
           "      --lc-monetary=, --lc-numeric=, --lc-time=LOCALE\n"
           "                            set default locale in the respective category for\n"
           "                            new databases (default taken from environment)\n"));
  printf(_("      --no-locale           equivalent to --locale=C\n"));
  printf(_("      --pwfile=FILE         read password for the new superuser from file\n"));
  printf(_("  -T, --text-search-config=CFG\n"
           "                            default text search configuration\n"));
  printf(_("  -U, --username=NAME       database superuser name\n"));
  printf(_("  -W, --pwprompt            prompt for a password for the new superuser\n"));
  printf(_("  -X, --waldir=WALDIR       location for the write-ahead log directory\n"));
  printf(_("      --wal-segsize=SIZE    size of WAL segments, in megabytes\n"));
  printf(_("\nLess commonly used options:\n"));
  printf(_("  -d, --debug               generate lots of debugging output\n"));
  printf(_("  -k, --data-checksums      use data page checksums\n"));
  printf(_("  -L DIRECTORY              where to find the input files\n"));
  printf(_("  -n, --no-clean            do not clean up after errors\n"));
  printf(_("  -N, --no-sync             do not wait for changes to be written safely to disk\n"));
  printf(_("  -s, --show                show internal settings\n"));
  printf(_("  -S, --sync-only           only sync data directory\n"));
  printf(_("\nOther options:\n"));
  printf(_("  -V, --version             output version information, then exit\n"));
  printf(_("  -?, --help                show this help, then exit\n"));
  printf(_("\nIf the data directory is not specified, the environment variable PGDATA\n"
           "is used.\n"));
  printf(_("\nReport bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

static void
check_authmethod_unspecified(const char **authmethod)
{
  if (*authmethod == NULL)
  {
    authwarning = true;
    *authmethod = "trust";
  }
}

static void
check_authmethod_valid(const char *authmethod, const char *const *valid_methods, const char *conntype)
{
  const char *const *p;

  for (p = valid_methods; *p; p++)
  {
    if (strcmp(authmethod, *p) == 0)
    {
      return;
    }
                            
    if (strchr(authmethod, ' '))
    {
      if (strncmp(authmethod, *p, (authmethod - strchr(authmethod, ' '))) == 0)
      {
        return;
      }
    }
  }

  pg_log_error("invalid authentication method \"%s\" for \"%s\" connections", authmethod, conntype);
  exit(1);
}

static void
check_need_password(const char *authmethodlocal, const char *authmethodhost)
{
  if ((strcmp(authmethodlocal, "md5") == 0 || strcmp(authmethodlocal, "password") == 0 || strcmp(authmethodlocal, "scram-sha-256") == 0) && (strcmp(authmethodhost, "md5") == 0 || strcmp(authmethodhost, "password") == 0 || strcmp(authmethodhost, "scram-sha-256") == 0) && !(pwprompt || pwfilename))
  {
    pg_log_error("must specify a password for the superuser to enable %s authentication", (strcmp(authmethodlocal, "md5") == 0 || strcmp(authmethodlocal, "password") == 0 || strcmp(authmethodlocal, "scram-sha-256") == 0) ? authmethodlocal : authmethodhost);
    exit(1);
  }
}

void
setup_pgdata(void)
{
  char *pgdata_get_env, *pgdata_set_env;

  if (!pg_data)
  {
    pgdata_get_env = getenv("PGDATA");
    if (pgdata_get_env && strlen(pgdata_get_env))
    {
                        
      pg_data = pg_strdup(pgdata_get_env);
    }
    else
    {
      pg_log_error("no data directory specified");
      fprintf(stderr, _("You must identify the directory where the data for this database system\n"
                        "will reside.  Do this with either the invocation option -D or the\n"
                        "environment variable PGDATA.\n"));
      exit(1);
    }
  }

  pgdata_native = pg_strdup(pg_data);
  canonicalize_path(pg_data);

     
                                                                           
                                                                             
                                                                             
                           
     
  pgdata_set_env = psprintf("PGDATA=%s", pg_data);
  putenv(pgdata_set_env);
}

void
setup_bin_paths(const char *argv0)
{
  int ret;

  if ((ret = find_other_exec(argv0, "postgres", PG_BACKEND_VERSIONSTR, backend_exec)) < 0)
  {
    char full_path[MAXPGPATH];

    if (find_my_exec(argv0, full_path) < 0)
    {
      strlcpy(full_path, progname, sizeof(full_path));
    }

    if (ret == -1)
    {
      pg_log_error("The program \"postgres\" is needed by %s but was not found in the\n"
                   "same directory as \"%s\".\n"
                   "Check your installation.",
          progname, full_path);
    }
    else
    {
      pg_log_error("The program \"postgres\" was found by \"%s\"\n"
                   "but was not the same version as %s.\n"
                   "Check your installation.",
          full_path, progname);
    }
    exit(1);
  }

                              
  strcpy(bin_path, backend_exec);
  *last_dir_separator(bin_path) = '\0';
  canonicalize_path(bin_path);

  if (!share_path)
  {
    share_path = pg_malloc(MAXPGPATH);
    get_share_path(backend_exec, share_path);
  }
  else if (!is_absolute_path(share_path))
  {
    pg_log_error("input file location must be an absolute path");
    exit(1);
  }

  canonicalize_path(share_path);
}

void
setup_locale_encoding(void)
{
  setlocales();

  if (strcmp(lc_ctype, lc_collate) == 0 && strcmp(lc_ctype, lc_time) == 0 && strcmp(lc_ctype, lc_numeric) == 0 && strcmp(lc_ctype, lc_monetary) == 0 && strcmp(lc_ctype, lc_messages) == 0)
  {
    printf(_("The database cluster will be initialized with locale \"%s\".\n"), lc_ctype);
  }
  else
  {
    printf(_("The database cluster will be initialized with locales\n"
             "  COLLATE:  %s\n"
             "  CTYPE:    %s\n"
             "  MESSAGES: %s\n"
             "  MONETARY: %s\n"
             "  NUMERIC:  %s\n"
             "  TIME:     %s\n"),
        lc_collate, lc_ctype, lc_messages, lc_monetary, lc_numeric, lc_time);
  }

  if (!encoding)
  {
    int ctype_enc;

    ctype_enc = pg_get_encoding_from_locale(lc_ctype, true);

    if (ctype_enc == -1)
    {
                                                   
      pg_log_error("could not find suitable encoding for locale \"%s\"", lc_ctype);
      fprintf(stderr, _("Rerun %s with the -E option.\n"), progname);
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
    else if (!pg_valid_server_encoding_id(ctype_enc))
    {
         
                                                                    
                                                                      
                
         
#ifdef WIN32
      encodingid = PG_UTF8;
      printf(_("Encoding \"%s\" implied by locale is not allowed as a server-side encoding.\n"
               "The default database encoding will be set to \"%s\" instead.\n"),
          pg_encoding_to_char(ctype_enc), pg_encoding_to_char(encodingid));
#else
      pg_log_error("locale \"%s\" requires unsupported encoding \"%s\"", lc_ctype, pg_encoding_to_char(ctype_enc));
      fprintf(stderr,
          _("Encoding \"%s\" is not allowed as a server-side encoding.\n"
            "Rerun %s with a different locale selection.\n"),
          pg_encoding_to_char(ctype_enc), progname);
      exit(1);
#endif
    }
    else
    {
      encodingid = ctype_enc;
      printf(_("The default database encoding has accordingly been set to \"%s\".\n"), pg_encoding_to_char(encodingid));
    }
  }
  else
  {
    encodingid = get_encoding_id(encoding);
  }

  if (!check_locale_encoding(lc_ctype, encodingid) || !check_locale_encoding(lc_collate, encodingid))
  {
    exit(1);                                              
  }
}

void
setup_data_file_paths(void)
{
  set_input(&bki_file, "postgres.bki");
  set_input(&desc_file, "postgres.description");
  set_input(&shdesc_file, "postgres.shdescription");
  set_input(&hba_file, "pg_hba.conf.sample");
  set_input(&ident_file, "pg_ident.conf.sample");
  set_input(&conf_file, "postgresql.conf.sample");
  set_input(&dictionary_file, "snowball_create.sql");
  set_input(&info_schema_file, "information_schema.sql");
  set_input(&features_file, "sql_features.txt");
  set_input(&system_views_file, "system_views.sql");

  if (show_setting || debug)
  {
    fprintf(stderr,
        "VERSION=%s\n"
        "PGDATA=%s\nshare_path=%s\nPGPATH=%s\n"
        "POSTGRES_SUPERUSERNAME=%s\nPOSTGRES_BKI=%s\n"
        "POSTGRES_DESCR=%s\nPOSTGRES_SHDESCR=%s\n"
        "POSTGRESQL_CONF_SAMPLE=%s\n"
        "PG_HBA_SAMPLE=%s\nPG_IDENT_SAMPLE=%s\n",
        PG_VERSION, pg_data, share_path, bin_path, username, bki_file, desc_file, shdesc_file, conf_file, hba_file, ident_file);
    if (show_setting)
    {
      exit(0);
    }
  }

  check_input(bki_file);
  check_input(desc_file);
  check_input(shdesc_file);
  check_input(hba_file);
  check_input(ident_file);
  check_input(conf_file);
  check_input(dictionary_file);
  check_input(info_schema_file);
  check_input(features_file);
  check_input(system_views_file);
}

void
setup_text_search(void)
{
  if (!default_text_search_config)
  {
    default_text_search_config = find_matching_ts_config(lc_ctype);
    if (!default_text_search_config)
    {
      pg_log_info("could not find suitable text search configuration for locale \"%s\"", lc_ctype);
      default_text_search_config = "simple";
    }
  }
  else
  {
    const char *checkmatch = find_matching_ts_config(lc_ctype);

    if (checkmatch == NULL)
    {
      pg_log_warning("suitable text search configuration for locale \"%s\" is unknown", lc_ctype);
    }
    else if (strcmp(checkmatch, default_text_search_config) != 0)
    {
      pg_log_warning("specified text search configuration \"%s\" might not match locale \"%s\"", default_text_search_config, lc_ctype);
    }
  }

  printf(_("The default text search configuration will be set to \"%s\".\n"), default_text_search_config);
}

void
setup_signals(void)
{
                                              
#ifdef SIGHUP
  pqsignal(SIGHUP, trapsig);
#endif
#ifdef SIGINT
  pqsignal(SIGINT, trapsig);
#endif
#ifdef SIGQUIT
  pqsignal(SIGQUIT, trapsig);
#endif
#ifdef SIGTERM
  pqsignal(SIGTERM, trapsig);
#endif

                                                                  
#ifdef SIGPIPE
  pqsignal(SIGPIPE, SIG_IGN);
#endif

                                                                           
#ifdef SIGSYS
  pqsignal(SIGSYS, SIG_IGN);
#endif
}

void
create_data_directory(void)
{
  int ret;

  switch ((ret = pg_check_dir(pg_data)))
  {
  case 0:
                                          
    printf(_("creating directory %s ... "), pg_data);
    fflush(stdout);

    if (pg_mkdir_p(pg_data, pg_dir_create_mode) != 0)
    {
      pg_log_error("could not create directory \"%s\": %m", pg_data);
      exit(1);
    }
    else
    {
      check_ok();
    }

    made_new_pgdata = true;
    break;

  case 1:
                                                       
    printf(_("fixing permissions on existing directory %s ... "), pg_data);
    fflush(stdout);

    if (chmod(pg_data, pg_dir_create_mode) != 0)
    {
      pg_log_error("could not change permissions of directory \"%s\": %m", pg_data);
      exit(1);
    }
    else
    {
      check_ok();
    }

    found_existing_pgdata = true;
    break;

  case 2:
  case 3:
  case 4:
                               
    pg_log_error("directory \"%s\" exists but is not empty", pg_data);
    if (ret != 4)
    {
      warn_on_mount_point(ret);
    }
    else
    {
      fprintf(stderr,
          _("If you want to create a new database system, either remove or empty\n"
            "the directory \"%s\" or run %s\n"
            "with an argument other than \"%s\".\n"),
          pg_data, progname, pg_data);
    }
    exit(1);                                

  default:
                                     
    pg_log_error("could not access directory \"%s\": %m", pg_data);
    exit(1);
  }
}

                                                   
void
create_xlog_or_symlink(void)
{
  char *subdirloc;

                                                              
  subdirloc = psprintf("%s/pg_wal", pg_data);

  if (xlog_dir)
  {
    int ret;

                                                           
    canonicalize_path(xlog_dir);
    if (!is_absolute_path(xlog_dir))
    {
      pg_log_error("WAL directory location must be an absolute path");
      exit(1);
    }

                                                               
    switch ((ret = pg_check_dir(xlog_dir)))
    {
    case 0:
                                                    
      printf(_("creating directory %s ... "), xlog_dir);
      fflush(stdout);

      if (pg_mkdir_p(xlog_dir, pg_dir_create_mode) != 0)
      {
        pg_log_error("could not create directory \"%s\": %m", xlog_dir);
        exit(1);
      }
      else
      {
        check_ok();
      }

      made_new_xlogdir = true;
      break;

    case 1:
                                                         
      printf(_("fixing permissions on existing directory %s ... "), xlog_dir);
      fflush(stdout);

      if (chmod(xlog_dir, pg_dir_create_mode) != 0)
      {
        pg_log_error("could not change permissions of directory \"%s\": %m", xlog_dir);
        exit(1);
      }
      else
      {
        check_ok();
      }

      found_existing_xlogdir = true;
      break;

    case 2:
    case 3:
    case 4:
                                 
      pg_log_error("directory \"%s\" exists but is not empty", xlog_dir);
      if (ret != 4)
      {
        warn_on_mount_point(ret);
      }
      else
      {
        fprintf(stderr,
            _("If you want to store the WAL there, either remove or empty the directory\n"
              "\"%s\".\n"),
            xlog_dir);
      }
      exit(1);

    default:
                                       
      pg_log_error("could not access directory \"%s\": %m", xlog_dir);
      exit(1);
    }

#ifdef HAVE_SYMLINK
    if (symlink(xlog_dir, subdirloc) != 0)
    {
      pg_log_error("could not create symbolic link \"%s\": %m", subdirloc);
      exit(1);
    }
#else
    pg_log_error("symlinks are not supported on this platform");
    exit(1);
#endif
  }
  else
  {
                                                                
    if (mkdir(subdirloc, pg_dir_create_mode) < 0)
    {
      pg_log_error("could not create directory \"%s\": %m", subdirloc);
      exit(1);
    }
  }

  free(subdirloc);
}

void
warn_on_mount_point(int error)
{
  if (error == 2)
  {
    fprintf(stderr, _("It contains a dot-prefixed/invisible file, perhaps due to it being a mount point.\n"));
  }
  else if (error == 3)
  {
    fprintf(stderr, _("It contains a lost+found directory, perhaps due to it being a mount point.\n"));
  }

  fprintf(stderr, _("Using a mount point directly as the data directory is not recommended.\n"
                    "Create a subdirectory under the mount point.\n"));
}

void
initialize_data_directory(void)
{
  PG_CMD_DECL;
  int i;

  setup_signals();

     
                                                                        
                                                                           
                                                                             
                                           
     
  umask(pg_mode_mask);

  create_data_directory();

  create_xlog_or_symlink();

                                                          
  printf(_("creating subdirectories ... "));
  fflush(stdout);

  for (i = 0; i < lengthof(subdirs); i++)
  {
    char *path;

    path = psprintf("%s/%s", pg_data, subdirs[i]);

       
                                                                        
                                                                          
       
    if (mkdir(path, pg_dir_create_mode) < 0)
    {
      pg_log_error("could not create directory \"%s\": %m", path);
      exit(1);
    }

    free(path);
  }

  check_ok();

                                                                         
  write_version_file(NULL);

                                              
  set_null_conf();
  test_config_settings();

                                            
  setup_config();

                           
  bootstrap_template1();

     
                                                                           
     
  write_version_file("base/1");

     
                                                                       
                                                
     
  fputs(_("performing post-bootstrap initialization ... "), stdout);
  fflush(stdout);

  snprintf(cmd, sizeof(cmd), "\"%s\" %s template1 >%s", backend_exec, backend_options, DEVNULL);

  PG_CMD_OPEN;

  setup_auth(cmdfd);

  setup_depend(cmdfd);

     
                                                                         
                                                    
     

  setup_sysviews(cmdfd);

  setup_description(cmdfd);

  setup_collation(cmdfd);

  setup_dictionary(cmdfd);

  setup_privileges(cmdfd);

  setup_schema(cmdfd);

  load_plpgsql(cmdfd);

  vacuum_db(cmdfd);

  make_template0(cmdfd);

  make_postgres(cmdfd);

  PG_CMD_CLOSE;

  check_ok();
}

int
main(int argc, char *argv[])
{
  static struct option long_options[] = {{"pgdata", required_argument, NULL, 'D'}, {"encoding", required_argument, NULL, 'E'}, {"locale", required_argument, NULL, 1}, {"lc-collate", required_argument, NULL, 2}, {"lc-ctype", required_argument, NULL, 3}, {"lc-monetary", required_argument, NULL, 4}, {"lc-numeric", required_argument, NULL, 5}, {"lc-time", required_argument, NULL, 6}, {"lc-messages", required_argument, NULL, 7}, {"no-locale", no_argument, NULL, 8}, {"text-search-config", required_argument, NULL, 'T'}, {"auth", required_argument, NULL, 'A'}, {"auth-local", required_argument, NULL, 10}, {"auth-host", required_argument, NULL, 11}, {"pwprompt", no_argument, NULL, 'W'}, {"pwfile", required_argument, NULL, 9}, {"username", required_argument, NULL, 'U'}, {"help", no_argument, NULL, '?'}, {"version", no_argument, NULL, 'V'}, {"debug", no_argument, NULL, 'd'}, {"show", no_argument, NULL, 's'}, {"noclean", no_argument, NULL, 'n'},                                  
      {"no-clean", no_argument, NULL, 'n'}, {"nosync", no_argument, NULL, 'N'},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
      {"no-sync", no_argument, NULL, 'N'}, {"sync-only", no_argument, NULL, 'S'}, {"waldir", required_argument, NULL, 'X'}, {"wal-segsize", required_argument, NULL, 12}, {"data-checksums", no_argument, NULL, 'k'}, {"allow-group-access", no_argument, NULL, 'g'}, {NULL, 0, NULL, 0}};

     
                                                                         
                               
     
  int c;
  int option_index;
  char *effective_user;
  PQExpBuffer start_db_cmd;
  char pg_ctl_path[MAXPGPATH];

     
                                                                    
                                                                    
                                                                          
                                                                       
     
  setvbuf(stdout, NULL, PG_IOLBF, 0);

  pg_logging_init(argv[0]);
  progname = get_progname(argv[0]);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("initdb"));

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      usage(progname);
      exit(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      puts("initdb (PostgreSQL) " PG_VERSION);
      exit(0);
    }
  }

                                    

  while ((c = getopt_long(argc, argv, "A:dD:E:gkL:nNsST:U:WX:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
    case 'A':
      authmethodlocal = authmethodhost = pg_strdup(optarg);

         
                                                                  
                                                                
                      
         
      if (strcmp(authmethodhost, "ident") == 0)
      {
        authmethodlocal = "peer";
      }
      else if (strcmp(authmethodlocal, "peer") == 0)
      {
        authmethodhost = "ident";
      }
      break;
    case 10:
      authmethodlocal = pg_strdup(optarg);
      break;
    case 11:
      authmethodhost = pg_strdup(optarg);
      break;
    case 'D':
      pg_data = pg_strdup(optarg);
      break;
    case 'E':
      encoding = pg_strdup(optarg);
      break;
    case 'W':
      pwprompt = true;
      break;
    case 'U':
      username = pg_strdup(optarg);
      break;
    case 'd':
      debug = true;
      printf(_("Running in debug mode.\n"));
      break;
    case 'n':
      noclean = true;
      printf(_("Running in no-clean mode.  Mistakes will not be cleaned up.\n"));
      break;
    case 'N':
      do_sync = false;
      break;
    case 'S':
      sync_only = true;
      break;
    case 'k':
      data_checksums = true;
      break;
    case 'L':
      share_path = pg_strdup(optarg);
      break;
    case 1:
      locale = pg_strdup(optarg);
      break;
    case 2:
      lc_collate = pg_strdup(optarg);
      break;
    case 3:
      lc_ctype = pg_strdup(optarg);
      break;
    case 4:
      lc_monetary = pg_strdup(optarg);
      break;
    case 5:
      lc_numeric = pg_strdup(optarg);
      break;
    case 6:
      lc_time = pg_strdup(optarg);
      break;
    case 7:
      lc_messages = pg_strdup(optarg);
      break;
    case 8:
      locale = "C";
      break;
    case 9:
      pwfilename = pg_strdup(optarg);
      break;
    case 's':
      show_setting = true;
      break;
    case 'T':
      default_text_search_config = pg_strdup(optarg);
      break;
    case 'X':
      xlog_dir = pg_strdup(optarg);
      break;
    case 12:
      str_wal_segment_size_mb = pg_strdup(optarg);
      break;
    case 'g':
      SetDataDirectoryCreatePerm(PG_DIR_MODE_GROUP);
      break;
    default:
                                                   
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
  }

     
                                                                       
                                          
     
  if (optind < argc && !pg_data)
  {
    pg_data = pg_strdup(argv[optind]);
    optind++;
  }

  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  atexit(cleanup_directories_atexit);

                                                     
  if (sync_only)
  {
    setup_pgdata();

                                               
    if (pg_check_dir(pg_data) <= 0)
    {
      pg_log_error("could not access directory \"%s\": %m", pg_data);
      exit(1);
    }

    fputs(_("syncing data to disk ... "), stdout);
    fflush(stdout);
    fsync_pgdata(pg_data, PG_VERSION_NUM);
    check_ok();
    return 0;
  }

  if (pwprompt && pwfilename)
  {
    pg_log_error("password prompt and password file cannot be specified together");
    exit(1);
  }

  check_authmethod_unspecified(&authmethodlocal);
  check_authmethod_unspecified(&authmethodhost);

  check_authmethod_valid(authmethodlocal, auth_methods_local, "local");
  check_authmethod_valid(authmethodhost, auth_methods_host, "host");

  check_need_password(authmethodlocal, authmethodhost);

                            
  if (str_wal_segment_size_mb == NULL)
  {
    wal_segment_size_mb = (DEFAULT_XLOG_SEG_SIZE) / (1024 * 1024);
  }
  else
  {
    char *endptr;

                                             
    wal_segment_size_mb = strtol(str_wal_segment_size_mb, &endptr, 10);

                                               
    if (endptr == str_wal_segment_size_mb || *endptr != '\0')
    {
      pg_log_error("argument of --wal-segsize must be a number");
      exit(1);
    }
    if (!IsValidWalSegSize(wal_segment_size_mb * 1024 * 1024))
    {
      pg_log_error("argument of --wal-segsize must be a power of 2 between 1 and 1024");
      exit(1);
    }
  }

  get_restricted_token();

  setup_pgdata();

  setup_bin_paths(argv[0]);

  effective_user = get_id();
  if (!username)
  {
    username = effective_user;
  }

  if (strncmp(username, "pg_", 3) == 0)
  {
    pg_log_error("superuser name \"%s\" is disallowed; role names cannot begin with \"pg_\"", username);
    exit(1);
  }

  printf(_("The files belonging to this database system will be owned "
           "by user \"%s\".\n"
           "This user must also own the server process.\n\n"),
      effective_user);

  set_info_version();

  setup_data_file_paths();

  setup_locale_encoding();

  setup_text_search();

  printf("\n");

  if (data_checksums)
  {
    printf(_("Data page checksums are enabled.\n"));
  }
  else
  {
    printf(_("Data page checksums are disabled.\n"));
  }

  if (pwprompt || pwfilename)
  {
    get_su_pwd();
  }

  printf("\n");

  initialize_data_directory();

  if (do_sync)
  {
    fputs(_("syncing data to disk ... "), stdout);
    fflush(stdout);
    fsync_pgdata(pg_data, PG_VERSION_NUM);
    check_ok();
  }
  else
  {
    printf(_("\nSync to disk skipped.\nThe data directory might become corrupt if the operating system crashes.\n"));
  }

  if (authwarning)
  {
    printf("\n");
    pg_log_warning("enabling \"trust\" authentication for local connections");
    fprintf(stderr, _("You can change this by editing pg_hba.conf or using the option -A, or\n"
                      "--auth-local and --auth-host, the next time you run initdb.\n"));
  }

     
                                                                       
     
  start_db_cmd = createPQExpBuffer();

                                                            
  strlcpy(pg_ctl_path, argv[0], sizeof(pg_ctl_path));
  canonicalize_path(pg_ctl_path);
  get_parent_directory(pg_ctl_path);
                                     
  join_path_components(pg_ctl_path, pg_ctl_path, "pg_ctl");

                                                 
  make_native_path(pg_ctl_path);

                                       
  appendShellString(start_db_cmd, pg_ctl_path);

                                                          
  appendPQExpBufferStr(start_db_cmd, " -D ");
  appendShellString(start_db_cmd, pgdata_native);

                                                   
                                                             
  appendPQExpBuffer(start_db_cmd, " -l %s start", _("logfile"));

  printf(_("\nSuccess. You can now start the database server using:\n\n"
           "    %s\n\n"),
      start_db_cmd->data);

  destroyPQExpBuffer(start_db_cmd);

  success = true;
  return 0;
}
