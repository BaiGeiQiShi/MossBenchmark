                                                                            
   
                      
   
                                        
   
                                                                   
                                         
   
                                                   
   
                                     
                                                               
                                   
   
                                                              
                                                                  
                
   
   
                  
                                       
   
                                                                            
   
#include "postgres_fe.h"

#include "compress_io.h"
#include "parallel.h"
#include "pg_backup_utils.h"
#include "common/file_utils.h"

           
                                    
           
   

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
_PrintData(ArchiveHandle *AH);
static void
_skipData(ArchiveHandle *AH);
static void
_skipBlobs(ArchiveHandle *AH);

static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te);
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid);
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid);
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te);
static void
_LoadBlobs(ArchiveHandle *AH, bool drop);

static void
_PrepParallelRestore(ArchiveHandle *AH);
static void
_Clone(ArchiveHandle *AH);
static void
_DeClone(ArchiveHandle *AH);

static int
_WorkerJobRestoreCustom(ArchiveHandle *AH, TocEntry *te);

typedef struct
{
  CompressorState *cs;
  int hasSeek;
                                                                             
  pgoff_t lastFilePos;                                                
} lclContext;

typedef struct
{
  int dataState;
  pgoff_t dataPos;                                               
} lclTocEntry;

         
                       
         
   
static void
_readBlockHeader(ArchiveHandle *AH, int *type, int *id);
static pgoff_t
_getFilePos(ArchiveHandle *AH, lclContext *ctx);

static void
_CustomWriteFunc(ArchiveHandle *AH, const char *buf, size_t len);
static size_t
_CustomReadFunc(ArchiveHandle *AH, char **buf, size_t *buflen);

   
                                                                  
                                                  
   
                                                                            
                                                      
   
                                                                             
                                                                               
   
void
InitArchiveFmt_Custom(ArchiveHandle *AH)
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

                                                                     
  AH->WorkerJobDumpPtr = NULL;
  AH->WorkerJobRestorePtr = _WorkerJobRestoreCustom;

                              
  ctx = (lclContext *)pg_malloc0(sizeof(lclContext));
  AH->formatData = (void *)ctx;

                               
  AH->lo_buf_size = LOBBUFSIZE;
  AH->lo_buf = (void *)pg_malloc(LOBBUFSIZE);

     
                       
     
  if (AH->mode == archModeWrite)
  {
    if (AH->fSpec && strcmp(AH->fSpec, "") != 0)
    {
      AH->FH = fopen(AH->fSpec, PG_BINARY_W);
      if (!AH->FH)
      {
        fatal("could not open output file \"%s\": %m", AH->fSpec);
      }
    }
    else
    {
      AH->FH = stdout;
      if (!AH->FH)
      {
        fatal("could not open output file: %m");
      }
    }

    ctx->hasSeek = checkSeek(AH->FH);
  }
  else
  {
    if (AH->fSpec && strcmp(AH->fSpec, "") != 0)
    {
      AH->FH = fopen(AH->fSpec, PG_BINARY_R);
      if (!AH->FH)
      {
        fatal("could not open input file \"%s\": %m", AH->fSpec);
      }
    }
    else
    {
      AH->FH = stdin;
      if (!AH->FH)
      {
        fatal("could not open input file: %m");
      }
    }

    ctx->hasSeek = checkSeek(AH->FH);

    ReadHead(AH);
    ReadToc(AH);

       
                                                                         
                                                          
       
    ctx->lastFilePos = _getFilePos(AH, ctx);
  }
}

   
                                                                   
   
             
   
                                           
   
static void
_ArchiveEntry(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx;

  ctx = (lclTocEntry *)pg_malloc0(sizeof(lclTocEntry));
  if (te->dataDumper)
  {
    ctx->dataState = K_OFFSET_POS_NOT_SET;
  }
  else
  {
    ctx->dataState = K_OFFSET_NO_DATA;
  }

  te->formatData = (void *)ctx;
}

   
                                                                     
         
   
             
   
                                                                      
                                              
   
static void
_WriteExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx = (lclTocEntry *)te->formatData;

  WriteOffset(AH, ctx->dataPos, ctx->dataState);
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

  ctx->dataState = ReadOffset(AH, &(ctx->dataPos));

     
                                                                           
                     
     
  if (AH->version < K_VERS_1_7)
  {
    ReadInt(AH);
  }
}

   
                                                                        
                                                         
   
             
   
   
static void
_PrintExtraToc(ArchiveHandle *AH, TocEntry *te)
{
  lclTocEntry *ctx = (lclTocEntry *)te->formatData;

  if (AH->public.verbose)
  {
    ahprintf(AH, "-- Data Pos: " INT64_FORMAT "\n", (int64)ctx->dataPos);
  }
}

   
                                                                            
                                                                      
                     
   
                                                                              
   
                                       
   
   
