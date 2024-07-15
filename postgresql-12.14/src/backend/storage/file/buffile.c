                                                                            
   
             
                                                   
   
                                                                         
                                                                        
   
                  
                                        
   
          
   
                                                                            
                                                                      
                                                                      
                                                                     
                                                                        
                                                                      
                                        
   
                                                                        
                                                                              
                                                                    
                                                                       
                                                                    
                                                                     
                                                                            
                                  
   
                                                                            
                                                                             
                                                     
   
                                                                               
                                                                              
                                                                          
                
                                                                            
   

#include "postgres.h"

#include "commands/tablespace.h"
#include "executor/instrument.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/buffile.h"
#include "storage/buf_internals.h"
#include "utils/resowner.h"

   
                                                                              
                                                                            
                               
   
#define MAX_PHYSICAL_FILESIZE 0x40000000
#define BUFFILE_SEG_SIZE (MAX_PHYSICAL_FILESIZE / BLCKSZ)

   
                                                                          
                                                                        
                     
   
struct BufFile
{
  int numFiles;                                      
                                                                           
  File *files;                                           

  bool isInterXact;                                   
  bool dirty;                                            
  bool readOnly;                                             

  SharedFileSet *fileset;                                        
  const char *name;                                           

     
                                                                          
                                                                       
                                                                 
     
  ResourceOwner resowner;

     
                                                                           
                                                                        
     
  int curFile;                                                
  off_t curOffset;                                 
  int pos;                                                 
  int nbytes;                                            
  PGAlignedBlock buffer;
};

static BufFile *
makeBufFileCommon(int nfiles);
static BufFile *
makeBufFile(File firstfile);
static void
extendBufFile(BufFile *file);
static void
BufFileLoadBuffer(BufFile *file);
static void
BufFileDumpBuffer(BufFile *file);
static void
BufFileFlush(BufFile *file);
static File
MakeNewSharedSegment(BufFile *file, int segment);

   
                                                         
   
static BufFile *
makeBufFileCommon(int nfiles)
{
  BufFile *file = (BufFile *)palloc(sizeof(BufFile));

  file->numFiles = nfiles;
  file->isInterXact = false;
  file->dirty = false;
  file->resowner = CurrentResourceOwner;
  file->curFile = 0;
  file->curOffset = 0L;
  file->pos = 0;
  file->nbytes = 0;

  return file;
}

   
                                                              
                                                     
   
static BufFile *
makeBufFile(File firstfile)
{
  BufFile *file = makeBufFileCommon(1);

  file->files = (File *)palloc(sizeof(File));
  file->files[0] = firstfile;
  file->readOnly = false;
  file->fileset = NULL;
  file->name = NULL;

  return file;
}

   
                                    
   
static void
extendBufFile(BufFile *file)
{
  File pfile;
  ResourceOwner oldowner;

                                                                       
  oldowner = CurrentResourceOwner;
  CurrentResourceOwner = file->resowner;

  if (file->fileset == NULL)
  {
    pfile = OpenTemporaryFile(file->isInterXact);
  }
  else
  {
    pfile = MakeNewSharedSegment(file, file->numFiles);
  }

  Assert(pfile >= 0);

  CurrentResourceOwner = oldowner;

  file->files = (File *)repalloc(file->files, (file->numFiles + 1) * sizeof(File));
  file->files[file->numFiles] = pfile;
  file->numFiles++;
}

   
                                                                          
                                                                         
                   
   
                                                                         
                          
   
                                                                        
                                                                       
                           
   
