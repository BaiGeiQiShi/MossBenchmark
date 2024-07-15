                                                                            
   
                 
                                                                       
            
   
                                                                         
                                                                        
   
                                                                           
                                                                          
                                                                          
                                                                        
                                                                         
                                                                          
                                      
   
                  
                  
   
                                                                        
                                                                            
                                                                             
                                                                          
                                                                  
                                                         
   
                                                                       
                                                                             
                                                                          
                                                                            
                                                                           
                  
   
                                                                      
   
                         
                          
   
                                                                            
                                                                     
                                                                          
                                                                        
                                                                         
                                                                           
                                                                           
                                      
   
                  
                                    
   
                                                                            
   
#include "postgres_fe.h"

#include "compress_io.h"
#include "pg_backup_utils.h"

                         
                  
                         
   

                                      
struct CompressorState
{
  CompressionAlgorithm comprAlg;
  WriteFunc writeF;

#ifdef HAVE_LIBZ
  z_streamp zp;
  char *zlibOut;
  size_t zlibOutSize;
#endif
};

static void
ParseCompressionOption(int compression, CompressionAlgorithm *alg, int *level);

                                                    
#ifdef HAVE_LIBZ
static void
InitCompressorZlib(CompressorState *cs, int level);
static void
DeflateCompressorZlib(ArchiveHandle *AH, CompressorState *cs, bool flush);
static void
ReadDataFromArchiveZlib(ArchiveHandle *AH, ReadFunc readF);
static void
WriteDataToArchiveZlib(ArchiveHandle *AH, CompressorState *cs, const char *data, size_t dLen);
static void
EndCompressorZlib(ArchiveHandle *AH, CompressorState *cs);
#endif

                                                 
static void
ReadDataFromArchiveNone(ArchiveHandle *AH, ReadFunc readF);
static void
WriteDataToArchiveNone(ArchiveHandle *AH, CompressorState *cs, const char *data, size_t dLen);

   
                                                                          
                                                                    
                                     
   
static void
ParseCompressionOption(int compression, CompressionAlgorithm *alg, int *level)
{
  if (compression == Z_DEFAULT_COMPRESSION || (compression > 0 && compression <= 9))
  {
    *alg = COMPR_ALG_LIBZ;
  }
  else if (compression == 0)
  {
    *alg = COMPR_ALG_NONE;
  }
  else
  {
    fatal("invalid compression code: %d", compression);
    *alg = COMPR_ALG_NONE;                          
  }

                                              
  if (level)
  {
    *level = compression;
  }
}

                               

                               
CompressorState *
AllocateCompressor(int compression, WriteFunc writeF)
{
  CompressorState *cs;
  CompressionAlgorithm alg;
  int level;

  ParseCompressionOption(compression, &alg, &level);

#ifndef HAVE_LIBZ
  if (alg == COMPR_ALG_LIBZ)
  {
    fatal("not built with zlib support");
  }
#endif

  cs = (CompressorState *)pg_malloc0(sizeof(CompressorState));
  cs->writeF = writeF;
  cs->comprAlg = alg;

     
                                                            
     
#ifdef HAVE_LIBZ
  if (alg == COMPR_ALG_LIBZ)
  {
    InitCompressorZlib(cs, level);
  }
#endif

  return cs;
}

   
                                                                           
                       
   
void
ReadDataFromArchive(ArchiveHandle *AH, int compression, ReadFunc readF)
{
  CompressionAlgorithm alg;

  ParseCompressionOption(compression, &alg, NULL);

  if (alg == COMPR_ALG_NONE)
  {
    ReadDataFromArchiveNone(AH, readF);
  }
  if (alg == COMPR_ALG_LIBZ)
  {
#ifdef HAVE_LIBZ
    ReadDataFromArchiveZlib(AH, readF);
#else
    fatal("not built with zlib support");
#endif
  }
}

   
                                                              
   
void
WriteDataToArchive(ArchiveHandle *AH, CompressorState *cs, const void *data, size_t dLen)
{
  switch (cs->comprAlg)
  {
  case COMPR_ALG_LIBZ:
#ifdef HAVE_LIBZ
    WriteDataToArchiveZlib(AH, cs, data, dLen);
#else
    fatal("not built with zlib support");
#endif
    break;
  case COMPR_ALG_NONE:
    WriteDataToArchiveNone(AH, cs, data, dLen);
    break;
  }
  return;
}

   
                                                                
   
void
EndCompressor(ArchiveHandle *AH, CompressorState *cs)
{
#ifdef HAVE_LIBZ
  if (cs->comprAlg == COMPR_ALG_LIBZ)
  {
    EndCompressorZlib(AH, cs);
  }
#endif
  free(cs);
}

                                                            