static void
_StartData(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  tctx->dataPos = _getFilePos(AH, ctx);
  if (tctx->dataPos >= 0)
  {
    tctx->dataState = K_OFFSET_POS_SET;
  }

  _WriteByte(AH, BLK_DATA);                 
  WriteInt(AH, te->dumpId);                       

  ctx->cs = AllocateCompressor(AH->compression, _CustomWriteFunc);
}

   
                                                                   
                                                                    
                                                                     
   
                                                              
   
              
   
static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  CompressorState *cs = ctx->cs;

  if (dLen > 0)
  {
                                                             
    WriteDataToArchive(AH, cs, data, dLen);
  }

  return;
}

   
                                                                   
             
   
             
   
   
static void
_EndData(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  EndCompressor(AH, ctx->cs);
                           
  WriteInt(AH, 0);
}

   
                                                                            
                                                                           
                                       
   
                                                               
   
                                       
   
static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;

  tctx->dataPos = _getFilePos(AH, ctx);
  if (tctx->dataPos >= 0)
  {
    tctx->dataState = K_OFFSET_POS_SET;
  }

  _WriteByte(AH, BLK_BLOBS);                 
  WriteInt(AH, te->dumpId);                        
}

   
                                                           
   
              
   
                                                           
   
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  if (oid == 0)
  {
    fatal("invalid OID for large object");
  }

  WriteInt(AH, oid);

  ctx->cs = AllocateCompressor(AH->compression, _CustomWriteFunc);
}

   
                                                         
   
             
   
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  EndCompressor(AH, ctx->cs);
                           
  WriteInt(AH, 0);
}

   
                                                               
   
             
   
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te)
{
                                                       
  WriteInt(AH, 0);
}

   
                                    
   
static void
_PrintTocData(ArchiveHandle *AH, TocEntry *te)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  lclTocEntry *tctx = (lclTocEntry *)te->formatData;
  int blkType;
  int id;

  if (tctx->dataState == K_OFFSET_NO_DATA)
  {
    return;
  }

  if (!ctx->hasSeek || tctx->dataState == K_OFFSET_POS_NOT_SET)
  {
       
                                                                         
                                                                  
                                                                       
                                                      
       
                                                                           
                                                                  
                                    
       
    if (ctx->hasSeek)
    {
      if (fseeko(AH->FH, ctx->lastFilePos, SEEK_SET) != 0)
      {
        fatal("error during file seek: %m");
      }
    }

    for (;;)
    {
      pgoff_t thisBlkPos = _getFilePos(AH, ctx);

      _readBlockHeader(AH, &blkType, &id);

      if (blkType == EOF || id == te->dumpId)
      {
        break;
      }

                                                      
      if (thisBlkPos >= 0)
      {
        TocEntry *otherte = getTocEntryByDumpId(AH, id);

        if (otherte && otherte->formatData)
        {
          lclTocEntry *othertctx = (lclTocEntry *)otherte->formatData;

             
                                                                    
                                                                   
                                                                 
                                                                   
                                                                     
                                                                  
                                                           
             
          if (othertctx->dataState == K_OFFSET_POS_NOT_SET)
          {
            othertctx->dataPos = thisBlkPos;
            othertctx->dataState = K_OFFSET_POS_SET;
          }
          else if (othertctx->dataPos != thisBlkPos || othertctx->dataState != K_OFFSET_POS_SET)
          {
                              
            pg_log_warning("data block %d has wrong seek position", id);
          }
        }
      }

      switch (blkType)
      {
      case BLK_DATA:
        _skipData(AH);
        break;

      case BLK_BLOBS:
        _skipBlobs(AH);
        break;

      default:                            
        fatal("unrecognized data block type (%d) while searching archive", blkType);
        break;
      }
    }
  }
  else
  {
                                                      
    if (fseeko(AH->FH, tctx->dataPos, SEEK_SET) != 0)
    {
      fatal("error during file seek: %m");
    }

    _readBlockHeader(AH, &blkType, &id);
  }

     
                                                                         
                                                                           
     
  if (blkType == EOF)
  {
    if (!ctx->hasSeek)
    {
      fatal("could not find block ID %d in archive -- "
            "possibly due to out-of-order restore request, "
            "which cannot be handled due to non-seekable input file",
          te->dumpId);
    }
    else
    {
      fatal("could not find block ID %d in archive -- "
            "possibly corrupt archive",
          te->dumpId);
    }
  }

                    
  if (id != te->dumpId)
  {
    fatal("found unexpected block ID (%d) when reading data -- expected %d", id, te->dumpId);
  }

  switch (blkType)
  {
  case BLK_DATA:
    _PrintData(AH);
    break;

  case BLK_BLOBS:
    _LoadBlobs(AH, AH->public.ropt->dropSchema);
    break;

  default:                            
    fatal("unrecognized data block type %d while restoring archive", blkType);
    break;
  }

     
                                                                      
                                                                          
                                                                        
                                                            
     
  if (ctx->hasSeek && tctx->dataState == K_OFFSET_POS_NOT_SET)
  {
    pgoff_t curPos = _getFilePos(AH, ctx);

    if (curPos > ctx->lastFilePos)
    {
      ctx->lastFilePos = curPos;
    }
  }
}

   
                                          
   
static void
_PrintData(ArchiveHandle *AH)
{
  ReadDataFromArchive(AH, AH->compression, _CustomReadFunc);
}

