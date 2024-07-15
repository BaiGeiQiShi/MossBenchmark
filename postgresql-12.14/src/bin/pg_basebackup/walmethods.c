                                                                            
   
                                                                          
   
                                                                        
                                                              
   
                                                                         
   
                  
                                         
                                                                            
   

#include "postgres_fe.h"

#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#include "pgtar.h"
#include "common/file_perm.h"
#include "common/file_utils.h"
#include "common/logging.h"
#include "receivelog.h"
#include "streamutil.h"

                                     
#define ZLIB_OUT_SIZE 4096

                                                                            
                                                                     
                                                                            
   

   
                                      
   
typedef struct DirectoryMethodData
{
  char *basedir;
  int compression;
  bool sync;
  const char *lasterrstring;                                              
  int lasterrno;
} DirectoryMethodData;
static DirectoryMethodData *dir_data = NULL;

   
                     
   
typedef struct DirectoryMethodFile
{
  int fd;
  off_t currpos;
  char *pathname;
  char *fullpath;
  char *temp_suffix;
#ifdef HAVE_LIBZ
  gzFile gzfp;
#endif
} DirectoryMethodFile;

#define dir_clear_error() (dir_data->lasterrstring = NULL, dir_data->lasterrno = 0)
#define dir_set_error(msg) (dir_data->lasterrstring = _(msg))

static const char *
dir_getlasterror(void)
{
  if (dir_data->lasterrstring)
  {
    return dir_data->lasterrstring;
  }
  return strerror(dir_data->lasterrno);
}

static char *
dir_get_file_name(const char *pathname, const char *temp_suffix)
{
  char *filename = pg_malloc0(MAXPGPATH * sizeof(char));

  snprintf(filename, MAXPGPATH, "%s%s%s", pathname, dir_data->compression > 0 ? ".gz" : "", temp_suffix ? temp_suffix : "");

  return filename;
}

static Walfile
dir_open_for_write(const char *pathname, const char *temp_suffix, size_t pad_to_size)
{
  char tmppath[MAXPGPATH];
  char *filename;
  int fd;
  DirectoryMethodFile *f;
#ifdef HAVE_LIBZ
  gzFile gzfp = NULL;
#endif

  dir_clear_error();

  filename = dir_get_file_name(pathname, temp_suffix);
  snprintf(tmppath, sizeof(tmppath), "%s/%s", dir_data->basedir, filename);
  pg_free(filename);

     
                                                                          
                                                                         
                                                                          
           
     
  fd = open(tmppath, O_WRONLY | O_CREAT | PG_BINARY, pg_file_create_mode);
  if (fd < 0)
  {
    dir_data->lasterrno = errno;
    return NULL;
  }

#ifdef HAVE_LIBZ
  if (dir_data->compression > 0)
  {
    gzfp = gzdopen(fd, "wb");
    if (gzfp == NULL)
    {
      dir_data->lasterrno = errno;
      close(fd);
      return NULL;
    }

    if (gzsetparams(gzfp, dir_data->compression, Z_DEFAULT_STRATEGY) != Z_OK)
    {
      dir_data->lasterrno = errno;
      gzclose(gzfp);
      return NULL;
    }
  }
#endif

                                              
  if (pad_to_size && dir_data->compression == 0)
  {
    PGAlignedXLogBlock zerobuf;
    int bytes;

    memset(zerobuf.data, 0, XLOG_BLCKSZ);
    for (bytes = 0; bytes < pad_to_size; bytes += XLOG_BLCKSZ)
    {
      errno = 0;
      if (write(fd, zerobuf.data, XLOG_BLCKSZ) != XLOG_BLCKSZ)
      {
                                                                        
        dir_data->lasterrno = errno ? errno : ENOSPC;
        close(fd);
        return NULL;
      }
    }

    if (lseek(fd, 0, SEEK_SET) != 0)
    {
      dir_data->lasterrno = errno;
      close(fd);
      return NULL;
    }
  }

     
                                                                    
                                                                      
                                                                           
                                                  
     
  if (dir_data->sync)
  {
    if (fsync_fname(tmppath, false) != 0 || fsync_parent_path(tmppath) != 0)
    {
      dir_data->lasterrno = errno;
#ifdef HAVE_LIBZ
      if (dir_data->compression > 0)
      {
        gzclose(gzfp);
      }
      else
#endif
        close(fd);
      return NULL;
    }
  }

  f = pg_malloc0(sizeof(DirectoryMethodFile));
#ifdef HAVE_LIBZ
  if (dir_data->compression > 0)
  {
    f->gzfp = gzfp;
  }
#endif
  f->fd = fd;
  f->currpos = 0;
  f->pathname = pg_strdup(pathname);
  f->fullpath = pg_strdup(tmppath);
  if (temp_suffix)
  {
    f->temp_suffix = pg_strdup(temp_suffix);
  }

  return f;
}

