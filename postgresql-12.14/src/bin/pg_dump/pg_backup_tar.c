                                                                            
   
                   
   
                                                                         
                                                          
   
                                                                          
                                                                   
   
                                                                      
                                                                          
         
   
                                                                         
   
                                     
                                                               
                                   
   
                                                              
                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres_fe.h"

#include "pg_backup_archiver.h"
#include "pg_backup_tar.h"
#include "pg_backup_utils.h"
#include "pgtar.h"
#include "common/file_utils.h"
#include "fe_utils/string_utils.h"

#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>

static void
_ArchiveEntry(ArchiveHandle *AH, TocEntry *te);
static void
_StartData(ArchiveHandle *AH, TocEntry *te);
static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen);
static void
_EndData(ArchiveHandle *AH, TocEntry *te);
static int
_WriteByte(ArchiveHandle *AH, const int i);
static int
_ReadByte(ArchiveHandle *);
static void
_WriteBuf(ArchiveHandle *AH, const void *buf, size_t len);
static void
_ReadBuf(ArchiveHandle *AH, void *buf, size_t len);
static void
_CloseArchive(ArchiveHandle *AH);
static void
_PrintTocData(ArchiveHandle *AH, TocEntry *te);
static void
_WriteExtraToc(ArchiveHandle *AH, TocEntry *te);
static void
_ReadExtraToc(ArchiveHandle *AH, TocEntry *te);
static void
_PrintExtraToc(ArchiveHandle *AH, TocEntry *te);

static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te);
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid);
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid);
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te);

#define K_STD_BUF_SIZE 1024

typedef struct
{
#ifdef HAVE_LIBZ
  gzFile zFH;
#else
  FILE *zFH;
#endif
  FILE *nFH;
  FILE *tarFH;
  FILE *tmpFH;
  char *targetFile;
  char mode;
  pgoff_t pos;
  pgoff_t fileLen;
  ArchiveHandle *AH;
} TAR_MEMBER;

typedef struct
{
  int hasSeek;
  pgoff_t filePos;
  TAR_MEMBER *blobToc;
  FILE *tarFH;
  pgoff_t tarFHpos;
  pgoff_t tarNextMember;
  TAR_MEMBER *FH;
  int isSpecialScript;
  TAR_MEMBER *scriptTH;
} lclContext;

typedef struct
{
  TAR_MEMBER *TH;
  char *filename;
} lclTocEntry;

static void
_LoadBlobs(ArchiveHandle *AH);

static TAR_MEMBER *
tarOpen(ArchiveHandle *AH, const char *filename, char mode);
static void
tarClose(ArchiveHandle *AH, TAR_MEMBER *TH);

#ifdef __NOT_USED__
static char *
tarGets(char *buf, size_t len, TAR_MEMBER *th);
#endif
static int
tarPrintf(ArchiveHandle *AH, TAR_MEMBER *th, const char *fmt, ...) pg_attribute_printf(3, 4);

static void
_tarAddFile(ArchiveHandle *AH, TAR_MEMBER *th);
static TAR_MEMBER *
_tarPositionTo(ArchiveHandle *AH, const char *filename);
static size_t
tarRead(void *buf, size_t len, TAR_MEMBER *th);
static size_t
tarWrite(const void *buf, size_t len, TAR_MEMBER *th);
static void
_tarWriteHeader(TAR_MEMBER *th);
static int
_tarGetHeader(ArchiveHandle *AH, TAR_MEMBER *th);
static size_t
_tarReadRaw(ArchiveHandle *AH, void *buf, size_t len, TAR_MEMBER *th, FILE *fh);

