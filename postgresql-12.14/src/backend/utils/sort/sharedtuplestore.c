                                                                            
   
                      
                                                           
   
                                                                             
                                                                               
                                                                              
                                                                             
                                                                             
            
   
                                                                         
                                                                        
   
                  
                                               
   
                                                                            
   

#include "postgres.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "miscadmin.h"
#include "storage/buffile.h"
#include "storage/lwlock.h"
#include "storage/sharedfileset.h"
#include "utils/sharedtuplestore.h"

#include <limits.h>

   
                                                                            
                                                                              
                                                                          
                     
   
#define STS_CHUNK_PAGES 4
#define STS_CHUNK_HEADER_SIZE offsetof(SharedTuplestoreChunk, data)
#define STS_CHUNK_DATA_SIZE (STS_CHUNK_PAGES * BLCKSZ - STS_CHUNK_HEADER_SIZE)

                            
typedef struct SharedTuplestoreChunk
{
  int ntuples;                                       
  int overflow;                                                
  char data[FLEXIBLE_ARRAY_MEMBER];
} SharedTuplestoreChunk;

                                   
typedef struct SharedTuplestoreParticipant
{
  LWLock lock;
  BlockNumber read_page;                                 
  BlockNumber npages;                                  
  bool writing;                                         
} SharedTuplestoreParticipant;

                                                     
struct SharedTuplestore
{
  int nparticipants;                                                  
  int flags;                                                        
  size_t meta_data_size;                                 
  char name[NAMEDATALEN];                                  

                                                 
  SharedTuplestoreParticipant participants[FLEXIBLE_ARRAY_MEMBER];
};

                                                               
struct SharedTuplestoreAccessor
{
  int participant;                                    
  SharedTuplestore *sts;                         
  SharedFileSet *fileset;                                       
  MemoryContext context;                                   

                          
  int read_participant;                                                  
  BufFile *read_file;                                             
  int read_ntuples_available;                                     
  int read_ntuples;                                                         
  size_t read_bytes;                                                       
  char *read_buffer;                                            
  size_t read_buffer_size;
  BlockNumber read_next_page;                                           

                          
  SharedTuplestoreChunk *write_chunk;                          
  BufFile *write_file;                                                   
  BlockNumber write_page;                                             
  char *write_pointer;                                                         
  char *write_end;                                                                
};

static void
sts_filename(char *name, SharedTuplestoreAccessor *accessor, int participant);

   
                                                                              
                                 
   
size_t
sts_estimate(int participants)
{
  return offsetof(SharedTuplestore, participants) + sizeof(SharedTuplestoreParticipant) * participants;
}

   
                                                                           
                                                                            
                                                                          
                                             
   
                                                                      
                                                                               
                                                                        
                 
   
                                                                            
                                                                          
                                                                   
   
SharedTuplestoreAccessor *
sts_initialize(SharedTuplestore *sts, int participants, int my_participant_number, size_t meta_data_size, int flags, SharedFileSet *fileset, const char *name)
{
  SharedTuplestoreAccessor *accessor;
  int i;

  Assert(my_participant_number < participants);

  sts->nparticipants = participants;
  sts->meta_data_size = meta_data_size;
  sts->flags = flags;

  if (strlen(name) > sizeof(sts->name) - 1)
  {
    elog(ERROR, "SharedTuplestore name too long");
  }
  strcpy(sts->name, name);

     
                                                                         
                                                                            
                                                                        
                                                           
     
  if (meta_data_size + sizeof(uint32) >= STS_CHUNK_DATA_SIZE)
  {
    elog(ERROR, "meta-data too long");
  }

  for (i = 0; i < participants; ++i)
  {
    LWLockInitialize(&sts->participants[i].lock, LWTRANCHE_SHARED_TUPLESTORE);
    sts->participants[i].read_page = 0;
    sts->participants[i].npages = 0;
    sts->participants[i].writing = false;
  }

  accessor = palloc0(sizeof(SharedTuplestoreAccessor));
  accessor->participant = my_participant_number;
  accessor->sts = sts;
  accessor->fileset = fileset;
  accessor->context = CurrentMemoryContext;

  return accessor;
}

   
                                                                              
                                                   
   
