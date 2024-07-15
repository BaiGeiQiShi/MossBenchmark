                                                                            
   
                       
                                              
   
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "access/xlog_internal.h"
#include "catalog/pg_control.h"
#include "common/controldata_utils.h"
#include "common/file_perm.h"
#ifdef FRONTEND
#include "common/logging.h"
#endif
#include "port/pg_crc32c.h"

#ifndef FRONTEND
#include "pgstat.h"
#include "storage/fd.h"
#endif

   
                     
   
                                                                             
                      
   
                                                                            
                         
   
ControlFileData *
get_controlfile(const char *DataDir, bool *crc_ok_p)
{
  ControlFileData *ControlFile;
  int fd;
  char ControlFilePath[MAXPGPATH];
  pg_crc32c crc;
  int r;

  AssertArg(crc_ok_p);

  ControlFile = palloc(sizeof(ControlFileData));
  snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", DataDir);

#ifndef FRONTEND
  if ((fd = OpenTransientFile(ControlFilePath, O_RDONLY | PG_BINARY)) == -1)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" for reading: %m", ControlFilePath)));
  }
#else
  if ((fd = open(ControlFilePath, O_RDONLY | PG_BINARY, 0)) == -1)
  {
    pg_log_fatal("could not open file \"%s\" for reading: %m", ControlFilePath);
    exit(EXIT_FAILURE);
  }
#endif

  r = read(fd, ControlFile, sizeof(ControlFileData));
  if (r != sizeof(ControlFileData))
  {
    if (r < 0)
#ifndef FRONTEND
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", ControlFilePath)));
#else
    {
      pg_log_fatal("could not read file \"%s\": %m", ControlFilePath);
      exit(EXIT_FAILURE);
    }
#endif
    else
#ifndef FRONTEND
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", ControlFilePath, r, sizeof(ControlFileData))));
#else
    {
      pg_log_fatal("could not read file \"%s\": read %d of %zu", ControlFilePath, r, sizeof(ControlFileData));
      exit(EXIT_FAILURE);
    }
#endif
  }

#ifndef FRONTEND
  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", ControlFilePath)));
  }
#else
  if (close(fd))
  {
    pg_log_fatal("could not close file \"%s\": %m", ControlFilePath);
    exit(EXIT_FAILURE);
  }
#endif

                      
  INIT_CRC32C(crc);
  COMP_CRC32C(crc, (char *)ControlFile, offsetof(ControlFileData, crc));
  FIN_CRC32C(crc);

  *crc_ok_p = EQ_CRC32C(crc, ControlFile->crc);

                                                       
  if (ControlFile->pg_control_version % 65536 == 0 && ControlFile->pg_control_version / 65536 != 0)
#ifndef FRONTEND
    elog(ERROR, _("byte ordering mismatch"));
#else
    pg_log_warning("possible byte ordering mismatch\n"
                   "The byte ordering used to store the pg_control file might not match the one\n"
                   "used by this program.  In that case the results below would be incorrect, and\n"
                   "the PostgreSQL installation would be incompatible with this data directory.");
#endif

  return ControlFile;
}

   
                        
   
                                                                     
                                                                     
                                                                          
                                                                    
                           
   
void
update_controlfile(const char *DataDir, ControlFileData *ControlFile, bool do_sync)
{
  int fd;
  char buffer[PG_CONTROL_FILE_SIZE];
  char ControlFilePath[MAXPGPATH];

     
                                                                          
     
  StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_MAX_SAFE_SIZE, "pg_control is too large for atomic disk writes");
  StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_FILE_SIZE, "sizeof(ControlFileData) exceeds PG_CONTROL_FILE_SIZE");

                                       
  INIT_CRC32C(ControlFile->crc);
  COMP_CRC32C(ControlFile->crc, (char *)ControlFile, offsetof(ControlFileData, crc));
  FIN_CRC32C(ControlFile->crc);

     
                                                                          
                                                                             
                             
     
  memset(buffer, 0, PG_CONTROL_FILE_SIZE);
  memcpy(buffer, ControlFile, sizeof(ControlFileData));

  snprintf(ControlFilePath, sizeof(ControlFilePath), "%s/%s", DataDir, XLOG_CONTROL_FILE);

#ifndef FRONTEND

     
                                                                            
                                        
     
  if ((fd = BasicOpenFile(ControlFilePath, O_RDWR | PG_BINARY)) < 0)
  {
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", ControlFilePath)));
  }
#else
  if ((fd = open(ControlFilePath, O_WRONLY | PG_BINARY, pg_file_create_mode)) == -1)
  {
    pg_log_fatal("could not open file \"%s\": %m", ControlFilePath);
    exit(EXIT_FAILURE);
  }
#endif

  errno = 0;
#ifndef FRONTEND
  pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_WRITE_UPDATE);
#endif
  if (write(fd, buffer, PG_CONTROL_FILE_SIZE) != PG_CONTROL_FILE_SIZE)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }

#ifndef FRONTEND
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", ControlFilePath)));
#else
    pg_log_fatal("could not write file \"%s\": %m", ControlFilePath);
    exit(EXIT_FAILURE);
#endif
  }
#ifndef FRONTEND
  pgstat_report_wait_end();
#endif

  if (do_sync)
  {
#ifndef FRONTEND
    pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_SYNC_UPDATE);
    if (pg_fsync(fd) != 0)
    {
      ereport(PANIC, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", ControlFilePath)));
    }
    pgstat_report_wait_end();
#else
    if (fsync(fd) != 0)
    {
      pg_log_fatal("could not fsync file \"%s\": %m", ControlFilePath);
      exit(EXIT_FAILURE);
    }
#endif
  }

  if (close(fd) < 0)
  {
#ifndef FRONTEND
    ereport(PANIC, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", ControlFilePath)));
#else
    pg_log_fatal("could not close file \"%s\": %m", ControlFilePath);
    exit(EXIT_FAILURE);
#endif
  }
}
