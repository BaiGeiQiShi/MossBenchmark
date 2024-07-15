                                                                            
   
          
                                              
   
                                                                          
                                                                  
                                                                   
                                          
   
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

                                                                               
                                    
                                                                               

   
                        
                                            
   
MemoryContext CurrentMemoryContext = NULL;

   
                                                                         
                                                             
   
MemoryContext TopMemoryContext = NULL;
MemoryContext ErrorContext = NULL;
MemoryContext PostmasterContext = NULL;
MemoryContext CacheMemoryContext = NULL;
MemoryContext MessageContext = NULL;
MemoryContext TopTransactionContext = NULL;
MemoryContext CurTransactionContext = NULL;

                                                                     
MemoryContext PortalContext = NULL;

static void
MemoryContextCallResetCallbacks(MemoryContext context);
static void
MemoryContextStatsInternal(MemoryContext context, int level, bool print, int max_children, MemoryContextCounters *totals);
static void
MemoryContextStatsPrint(MemoryContext context, void *passthru, const char *stats_string);

   
                                                                           
                                                                        
                                               
   
#define AssertNotInCriticalSection(context) Assert(CritSectionCount == 0 || (context)->allowInCritSection)

                                                                               
                                       
                                                                               

   
                     
                                           
   
                                                                        
                                                                      
                                              
   
                                                                 
                                                                    
                                                                        
                                                                          
                                                
   
                                                                       
   
void
MemoryContextInit(void)
{
  AssertState(TopMemoryContext == NULL);

     
                                                                            
     
  TopMemoryContext = AllocSetContextCreate((MemoryContext)NULL, "TopMemoryContext", ALLOCSET_DEFAULT_SIZES);

     
                                                                             
                                                           
     
  CurrentMemoryContext = TopMemoryContext;

     
                                                                             
                                                                           
                                                                           
                                                                         
                                                                   
                                                                          
                                                                             
                                                                        
     
                                                                             
                                                     
     
  ErrorContext = AllocSetContextCreate(TopMemoryContext, "ErrorContext", 8 * 1024, 8 * 1024, 8 * 1024);
  MemoryContextAllowInCriticalSection(ErrorContext, true);
}

   
                      
                                                                    
                                                            
   
void
MemoryContextReset(MemoryContext context)
{
  AssertArg(MemoryContextIsValid(context));

                                                                       
  if (context->firstchild != NULL)
  {
    MemoryContextDeleteChildren(context);
  }

                                                                      
  if (!context->isReset)
  {
    MemoryContextResetOnly(context);
  }
}

   
                          
                                                  
                                                          
   
void
MemoryContextResetOnly(MemoryContext context)
{
  AssertArg(MemoryContextIsValid(context));

                                                               
  if (!context->isReset)
  {
    MemoryContextCallResetCallbacks(context);

       
                                                                          
                                                                        
                                                                      
                                                                          
                                                                         
                                                                         
                                                      
       

    context->methods->reset(context);
    context->isReset = true;
    VALGRIND_DESTROY_MEMPOOL(context);
    VALGRIND_CREATE_MEMPOOL(context, 0, false);
  }
}

   
                              
                                                                
                                                                 
                           
   
void
MemoryContextResetChildren(MemoryContext context)
{
  MemoryContext child;

  AssertArg(MemoryContextIsValid(context));

  for (child = context->firstchild; child != NULL; child = child->nextchild)
  {
    MemoryContextResetChildren(child);
    MemoryContextResetOnly(child);
  }
}

   
                       
                                                                
                       
   
                                                                         
                                                  
                                                                   
   
void
MemoryContextDelete(MemoryContext context)
{
  AssertArg(MemoryContextIsValid(context));
                                                          
  Assert(context != TopMemoryContext);
                                            
  Assert(context != CurrentMemoryContext);

                                                                       
  if (context->firstchild != NULL)
  {
    MemoryContextDeleteChildren(context);
  }

     
                                                                            
                                                                             
                                                                        
                                   
     
  MemoryContextCallResetCallbacks(context);

     
                                                                          
                                                                           
                                                       
     
  MemoryContextSetParent(context, NULL);

     
                                                                        
                                                                           
                                                                       
     
  context->ident = NULL;

  context->methods->delete_context(context);

  VALGRIND_DESTROY_MEMPOOL(context);
}

   
                               
                                                                    
                                                                       
   
void
MemoryContextDeleteChildren(MemoryContext context)
{
  AssertArg(MemoryContextIsValid(context));

     
                                                                           
                               
     
  while (context->firstchild != NULL)
  {
    MemoryContextDelete(context->firstchild);
  }
}

   
                                      
                                                                       
                                                                    
   
                                                                           
                                                                        
                                                                            
                                                                            
                                                                          
                          
   
                                                                         
                                                                          
                           
   