BufFile *
BufFileCreateTemp(bool interXact)
{
  BufFile *file;
  File pfile;

     
                                                                           
                                                                             
                                                                            
                                                                       
                                                                             
                                                                            
                       
     
  PrepareTempTablespaces();

  pfile = OpenTemporaryFile(interXact);
  Assert(pfile >= 0);

  file = makeBufFile(pfile);
  file->isInterXact = interXact;

  return file;
}

   
                                                          
   
static void
SharedSegmentName(char *name, const char *buffile_name, int segment)
{
  snprintf(name, MAXPGPATH, "%s.%d", buffile_name, segment);
}

   
                                                       
   
static File
MakeNewSharedSegment(BufFile *buffile, int segment)
{
  char name[MAXPGPATH];
  File file;

     
                                                                       
                                                                          
                                                                           
                                          
     
  SharedSegmentName(name, buffile->name, segment + 1);
  SharedFileSetDelete(buffile->fileset, name, true);

                               
  SharedSegmentName(name, buffile->name, segment);
  file = SharedFileSetCreate(buffile->fileset, name);

                                                
  Assert(file > 0);

  return file;
}

   
                                                                         
                                                                             
   
                                                                              
                                                                        
                                                                       
                                                                             
                                                                      
                                    
   
BufFile *
BufFileCreateShared(SharedFileSet *fileset, const char *name)
{
  BufFile *file;

  file = makeBufFileCommon(1);
  file->fileset = fileset;
  file->name = pstrdup(name);
  file->files = (File *)palloc(sizeof(File));
  file->files[0] = MakeNewSharedSegment(file, 0);
  file->readOnly = false;

  return file;
}

   
                                                                            
                                                                           
                                                                        
                                                                             
                                     
   
BufFile *
BufFileOpenShared(SharedFileSet *fileset, const char *name)
{
  BufFile *file;
  char segment_name[MAXPGPATH];
  Size capacity = 16;
  File *files;
  int nfiles = 0;

  files = palloc(sizeof(File) * capacity);

     
                                                                   
                             
     
  for (;;)
  {
                                                          
    if (nfiles + 1 > capacity)
    {
      capacity *= 2;
      files = repalloc(files, sizeof(File) * capacity);
    }
                                
    SharedSegmentName(segment_name, name, nfiles);
    files[nfiles] = SharedFileSetOpen(fileset, segment_name);
    if (files[nfiles] <= 0)
    {
      break;
    }
    ++nfiles;

    CHECK_FOR_INTERRUPTS();
  }

     
                                                                          
           
     
  if (nfiles == 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open temporary file \"%s\" from BufFile \"%s\": %m", segment_name, name)));
  }

  file = makeBufFileCommon(nfiles);
  file->files = files;
  file->readOnly = true;                                           
  file->fileset = fileset;
  file->name = pstrdup(name);

  return file;
}

   
                                                                         
                                       
   
                                                                             
                                                                               
                                       
   
                                                                           
                                                   
   
void
BufFileDeleteShared(SharedFileSet *fileset, const char *name)
{
  char segment_name[MAXPGPATH];
  int segment = 0;
  bool found = false;

     
                                                                        
                                                                            
                     
     
  for (;;)
  {
    SharedSegmentName(segment_name, name, segment);
    if (!SharedFileSetDelete(fileset, segment_name, true))
    {
      break;
    }
    found = true;
    ++segment;

    CHECK_FOR_INTERRUPTS();
  }

  if (!found)
  {
    elog(ERROR, "could not delete unknown shared BufFile \"%s\"", name);
  }
}

   
                                                                                 
   
void
BufFileExportShared(BufFile *file)
{
                                                    
  Assert(file->fileset != NULL);

                                                        
  Assert(!file->readOnly);

  BufFileFlush(file);
  file->readOnly = true;
}

   
                   
   
                                                                       
   
void
BufFileClose(BufFile *file)
{
  int i;

                                
  BufFileFlush(file);
                                               
  for (i = 0; i < file->numFiles; i++)
  {
    FileClose(file->files[i]);
  }
                                
  pfree(file->files);
  pfree(file);
}

   
                     
   
                                                                     
                                                         
                                              
   
static void
BufFileLoadBuffer(BufFile *file)
{
  File thisfile;

     
                                                               
     
  if (file->curOffset >= MAX_PHYSICAL_FILESIZE && file->curFile + 1 < file->numFiles)
  {
    file->curFile++;
    file->curOffset = 0L;
  }

     
                                                        
     
  thisfile = file->files[file->curFile];
  file->nbytes = FileRead(thisfile, file->buffer.data, sizeof(file->buffer), file->curOffset, WAIT_EVENT_BUFFILE_READ);
  if (file->nbytes < 0)
  {
    file->nbytes = 0;
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", FilePathName(thisfile))));
  }

                                               

  if (file->nbytes > 0)
  {
    pgBufferUsage.temp_blks_read++;
  }
}

   
                     
   
                                               
                                                  
                                                                             
   
static void
BufFileDumpBuffer(BufFile *file)
{
  int wpos = 0;
  int bytestowrite;
  File thisfile;

     
                                                                        
                                                           
     
  while (wpos < file->nbytes)
  {
    off_t availbytes;

       
                                                                 
       
    if (file->curOffset >= MAX_PHYSICAL_FILESIZE)
    {
      while (file->curFile + 1 >= file->numFiles)
      {
        extendBufFile(file);
      }
      file->curFile++;
      file->curOffset = 0L;
    }

       
                                                           
       
    bytestowrite = file->nbytes - wpos;
    availbytes = MAX_PHYSICAL_FILESIZE - file->curOffset;

    if ((off_t)bytestowrite > availbytes)
    {
      bytestowrite = (int)availbytes;
    }

    thisfile = file->files[file->curFile];
    bytestowrite = FileWrite(thisfile, file->buffer.data + wpos, bytestowrite, file->curOffset, WAIT_EVENT_BUFFILE_WRITE);
    if (bytestowrite <= 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", FilePathName(thisfile))));
    }
    file->curOffset += bytestowrite;
    wpos += bytestowrite;

    pgBufferUsage.temp_blks_written++;
  }
  file->dirty = false;

     
                                                                          
                                                                       
                                                                           
                                                                        
     
  file->curOffset -= (file->nbytes - file->pos);
  if (file->curOffset < 0)                                       
  {
    file->curFile--;
    Assert(file->curFile >= 0);
    file->curOffset += MAX_PHYSICAL_FILESIZE;
  }

     
                                                                           
     
  file->pos = 0;
  file->nbytes = 0;
}

   
               
   
                                                                               
              
   