SharedTuplestoreAccessor *
sts_attach(SharedTuplestore *sts, int my_participant_number, SharedFileSet *fileset)
{
  SharedTuplestoreAccessor *accessor;

  Assert(my_participant_number < sts->nparticipants);

  accessor = palloc0(sizeof(SharedTuplestoreAccessor));
  accessor->participant = my_participant_number;
  accessor->sts = sts;
  accessor->fileset = fileset;
  accessor->context = CurrentMemoryContext;

  return accessor;
}

static void
sts_flush_chunk(SharedTuplestoreAccessor *accessor)
{
  size_t size;

  size = STS_CHUNK_PAGES * BLCKSZ;
  BufFileWrite(accessor->write_file, accessor->write_chunk, size);
  memset(accessor->write_chunk, 0, size);
  accessor->write_pointer = &accessor->write_chunk->data[0];
  accessor->sts->participants[accessor->participant].npages += STS_CHUNK_PAGES;
}

   
                                                                         
                                                      
   
void
sts_end_write(SharedTuplestoreAccessor *accessor)
{
  if (accessor->write_file != NULL)
  {
    sts_flush_chunk(accessor);
    BufFileClose(accessor->write_file);
    pfree(accessor->write_chunk);
    accessor->write_chunk = NULL;
    accessor->write_file = NULL;
    accessor->sts->participants[accessor->participant].writing = false;
  }
}

   
                                                                               
                                                                          
                                                                            
                                                                  
                   
   
void
sts_reinitialize(SharedTuplestoreAccessor *accessor)
{
  int i;

     
                                                                           
                                                                             
                                       
     
  for (i = 0; i < accessor->sts->nparticipants; ++i)
  {
    accessor->sts->participants[i].read_page = 0;
  }
}

   
                                            
   
void
sts_begin_parallel_scan(SharedTuplestoreAccessor *accessor)
{
  int i PG_USED_FOR_ASSERTS_ONLY;

                                                   
  sts_end_parallel_scan(accessor);

     
                                                                          
                                                                          
                                 
     
  for (i = 0; i < accessor->sts->nparticipants; ++i)
  {
    Assert(!accessor->sts->participants[i].writing);
  }

     
                                                                            
                                                 
     
  accessor->read_participant = accessor->participant;
  accessor->read_file = NULL;
  accessor->read_next_page = 0;
}

   
                                                                       
   
void
sts_end_parallel_scan(SharedTuplestoreAccessor *accessor)
{
     
                                                                          
                                                                             
                                                                    
     
  if (accessor->read_file != NULL)
  {
    BufFileClose(accessor->read_file);
    accessor->read_file = NULL;
  }
}

   
                                                                              
                                                       
   