void
MemoryContextRegisterResetCallback(MemoryContext context, MemoryContextCallback *cb)
{
  AssertArg(MemoryContextIsValid(context));

                                                                       
  cb->next = context->reset_cbs;
  context->reset_cbs = cb;
                                                               
  context->isReset = false;
}

   
                                   
                                                                    
   
static void
MemoryContextCallResetCallbacks(MemoryContext context)
{
  MemoryContextCallback *cb;

     
                                                                         
                                                                             
                                                                    
     
  while ((cb = context->reset_cbs) != NULL)
  {
    context->reset_cbs = cb->next;
    cb->func(cb->arg);
  }
}

   
                              
                                                    
   
                                                                              
                                                                          
                                                                        
                                                                        
                                                                   
   
void
MemoryContextSetIdentifier(MemoryContext context, const char *id)
{
  AssertArg(MemoryContextIsValid(context));
  context->ident = id;
}

   
                          
                                                               
   
                                                                        
                                                                       
                                                                      
                                                                            
                                                                             
                                                               
   
                                                                         
                            
   
                                                                           
                                                                            
                                                                          
   
void
MemoryContextSetParent(MemoryContext context, MemoryContext new_parent)
{
  AssertArg(MemoryContextIsValid(context));
  AssertArg(context != new_parent);

                                                    
  if (new_parent == context->parent)
  {
    return;
  }

                                           
  if (context->parent)
  {
    MemoryContext parent = context->parent;

    if (context->prevchild != NULL)
    {
      context->prevchild->nextchild = context->nextchild;
    }
    else
    {
      Assert(parent->firstchild == context);
      parent->firstchild = context->nextchild;
    }

    if (context->nextchild != NULL)
    {
      context->nextchild->prevchild = context->prevchild;
    }
  }

                  
  if (new_parent)
  {
    AssertArg(MemoryContextIsValid(new_parent));
    context->parent = new_parent;
    context->prevchild = NULL;
    context->nextchild = new_parent->firstchild;
    if (new_parent->firstchild != NULL)
    {
      new_parent->firstchild->prevchild = context;
    }
    new_parent->firstchild = context;
  }
  else
  {
    context->parent = NULL;
    context->prevchild = NULL;
    context->nextchild = NULL;
  }
}

   
                                       
                                                                        
             
   
                                                                           
                                                                         
                                                                            
                                                                           
                                                   
   
void
MemoryContextAllowInCriticalSection(MemoryContext context, bool allow)
{
  AssertArg(MemoryContextIsValid(context));

  context->allowInCritSection = allow;
}

   
                       
                                                                 
                                                            
   
                                                                     
                     
   
Size
GetMemoryChunkSpace(void *pointer)
{
  MemoryContext context = GetMemoryChunkContext(pointer);

  return context->methods->get_chunk_space(context, pointer);
}

   
                          
                                                             
   
MemoryContext
MemoryContextGetParent(MemoryContext context)
{
  AssertArg(MemoryContextIsValid(context));

  return context->parent;
}

   
                        
                                                      
   
bool
MemoryContextIsEmpty(MemoryContext context)
{
  AssertArg(MemoryContextIsValid(context));

     
                                                                            
                                           
     
  if (context->firstchild != NULL)
  {
    return false;
  }
                                               
  return context->methods->is_empty(context);
}

   
                      
                                                                      
   
                                                                             
                                                                               
                                      
   