static size_t
_scriptOut(ArchiveHandle *AH, const void *buf, size_t len);

   
               
   
void
InitArchiveFmt_Tar(ArchiveHandle *AH)
{
  lclContext *ctx;

                                                                      
  AH->ArchiveEntryPtr = _ArchiveEntry;
  AH->StartDataPtr = _StartData;
  AH->WriteDataPtr = _WriteData;
  AH->EndDataPtr = _EndData;
  AH->WriteBytePtr = _WriteByte;
  AH->ReadBytePtr = _ReadByte;
  AH->WriteBufPtr = _WriteBuf;
  AH->ReadBufPtr = _ReadBuf;
  AH->ClosePtr = _CloseArchive;
  AH->ReopenPtr = NULL;
  AH->PrintTocDataPtr = _PrintTocData;
  AH->ReadExtraTocPtr = _ReadExtraToc;
  AH->WriteExtraTocPtr = _WriteExtraToc;
  AH->PrintExtraTocPtr = _PrintExtraToc;

  AH->StartBlobsPtr = _StartBlobs;
  AH->StartBlobPtr = _StartBlob;
  AH->EndBlobPtr = _EndBlob;
  AH->EndBlobsPtr = _EndBlobs;
  AH->ClonePtr = NULL;
  AH->DeClonePtr = NULL;

  AH->WorkerJobDumpPtr = NULL;
  AH->WorkerJobRestorePtr = NULL;

     
                                                           
     
  ctx = (lclContext *)pg_malloc0(sizeof(lclContext));
  AH->formatData = (void *)ctx;
  ctx->filePos = 0;
  ctx->isSpecialScript = 0;

                               
  AH->lo_buf_size = LOBBUFSIZE;
  AH->lo_buf = (void *)pg_malloc(LOBBUFSIZE);

     
                                                                    
     
  if (AH->mode == archModeWrite)
  {
    if (AH->fSpec && strcmp(AH->fSpec, "") != 0)
    {
      ctx->tarFH = fopen(AH->fSpec, PG_BINARY_W);
      if (ctx->tarFH == NULL)
      {
        fatal("could not open TOC file \"%s\" for output: %m", AH->fSpec);
      }
    }
    else
    {
      ctx->tarFH = stdout;
      if (ctx->tarFH == NULL)
      {
        fatal("could not open TOC file for output: %m");
      }
    }

    ctx->tarFHpos = 0;

       
                                                                          
             
       
                                               

    ctx->hasSeek = checkSeek(ctx->tarFH);

       
                                                                          
                                                                         
                    
       
    if (AH->compression != 0)
    {
      fatal("compression is not supported by tar archive format");
    }
  }
  else
  {                
    if (AH->fSpec && strcmp(AH->fSpec, "") != 0)
    {
      ctx->tarFH = fopen(AH->fSpec, PG_BINARY_R);
      if (ctx->tarFH == NULL)
      {
        fatal("could not open TOC file \"%s\" for input: %m", AH->fSpec);
      }
    }
    else
    {
      ctx->tarFH = stdin;
      if (ctx->tarFH == NULL)
      {
        fatal("could not open TOC file for input: %m");
      }
    }

       
                                                                          
             
       
                                               

    ctx->tarFHpos = 0;

    ctx->hasSeek = checkSeek(ctx->tarFH);

    ctx->FH = (void *)tarOpen(AH, "toc.dat", 'r');
    ReadHead(AH);
    ReadToc(AH);
    tarClose(AH, ctx->FH);                                  
  }
}

   
                           
                                
   
static void
_ArchiveEntry(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx;
  char fn[K_STD_BUF_SIZE];

  ctx = (lclTocEntry *)pg_malloc0(sizeof(lclTocEntry));
  if (te->dataDumper != NULL)
  {
#ifdef HAVE_LIBZ
    if (AH->compression == 0)
    {
      sprintf(fn, "%d.dat", te->dumpId);
    }
    else
    {
      sprintf(fn, "%d.dat.gz", te->dumpId);
    }
#else
    sprintf(fn, "%d.dat", te->dumpId);
#endif
    ctx->filename = pg_strdup(fn);
  }
  else
  {
    ctx->filename = NULL;
    ctx->TH = NULL;
  }
  te->formatData = (void *)ctx;
}

static void
_WriteExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx = (lclTocEntry *)te->formatData;

  if (ctx->filename)
  {
    WriteStr(AH, ctx->filename);
  }
  else
  {
    WriteStr(AH, "");
  }
}

static void
_ReadExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx = (lclTocEntry *)te->formatData;

  if (ctx == NULL)
  {
    ctx = (lclTocEntry *)pg_malloc0(sizeof(lclTocEntry));
    te->formatData = (void *)ctx;
  }

  ctx->filename = ReadStr(AH);
  if (strlen(ctx->filename) == 0)
  {
    free(ctx->filename);
    ctx->filename = NULL;
  }
  ctx->TH = NULL;
}

