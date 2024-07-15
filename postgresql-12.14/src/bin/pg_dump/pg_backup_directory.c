                                                                            
   
                         
   
                                                                           
                                                                            
                                                                              
                                                                     
                                                                          
                                                                   
                                                                             
                                                              
   
                                                                          
                                                                             
                                                                              
         
   
   
                                                                         
                                                                        
                                              
   
                                                              
                                  
   
                                                              
                        
   
                  
                                          
   
                                                                            
   
#include "postgres_fe.h"

#include "compress_io.h"
#include "parallel.h"
#include "pg_backup_utils.h"
#include "common/file_utils.h"

#include <dirent.h>
#include <sys/stat.h>

typedef struct
{
     
                                                                            
                                                       
     
  char *directory;

  cfp *dataFH;                               

  cfp *blobsTocFH;                                      
  ParallelState *pstate;                                    
} lclContext;

typedef struct
{
  char *filename;                                                  
} lclTocEntry;

                                      
static void
_ArchiveEntry(ArchiveHandle *AH, TocEntry *te);
static void
_StartData(ArchiveHandle *AH, TocEntry *te);
static void
_EndData(ArchiveHandle *AH, TocEntry *te);
static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen);
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
_ReopenArchive(ArchiveHandle *AH);
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
static void
_LoadBlobs(ArchiveHandle *AH);

static void
_PrepParallelRestore(ArchiveHandle *AH);
static void
_Clone(ArchiveHandle *AH);
static void
_DeClone(ArchiveHandle *AH);

static int
_WorkerJobRestoreDirectory(ArchiveHandle *AH, TocEntry *te);
static int
_WorkerJobDumpDirectory(ArchiveHandle *AH, TocEntry *te);

static void
setFilePath(ArchiveHandle *AH, char *buf, const char *relativeFilename);

   
                                                                  
                                                  
   
                                                                           
                                                      
   
                                                                            
                                                                               
   
void
InitArchiveFmt_Directory(ArchiveHandle *AH)
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
  AH->ReopenPtr = _ReopenArchive;
  AH->PrintTocDataPtr = _PrintTocData;
  AH->ReadExtraTocPtr = _ReadExtraToc;
  AH->WriteExtraTocPtr = _WriteExtraToc;
  AH->PrintExtraTocPtr = _PrintExtraToc;

  AH->StartBlobsPtr = _StartBlobs;
  AH->StartBlobPtr = _StartBlob;
  AH->EndBlobPtr = _EndBlob;
  AH->EndBlobsPtr = _EndBlobs;

  AH->PrepParallelRestorePtr = _PrepParallelRestore;
  AH->ClonePtr = _Clone;
  AH->DeClonePtr = _DeClone;

  AH->WorkerJobRestorePtr = _WorkerJobRestoreDirectory;
  AH->WorkerJobDumpPtr = _WorkerJobDumpDirectory;

                                  
  ctx = (lclContext *)pg_malloc0(sizeof(lclContext));
  AH->formatData = (void *)ctx;

  ctx->dataFH = NULL;
  ctx->blobsTocFH = NULL;

                               
  AH->lo_buf_size = LOBBUFSIZE;
  AH->lo_buf = (void *)pg_malloc(LOBBUFSIZE);

     
                           
     

  if (!AH->fSpec || strcmp(AH->fSpec, "") == 0)
  {
    fatal("no output directory specified");
  }

  ctx->directory = AH->fSpec;

  if (AH->mode == archModeWrite)
  {
    struct stat st;
    bool is_empty = false;

                                               
    if (stat(ctx->directory, &st) == 0 && S_ISDIR(st.st_mode))
    {
      DIR *dir = opendir(ctx->directory);

      if (dir)
      {
        struct dirent *d;

        is_empty = true;
        while (errno = 0, (d = readdir(dir)))
        {
          if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0)
          {
            is_empty = false;
            break;
          }
        }

        if (errno)
        {
          fatal("could not read directory \"%s\": %m", ctx->directory);
        }

        if (closedir(dir))
        {
          fatal("could not close directory \"%s\": %m", ctx->directory);
        }
      }
    }

    if (!is_empty && mkdir(ctx->directory, 0700) < 0)
    {
      fatal("could not create directory \"%s\": %m", ctx->directory);
    }
  }
  else
  {                
    char fname[MAXPGPATH];
    cfp *tocFH;

    setFilePath(AH, fname, "toc.dat");

    tocFH = cfopen_read(fname, PG_BINARY_R);
    if (tocFH == NULL)
    {
      fatal("could not open input file \"%s\": %m", fname);
    }

    ctx->dataFH = tocFH;

       
                                                                        
                   
       
    AH->format = archTar;
    ReadHead(AH);
    AH->format = archDirectory;
    ReadToc(AH);

                                                        
    if (cfclose(tocFH) != 0)
    {
      fatal("could not close TOC file: %m");
    }
    ctx->dataFH = NULL;
  }
}

   
                                                                   
   
                                             
   
static void
_ArchiveEntry(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx;
  char fn[MAXPGPATH];

  tctx = (lclTocEntry *)pg_malloc0(sizeof(lclTocEntry));
  if (strcmp(te->desc, "BLOBS") == 0)
  {
    tctx->filename = pg_strdup("blobs.toc");
  }
  else if (te->dataDumper)
  {
    snprintf(fn, MAXPGPATH, "%d.dat", te->dumpId);
    tctx->filename = pg_strdup(fn);
  }
  else
  {
    tctx->filename = NULL;
  }

  te->formatData = (void *)tctx;
}

   
                                                                     
         
   
                                                                      
                                              
   
static void
_WriteExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

     
                                                                         
                          
     
  if (tctx->filename)
  {
    WriteStr(AH, tctx->filename);
  }
  else
  {
    WriteStr(AH, "");
  }
}

   
                                                                     
   
                                                                       
                                    
   