void
MemoryContextStats(MemoryContext context)
{
                                                                           
  MemoryContextStatsDetail(context, 100);
}

   
                            
   
                                                                               
   
void
MemoryContextStatsDetail(MemoryContext context, int max_children)
{
  MemoryContextCounters grand_totals;

  memset(&grand_totals, 0, sizeof(grand_totals));

  MemoryContextStatsInternal(context, 0, true, max_children, &grand_totals);

  fprintf(stderr, "Grand total: %zu bytes in %zd blocks; %zu free (%zd chunks); %zu used\n", grand_totals.totalspace, grand_totals.nblocks, grand_totals.freespace, grand_totals.freechunks, grand_totals.totalspace - grand_totals.freespace);
}

   
                              
                                               
   
                                                                               
                       
   
static void
MemoryContextStatsInternal(MemoryContext context, int level, bool print, int max_children, MemoryContextCounters *totals)
{
  MemoryContextCounters local_totals;
  MemoryContext child;
  int ichild;

  AssertArg(MemoryContextIsValid(context));

                                  
  context->methods->stats(context, print ? MemoryContextStatsPrint : NULL, (void *)&level, totals);

     
                                                                           
                                                             
     
  memset(&local_totals, 0, sizeof(local_totals));

  for (child = context->firstchild, ichild = 0; child != NULL; child = child->nextchild, ichild++)
  {
    if (ichild < max_children)
    {
      MemoryContextStatsInternal(child, level + 1, print, max_children, totals);
    }
    else
    {
      MemoryContextStatsInternal(child, level + 1, false, max_children, &local_totals);
    }
  }

                                 
  if (ichild > max_children)
  {
    if (print)
    {
      int i;

      for (i = 0; i <= level; i++)
      {
        fprintf(stderr, "  ");
      }
      fprintf(stderr, "%d more child contexts containing %zu total in %zd blocks; %zu free (%zd chunks); %zu used\n", ichild - max_children, local_totals.totalspace, local_totals.nblocks, local_totals.freespace, local_totals.freechunks, local_totals.totalspace - local_totals.freespace);
    }

    if (totals)
    {
      totals->nblocks += local_totals.nblocks;
      totals->freechunks += local_totals.freechunks;
      totals->totalspace += local_totals.totalspace;
      totals->freespace += local_totals.freespace;
    }
  }
}

   
                           
                                                      
   
                                                                            
                               
   
static void
MemoryContextStatsPrint(MemoryContext context, void *passthru, const char *stats_string)
{
  int level = *(int *)passthru;
  const char *name = context->name;
  const char *ident = context->ident;
  int i;

     
                                                                             
                                                                          
                                                                          
     
  if (ident && strcmp(name, "dynahash") == 0)
  {
    name = ident;
    ident = NULL;
  }

  for (i = 0; i < level; i++)
  {
    fprintf(stderr, "  ");
  }
  fprintf(stderr, "%s: %s", name, stats_string);
  if (ident)
  {
       
                                                                         
                                                                      
                                                                           
                                 
       
    int idlen = strlen(ident);
    bool truncated = false;

    if (idlen > 100)
    {
      idlen = pg_mbcliplen(ident, idlen, 100);
      truncated = true;
    }
    fprintf(stderr, ": ");
    while (idlen-- > 0)
    {
      unsigned char c = *ident++;

      if (c < ' ')
      {
        c = ' ';
      }
      fputc(c, stderr);
    }
    if (truncated)
    {
      fprintf(stderr, "...");
    }
  }
  fputc('\n', stderr);
}

   
                      
                                           
   
                                                        
   
#ifdef MEMORY_CONTEXT_CHECKING
void
MemoryContextCheck(MemoryContext context)
{
  MemoryContext child;

  AssertArg(MemoryContextIsValid(context));

  context->methods->check(context);
  for (child = context->firstchild; child != NULL; child = child->nextchild)
  {
    MemoryContextCheck(child);
  }
}
#endif

   
                         
                                                                   
                    
   
                                                                     
                                                                         
                                                                      
                                                                         
                                   
   
bool
MemoryContextContains(MemoryContext context, void *pointer)
{
  MemoryContext ptr_context;

     
                                                                           
                                                                     
                                      
     
                                                                      
                                                                      
                      
     
  if (pointer == NULL || pointer != (void *)MAXALIGN(pointer))
  {
    return false;
  }

     
                                                    
     
  ptr_context = *(MemoryContext *)(((char *)pointer) - sizeof(void *));

  return ptr_context == context;
}

   
                       
                                                       
   
                                                               
                                                          
   
                                                         
                                                                          
                                                                 
                                          
                                                                        
                                                                       
                                                                    
                                                                      
                                                                     
                                                                           
                                                                          
                                                                      
                                                                     
                                                                 
                                                              
   
                                                                          
                                                          
                                                                          
                                                                        
                                                         
   
                                                                          
                                                    
   