static void
_LoadBlobs(ArchiveHandle *AH, bool drop)
{
  Oid oid;

  StartRestoreBlobs(AH);

  oid = ReadInt(AH);
  while (oid != 0)
  {
    StartRestoreBlob(AH, oid, drop);
    _PrintData(AH);
    EndRestoreBlob(AH, oid);
    oid = ReadInt(AH);
  }

  EndRestoreBlobs(AH);
}

   
                                                  
                                                              
                                               
                                             
   
static void
_skipBlobs(ArchiveHandle *AH)
{
  Oid oid;

  oid = ReadInt(AH);
  while (oid != 0)
  {
    _skipData(AH);
    oid = ReadInt(AH);
  }
}

   
                                         
                                                                     
                                               
   
static void
_skipData(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  size_t blkLen;
  char *buf = NULL;
  int buflen = 0;
  size_t cnt;

  blkLen = ReadInt(AH);
  while (blkLen != 0)
  {
    if (ctx->hasSeek)
    {
      if (fseeko(AH->FH, blkLen, SEEK_CUR) != 0)
      {
        fatal("error during file seek: %m");
      }
    }
    else
    {
      if (blkLen > buflen)
      {
        if (buf)
        {
          free(buf);
        }
        buf = (char *)pg_malloc(blkLen);
        buflen = blkLen;
      }
      if ((cnt = fread(buf, 1, blkLen, AH->FH)) != blkLen)
      {
        if (feof(AH->FH))
        {
          fatal("could not read from input file: end of file");
        }
        else
        {
          fatal("could not read from input file: %m");
        }
      }
    }

    blkLen = ReadInt(AH);
  }

  if (buf)
  {
    free(buf);
  }
}

   
                                        
   
              
   
                                                                      
   
static int
_WriteByte(ArchiveHandle *AH, const int i)
{
  int res;

  if ((res = fputc(i, AH->FH)) == EOF)
  {
    WRITE_ERROR_EXIT;
  }

  return 1;
}

   
                                         
   
             
   
                                                                     
                                           
   
static int
_ReadByte(ArchiveHandle *AH)
{
  int res;

  res = getc(AH->FH);
  if (res == EOF)
  {
    READ_ERROR_EXIT(AH->FH);
  }
  return res;
}

   
                                          
   
              
   
                                                                    
   
static void
_WriteBuf(ArchiveHandle *AH, const void *buf, size_t len)
{
  if (fwrite(buf, 1, len, AH->FH) != len)
  {
    WRITE_ERROR_EXIT;
  }
}

   
                                           
   
              
   
                                                                    
   
static void
_ReadBuf(ArchiveHandle *AH, void *buf, size_t len)
{
  if (fread(buf, 1, len, AH->FH) != len)
  {
    READ_ERROR_EXIT(AH->FH);
  }
}

   
                      
   
              
   
                                                                      
                                                                      
                                                                       
   
                                                           
                                           
                                       
                                               
   
   
static void
_CloseArchive(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  pgoff_t tpos;

  if (AH->mode == archModeWrite)
  {
    WriteHead(AH);
                                                    
    tpos = ftello(AH->FH);
    if (tpos < 0 && ctx->hasSeek)
    {
      fatal("could not determine seek position in archive file: %m");
    }
    WriteToc(AH);
    WriteDataChunks(AH, NULL);

       
                                                                        
                                                                           
                                                                         
                                                         
       
    if (ctx->hasSeek && fseeko(AH->FH, tpos, SEEK_SET) == 0)
    {
      WriteToc(AH);
    }
  }

  if (fclose(AH->FH) != 0)
  {
    fatal("could not close archive file: %m");
  }

                                              
  if (AH->dosync && AH->mode == archModeWrite && AH->fSpec)
  {
    (void)fsync_fname(AH->fSpec, false);
  }

  AH->FH = NULL;
}

   
                                     
   
                                                                          
                                                                        
                                                               
   
static void
_ReopenArchive(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  pgoff_t tpos;

  if (AH->mode == archModeWrite)
  {
    fatal("can only reopen input archives");
  }

     
                                                                             
                                                                          
     
  if (AH->fSpec == NULL || strcmp(AH->fSpec, "") == 0)
  {
    fatal("parallel restore from standard input is not supported");
  }
  if (!ctx->hasSeek)
  {
    fatal("parallel restore from non-seekable file is not supported");
  }

  tpos = ftello(AH->FH);
  if (tpos < 0)
  {
    fatal("could not determine seek position in archive file: %m");
  }

#ifndef WIN32
  if (fclose(AH->FH) != 0)
  {
    fatal("could not close archive file: %m");
  }
#endif

  AH->FH = fopen(AH->fSpec, PG_BINARY_R);
  if (!AH->FH)
  {
    fatal("could not open input file \"%s\": %m", AH->fSpec);
  }

  if (fseeko(AH->FH, tpos, SEEK_SET) != 0)
  {
    fatal("could not set seek position in archive file: %m");
  }
}

   
                                 
   
                                                                               
                                                                       
                                                                           
                                          
   
                                                             
   