size_t
BufFileRead(BufFile *file, void *ptr, size_t size)
{
  size_t nread = 0;
  size_t nthistime;

  BufFileFlush(file);

  while (size > 0)
  {
    if (file->pos >= file->nbytes)
    {
                                              
      file->curOffset += file->pos;
      file->pos = 0;
      file->nbytes = 0;
      BufFileLoadBuffer(file);
      if (file->nbytes <= 0)
      {
        break;                             
      }
    }

    nthistime = file->nbytes - file->pos;
    if (nthistime > size)
    {
      nthistime = size;
    }
    Assert(nthistime > 0);

    memcpy(ptr, file->buffer.data + file->pos, nthistime);

    file->pos += nthistime;
    ptr = (void *)((char *)ptr + nthistime);
    size -= nthistime;
    nread += nthistime;
  }

  return nread;
}

   
                
   
                                                                            
              
   
size_t
BufFileWrite(BufFile *file, void *ptr, size_t size)
{
  size_t nwritten = 0;
  size_t nthistime;

  Assert(!file->readOnly);

  while (size > 0)
  {
    if (file->pos >= BLCKSZ)
    {
                                    
      if (file->dirty)
      {
        BufFileDumpBuffer(file);
      }
      else
      {
                                                         
        file->curOffset += file->pos;
        file->pos = 0;
        file->nbytes = 0;
      }
    }

    nthistime = BLCKSZ - file->pos;
    if (nthistime > size)
    {
      nthistime = size;
    }
    Assert(nthistime > 0);

    memcpy(file->buffer.data + file->pos, ptr, nthistime);

    file->dirty = true;
    file->pos += nthistime;
    if (file->nbytes < file->pos)
    {
      file->nbytes = file->pos;
    }
    ptr = (void *)((char *)ptr + nthistime);
    size -= nthistime;
    nwritten += nthistime;
  }

  return nwritten;
}

   
                
   
                                                                      
   
static void
BufFileFlush(BufFile *file)
{
  if (file->dirty)
  {
    BufFileDumpBuffer(file);
  }

  Assert(!file->dirty);
}

   
               
   
                                                                          
                                                                            
                                                                    
                                         
   
                                                                       
                                 
   
int
BufFileSeek(BufFile *file, int fileno, off_t offset, int whence)
{
  int newFile;
  off_t newOffset;

  switch (whence)
  {
  case SEEK_SET:
    if (fileno < 0)
    {
      return EOF;
    }
    newFile = fileno;
    newOffset = offset;
    break;
  case SEEK_CUR:

       
                                                                
                                                                       
                                         
       
    newFile = file->curFile;
    newOffset = (file->curOffset + file->pos) + offset;
    break;
#ifdef NOT_USED
  case SEEK_END:
                                                    
    break;
#endif
  default:
    elog(ERROR, "invalid whence: %d", whence);
    return EOF;
  }
  while (newOffset < 0)
  {
    if (--newFile < 0)
    {
      return EOF;
    }
    newOffset += MAX_PHYSICAL_FILESIZE;
  }
  if (newFile == file->curFile && newOffset >= file->curOffset && newOffset <= file->curOffset + file->nbytes)
  {
       
                                                                     
                                                                    
                                                                       
                
       
    file->pos = (int)(newOffset - file->curOffset);
    return 0;
  }
                                                                  
  BufFileFlush(file);

     
                                                                        
                                                                            
                                             
     

                                                                
  if (newFile == file->numFiles && newOffset == 0)
  {
    newFile--;
    newOffset = MAX_PHYSICAL_FILESIZE;
  }
  while (newOffset > MAX_PHYSICAL_FILESIZE)
  {
    if (++newFile >= file->numFiles)
    {
      return EOF;
    }
    newOffset -= MAX_PHYSICAL_FILESIZE;
  }
  if (newFile >= file->numFiles)
  {
    return EOF;
  }
                   
  file->curFile = newFile;
  file->curOffset = newOffset;
  file->pos = 0;
  file->nbytes = 0;
  return 0;
}