void
sts_puttuple(SharedTuplestoreAccessor *accessor, void *meta_data, MinimalTuple tuple)
{
  size_t size;

                                    
  if (accessor->write_file == NULL)
  {
    SharedTuplestoreParticipant *participant;
    char name[MAXPGPATH];

                                                            
    sts_filename(name, accessor, accessor->participant);
    accessor->write_file = BufFileCreateShared(accessor->fileset, name);

                                                          
    participant = &accessor->sts->participants[accessor->participant];
    participant->writing = true;                          
  }

                         
  size = accessor->sts->meta_data_size + tuple->t_len;
  if (accessor->write_pointer + size > accessor->write_end)
  {
    if (accessor->write_chunk == NULL)
    {
                                                
      accessor->write_chunk = (SharedTuplestoreChunk *)MemoryContextAllocZero(accessor->context, STS_CHUNK_PAGES * BLCKSZ);
      accessor->write_chunk->ntuples = 0;
      accessor->write_pointer = &accessor->write_chunk->data[0];
      accessor->write_end = (char *)accessor->write_chunk + STS_CHUNK_PAGES * BLCKSZ;
    }
    else
    {
                                  
      sts_flush_chunk(accessor);
    }

                                                                     
    if (accessor->write_pointer + size > accessor->write_end)
    {
      size_t written;

         
                                                                    
                                                             
         
                                                                
                                                                        
                                                                       
                          
         
      Assert(accessor->write_pointer + accessor->sts->meta_data_size + sizeof(uint32) < accessor->write_end);

                                             
      if (accessor->sts->meta_data_size > 0)
      {
        memcpy(accessor->write_pointer, meta_data, accessor->sts->meta_data_size);
      }

         
                                                                     
                                    
         
      written = accessor->write_end - accessor->write_pointer - accessor->sts->meta_data_size;
      memcpy(accessor->write_pointer + accessor->sts->meta_data_size, tuple, written);
      ++accessor->write_chunk->ntuples;
      size -= accessor->sts->meta_data_size;
      size -= written;
                                                                      
      while (size > 0)
      {
        size_t written_this_chunk;

        sts_flush_chunk(accessor);

           
                                                                       
                                                                 
           
        accessor->write_chunk->overflow = (size + STS_CHUNK_DATA_SIZE - 1) / STS_CHUNK_DATA_SIZE;
        written_this_chunk = Min(accessor->write_end - accessor->write_pointer, size);
        memcpy(accessor->write_pointer, (char *)tuple + written, written_this_chunk);
        accessor->write_pointer += written_this_chunk;
        size -= written_this_chunk;
        written += written_this_chunk;
      }
      return;
    }
  }

                                             
  if (accessor->sts->meta_data_size > 0)
  {
    memcpy(accessor->write_pointer, meta_data, accessor->sts->meta_data_size);
  }
  memcpy(accessor->write_pointer + accessor->sts->meta_data_size, tuple, tuple->t_len);
  accessor->write_pointer += size;
  ++accessor->write_chunk->ntuples;
}