static ssize_t
dir_write(Walfile f, const void *buf, size_t count)
{
  ssize_t r;
  DirectoryMethodFile *df = (DirectoryMethodFile *)f;

  Assert(f != NULL);
  dir_clear_error();

#ifdef HAVE_LIBZ
  if (dir_data->compression > 0)
  {
    errno = 0;
    r = (ssize_t)gzwrite(df->gzfp, buf, count);
    if (r != count)
    {
                                                                      
      dir_data->lasterrno = errno ? errno : ENOSPC;
    }
  }
  else
#endif
  {
    errno = 0;
    r = write(df->fd, buf, count);
    if (r != count)
    {
                                                                      
      dir_data->lasterrno = errno ? errno : ENOSPC;
    }
  }
  if (r > 0)
  {
    df->currpos += r;
  }
  return r;
}

static off_t
dir_get_current_pos(Walfile f)
{
  Assert(f != NULL);
  dir_clear_error();

                                                     
  return ((DirectoryMethodFile *)f)->currpos;
}

static int
dir_close(Walfile f, WalCloseMethod method)
{
  int r;
  DirectoryMethodFile *df = (DirectoryMethodFile *)f;
  char tmppath[MAXPGPATH];
  char tmppath2[MAXPGPATH];

  Assert(f != NULL);
  dir_clear_error();

#ifdef HAVE_LIBZ
  if (dir_data->compression > 0)
  {
    errno = 0;                                       
    r = gzclose(df->gzfp);
  }
  else
#endif
    r = close(df->fd);

  if (r == 0)
  {
                                                       
    if (method == CLOSE_NORMAL && df->temp_suffix)
    {
      char *filename;
      char *filename2;

         
                                                                     
               
         
      filename = dir_get_file_name(df->pathname, df->temp_suffix);
      snprintf(tmppath, sizeof(tmppath), "%s/%s", dir_data->basedir, filename);
      pg_free(filename);

                                                     
      filename2 = dir_get_file_name(df->pathname, NULL);
      snprintf(tmppath2, sizeof(tmppath2), "%s/%s", dir_data->basedir, filename2);
      pg_free(filename2);
      r = durable_rename(tmppath, tmppath2);
    }
    else if (method == CLOSE_UNLINK)
    {
      char *filename;

                                            
      filename = dir_get_file_name(df->pathname, df->temp_suffix);
      snprintf(tmppath, sizeof(tmppath), "%s/%s", dir_data->basedir, filename);
      pg_free(filename);
      r = unlink(tmppath);
    }
    else
    {
         
                                                         
                                                                      
                                              
         
      if (dir_data->sync)
      {
        r = fsync_fname(df->fullpath, false);
        if (r == 0)
        {
          r = fsync_parent_path(df->fullpath);
        }
      }
    }
  }

  if (r != 0)
  {
    dir_data->lasterrno = errno;
  }

  pg_free(df->pathname);
  pg_free(df->fullpath);
  if (df->temp_suffix)
  {
    pg_free(df->temp_suffix);
  }
  pg_free(df);

  return r;
}