static void
_PrintExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx = (lclTocEntry *)te->formatData;

  if (AH->public.verbose && ctx->filename != NULL)
  {
    ahprintf(AH, "-- File: %s\n", ctx->filename);
  }
}

static void
_StartData(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  tctx->TH = tarOpen(AH, tctx->filename, 'w');
}

static TAR_MEMBER *
tarOpen(ArchiveHandle *AH, const char *filename, char mode)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  TAR_MEMBER *tm;

#ifdef HAVE_LIBZ
  char fmode[14];
#endif

  if (mode == 'r')
  {
    tm = _tarPositionTo(AH, filename);
    if (!tm)                
    {
      if (filename)
      {
           
                                                                    
                  
           
        fatal("could not find file \"%s\" in archive", filename);
      }
      else
      {
                                                    
        return NULL;
      }
    }

#ifdef HAVE_LIBZ

    if (AH->compression == 0)
    {
      tm->nFH = ctx->tarFH;
    }
    else
    {
      fatal("compression is not supported by tar archive format");
    }
                                                           
#else
    tm->nFH = ctx->tarFH;
#endif
  }
  else
  {
    int old_umask;

    tm = pg_malloc0(sizeof(TAR_MEMBER));

       
                                                                       
                                                                           
                                                                          
                                                               
       
    old_umask = umask(S_IRWXG | S_IRWXO);

#ifndef WIN32
    tm->tmpFH = tmpfile();
#else

       
                                                                       
                                                                          
                                                       
       
    while (1)
    {
      char *name;
      int fd;

      name = _tempnam(NULL, "pg_temp_");
      if (name == NULL)
      {
        break;
      }
      fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_BINARY | O_TEMPORARY, S_IRUSR | S_IWUSR);
      free(name);

      if (fd != -1)                     
      {
        tm->tmpFH = fdopen(fd, "w+b");
        break;
      }
      else if (errno != EEXIST)                                     
      {
        break;
      }
    }
#endif

    if (tm->tmpFH == NULL)
    {
      fatal("could not generate temporary file name: %m");
    }

    umask(old_umask);

#ifdef HAVE_LIBZ

    if (AH->compression != 0)
    {
      sprintf(fmode, "wb%d", AH->compression);
      tm->zFH = gzdopen(dup(fileno(tm->tmpFH)), fmode);
      if (tm->zFH == NULL)
      {
        fatal("could not open temporary file");
      }
    }
    else
    {
      tm->nFH = tm->tmpFH;
    }
#else

    tm->nFH = tm->tmpFH;
#endif

    tm->AH = AH;
    tm->targetFile = pg_strdup(filename);
  }

  tm->mode = mode;
  tm->tarFH = ctx->tarFH;

  return tm;
}

static void
tarClose(ArchiveHandle *AH, TAR_MEMBER *th)
{
     
                                                                    
     
  if (AH->compression != 0)
  {
    errno = 0;                                       
    if (GZCLOSE(th->zFH) != 0)
    {
      fatal("could not close tar member: %m");
    }
  }

  if (th->mode == 'w')
  {
    _tarAddFile(AH, th);                                    
  }

     
                                                                         
                                          
     

  if (th->targetFile)
  {
    free(th->targetFile);
  }

  th->nFH = NULL;
  th->zFH = NULL;
}

#ifdef __NOT_USED__
static char *
tarGets(char *buf, size_t len, TAR_MEMBER *th)
{
  char *s;
  size_t cnt = 0;
  char c = ' ';
  int eof = 0;

                                   
  if (len > (th->fileLen - th->pos))
  {
    len = th->fileLen - th->pos;
  }

  while (cnt < len && c != '\n')
  {
    if (_tarReadRaw(th->AH, &c, 1, th, NULL) <= 0)
    {
      eof = 1;
      break;
    }
    buf[cnt++] = c;
  }

  if (eof && cnt == 0)
  {
    s = NULL;
  }
  else
  {
    buf[cnt++] = '\0';
    s = buf;
  }

  if (s)
  {
    len = strlen(s);
    th->pos += len;
  }

  return s;
}
#endif

   
                                                                        
                                             
   