#ifdef HAVE_LIBZ
   
                                         
   

static void
InitCompressorZlib(CompressorState *cs, int level)
{
  z_streamp zp;

  zp = cs->zp = (z_streamp)pg_malloc(sizeof(z_stream));
  zp->zalloc = Z_NULL;
  zp->zfree = Z_NULL;
  zp->opaque = Z_NULL;

     
                                                                       
                                                                             
                                            
     
  cs->zlibOut = (char *)pg_malloc(ZLIB_OUT_SIZE + 1);
  cs->zlibOutSize = ZLIB_OUT_SIZE;

  if (deflateInit(zp, level) != Z_OK)
  {
    fatal("could not initialize compression library: %s", zp->msg);
  }

                                                                         
  zp->next_out = (void *)cs->zlibOut;
  zp->avail_out = cs->zlibOutSize;
}

static void
EndCompressorZlib(ArchiveHandle *AH, CompressorState *cs)
{
  z_streamp zp = cs->zp;

  zp->next_in = NULL;
  zp->avail_in = 0;

                                                 
  DeflateCompressorZlib(AH, cs, true);

  if (deflateEnd(zp) != Z_OK)
  {
    fatal("could not close compression stream: %s", zp->msg);
  }

  free(cs->zlibOut);
  free(cs->zp);
}

static void
DeflateCompressorZlib(ArchiveHandle *AH, CompressorState *cs, bool flush)
{
  z_streamp zp = cs->zp;
  char *out = cs->zlibOut;
  int res = Z_OK;

  while (cs->zp->avail_in != 0 || flush)
  {
    res = deflate(zp, flush ? Z_FINISH : Z_NO_FLUSH);
    if (res == Z_STREAM_ERROR)
    {
      fatal("could not compress data: %s", zp->msg);
    }
    if ((flush && (zp->avail_out < cs->zlibOutSize)) || (zp->avail_out == 0) || (zp->avail_in != 0))
    {
         
                                                                       
                                                                         
                       
         
      if (zp->avail_out < cs->zlibOutSize)
      {
           
                                                                      
                                                   
           
        size_t len = cs->zlibOutSize - zp->avail_out;

        cs->writeF(AH, out, len);
      }
      zp->next_out = (void *)out;
      zp->avail_out = cs->zlibOutSize;
    }

    if (res == Z_STREAM_END)
    {
      break;
    }
  }
}

static void
WriteDataToArchiveZlib(ArchiveHandle *AH, CompressorState *cs, const char *data, size_t dLen)
{
  cs->zp->next_in = (void *)unconstify(char *, data);
  cs->zp->avail_in = dLen;
  DeflateCompressorZlib(AH, cs, false);

  return;
}