static int
dir_sync(Walfile f)
{
  int r;

  Assert(f != NULL);
  dir_clear_error();

  if (!dir_data->sync)
  {
    return 0;
  }

#ifdef HAVE_LIBZ
  if (dir_data->compression > 0)
  {
    if (gzflush(((DirectoryMethodFile *)f)->gzfp, Z_SYNC_FLUSH) != Z_OK)
    {
      dir_data->lasterrno = errno;
      return -1;
    }
  }
#endif

  r = fsync(((DirectoryMethodFile *)f)->fd);
  if (r < 0)
  {
    dir_data->lasterrno = errno;
  }
  return r;
}

static ssize_t
dir_get_file_size(const char *pathname)
{
  struct stat statbuf;
  char tmppath[MAXPGPATH];

  snprintf(tmppath, sizeof(tmppath), "%s/%s", dir_data->basedir, pathname);

  if (stat(tmppath, &statbuf) != 0)
  {
    dir_data->lasterrno = errno;
    return -1;
  }

  return statbuf.st_size;
}

static int
dir_compression(void)
{
  return dir_data->compression;
}

static bool
dir_existsfile(const char *pathname)
{
  char tmppath[MAXPGPATH];
  int fd;

  dir_clear_error();

  snprintf(tmppath, sizeof(tmppath), "%s/%s", dir_data->basedir, pathname);

  fd = open(tmppath, O_RDONLY | PG_BINARY, 0);
  if (fd < 0)
  {
    return false;
  }
  close(fd);
  return true;
}

static bool
dir_finish(void)
{
  dir_clear_error();

  if (dir_data->sync)
  {
       
                                                                        
                                     
       
    if (fsync_fname(dir_data->basedir, true) != 0)
    {
      dir_data->lasterrno = errno;
      return false;
    }
  }
  return true;
}

WalWriteMethod *
CreateWalDirectoryMethod(const char *basedir, int compression, bool sync)
{
  WalWriteMethod *method;

  method = pg_malloc0(sizeof(WalWriteMethod));
  method->open_for_write = dir_open_for_write;
  method->write = dir_write;
  method->get_current_pos = dir_get_current_pos;
  method->get_file_size = dir_get_file_size;
  method->get_file_name = dir_get_file_name;
  method->compression = dir_compression;
  method->close = dir_close;
  method->sync = dir_sync;
  method->existsfile = dir_existsfile;
  method->finish = dir_finish;
  method->getlasterror = dir_getlasterror;

  dir_data = pg_malloc0(sizeof(DirectoryMethodData));
  dir_data->compression = compression;
  dir_data->basedir = pg_strdup(basedir);
  dir_data->sync = sync;

  return method;
}

void
FreeWalDirectoryMethod(void)
{
  pg_free(dir_data->basedir);
  pg_free(dir_data);
  dir_data = NULL;
}

                                                                            
                                                                     
                                                                            
   

typedef struct TarMethodFile
{
  off_t ofs_start;                                                  
  off_t currpos;
  char header[512];
  char *pathname;
  size_t pad_to_size;
} TarMethodFile;

typedef struct TarMethodData
{
  char *tarfilename;
  int fd;
  int compression;
  bool sync;
  TarMethodFile *currentfile;
  const char *lasterrstring;                                              
  int lasterrno;
#ifdef HAVE_LIBZ
  z_streamp zp;
  void *zlibOut;
#endif
} TarMethodData;
static TarMethodData *tar_data = NULL;

#define tar_clear_error() (tar_data->lasterrstring = NULL, tar_data->lasterrno = 0)
#define tar_set_error(msg) (tar_data->lasterrstring = _(msg))

static const char *
tar_getlasterror(void)
{
  if (tar_data->lasterrstring)
  {
    return tar_data->lasterrstring;
  }
  return strerror(tar_data->lasterrno);
}