static size_t
_tarReadRaw(ArchiveHandle *AH, void *buf, size_t len, TAR_MEMBER *th, FILE *fh)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  size_t avail;
  size_t used = 0;
  size_t res = 0;

  avail = AH->lookaheadLen - AH->lookaheadPos;
  if (avail > 0)
  {
                                             
    if (avail >= len)                                    
    {
      used = len;
    }
    else
    {
      used = avail;
    }

                                     
    memcpy(buf, AH->lookahead + AH->lookaheadPos, used);
    AH->lookaheadPos += used;

                                
    len -= used;
  }

                                
  if (len > 0)
  {
    if (fh)
    {
      res = fread(&((char *)buf)[used], 1, len, fh);
      if (res != len && !feof(fh))
      {
        READ_ERROR_EXIT(fh);
      }
    }
    else if (th)
    {
      if (th->zFH)
      {
        res = GZREAD(&((char *)buf)[used], 1, len, th->zFH);
        if (res != len && !GZEOF(th->zFH))
        {
#ifdef HAVE_LIBZ
          int errnum;
          const char *errmsg = gzerror(th->zFH, &errnum);

          fatal("could not read from input file: %s", errnum == Z_ERRNO ? strerror(errno) : errmsg);
#else
          fatal("could not read from input file: %s", strerror(errno));
#endif
        }
      }
      else
      {
        res = fread(&((char *)buf)[used], 1, len, th->nFH);
        if (res != len && !feof(th->nFH))
        {
          READ_ERROR_EXIT(th->nFH);
        }
      }
    }
    else
    {
      fatal("internal error -- neither th nor fh specified in tarReadRaw()");
    }
  }

  ctx->tarFHpos += res + used;

  return (res + used);
}

static size_t
tarRead(void *buf, size_t len, TAR_MEMBER *th)
{
  size_t res;

  if (th->pos + len > th->fileLen)
  {
    len = th->fileLen - th->pos;
  }

  if (len <= 0)
  {
    return 0;
  }

  res = _tarReadRaw(th->AH, buf, len, th, NULL);

  th->pos += res;

  return res;
}

static size_t
tarWrite(const void *buf, size_t len, TAR_MEMBER *th)
{
  size_t res;

  if (th->zFH != NULL)
  {
    res = GZWRITE(buf, 1, len, th->zFH);
  }
  else
  {
    res = fwrite(buf, 1, len, th->nFH);
  }

  th->pos += res;
  return res;
}

static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen)
{
  lclTocEntry *tctx = (lclTocEntry *)AH->currToc->formatData;

  if (tarWrite(data, dLen, tctx->TH) != dLen)
  {
    WRITE_ERROR_EXIT;
  }

  return;
}

static void
_EndData(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

                      
  tarClose(AH, tctx->TH);
  tctx->TH = NULL;
}

   
                               
   
static void
_PrintFileData(ArchiveHandle *AH, char *filename)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char buf[4096];
  size_t cnt;
  TAR_MEMBER *th;

  if (!filename)
  {
    return;
  }

  th = tarOpen(AH, filename, 'r');
  ctx->FH = th;

  while ((cnt = tarRead(buf, 4095, th)) > 0)
  {
    buf[cnt] = '\0';
    ahwrite(buf, 1, cnt, AH);
  }

  tarClose(AH, th);
}

   
                                    
   
static void
_PrintTocData(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;
  int pos1;

  if (!tctx->filename)
  {
    return;
  }

     
                                                                      
                                                                       
     
                                                                           
                                                
     
  if (ctx->isSpecialScript)
  {
    if (te->copyStmt)
    {
                                     
      ahprintf(AH, "\\.\n");

         
                                                                       
                              
         
      pos1 = (int)strlen(te->copyStmt) - 13;
      if (pos1 < 6 || strncmp(te->copyStmt, "COPY ", 5) != 0 || strcmp(te->copyStmt + pos1, " FROM stdin;\n") != 0)
      {
        fatal("unexpected COPY statement syntax: \"%s\"", te->copyStmt);
      }

                                          
      ahwrite(te->copyStmt, 1, pos1, AH);
                                        
      ahprintf(AH, " FROM '$$PATH$$/%s';\n\n", tctx->filename);
    }
    else
    {
                                                                  
      ahprintf(AH, "\\i $$PATH$$/%s\n\n", tctx->filename);
    }

    return;
  }

  if (strcmp(te->desc, "BLOBS") == 0)
  {
    _LoadBlobs(AH);
  }
  else
  {
    _PrintFileData(AH, tctx->filename);
  }
}