static void
ReadDataFromArchiveZlib(ArchiveHandle *AH, ReadFunc readF)
{
  z_streamp zp;
  char *out;
  int res = Z_OK;
  size_t cnt;
  char *buf;
  size_t buflen;

  zp = (z_streamp)pg_malloc(sizeof(z_stream));
  zp->zalloc = Z_NULL;
  zp->zfree = Z_NULL;
  zp->opaque = Z_NULL;

  buf = pg_malloc(ZLIB_IN_SIZE);
  buflen = ZLIB_IN_SIZE;

  out = pg_malloc(ZLIB_OUT_SIZE + 1);

  if (inflateInit(zp) != Z_OK)
  {
    fatal("could not initialize compression library: %s", zp->msg);
  }

                                      
  while ((cnt = readF(AH, &buf, &buflen)))
  {
    zp->next_in = (void *)buf;
    zp->avail_in = cnt;

    while (zp->avail_in > 0)
    {
      zp->next_out = (void *)out;
      zp->avail_out = ZLIB_OUT_SIZE;

      res = inflate(zp, 0);
      if (res != Z_OK && res != Z_STREAM_END)
      {
        fatal("could not uncompress data: %s", zp->msg);
      }

      out[ZLIB_OUT_SIZE - zp->avail_out] = '\0';
      ahwrite(out, 1, ZLIB_OUT_SIZE - zp->avail_out, AH);
    }
  }

  zp->next_in = NULL;
  zp->avail_in = 0;
  while (res != Z_STREAM_END)
  {
    zp->next_out = (void *)out;
    zp->avail_out = ZLIB_OUT_SIZE;
    res = inflate(zp, 0);
    if (res != Z_OK && res != Z_STREAM_END)
    {
      fatal("could not uncompress data: %s", zp->msg);
    }

    out[ZLIB_OUT_SIZE - zp->avail_out] = '\0';
    ahwrite(out, 1, ZLIB_OUT_SIZE - zp->avail_out, AH);
  }

  if (inflateEnd(zp) != Z_OK)
  {
    fatal("could not close compression library: %s", zp->msg);
  }

  free(buf);
  free(out);
  free(zp);
}
#endif                

   
                                      
   

static void
ReadDataFromArchiveNone(ArchiveHandle *AH, ReadFunc readF)
{
  size_t cnt;
  char *buf;
  size_t buflen;

  buf = pg_malloc(ZLIB_OUT_SIZE);
  buflen = ZLIB_OUT_SIZE;

  while ((cnt = readF(AH, &buf, &buflen)))
  {
    ahwrite(buf, 1, cnt, AH);
  }

  free(buf);
}

static void
WriteDataToArchiveNone(ArchiveHandle *AH, CompressorState *cs, const char *data, size_t dLen)
{
  cs->writeF(AH, data, dLen);
  return;
}

                         
                         
                         
   

   
                                                                         
                                           
   
struct cfp
{
  FILE *uncompressedfp;
#ifdef HAVE_LIBZ
  gzFile compressedfp;
#endif
};

#ifdef HAVE_LIBZ
static int
hasSuffix(const char *filename, const char *suffix);
#endif

                                                                   
static void
free_keep_errno(void *p)
{
  int save_errno = errno;

  free(p);
  errno = save_errno;
}

   
                                                                          
                          
   
                                                                               
                                                                           
                                            
   
                                                        
   
cfp *
cfopen_read(const char *path, const char *mode)
{
  cfp *fp;

#ifdef HAVE_LIBZ
  if (hasSuffix(path, ".gz"))
  {
    fp = cfopen(path, mode, 1);
  }
  else
#endif
  {
    fp = cfopen(path, mode, 0);
#ifdef HAVE_LIBZ
    if (fp == NULL)
    {
      char *fname;

      fname = psprintf("%s.gz", path);
      fp = cfopen(fname, mode, 1);
      free_keep_errno(fname);
    }
#endif
  }
  return fp;
}

   
                                                                            
                                                                            
                              
   
                                                                         
                                                                        
                                                  
   
                                                        
   
cfp *
cfopen_write(const char *path, const char *mode, int compression)
{
  cfp *fp;

  if (compression == 0)
  {
    fp = cfopen(path, mode, 0);
  }
  else
  {
#ifdef HAVE_LIBZ
    char *fname;

    fname = psprintf("%s.gz", path);
    fp = cfopen(fname, mode, compression);
    free_keep_errno(fname);
#else
    fatal("not built with zlib support");
    fp = NULL;                          
#endif
  }
  return fp;
}

   
                                                                       
                                                               
   
                                                        
   
