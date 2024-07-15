                                                                            
   
                    
   
                                                                   
                                                               
                   
   
                                                   
   
                                     
                                                               
                                   
   
                                                              
                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres_fe.h"

#include "pg_backup_archiver.h"
#include "pg_backup_utils.h"
#include "fe_utils/string_utils.h"

#include "libpq/libpq-fs.h"

static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen);
static void
_WriteBlobData(ArchiveHandle *AH, const void *data, size_t dLen);
static void
_EndData(ArchiveHandle *AH, TocEntry *te);
static int
_WriteByte(ArchiveHandle *AH, const int i);
static void
_WriteBuf(ArchiveHandle *AH, const void *buf, size_t len);
static void
_CloseArchive(ArchiveHandle *AH);
static void
_PrintTocData(ArchiveHandle *AH, TocEntry *te);
static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te);
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid);
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid);
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te);

   
               
   
void
InitArchiveFmt_Null(ArchiveHandle *AH)
{
                                                                      
  AH->WriteDataPtr = _WriteData;
  AH->EndDataPtr = _EndData;
  AH->WriteBytePtr = _WriteByte;
  AH->WriteBufPtr = _WriteBuf;
  AH->ClosePtr = _CloseArchive;
  AH->ReopenPtr = NULL;
  AH->PrintTocDataPtr = _PrintTocData;

  AH->StartBlobsPtr = _StartBlobs;
  AH->StartBlobPtr = _StartBlob;
  AH->EndBlobPtr = _EndBlob;
  AH->EndBlobsPtr = _EndBlobs;
  AH->ClonePtr = NULL;
  AH->DeClonePtr = NULL;

                               
  AH->lo_buf_size = LOBBUFSIZE;
  AH->lo_buf = (void *)pg_malloc(LOBBUFSIZE);

     
                            
     
  if (AH->mode == archModeRead)
  {
    fatal("this format cannot be read");
  }
}

   
                           
   

   
                                                                 
   
static void
_WriteData(ArchiveHandle *AH, const void *data, size_t dLen)
{
                                                                   
  ahwrite(data, 1, dLen, AH);
  return;
}

   
                                                                 
                                                           
   
static void
_WriteBlobData(ArchiveHandle *AH, const void *data, size_t dLen)
{
  if (dLen > 0)
  {
    PQExpBuffer buf = createPQExpBuffer();

    appendByteaLiteralAHX(buf, (const unsigned char *)data, dLen, AH);

    ahprintf(AH, "SELECT pg_catalog.lowrite(0, %s);\n", buf->data);

    destroyPQExpBuffer(buf);
  }
  return;
}

static void
_EndData(ArchiveHandle *AH, TocEntry *te)
{
  ahprintf(AH, "\n\n");
}

   
                                                                            
                                                                           
                                       
   
                                                               
   
                                       
   
static void
_StartBlobs(ArchiveHandle *AH, TocEntry *te)
{
  ahprintf(AH, "BEGIN;\n\n");
}

   
                                                           
   
              
   
                                                           
   
static void
_StartBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  bool old_blob_style = (AH->version < K_VERS_1_12);

  if (oid == 0)
  {
    fatal("invalid OID for large object");
  }

                                                                 
  if (old_blob_style && AH->public.ropt->dropSchema)
  {
    DropBlobIfExists(AH, oid);
  }

  if (old_blob_style)
  {
    ahprintf(AH, "SELECT pg_catalog.lo_open(pg_catalog.lo_create('%u'), %d);\n", oid, INV_WRITE);
  }
  else
  {
    ahprintf(AH, "SELECT pg_catalog.lo_open('%u', %d);\n", oid, INV_WRITE);
  }

  AH->WriteDataPtr = _WriteBlobData;
}

   
                                                         
   
             
   
static void
_EndBlob(ArchiveHandle *AH, TocEntry *te, Oid oid)
{
  AH->WriteDataPtr = _WriteData;

  ahprintf(AH, "SELECT pg_catalog.lo_close(0);\n\n");
}

   
                                                               
   
             
   
static void
_EndBlobs(ArchiveHandle *AH, TocEntry *te)
{
  ahprintf(AH, "COMMIT;\n\n");
}

         
                                                                       
                                                            
         
   
static void
_PrintTocData(ArchiveHandle *AH, TocEntry *te)
{
  if (te->dataDumper)
  {
    AH->currToc = te;

    if (strcmp(te->desc, "BLOBS") == 0)
    {
      _StartBlobs(AH, te);
    }

    te->dataDumper((Archive *)AH, te->dataDumperArg);

    if (strcmp(te->desc, "BLOBS") == 0)
    {
      _EndBlobs(AH, te);
    }

    AH->currToc = NULL;
  }
}

static int
_WriteByte(ArchiveHandle *AH, const int i)
{
                         
  return 0;
}

static void
_WriteBuf(ArchiveHandle *AH, const void *buf, size_t len)
{
                         
  return;
}

static void
_CloseArchive(ArchiveHandle *AH)
{
                     
}