static void
_ReadExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  if (tctx == NULL)
  {
    tctx = (lclTocEntry *)pg_malloc0(sizeof(lclTocEntry));
    te->formatData = (void *)tctx;
  }

  tctx->filename = ReadStr(AH);
  if (strlen(tctx->filename) == 0)
  {
    free(tctx->filename);
    tctx->filename = NULL;
  }
}

   
                                                                        
                                                         
   
static void
_PrintExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  if (AH->public.verbose && tctx->filename)
  {
    ahprintf(AH, "-- File: %s\n", tctx->filename);
  }
}

   
                                                                            
                                                                      
                     
   
                                                                              
   
                                        
   
static void
_StartData(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;
  lclContext *ctx = (lclContext *)AH->formatData;
  char fname[MAXPGPATH];

  setFilePath(AH, fname, tctx->filename);

  ctx->dataFH = cfopen_write(fname, PG_BINARY_W, AH->compression);
  if (ctx->dataFH == NULL)
  {
    fatal("could not open output file \"%s\": %m", fname);
  }
}

   
                                                                   
                                                                    
                                                                     
   
                                                              
   
                                            
   
static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  errno = 0;
  if (dLen > 0 && cfwrite(data, dLen, ctx->dataFH) != dLen)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    fatal("could not write to output file: %s", get_cfp_error(ctx->dataFH));
  }

  return;
}

   
                                                                   
             
   
                           
   
static void
_EndData(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;

                      
  if (cfclose(ctx->dataFH) != 0)
  {
    fatal("could not close data file: %m");
  }

  ctx->dataFH = NULL;
}

   
                                                       
   
static void
_PrintFileData(ArchiveHandle *AH, char *filename)
{
  size_t cnt;
  char *buf;
  size_t buflen;
  cfp *cfp;

  if (!filename)
  {
    return;
  }

  cfp = cfopen_read(filename, PG_BINARY_R);

  if (!cfp)
  {
    fatal("could not open input file \"%s\": %m", filename);
  }

  buf = pg_malloc(ZLIB_OUT_SIZE);
  buflen = ZLIB_OUT_SIZE;

  while ((cnt = cfread(buf, buflen, cfp)))
  {
    ahwrite(buf, 1, cnt, AH);
  }

  free(buf);
  if (cfclose(cfp) != 0)
  {
    fatal("could not close data file \"%s\": %m", filename);
  }
}

   
                                    
   
static void
_PrintTocData(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  if (!tctx->filename)
  {
    return;
  }

  if (strcmp(te->desc, "BLOBS") == 0)
  {
    _LoadBlobs(AH);
  }
  else
  {
    char fname[MAXPGPATH];

    setFilePath(AH, fname, tctx->filename);
    _PrintFileData(AH, fname);
  }
}