static void
_LoadBlobs(ArchiveHandle *AH)
{
  Oid oid;
  lclContext *ctx = (lclContext *)AH->formatData;
  TAR_MEMBER *th;
  size_t cnt;
  bool foundBlob = false;
  char buf[4096];

  StartRestoreBlobs(AH);

  th = tarOpen(AH, NULL, 'r');                     
  while (th != NULL)
  {
    ctx->FH = th;

    if (strncmp(th->targetFile, "blob_", 5) == 0)
    {
      oid = atooid(&th->targetFile[5]);
      if (oid != 0)
      {
        pg_log_info("restoring large object with OID %u", oid);

        StartRestoreBlob(AH, oid, AH->public.ropt->dropSchema);

        while ((cnt = tarRead(buf, 4095, th)) > 0)
        {
          buf[cnt] = '\0';
          ahwrite(buf, 1, cnt, AH);
        }
        EndRestoreBlob(AH, oid);
        foundBlob = true;
      }
      tarClose(AH, th);
    }
    else
    {
      tarClose(AH, th);

         
                                                                       
                                                                       
                                                                    
                                                           
         
      if (foundBlob)
      {
        break;
      }
    }

    th = tarOpen(AH, NULL, 'r');
  }
  EndRestoreBlobs(AH);
}

static int
_WriteByte(ArchiveHandle *AH, const int i)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char b = i;                            

  if (tarWrite(&b, 1, ctx->FH) != 1)
  {
    WRITE_ERROR_EXIT;
  }

  ctx->filePos += 1;
  return 1;
}

static int
_ReadByte(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  size_t res;
  unsigned char c;

  res = tarRead(&c, 1, ctx->FH);
  if (res != 1)
  {
                                                                       
    fatal("could not read from input file: end of file");
  }
  ctx->filePos += 1;
  return c;
}

static void
_WriteBuf(ArchiveHandle *AH, const void *buf, size_t len)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  if (tarWrite(buf, len, ctx->FH) != len)
  {
    WRITE_ERROR_EXIT;
  }

  ctx->filePos += len;
}

static void
_ReadBuf(ArchiveHandle *AH, void *buf, size_t len)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  if (tarRead(buf, len, ctx->FH) != len)
  {
                                                                       
    fatal("could not read from input file: end of file");
  }

  ctx->filePos += len;
  return;
}

static void
_CloseArchive(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  TAR_MEMBER *th;
  RestoreOptions *ropt;
  RestoreOptions *savRopt;
  DumpOptions *savDopt;
  int savVerbose, i;

  if (AH->mode == archModeWrite)
  {
       
                                                   
       
    th = tarOpen(AH, "toc.dat", 'w');
    ctx->FH = th;
    WriteHead(AH);
    WriteToc(AH);
    tarClose(AH, th);                          

       
                                          
       
    WriteDataChunks(AH, NULL);

       
                                                                          
                                         
       
    th = tarOpen(AH, "restore.sql", 'w');

    tarPrintf(AH, th,
        "--\n"
        "-- NOTE:\n"
        "--\n"
        "-- File paths need to be edited. Search for $$PATH$$ and\n"
        "-- replace it with the path to the directory containing\n"
        "-- the extracted data files.\n"
        "--\n");

    AH->CustomOutPtr = _scriptOut;

    ctx->isSpecialScript = 1;
    ctx->scriptTH = th;

    ropt = NewRestoreOptions();
    memcpy(ropt, AH->public.ropt, sizeof(RestoreOptions));
    ropt->filename = NULL;
    ropt->dropSchema = 1;
    ropt->compression = 0;
    ropt->superuser = NULL;
    ropt->suppressDumpWarnings = true;

    savDopt = AH->public.dopt;
    savRopt = AH->public.ropt;

    SetArchiveOptions((Archive *)AH, NULL, ropt);

    savVerbose = AH->public.verbose;
    AH->public.verbose = 0;

    RestoreArchive((Archive *)AH);

    SetArchiveOptions((Archive *)AH, savDopt, savRopt);

    AH->public.verbose = savVerbose;

    tarClose(AH, th);

    ctx->isSpecialScript = 0;

       
                                                        
       
    for (i = 0; i < 512 * 2; i++)
    {
      if (fputc(0, ctx->tarFH) == EOF)
      {
        WRITE_ERROR_EXIT;
      }
    }

                                                
    if (AH->dosync && AH->fSpec)
    {
      (void)fsync_fname(AH->fSpec, false);
    }
  }

  AH->FH = NULL;
}