static MinimalTuple
sts_read_tuple(SharedTuplestoreAccessor *accessor, void *meta_data)
{
  MinimalTuple tuple;
  uint32 size;
  size_t remaining_size;
  size_t this_chunk_size;
  char *destination;

     
                                                                             
                                                             
     
  if (accessor->sts->meta_data_size > 0)
  {
    if (BufFileRead(accessor->read_file, meta_data, accessor->sts->meta_data_size) != accessor->sts->meta_data_size)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from shared tuplestore temporary file"), errdetail_internal("Short read while reading meta-data.")));
    }
    accessor->read_bytes += accessor->sts->meta_data_size;
  }
  if (BufFileRead(accessor->read_file, &size, sizeof(size)) != sizeof(size))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from shared tuplestore temporary file"), errdetail_internal("Short read while reading size.")));
  }
  accessor->read_bytes += sizeof(size);
  if (size > accessor->read_buffer_size)
  {
    size_t new_read_buffer_size;

    if (accessor->read_buffer != NULL)
    {
      pfree(accessor->read_buffer);
    }
    new_read_buffer_size = Max(size, accessor->read_buffer_size * 2);
    accessor->read_buffer = MemoryContextAlloc(accessor->context, new_read_buffer_size);
    accessor->read_buffer_size = new_read_buffer_size;
  }
  remaining_size = size - sizeof(uint32);
  this_chunk_size = Min(remaining_size, BLCKSZ * STS_CHUNK_PAGES - accessor->read_bytes);
  destination = accessor->read_buffer + sizeof(uint32);
  if (BufFileRead(accessor->read_file, destination, this_chunk_size) != this_chunk_size)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from shared tuplestore temporary file"), errdetail_internal("Short read while reading tuple.")));
  }
  accessor->read_bytes += this_chunk_size;
  remaining_size -= this_chunk_size;
  destination += this_chunk_size;
  ++accessor->read_ntuples;

                                                     
  while (remaining_size > 0)
  {
                                                                  
    SharedTuplestoreChunk chunk_header;

    if (BufFileRead(accessor->read_file, &chunk_header, STS_CHUNK_HEADER_SIZE) != STS_CHUNK_HEADER_SIZE)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from shared tuplestore temporary file"), errdetail_internal("Short read while reading overflow chunk header.")));
    }
    accessor->read_bytes = STS_CHUNK_HEADER_SIZE;
    if (chunk_header.overflow == 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("unexpected chunk in shared tuplestore temporary file"), errdetail_internal("Expected overflow chunk.")));
    }
    accessor->read_next_page += STS_CHUNK_PAGES;
    this_chunk_size = Min(remaining_size, BLCKSZ * STS_CHUNK_PAGES - STS_CHUNK_HEADER_SIZE);
    if (BufFileRead(accessor->read_file, destination, this_chunk_size) != this_chunk_size)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from shared tuplestore temporary file"), errdetail_internal("Short read while reading tuple.")));
    }
    accessor->read_bytes += this_chunk_size;
    remaining_size -= this_chunk_size;
    destination += this_chunk_size;

       
                                                                          
                                                    
       
    accessor->read_ntuples = 0;
    accessor->read_ntuples_available = chunk_header.ntuples;
  }

  tuple = (MinimalTuple)accessor->read_buffer;
  tuple->t_len = size;

  return tuple;
}

   
                                                    
   
MinimalTuple
sts_parallel_scan_next(SharedTuplestoreAccessor *accessor, void *meta_data)
{
  SharedTuplestoreParticipant *p;
  BlockNumber read_page;
  bool eof;

  for (;;)
  {
                                                         
    if (accessor->read_ntuples < accessor->read_ntuples_available)
    {
      return sts_read_tuple(accessor, meta_data);
    }

                                                   
    p = &accessor->sts->participants[accessor->read_participant];

    LWLockAcquire(&p->lock, LW_EXCLUSIVE);
                                                                 
    if (p->read_page < accessor->read_next_page)
    {
      p->read_page = accessor->read_next_page;
    }
    eof = p->read_page >= p->npages;
    if (!eof)
    {
                                 
      read_page = p->read_page;
                                                      
      p->read_page += STS_CHUNK_PAGES;
      accessor->read_next_page = p->read_page;
    }
    LWLockRelease(&p->lock);

    if (!eof)
    {
      SharedTuplestoreChunk chunk_header;
      size_t nread;

                                            
      if (accessor->read_file == NULL)
      {
        char name[MAXPGPATH];

        sts_filename(name, accessor, accessor->read_participant);
        accessor->read_file = BufFileOpenShared(accessor->fileset, name);
      }

                                           
      if (BufFileSeekBlock(accessor->read_file, read_page) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek block %u in shared tuplestore temporary file", read_page)));
      }
      nread = BufFileRead(accessor->read_file, &chunk_header, STS_CHUNK_HEADER_SIZE);
      if (nread != STS_CHUNK_HEADER_SIZE)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from shared tuplestore temporary file: read only %zu of %zu bytes", nread, STS_CHUNK_HEADER_SIZE)));
      }

         
                                                                    
                                      
         
      if (chunk_header.overflow > 0)
      {
        accessor->read_next_page = read_page + chunk_header.overflow * STS_CHUNK_PAGES;
        continue;
      }

      accessor->read_ntuples = 0;
      accessor->read_ntuples_available = chunk_header.ntuples;
      accessor->read_bytes = STS_CHUNK_HEADER_SIZE;

                                                                   
    }
    else
    {
      if (accessor->read_file != NULL)
      {
        BufFileClose(accessor->read_file);
        accessor->read_file = NULL;
      }

         
                                                                      
                     
         
      accessor->read_participant = (accessor->read_participant + 1) % accessor->sts->nparticipants;
      if (accessor->read_participant == accessor->participant)
      {
        break;
      }
      accessor->read_next_page = 0;

                                                                  
    }
  }

  return NULL;
}

   
                                                                             
   
static void
sts_filename(char *name, SharedTuplestoreAccessor *accessor, int participant)
{
  snprintf(name, MAXPGPATH, "%s.p%d", accessor->sts->name, participant);
}