static void
_LoadBlobs(ArchiveHandle *AH)
{
  Oid oid;
  lclContext *ctx = (lclContext *)AH->formatData;
  char tocfname[MAXPGPATH];
  char line[MAXPGPATH];

  StartRestoreBlobs(AH);

  setFilePath(AH, tocfname, "blobs.toc");

  ctx->blobsTocFH = cfopen_read(tocfname, PG_BINARY_R);

  if (ctx->blobsTocFH == NULL)
  {
    fatal("could not open large object TOC file \"%s\" for input: %m", tocfname);
  }

                                                                   
  while ((cfgets(ctx->blobsTocFH, line, MAXPGPATH)) != NULL)
  {
    char blobfname[MAXPGPATH + 1];
    char path[MAXPGPATH];

                                                                       
    if (sscanf(line, "%u %" CppAsString2(MAXPGPATH) "s\n", &oid, blobfname) != 2)
    {
      fatal("invalid line in large object TOC file \"%s\": \"%s\"", tocfname, line);
    }

    StartRestoreBlob(AH, oid, AH->public.ropt->dropSchema);
    snprintf(path, MAXPGPATH, "%s/%s", ctx->directory, blobfname);
    _PrintFileData(AH, path);
    EndRestoreBlob(AH, oid);
  }
  if (!cfeof(ctx->blobsTocFH))
  {
    fatal("error reading large object TOC file \"%s\"", tocfname);
  }

  if (cfclose(ctx->blobsTocFH) != 0)
  {
    fatal("could not close large object TOC file \"%s\": %m", tocfname);
  }

  ctx->blobsTocFH = NULL;

  EndRestoreBlobs(AH);
}

   
                                        
                                                                      
                                                                   
   
static int
_WriteByte(ArchiveHandle *AH, const int i)
{
  unsigned char c = (unsigned char)i;
  lclContext *ctx = (lclContext *)AH->formatData;

  errno = 0;
  if (cfwrite(&c, 1, ctx->dataFH) != 1)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    fatal("could not write to output file: %s", get_cfp_error(ctx->dataFH));
  }

  return 1;
}

   
                                         
                                                                     
                                                               
                                           
   
static int
_ReadByte(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  return cfgetc(ctx->dataFH);
}

   
                                          
                                                                               
   
static void
_WriteBuf(ArchiveHandle *AH, const void *buf, size_t len)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  errno = 0;
  if (cfwrite(buf, len, ctx->dataFH) != len)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    fatal("could not write to output file: %s", get_cfp_error(ctx->dataFH));
  }

  return;
}

   
                                           
   
                                                                    
   
static void
_ReadBuf(ArchiveHandle *AH, void *buf, size_t len)
{
  lclContext *ctx = (lclContext *)AH->formatData;

     
                                                                          
                          
     
  if (cfread(buf, len, ctx->dataFH) != len)
  {
    fatal("could not read from input file: end of file");
  }

  return;
}

   
                      
   
                                                                      
                                                                      
                                                                       
   
                                                           
                                           
                                       
                                               
   