static size_t
_scriptOut(ArchiveHandle *AH, const void *buf, size_t len)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  return tarWrite(buf, len, ctx->scriptTH);
}

   
                
   

   
                                                                            
                                                                           
                                       
   
                                                               
   
                                       
   
   
static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char fname[K_STD_BUF_SIZE];

  sprintf(fname, "blobs.toc");
  ctx->blobToc = tarOpen(AH, fname, 'w');
}

   
                                                           
   
              
   
                                                           
   
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;
  char fname[255];
  char *sfx;

  if (oid == 0)
  {
    fatal("invalid OID for large object (%u)", oid);
  }

  if (AH->compression != 0)
  {
    sfx = ".gz";
  }
  else
  {
    sfx = "";
  }

  sprintf(fname, "blob_%u.dat%s", oid, sfx);

  tarPrintf(AH, ctx->blobToc, "%u %s\n", oid, fname);

  tctx->TH = tarOpen(AH, fname, 'w');
}

   
                                                         
   
             
   
   
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  tarClose(AH, tctx->TH);
}

   
                                                               
   
             
   
   
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;

                                                       
                        

  tarClose(AH, ctx->blobToc);
}

               
               
               
   

static int
tarPrintf(ArchiveHandle *AH, TAR_MEMBER *th, const char *fmt, ...)
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

  cnt = tarWrite(p, cnt, th);
  free(p);
  return (int)cnt;
}