void
MemoryContextCreate(MemoryContext node, NodeTag tag, const MemoryContextMethods *methods, MemoryContext parent, const char *name)
{
                                                                         
  Assert(CritSectionCount == 0);

                                                               
  node->type = tag;
  node->isReset = true;
  node->methods = methods;
  node->parent = parent;
  node->firstchild = NULL;
  node->prevchild = NULL;
  node->name = name;
  node->ident = NULL;
  node->reset_cbs = NULL;

                                         
  if (parent)
  {
    node->nextchild = parent->firstchild;
    if (parent->firstchild != NULL)
    {
      parent->firstchild->prevchild = node;
    }
    parent->firstchild = node;
                                                     
    node->allowInCritSection = parent->allowInCritSection;
  }
  else
  {
    node->nextchild = NULL;
    node->allowInCritSection = false;
  }

  VALGRIND_CREATE_MEMPOOL(node, 0, false);
}

   
                      
                                                 
   
                                                              
                                                            
   
void *
MemoryContextAlloc(MemoryContext context, Size size)
{
  void *ret;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (!AllocSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);

       
                                                                        
                                                                           
                                                                         
                               
       
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  return ret;
}

   
                          
                                                         
   
                                                                         
                                                                       
   
void *
MemoryContextAllocZero(MemoryContext context, Size size)
{
  void *ret;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (!AllocSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  MemSetAligned(ret, 0, size);

  return ret;
}

   
                                 
                                                                   
   
                                                                      
                                                        
   
void *
MemoryContextAllocZeroAligned(MemoryContext context, Size size)
{
  void *ret;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (!AllocSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  MemSetLoop(ret, 0, size);

  return ret;
}

   
                              
                                                                       
   
void *
MemoryContextAllocExtended(MemoryContext context, Size size, int flags)
{
  void *ret;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (((flags & MCXT_ALLOC_HUGE) != 0 && !AllocHugeSizeIsValid(size)) || ((flags & MCXT_ALLOC_HUGE) == 0 && !AllocSizeIsValid(size)))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    if ((flags & MCXT_ALLOC_NO_OOM) == 0)
    {
      MemoryContextStats(TopMemoryContext);
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
    }
    return NULL;
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  if ((flags & MCXT_ALLOC_ZERO) != 0)
  {
    MemSetAligned(ret, 0, size);
  }

  return ret;
}

void *
palloc(Size size)
{
                                                                 
  void *ret;
  MemoryContext context = CurrentMemoryContext;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (!AllocSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  return ret;
}

void *
palloc0(Size size)
{
                                                                     
  void *ret;
  MemoryContext context = CurrentMemoryContext;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (!AllocSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  MemSetAligned(ret, 0, size);

  return ret;
}

void *
palloc_extended(Size size, int flags)
{
                                                                         
  void *ret;
  MemoryContext context = CurrentMemoryContext;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (((flags & MCXT_ALLOC_HUGE) != 0 && !AllocHugeSizeIsValid(size)) || ((flags & MCXT_ALLOC_HUGE) == 0 && !AllocSizeIsValid(size)))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    if ((flags & MCXT_ALLOC_NO_OOM) == 0)
    {
      MemoryContextStats(TopMemoryContext);
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
    }
    return NULL;
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  if ((flags & MCXT_ALLOC_ZERO) != 0)
  {
    MemSetAligned(ret, 0, size);
  }

  return ret;
}

   
         
                                
   
void
pfree(void *pointer)
{
  MemoryContext context = GetMemoryChunkContext(pointer);

  context->methods->free_p(context, pointer);
  VALGRIND_MEMPOOL_FREE(context, pointer);
}

   
            
                                                     
   
void *
repalloc(void *pointer, Size size)
{
  MemoryContext context = GetMemoryChunkContext(pointer);
  void *ret;

  if (!AllocSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  AssertNotInCriticalSection(context);

                                     
  Assert(!context->isReset);

  ret = context->methods->realloc(context, pointer, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_CHANGE(context, pointer, ret, size);

  return ret;
}

   
                          
                                                                      
   
                                                      
   
void *
MemoryContextAllocHuge(MemoryContext context, Size size)
{
  void *ret;

  AssertArg(MemoryContextIsValid(context));
  AssertNotInCriticalSection(context);

  if (!AllocHugeSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  context->isReset = false;

  ret = context->methods->alloc(context, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_ALLOC(context, ret, size);

  return ret;
}

   
                 
                                                                        
                                                               
   
void *
repalloc_huge(void *pointer, Size size)
{
  MemoryContext context = GetMemoryChunkContext(pointer);
  void *ret;

  if (!AllocHugeSizeIsValid(size))
  {
    elog(ERROR, "invalid memory alloc request size %zu", size);
  }

  AssertNotInCriticalSection(context);

                                     
  Assert(!context->isReset);

  ret = context->methods->realloc(context, pointer, size);
  if (unlikely(ret == NULL))
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on request of size %zu in memory context \"%s\".", size, context->name)));
  }

  VALGRIND_MEMPOOL_CHANGE(context, pointer, ret, size);

  return ret;
}

   
                       
                                                           
   
char *
MemoryContextStrdup(MemoryContext context, const char *string)
{
  char *nstr;
  Size len = strlen(string) + 1;

  nstr = (char *)MemoryContextAlloc(context, len);

  memcpy(nstr, string, len);

  return nstr;
}

char *
pstrdup(const char *in)
{
  return MemoryContextStrdup(CurrentMemoryContext, in);
}

   
            
                                              
                                                  
   
char *
pnstrdup(const char *in, Size len)
{
  char *out;

  len = strnlen(in, len);

  out = palloc(len + 1);
  memcpy(out, in, len);
  out[len] = '\0';

  return out;
}

   
                                                                     
   
char *
pchomp(const char *in)
{
  size_t n;

  n = strlen(in);
  while (n > 0 && in[n - 1] == '\n')
  {
    n--;
  }
  return pnstrdup(in, n);
}