static void
_CloseArchive(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  if (AH->mode == archModeWrite)
  {
    cfp *tocFH;
    char fname[MAXPGPATH];

    setFilePath(AH, fname, "toc.dat");

                                                                     
    ctx->pstate = ParallelBackupStart(AH);

                                                
    tocFH = cfopen_write(fname, PG_BINARY_W, 0);
    if (tocFH == NULL)
    {
      fatal("could not open output file \"%s\": %m", fname);
    }
    ctx->dataFH = tocFH;

       
                                                                          
                                                                        
                           
       
    AH->format = archTar;
    WriteHead(AH);
    AH->format = archDirectory;
    WriteToc(AH);
    if (cfclose(tocFH) != 0)
    {
      fatal("could not close TOC file: %m");
    }
    WriteDataChunks(AH, ctx->pstate);

    ParallelBackupEnd(AH, ctx->pstate);

       
                                                                   
                                                                        
       
    if (AH->dosync)
    {
      fsync_dir_recurse(ctx->directory);
    }
  }
  AH->FH = NULL;
}

   
                                     
   
static void
_ReopenArchive(ArchiveHandle *AH)
{
     
                                                                             
                                                                       
              
     
}

   
                
   

   
                                                                            
                                                               
   
                                                                           
                     
   
static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char fname[MAXPGPATH];

  setFilePath(AH, fname, "blobs.toc");

                                             
  ctx->blobsTocFH = cfopen_write(fname, "ab", 0);
  if (ctx->blobsTocFH == NULL)
  {
    fatal("could not open output file \"%s\": %m", fname);
  }
}

   
                                                                    
   
                                          
   
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char fname[MAXPGPATH];

  snprintf(fname, MAXPGPATH, "%s/blob_%u.dat", ctx->directory, oid);

  ctx->dataFH = cfopen_write(fname, PG_BINARY_W, AH->compression);

  if (ctx->dataFH == NULL)
  {
    fatal("could not open output file \"%s\": %m", fname);
  }
}

   
                                                                      
   
                                                                          
   
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char buf[50];
  int len;

                                       
  if (cfclose(ctx->dataFH) != 0)
  {
    fatal("could not close blob data file: %m");
  }
  ctx->dataFH = NULL;

                                      
  len = snprintf(buf, sizeof(buf), "%u blob_%u.dat\n", oid, oid);
  if (cfwrite(buf, len, ctx->blobsTocFH) != len)
  {
    fatal("could not write to blobs TOC file");
  }
}

   
                                                               
   
                                
   
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  if (cfclose(ctx->blobsTocFH) != 0)
  {
    fatal("could not close blobs TOC file: %m");
  }
  ctx->blobsTocFH = NULL;
}

   
                                                                            
                                                                            
                                                                              
                             
   
static void
setFilePath(ArchiveHandle *AH, char *buf, const char *relativeFilename)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  char *dname;

  dname = ctx->directory;

  if (strlen(dname) + 1 + strlen(relativeFilename) + 1 > MAXPGPATH)
  {
    fatal("file name too long: \"%s\"", dname);
  }

  strcpy(buf, dname);
  strcat(buf, "/");
  strcat(buf, relativeFilename);
}

   
                                 
   
                                                                               
                                                                       
                                                                           
                                          
   
                                                             
   
static void
_PrepParallelRestore(ArchiveHandle *AH)
{
  TocEntry *te;

  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    lclTocEntry *tctx = (lclTocEntry *)te->formatData;
    char fname[MAXPGPATH];
    struct stat st;

       
                                                                           
                            
       
    if (tctx->filename == NULL)
    {
      continue;
    }

                                                    
    if ((te->reqs & REQ_DATA) == 0)
    {
      continue;
    }

       
                                                                           
                                                                          
                                                                          
                                                   
       
    setFilePath(AH, fname, tctx->filename);

    if (stat(fname, &st) == 0)
    {
      te->dataLength = st.st_size;
    }
    else
    {
                                  
      strlcat(fname, ".gz", sizeof(fname));
      if (stat(fname, &st) == 0)
      {
        te->dataLength = st.st_size;
      }
    }

       
                                                                       
                                                                         
                                                                         
                                                                     
                                                                  
       
    if (strcmp(te->desc, "BLOBS") == 0)
    {
      te->dataLength *= 1024;
    }
  }
}

   
                                                             
   
static void
_Clone(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  AH->formatData = (lclContext *)pg_malloc(sizeof(lclContext));
  memcpy(AH->formatData, ctx, sizeof(lclContext));
  ctx = (lclContext *)AH->formatData;

     
                                                                             
                                                                  
                                                                       
                                       
     

     
                                                                            
                                
     
}

static void
_DeClone(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  free(ctx);
}

   
                                                                     
                                                                         
   
static int
_WorkerJobDumpDirectory(ArchiveHandle *AH, TocEntry *te)
{
     
                                                                    
                                                                             
                   
     
  WriteDataChunksForTocEntry(AH, te);

  return 0;
}

   
                                                                       
                                                                            
   
static int
_WorkerJobRestoreDirectory(ArchiveHandle *AH, TocEntry *te)
{
  return parallel_restore(AH, te);
}