#ifdef HAVE_LIBZ
static bool
tar_write_compressed_data(void *buf, size_t count, bool flush)
{
  tar_data->zp->next_in = buf;
  tar_data->zp->avail_in = count;

  while (tar_data->zp->avail_in || flush)
  {
    int r;

    r = deflate(tar_data->zp, flush ? Z_FINISH : Z_NO_FLUSH);
    if (r == Z_STREAM_ERROR)
    {
      tar_set_error("could not compress data");
      return false;
    }

    if (tar_data->zp->avail_out < ZLIB_OUT_SIZE)
    {
      size_t len = ZLIB_OUT_SIZE - tar_data->zp->avail_out;

      errno = 0;
      if (write(tar_data->fd, tar_data->zlibOut, len) != len)
      {
                                                                        
        tar_data->lasterrno = errno ? errno : ENOSPC;
        return false;
      }

      tar_data->zp->next_out = tar_data->zlibOut;
      tar_data->zp->avail_out = ZLIB_OUT_SIZE;
    }

    if (r == Z_STREAM_END)
    {
      break;
    }
  }

  if (flush)
  {
                                      
    if (deflateReset(tar_data->zp) != Z_OK)
    {
      tar_set_error("could not reset compression stream");
      return false;
    }
  }

  return true;
}
#endif

static ssize_t
tar_write(Walfile f, const void *buf, size_t count)
{
  ssize_t r;

  Assert(f != NULL);
  tar_clear_error();

                                                    
  if (!tar_data->compression)
  {
    errno = 0;
    r = write(tar_data->fd, buf, count);
    if (r != count)
    {
                                                                      
      tar_data->lasterrno = errno ? errno : ENOSPC;
      return -1;
    }
    ((TarMethodFile *)f)->currpos += r;
    return r;
  }
#ifdef HAVE_LIBZ
  else
  {
    if (!tar_write_compressed_data(unconstify(void *, buf), count, false))
    {
      return -1;
    }
    ((TarMethodFile *)f)->currpos += count;
    return count;
  }
#else
  else
  {
                                                         
    tar_data->lasterrno = ENOSYS;
    return -1;
  }
#endif
}

static bool
tar_write_padding_data(TarMethodFile *f, size_t bytes)
{
  PGAlignedXLogBlock zerobuf;
  size_t bytesleft = bytes;

  memset(zerobuf.data, 0, XLOG_BLCKSZ);
  while (bytesleft)
  {
    size_t bytestowrite = Min(bytesleft, XLOG_BLCKSZ);
    ssize_t r = tar_write(f, zerobuf.data, bytestowrite);

    if (r < 0)
    {
      return false;
    }
    bytesleft -= r;
  }

  return true;
}

static char *
tar_get_file_name(const char *pathname, const char *temp_suffix)
{
  char *filename = pg_malloc0(MAXPGPATH * sizeof(char));

  snprintf(filename, MAXPGPATH, "%s%s", pathname, temp_suffix ? temp_suffix : "");

  return filename;
}