cfp *
cfopen(const char *path, const char *mode, int compression)
{
  cfp *fp = pg_malloc(sizeof(cfp));

  if (compression != 0)
  {
#ifdef HAVE_LIBZ
    if (compression != Z_DEFAULT_COMPRESSION)
    {
                                                                          
      char mode_compression[32];

      snprintf(mode_compression, sizeof(mode_compression), "%s%d", mode, compression);
      fp->compressedfp = gzopen(path, mode_compression);
    }
    else
    {
                                                            
      fp->compressedfp = gzopen(path, mode);
    }

    fp->uncompressedfp = NULL;
    if (fp->compressedfp == NULL)
    {
      free_keep_errno(fp);
      fp = NULL;
    }
#else
    fatal("not built with zlib support");
#endif
  }
  else
  {
#ifdef HAVE_LIBZ
    fp->compressedfp = NULL;
#endif
    fp->uncompressedfp = fopen(path, mode);
    if (fp->uncompressedfp == NULL)
    {
      free_keep_errno(fp);
      fp = NULL;
    }
  }

  return fp;
}

int
cfread(void *ptr, int size, cfp *fp)
{
  int ret;

  if (size == 0)
  {
    return 0;
  }

#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    ret = gzread(fp->compressedfp, ptr, size);
    if (ret != size && !gzeof(fp->compressedfp))
    {
      int errnum;
      const char *errmsg = gzerror(fp->compressedfp, &errnum);

      fatal("could not read from input file: %s", errnum == Z_ERRNO ? strerror(errno) : errmsg);
    }
  }
  else
#endif
  {
    ret = fread(ptr, 1, size, fp->uncompressedfp);
    if (ret != size && !feof(fp->uncompressedfp))
    {
      READ_ERROR_EXIT(fp->uncompressedfp);
    }
  }
  return ret;
}

int
cfwrite(const void *ptr, int size, cfp *fp)
{
#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    return gzwrite(fp->compressedfp, ptr, size);
  }
  else
#endif
    return fwrite(ptr, 1, size, fp->uncompressedfp);
}

int
cfgetc(cfp *fp)
{
  int ret;

#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    ret = gzgetc(fp->compressedfp);
    if (ret == EOF)
    {
      if (!gzeof(fp->compressedfp))
      {
        fatal("could not read from input file: %s", strerror(errno));
      }
      else
      {
        fatal("could not read from input file: end of file");
      }
    }
  }
  else
#endif
  {
    ret = fgetc(fp->uncompressedfp);
    if (ret == EOF)
    {
      READ_ERROR_EXIT(fp->uncompressedfp);
    }
  }

  return ret;
}

char *
cfgets(cfp *fp, char *buf, int len)
{
#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    return gzgets(fp->compressedfp, buf, len);
  }
  else
#endif
    return fgets(buf, len, fp->uncompressedfp);
}

int
cfclose(cfp *fp)
{
  int result;

  if (fp == NULL)
  {
    errno = EBADF;
    return EOF;
  }
#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    result = gzclose(fp->compressedfp);
    fp->compressedfp = NULL;
  }
  else
#endif
  {
    result = fclose(fp->uncompressedfp);
    fp->uncompressedfp = NULL;
  }
  free_keep_errno(fp);

  return result;
}

int
cfeof(cfp *fp)
{
#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    return gzeof(fp->compressedfp);
  }
  else
#endif
    return feof(fp->uncompressedfp);
}

const char *
get_cfp_error(cfp *fp)
{
#ifdef HAVE_LIBZ
  if (fp->compressedfp)
  {
    int errnum;
    const char *errmsg = gzerror(fp->compressedfp, &errnum);

    if (errnum != Z_ERRNO)
    {
      return errmsg;
    }
  }
#endif
  return strerror(errno);
}

#ifdef HAVE_LIBZ
static int
hasSuffix(const char *filename, const char *suffix)
{
  int filenamelen = strlen(filename);
  int suffixlen = strlen(suffix);

  if (filenamelen < suffixlen)
  {
    return 0;
  }

  return memcmp(&filename[filenamelen - suffixlen], suffix, suffixlen) == 0;
}

#endif