static void
_PrepParallelRestore(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;
  TocEntry *prev_te = NULL;
  lclTocEntry *prev_tctx = NULL;
  TocEntry *te;

     
                                                                      
                                                                             
                         
     
  for (te = AH->toc->next; te != AH->toc; te = te->next)
  {
    lclTocEntry *tctx = (lclTocEntry *)te->formatData;

       
                                                                        
                                                                         
                                                         
       
    if (tctx->dataState != K_OFFSET_POS_SET)
    {
      continue;
    }

                                             
    if (prev_te)
    {
      if (tctx->dataPos > prev_tctx->dataPos)
      {
        prev_te->dataLength = tctx->dataPos - prev_tctx->dataPos;
      }
    }

    prev_te = te;
    prev_tctx = tctx;
  }

                                                                   
  if (prev_te && ctx->hasSeek)
  {
    pgoff_t endpos;

    if (fseeko(AH->FH, 0, SEEK_END) != 0)
    {
      fatal("error during file seek: %m");
    }
    endpos = ftello(AH->FH);
    if (endpos > prev_tctx->dataPos)
    {
      prev_te->dataLength = endpos - prev_tctx->dataPos;
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

                                      
  if (ctx->cs != NULL)
  {
    fatal("compressor active");
  }

     
                                                                         
                                                                     
                                                                           
                    
     
                                                                             
                                                       
     
}

static void
_DeClone(ArchiveHandle *AH)
{
  lclContext *ctx = (lclContext *)AH->formatData;

  free(ctx);
}

   
                                                                       
                                                                         
   
static int
_WorkerJobRestoreCustom(ArchiveHandle *AH, TocEntry *te)
{
  return parallel_restore(AH, te);
}

                                                     
                           
                                                     
   

   
                                                 
   
                                                                      
                                                                     
                                                                    
                              
   
static pgoff_t
_getFilePos(ArchiveHandle *AH, lclContext *ctx)
{
  pgoff_t pos;

  pos = ftello(AH->FH);
  if (pos < 0)
  {
                                               
    if (ctx->hasSeek)
    {
      fatal("could not determine seek position in archive file: %m");
    }
  }
  return pos;
}

   
                                                               
                                                                 
              
   
static void
_readBlockHeader(ArchiveHandle *AH, int *type, int *id)
{
  int byt;

     
                                                                            
                                                                       
                                                                            
                                                                        
                                        
     
  if (AH->version < K_VERS_1_3)
  {
    *type = BLK_DATA;
  }
  else
  {
    byt = getc(AH->FH);
    *type = byt;
    if (byt == EOF)
    {
      *id = 0;                                          
      return;
    }
  }

  *id = ReadInt(AH);
}

   
                                                                              
                        
   
static void
_CustomWriteFunc(ArchiveHandle *AH, const char *buf, size_t len)
{
                                                          
  if (len > 0)
  {
    WriteInt(AH, len);
    _WriteBuf(AH, buf, len);
  }
  return;
}

   
                                                                        
                                               
   
static size_t
_CustomReadFunc(ArchiveHandle *AH, char **buf, size_t *buflen)
{
  size_t blkLen;

                   
  blkLen = ReadInt(AH);
  if (blkLen == 0)
  {
    return 0;
  }

                                                                         
  if (blkLen > *buflen)
  {
    free(*buf);
    *buf = (char *)pg_malloc(blkLen);
    *buflen = blkLen;
  }

                                
  _ReadBuf(AH, *buf, blkLen);

  return blkLen;
}