static Walfile
tar_open_for_write(const char *pathname, const char *temp_suffix, size_t pad_to_size)
{
  char *tmppath;

  tar_clear_error();

  if (tar_data->fd < 0)
  {
       
                                                                   
       
    tar_data->fd = open(tar_data->tarfilename, O_WRONLY | O_CREAT | PG_BINARY, pg_file_create_mode);
    if (tar_data->fd < 0)
    {
      tar_data->lasterrno = errno;
      return NULL;
    }

#ifdef HAVE_LIBZ
    if (tar_data->compression)
    {
      tar_data->zp = (z_streamp)pg_malloc(sizeof(z_stream));
      tar_data->zp->zalloc = Z_NULL;
      tar_data->zp->zfree = Z_NULL;
      tar_data->zp->opaque = Z_NULL;
      tar_data->zp->next_out = tar_data->zlibOut;
      tar_data->zp->avail_out = ZLIB_OUT_SIZE;

         
                                                                        
                                                                     
                               
         
      if (deflateInit2(tar_data->zp, tar_data->compression, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
      {
        pg_free(tar_data->zp);
        tar_data->zp = NULL;
        tar_set_error("could not initialize compression library");
        return NULL;
      }
    }
#endif

                                                                          
  }

  if (tar_data->currentfile != NULL)
  {
    tar_set_error("implementation error: tar files can't have more than one open file");
    return NULL;
  }

  tar_data->currentfile = pg_malloc0(sizeof(TarMethodFile));

  tmppath = tar_get_file_name(pathname, temp_suffix);

                                                                               
  if (tarCreateHeader(tar_data->currentfile->header, tmppath, NULL, 0, S_IRUSR | S_IWUSR, 0, 0, time(NULL)) != TAR_OK)
  {
    pg_free(tar_data->currentfile);
    pg_free(tmppath);
    tar_data->currentfile = NULL;
    tar_set_error("could not create tar header");
    return NULL;
  }

  pg_free(tmppath);

#ifdef HAVE_LIBZ
  if (tar_data->compression)
  {
                             
    if (!tar_write_compressed_data(NULL, 0, true))
    {
      return NULL;
    }

                                         
    if (deflateParams(tar_data->zp, 0, Z_DEFAULT_STRATEGY) != Z_OK)
    {
      tar_set_error("could not change compression parameters");
      return NULL;
    }
  }
#endif

  tar_data->currentfile->ofs_start = lseek(tar_data->fd, 0, SEEK_CUR);
  if (tar_data->currentfile->ofs_start == -1)
  {
    tar_data->lasterrno = errno;
    pg_free(tar_data->currentfile);
    tar_data->currentfile = NULL;
    return NULL;
  }
  tar_data->currentfile->currpos = 0;

  if (!tar_data->compression)
  {
    errno = 0;
    if (write(tar_data->fd, tar_data->currentfile->header, 512) != 512)
    {
                                                                      
      tar_data->lasterrno = errno ? errno : ENOSPC;
      pg_free(tar_data->currentfile);
      tar_data->currentfile = NULL;
      return NULL;
    }
  }
#ifdef HAVE_LIBZ
  else
  {
                                                                    
    if (!tar_write_compressed_data(tar_data->currentfile->header, 512, true))
    {
      return NULL;
    }

                                                        
    if (deflateParams(tar_data->zp, tar_data->compression, Z_DEFAULT_STRATEGY) != Z_OK)
    {
      tar_set_error("could not change compression parameters");
      return NULL;
    }
  }
#endif

  tar_data->currentfile->pathname = pg_strdup(pathname);

     
                                                                             
             
     
  if (pad_to_size)
  {
    tar_data->currentfile->pad_to_size = pad_to_size;
    if (!tar_data->compression)
    {
                                    
      if (!tar_write_padding_data(tar_data->currentfile, pad_to_size))
      {
        return NULL;
      }
                              
      if (lseek(tar_data->fd, tar_data->currentfile->ofs_start + 512, SEEK_SET) != tar_data->currentfile->ofs_start + 512)
      {
        tar_data->lasterrno = errno;
        return NULL;
      }

      tar_data->currentfile->currpos = 0;
    }
  }

  return tar_data->currentfile;
}

static ssize_t
tar_get_file_size(const char *pathname)
{
  tar_clear_error();

                                            
  tar_data->lasterrno = ENOSYS;
  return -1;
}

static int
tar_compression(void)
{
  return tar_data->compression;
}

static off_t
tar_get_current_pos(Walfile f)
{
  Assert(f != NULL);
  tar_clear_error();

  return ((TarMethodFile *)f)->currpos;
}

static int
tar_sync(Walfile f)
{
  int r;

  Assert(f != NULL);
  tar_clear_error();

  if (!tar_data->sync)
  {
    return 0;
  }

     
                                                                             
                                                         
     
  if (tar_data->compression)
  {
    return 0;
  }

  r = fsync(tar_data->fd);
  if (r < 0)
  {
    tar_data->lasterrno = errno;
  }
  return r;
}

static int
tar_close(Walfile f, WalCloseMethod method)
{
  ssize_t filesize;
  int padding;
  TarMethodFile *tf = (TarMethodFile *)f;

  Assert(f != NULL);
  tar_clear_error();

  if (method == CLOSE_UNLINK)
  {
    if (tar_data->compression)
    {
      tar_set_error("unlink not supported with compression");
      return -1;
    }

       
                                                                    
                                                                         
                                            
       
    if (ftruncate(tar_data->fd, tf->ofs_start) != 0)
    {
      tar_data->lasterrno = errno;
      return -1;
    }

    pg_free(tf->pathname);
    pg_free(tf);
    tar_data->currentfile = NULL;

    return 0;
  }

     
                                                                     
                                                                           
                                   
     
  if (tf->pad_to_size)
  {
    if (tar_data->compression)
    {
         
                                                                      
                                                          
         
      size_t sizeleft = tf->pad_to_size - tf->currpos;

      if (sizeleft)
      {
        if (!tar_write_padding_data(tf, sizeleft))
        {
          return -1;
        }
      }
    }
    else
    {
         
                                                                        
                                                          
         
      tf->currpos = tf->pad_to_size;
    }
  }

     
                                                                          
                        
     
  filesize = tar_get_current_pos(f);
  padding = ((filesize + 511) & ~511) - filesize;
  if (padding)
  {
    char zerobuf[512];

    MemSet(zerobuf, 0, padding);
    if (tar_write(f, zerobuf, padding) != padding)
    {
      return -1;
    }
  }

#ifdef HAVE_LIBZ
  if (tar_data->compression)
  {
                                  
    if (!tar_write_compressed_data(NULL, 0, true))
    {
      return -1;
    }
  }
#endif

     
                                                                     
                                                                             
                                        
     
  print_tar_number(&(tf->header[124]), 12, filesize);

  if (method == CLOSE_NORMAL)
  {

       
                                                                       
                                                     
       
    strlcpy(&(tf->header[0]), tf->pathname, 100);
  }

  print_tar_number(&(tf->header[148]), 8, tarChecksum(((TarMethodFile *)f)->header));
  if (lseek(tar_data->fd, tf->ofs_start, SEEK_SET) != ((TarMethodFile *)f)->ofs_start)
  {
    tar_data->lasterrno = errno;
    return -1;
  }
  if (!tar_data->compression)
  {
    errno = 0;
    if (write(tar_data->fd, tf->header, 512) != 512)
    {
                                                                      
      tar_data->lasterrno = errno ? errno : ENOSPC;
      return -1;
    }
  }
#ifdef HAVE_LIBZ
  else
  {
                              
    if (deflateParams(tar_data->zp, 0, Z_DEFAULT_STRATEGY) != Z_OK)
    {
      tar_set_error("could not change compression parameters");
      return -1;
    }

                                                                  
    if (!tar_write_compressed_data(tar_data->currentfile->header, 512, true))
    {
      return -1;
    }

                                  
    if (deflateParams(tar_data->zp, tar_data->compression, Z_DEFAULT_STRATEGY) != Z_OK)
    {
      tar_set_error("could not change compression parameters");
      return -1;
    }
  }
#endif

                                                                         
  if (lseek(tar_data->fd, 0, SEEK_END) < 0)
  {
    tar_data->lasterrno = errno;
    return -1;
  }

                                                          
  if (tar_sync(f) < 0)
  {
    return -1;
  }

                         
  pg_free(tf->pathname);
  pg_free(tf);
  tar_data->currentfile = NULL;

  return 0;
}

static bool
tar_existsfile(const char *pathname)
{
  tar_clear_error();
                                                                            
  return false;
}

static bool
tar_finish(void)
{
  char zerobuf[1024];

  tar_clear_error();

  if (tar_data->currentfile)
  {
    if (tar_close(tar_data->currentfile, CLOSE_NORMAL) != 0)
    {
      return false;
    }
  }

                                                   
  MemSet(zerobuf, 0, sizeof(zerobuf));
  if (!tar_data->compression)
  {
    errno = 0;
    if (write(tar_data->fd, zerobuf, sizeof(zerobuf)) != sizeof(zerobuf))
    {
                                                                      
      tar_data->lasterrno = errno ? errno : ENOSPC;
      return false;
    }
  }
#ifdef HAVE_LIBZ
  else
  {
    if (!tar_write_compressed_data(zerobuf, sizeof(zerobuf), false))
    {
      return false;
    }

                                                                      
    tar_data->zp->next_in = NULL;
    tar_data->zp->avail_in = 0;
    while (true)
    {
      int r;

      r = deflate(tar_data->zp, Z_FINISH);

      if (r == Z_STREAM_ERROR)
      {
        tar_set_error("could not compress data");
        return false;
      }
      if (tar_data->zp->avail_out < ZLIB_OUT_SIZE)
      {
        size_t len = ZLIB_OUT_SIZE - tar_data->zp->avail_out;

        errno = 0;
        if (write(tar_data->fd, tar_data->zlibOut, len) != len)
        {
             
                                                                  
                    
             
          tar_data->lasterrno = errno ? errno : ENOSPC;
          return false;
        }
      }
      if (r == Z_STREAM_END)
      {
        break;
      }
    }

    if (deflateEnd(tar_data->zp) != Z_OK)
    {
      tar_set_error("could not close compression stream");
      return false;
    }
  }
#endif

                                                                        
  if (tar_data->sync)
  {
    if (fsync(tar_data->fd) != 0)
    {
      tar_data->lasterrno = errno;
      return false;
    }
  }

  if (close(tar_data->fd) != 0)
  {
    tar_data->lasterrno = errno;
    return false;
  }

  tar_data->fd = -1;

  if (tar_data->sync)
  {
    if (fsync_fname(tar_data->tarfilename, false) != 0 || fsync_parent_path(tar_data->tarfilename) != 0)
    {
      tar_data->lasterrno = errno;
      return false;
    }
  }

  return true;
}

WalWriteMethod *
CreateWalTarMethod(const char *tarbase, int compression, bool sync)
{
  WalWriteMethod *method;
  const char *suffix = (compression != 0) ? ".tar.gz" : ".tar";

  method = pg_malloc0(sizeof(WalWriteMethod));
  method->open_for_write = tar_open_for_write;
  method->write = tar_write;
  method->get_current_pos = tar_get_current_pos;
  method->get_file_size = tar_get_file_size;
  method->get_file_name = tar_get_file_name;
  method->compression = tar_compression;
  method->close = tar_close;
  method->sync = tar_sync;
  method->existsfile = tar_existsfile;
  method->finish = tar_finish;
  method->getlasterror = tar_getlasterror;

  tar_data = pg_malloc0(sizeof(TarMethodData));
  tar_data->tarfilename = pg_malloc0(strlen(tarbase) + strlen(suffix) + 1);
  sprintf(tar_data->tarfilename, "%s%s", tarbase, suffix);
  tar_data->fd = -1;
  tar_data->compression = compression;
  tar_data->sync = sync;
#ifdef HAVE_LIBZ
  if (compression)
  {
    tar_data->zlibOut = (char *)pg_malloc(ZLIB_OUT_SIZE + 1);
  }
#endif

  return method;
}

void
FreeWalTarMethod(void)
{
  pg_free(tar_data->tarfilename);
#ifdef HAVE_LIBZ
  if (tar_data->compression)
  {
    pg_free(tar_data->zlibOut);
  }
#endif
  pg_free(tar_data);
  tar_data = NULL;
}