void
BufFileTell(BufFile *file, int *fileno, off_t *offset)
{
  *fileno = file->curFile;
  *offset = file->curOffset + file->pos;
}

   
                                            
   
                                                                         
                                                                         
                                                                          
                                           
   
                                                                       
                                 
   
int
BufFileSeekBlock(BufFile *file, long blknum)
{
  return BufFileSeek(file, (int)(blknum / BUFFILE_SEG_SIZE), (off_t)(blknum % BUFFILE_SEG_SIZE) * BLCKSZ, SEEK_SET);
}

#ifdef NOT_USED
   
                                            
   
                                                                           
   
long
BufFileTellBlock(BufFile *file)
{
  long blknum;

  blknum = (file->curOffset + file->pos) / BLCKSZ;
  blknum += file->curFile * BUFFILE_SEG_SIZE;
  return blknum;
}

#endif

   
                                           
   
                                                                      
                          
   
int64
BufFileSize(BufFile *file)
{
  int64 lastFileSize;

  Assert(file->fileset != NULL);

                                               
  lastFileSize = FileSize(file->files[file->numFiles - 1]);
  if (lastFileSize < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not determine size of temporary file \"%s\" from BufFile \"%s\": %m", FilePathName(file->files[file->numFiles - 1]), file->name)));
  }

  return ((file->numFiles - 1) * (int64)MAX_PHYSICAL_FILESIZE) + lastFileSize;
}

   
                                                                         
                                                            
   
                                                                       
                                                                          
                                                                         
        
   
                                                                       
                                                                      
                                                                        
                                                                         
           
   
                                                                       
                                                                         
                                                              
   
long
BufFileAppend(BufFile *target, BufFile *source)
{
  long startBlock = target->numFiles * BUFFILE_SEG_SIZE;
  int newNumFiles = target->numFiles + source->numFiles;
  int i;

  Assert(target->fileset != NULL);
  Assert(source->readOnly);
  Assert(!source->dirty);
  Assert(source->fileset != NULL);

  if (target->resowner != source->resowner)
  {
    elog(ERROR, "could not append BufFile with non-matching resource owner");
  }

  target->files = (File *)repalloc(target->files, sizeof(File) * newNumFiles);
  for (i = target->numFiles; i < newNumFiles; i++)
  {
    target->files[i] = source->files[i - target->numFiles];
  }
  target->numFiles = newNumFiles;

  return startBlock;
}