bool
isValidTarHeader(char *header)
{
  int sum;
  int chk = tarChecksum(header);

  sum = read_tar_number(&header[148], 8);

  if (sum != chk)
  {
    return false;
  }

                        
  if (memcmp(&header[257], "ustar\0", 6) == 0 && memcmp(&header[263], "00", 2) == 0)
  {
    return true;
  }
                      
  if (memcmp(&header[257], "ustar  \0", 8) == 0)
  {
    return true;
  }
                                                         
  if (memcmp(&header[257], "ustar00\0", 8) == 0)
  {
    return true;
  }

  return false;
}

                                                            
static void
_tarAddFile(ArchiveHandle *AH, TAR_MEMBER *th)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  FILE *tmp = th->tmpFH;                              
  char buf[32768];
  size_t cnt;
  pgoff_t len = 0;
  size_t res;
  size_t i, pad;

     
                                       
     
  if (fseeko(tmp, 0, SEEK_END) != 0)
  {
    fatal("error during file seek: %m");
  }
  th->fileLen = ftello(tmp);
  if (th->fileLen < 0)
  {
    fatal("could not determine seek position in archive file: %m");
  }
  if (fseeko(tmp, 0, SEEK_SET) != 0)
  {
    fatal("error during file seek: %m");
  }

  _tarWriteHeader(th);

  while ((cnt = fread(buf, 1, sizeof(buf), tmp)) > 0)
  {
    if ((res = fwrite(buf, 1, cnt, th->tarFH)) != cnt)
    {
      WRITE_ERROR_EXIT;
    }
    len += res;
  }
  if (!feof(tmp))
  {
    READ_ERROR_EXIT(tmp);
  }

  if (fclose(tmp) != 0)                                 
  {
    fatal("could not close temporary file: %m");
  }

  if (len != th->fileLen)
  {
    char buf1[32], buf2[32];

    snprintf(buf1, sizeof(buf1), INT64_FORMAT, (int64)len);
    snprintf(buf2, sizeof(buf2), INT64_FORMAT, (int64)th->fileLen);
    fatal("actual file length (%s) does not match expected (%s)", buf1, buf2);
  }

  pad = ((len + 511) & ~511) - len;
  for (i = 0; i < pad; i++)
  {
    if (fputc('\0', th->tarFH) == EOF)
    {
      WRITE_ERROR_EXIT;
    }
  }

  ctx->tarFHpos += len + pad;
}

                                                                      
static TAR_MEMBER *
_tarPositionTo(ArchiveHandle *AH, const char *filename)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  TAR_MEMBER *th = pg_malloc0(sizeof(TAR_MEMBER));
  char c;
  char header[512];
  size_t i, len, blks;
  int id;

  th->AH = AH;

                                         
  if (ctx->tarFHpos != 0)
  {
    char buf1[100], buf2[100];

    snprintf(buf1, sizeof(buf1), INT64_FORMAT, (int64)ctx->tarFHpos);
    snprintf(buf2, sizeof(buf2), INT64_FORMAT, (int64)ctx->tarNextMember);
    pg_log_debug("moving from position %s to next member at file position %s", buf1, buf2);

    while (ctx->tarFHpos < ctx->tarNextMember)
    {
      _tarReadRaw(AH, &c, 1, NULL, ctx->tarFH);
    }
  }

  {
    char buf[100];

    snprintf(buf, sizeof(buf), INT64_FORMAT, (int64)ctx->tarFHpos);
    pg_log_debug("now at file position %s", buf);
  }

                                                              

                      
  if (!_tarGetHeader(AH, th))
  {
    if (filename)
    {
      fatal("could not find header for file \"%s\" in tar archive", filename);
    }
    else
    {
         
                                                                      
              
         
      free(th);
      return NULL;
    }
  }

  while (filename != NULL && strcmp(th->targetFile, filename) != 0)
  {
    pg_log_debug("skipping tar member %s", th->targetFile);

    id = atoi(th->targetFile);
    if ((TocIDRequired(AH, id) & REQ_DATA) != 0)
    {
      fatal("restoring data out of order is not supported in this archive format: "
            "\"%s\" is required, but comes before \"%s\" in the archive file.",
          th->targetFile, filename);
    }

                                                      
    len = ((th->fileLen + 511) & ~511);                    
    blks = len >> 9;                                              

    for (i = 0; i < blks; i++)
    {
      _tarReadRaw(AH, &header[0], 512, NULL, ctx->tarFH);
    }

    if (!_tarGetHeader(AH, th))
    {
      fatal("could not find header for file \"%s\" in tar archive", filename);
    }
  }

  ctx->tarNextMember = ctx->tarFHpos + ((th->fileLen + 511) & ~511);
  th->pos = 0;

  return th;
}

                            
static int
_tarGetHeader(ArchiveHandle *AH, TAR_MEMBER *th)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char h[512];
  char tag[100 + 1];
  int sum, chk;
  pgoff_t len;
  pgoff_t hPos;
  bool gotBlock = false;

  while (!gotBlock)
  {
                                             
    hPos = ctx->tarFHpos;

                                                          
    len = _tarReadRaw(AH, h, 512, NULL, ctx->tarFH);
    if (len == 0)          
    {
      return 0;
    }

    if (len != 512)
    {
      fatal(ngettext("incomplete tar header found (%lu byte)", "incomplete tar header found (%lu bytes)", len), (unsigned long)len);
    }

                       
    chk = tarChecksum(h);
    sum = read_tar_number(&h[148], 8);

       
                                                                          
                                   
       
    if (chk == sum)
    {
      gotBlock = true;
    }
    else
    {
      int i;

      for (i = 0; i < 512; i++)
      {
        if (h[i] != 0)
        {
          gotBlock = true;
          break;
        }
      }
    }
  }

                                                             
  strlcpy(tag, &h[0], 100 + 1);

  len = read_tar_number(&h[124], 12);

  {
    char posbuf[32];
    char lenbuf[32];

    snprintf(posbuf, sizeof(posbuf), UINT64_FORMAT, (uint64)hPos);
    snprintf(lenbuf, sizeof(lenbuf), UINT64_FORMAT, (uint64)len);
    pg_log_debug("TOC Entry %s at %s (length %s, checksum %d)", tag, posbuf, lenbuf, sum);
  }

  if (chk != sum)
  {
    char posbuf[32];

    snprintf(posbuf, sizeof(posbuf), UINT64_FORMAT, (uint64)ftello(ctx->tarFH));
    fatal("corrupt tar header found in %s (expected %d, computed %d) file position %s", tag, sum, chk, posbuf);
  }

  th->targetFile = pg_strdup(tag);
  th->fileLen = len;

  return 1;
}

static void
_tarWriteHeader(TAR_MEMBER *th)
{
  char h[512];

  tarCreateHeader(h, th->targetFile, NULL, th->fileLen, 0600, 04000, 02000, time(NULL));

                                       
  if (fwrite(h, 1, 512, th->tarFH) != 512)
  {
    WRITE_ERROR_EXIT;
  }
}
