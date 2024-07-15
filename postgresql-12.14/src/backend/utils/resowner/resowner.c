                                                                            
   
              
                                              
   
                                                                 
                                                                         
                                                    
                                            
   
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "jit/jit.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "utils/hashutils.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/snapmgr.h"

   
                                                                           
                                                                
   
                                                                          
               
   
#define FileGetDatum(file) Int32GetDatum(file)
#define DatumGetFile(datum) ((File)DatumGetInt32(datum))
#define BufferGetDatum(buffer) Int32GetDatum(buffer)
#define DatumGetBuffer(datum) ((Buffer)DatumGetInt32(datum))

   
                                                                              
   
                                                                           
                                                                        
   
                                                                          
                                                                   
                                                                             
                                                                        
                                                                           
                                                                
   
                                                                            
                                                                          
   
typedef struct ResourceArray
{
  Datum *itemsarr;                                 
  Datum invalidval;                                       
  uint32 capacity;                                      
  uint32 nitems;                                                  
  uint32 maxitems;                                                
  uint32 lastidx;                                              
} ResourceArray;

   
                                                                            
                                                  
   
#define RESARRAY_INIT_SIZE 16

   
                                                                        
   
#define RESARRAY_MAX_ARRAY 64
#define RESARRAY_IS_ARRAY(resarr) ((resarr)->capacity <= RESARRAY_MAX_ARRAY)

   
                                                                       
                                                
   
#define RESARRAY_MAX_ITEMS(capacity) ((capacity) <= RESARRAY_MAX_ARRAY ? (capacity) : (capacity) / 4 * 3)

   
                                                                            
                                                                           
                                                                           
                                                                          
                                                                               
   
                                                                        
                                                                           
                                                                           
                                                                          
                                                                            
                                                                               
                                                                            
                                                                               
   
#define MAX_RESOWNER_LOCKS 15

   
                                        
   
typedef struct ResourceOwnerData
{
  ResourceOwner parent;                                             
  ResourceOwner firstchild;                                      
  ResourceOwner nextchild;                                 
  const char *name;                                        

                                                 
  ResourceArray bufferarr;                        
  ResourceArray catrefarr;                              
  ResourceArray catlistrefarr;                         
  ResourceArray relrefarr;                              
  ResourceArray planrefarr;                              
  ResourceArray tupdescarr;                            
  ResourceArray snapshotarr;                            
  ResourceArray filearr;                                 
  ResourceArray dsmarr;                                    
  ResourceArray jitarr;                          

                                                                           
  int nlocks;                                                      
  LOCALLOCK *locks[MAX_RESOWNER_LOCKS];                          
} ResourceOwnerData;

                                                                               
                                    
                                                                               

ResourceOwner CurrentResourceOwner = NULL;
ResourceOwner CurTransactionResourceOwner = NULL;
ResourceOwner TopTransactionResourceOwner = NULL;
ResourceOwner AuxProcessResourceOwner = NULL;

   
                                                   
   
typedef struct ResourceReleaseCallbackItem
{
  struct ResourceReleaseCallbackItem *next;
  ResourceReleaseCallback callback;
  void *arg;
} ResourceReleaseCallbackItem;

static ResourceReleaseCallbackItem *ResourceRelease_callbacks = NULL;

                       
static void
ResourceArrayInit(ResourceArray *resarr, Datum invalidval);
static void
ResourceArrayEnlarge(ResourceArray *resarr);
static void
ResourceArrayAdd(ResourceArray *resarr, Datum value);
static bool
ResourceArrayRemove(ResourceArray *resarr, Datum value);
static bool
ResourceArrayGetAny(ResourceArray *resarr, Datum *value);
static void
ResourceArrayFree(ResourceArray *resarr);
static void
ResourceOwnerReleaseInternal(ResourceOwner owner, ResourceReleasePhase phase, bool isCommit, bool isTopLevel);
static void
ReleaseAuxProcessResourcesCallback(int code, Datum arg);
static void
PrintRelCacheLeakWarning(Relation rel);
static void
PrintPlanCacheLeakWarning(CachedPlan *plan);
static void
PrintTupleDescLeakWarning(TupleDesc tupdesc);
static void
PrintSnapshotLeakWarning(Snapshot snapshot);
static void
PrintFileLeakWarning(File file);
static void
PrintDSMLeakWarning(dsm_segment *seg);

                                                                               
                                       
                                                                               

   
                              
   
static void
ResourceArrayInit(ResourceArray *resarr, Datum invalidval)
{
                         
  Assert(resarr->itemsarr == NULL);
  Assert(resarr->capacity == 0);
  Assert(resarr->nitems == 0);
  Assert(resarr->maxitems == 0);
                                                
  resarr->invalidval = invalidval;
                                                  
}

   
                                                                       
   
                                                                             
                                                                      
   
static void
ResourceArrayEnlarge(ResourceArray *resarr)
{
  uint32 i, oldcap, newcap;
  Datum *olditemsarr;
  Datum *newitemsarr;

  if (resarr->nitems < resarr->maxitems)
  {
    return;                     
  }

  olditemsarr = resarr->itemsarr;
  oldcap = resarr->capacity;

                                                                           
  newcap = (oldcap > 0) ? oldcap * 2 : RESARRAY_INIT_SIZE;
  newitemsarr = (Datum *)MemoryContextAlloc(TopMemoryContext, newcap * sizeof(Datum));
  for (i = 0; i < newcap; i++)
  {
    newitemsarr[i] = resarr->invalidval;
  }

                                                                             
  resarr->itemsarr = newitemsarr;
  resarr->capacity = newcap;
  resarr->maxitems = RESARRAY_MAX_ITEMS(newcap);
  resarr->nitems = 0;

  if (olditemsarr != NULL)
  {
       
                                                                        
                                                                          
                                                                           
                                                                       
                                                                    
       
    for (i = 0; i < oldcap; i++)
    {
      if (olditemsarr[i] != resarr->invalidval)
      {
        ResourceArrayAdd(resarr, olditemsarr[i]);
      }
    }

                                
    pfree(olditemsarr);
  }

  Assert(resarr->nitems < resarr->maxitems);
}

   
                                   
   
                                                           
   
static void
ResourceArrayAdd(ResourceArray *resarr, Datum value)
{
  uint32 idx;

  Assert(value != resarr->invalidval);
  Assert(resarr->nitems < resarr->maxitems);

  if (RESARRAY_IS_ARRAY(resarr))
  {
                                 
    idx = resarr->nitems;
  }
  else
  {
                                                                
    uint32 mask = resarr->capacity - 1;

    idx = DatumGetUInt32(hash_any((void *)&value, sizeof(value))) & mask;
    for (;;)
    {
      if (resarr->itemsarr[idx] == resarr->invalidval)
      {
        break;
      }
      idx = (idx + 1) & mask;
    }
  }
  resarr->lastidx = idx;
  resarr->itemsarr[idx] = value;
  resarr->nitems++;
}

   
                                        
   
                                                             
   
                                                                              
   
static bool
ResourceArrayRemove(ResourceArray *resarr, Datum value)
{
  uint32 i, idx, lastidx = resarr->lastidx;

  Assert(value != resarr->invalidval);

                                                        
  if (RESARRAY_IS_ARRAY(resarr))
  {
    if (lastidx < resarr->nitems && resarr->itemsarr[lastidx] == value)
    {
      resarr->itemsarr[lastidx] = resarr->itemsarr[resarr->nitems - 1];
      resarr->nitems--;
                                                               
      resarr->lastidx = resarr->nitems - 1;
      return true;
    }
    for (i = 0; i < resarr->nitems; i++)
    {
      if (resarr->itemsarr[i] == value)
      {
        resarr->itemsarr[i] = resarr->itemsarr[resarr->nitems - 1];
        resarr->nitems--;
                                                                 
        resarr->lastidx = resarr->nitems - 1;
        return true;
      }
    }
  }
  else
  {
    uint32 mask = resarr->capacity - 1;

    if (lastidx < resarr->capacity && resarr->itemsarr[lastidx] == value)
    {
      resarr->itemsarr[lastidx] = resarr->invalidval;
      resarr->nitems--;
      return true;
    }
    idx = DatumGetUInt32(hash_any((void *)&value, sizeof(value))) & mask;
    for (i = 0; i < resarr->capacity; i++)
    {
      if (resarr->itemsarr[idx] == value)
      {
        resarr->itemsarr[idx] = resarr->invalidval;
        resarr->nitems--;
        return true;
      }
      idx = (idx + 1) & mask;
    }
  }

  return false;
}

   
                                                
   
                                                                        
                                                                            
                                                                           
   
                                                                        
   
static bool
ResourceArrayGetAny(ResourceArray *resarr, Datum *value)
{
  if (resarr->nitems == 0)
  {
    return false;
  }

  if (RESARRAY_IS_ARRAY(resarr))
  {
                                                      
    resarr->lastidx = 0;
  }
  else
  {
                                                          
    uint32 mask = resarr->capacity - 1;

    for (;;)
    {
      resarr->lastidx &= mask;
      if (resarr->itemsarr[resarr->lastidx] != resarr->invalidval)
      {
        break;
      }
      resarr->lastidx++;
    }
  }

  *value = resarr->itemsarr[resarr->lastidx];
  return true;
}

   
                                                                    
   
static void
ResourceArrayFree(ResourceArray *resarr)
{
  if (resarr->itemsarr)
  {
    pfree(resarr->itemsarr);
  }
}

                                                                               
                                       
                                                                               

   
                       
                                   
   
                                                                             
                             
   
ResourceOwner
ResourceOwnerCreate(ResourceOwner parent, const char *name)
{
  ResourceOwner owner;

  owner = (ResourceOwner)MemoryContextAllocZero(TopMemoryContext, sizeof(ResourceOwnerData));
  owner->name = name;

  if (parent)
  {
    owner->parent = parent;
    owner->nextchild = parent->firstchild;
    parent->firstchild = owner;
  }

  ResourceArrayInit(&(owner->bufferarr), BufferGetDatum(InvalidBuffer));
  ResourceArrayInit(&(owner->catrefarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->catlistrefarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->relrefarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->planrefarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->tupdescarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->snapshotarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->filearr), FileGetDatum(-1));
  ResourceArrayInit(&(owner->dsmarr), PointerGetDatum(NULL));
  ResourceArrayInit(&(owner->jitarr), PointerGetDatum(NULL));

  return owner;
}

   
                        
                                                                        
                                                   
   
                                                                       
                                                                         
                                                                      
                                                                            
                             
   
                                   
                                                                       
                            
                                                                 
   
                                                                           
                                                                             
                                                                      
                                                                           
                                
   
                                                                          
                                                                         
                                                                    
   
void
ResourceOwnerRelease(ResourceOwner owner, ResourceReleasePhase phase, bool isCommit, bool isTopLevel)
{
                                                               
  ResourceOwnerReleaseInternal(owner, phase, isCommit, isTopLevel);
}

static void
ResourceOwnerReleaseInternal(ResourceOwner owner, ResourceReleasePhase phase, bool isCommit, bool isTopLevel)
{
  ResourceOwner child;
  ResourceOwner save;
  ResourceReleaseCallbackItem *item;
  Datum foundres;

                                     
  for (child = owner->firstchild; child != NULL; child = child->nextchild)
  {
    ResourceOwnerReleaseInternal(child, phase, isCommit, isTopLevel);
  }

     
                                                                            
                   
     
  save = CurrentResourceOwner;
  CurrentResourceOwner = owner;

  if (phase == RESOURCE_RELEASE_BEFORE_LOCKS)
  {
       
                                                                     
                                                                          
                 
       
                                                                       
                                                                        
                                                                  
       
    while (ResourceArrayGetAny(&(owner->bufferarr), &foundres))
    {
      Buffer res = DatumGetBuffer(foundres);

      if (isCommit)
      {
        PrintBufferLeakWarning(res);
      }
      ReleaseBuffer(res);
    }

                                       
    while (ResourceArrayGetAny(&(owner->relrefarr), &foundres))
    {
      Relation res = (Relation)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintRelCacheLeakWarning(res);
      }
      RelationClose(res);
    }

                                                  
    while (ResourceArrayGetAny(&(owner->dsmarr), &foundres))
    {
      dsm_segment *res = (dsm_segment *)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintDSMLeakWarning(res);
      }
      dsm_detach(res);
    }

                                
    while (ResourceArrayGetAny(&(owner->jitarr), &foundres))
    {
      JitContext *context = (JitContext *)PointerGetDatum(foundres);

      jit_release_context(context);
    }
  }
  else if (phase == RESOURCE_RELEASE_LOCKS)
  {
    if (isTopLevel)
    {
         
                                                                       
                                                                        
                                   
         
      if (owner == TopTransactionResourceOwner)
      {
        ProcReleaseLocks(isCommit);
        ReleasePredicateLocks(isCommit, false);
      }
    }
    else
    {
         
                                                                 
                                                                       
                             
         
      LOCALLOCK **locks;
      int nlocks;

      Assert(owner->parent != NULL);

         
                                                                         
                                            
         
      if (owner->nlocks > MAX_RESOWNER_LOCKS)
      {
        locks = NULL;
        nlocks = 0;
      }
      else
      {
        locks = owner->locks;
        nlocks = owner->nlocks;
      }

      if (isCommit)
      {
        LockReassignCurrentOwner(locks, nlocks);
      }
      else
      {
        LockReleaseCurrentOwner(locks, nlocks);
      }
    }
  }
  else if (phase == RESOURCE_RELEASE_AFTER_LOCKS)
  {
       
                                                                           
                                                                        
                       
       
                                                                 
       
    while (ResourceArrayGetAny(&(owner->catrefarr), &foundres))
    {
      HeapTuple res = (HeapTuple)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintCatCacheLeakWarning(res);
      }
      ReleaseCatCache(res);
    }

                                  
    while (ResourceArrayGetAny(&(owner->catlistrefarr), &foundres))
    {
      CatCList *res = (CatCList *)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintCatCacheListLeakWarning(res);
      }
      ReleaseCatCacheList(res);
    }

                                        
    while (ResourceArrayGetAny(&(owner->planrefarr), &foundres))
    {
      CachedPlan *res = (CachedPlan *)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintPlanCacheLeakWarning(res);
      }
      ReleaseCachedPlan(res, true);
    }

                                      
    while (ResourceArrayGetAny(&(owner->tupdescarr), &foundres))
    {
      TupleDesc res = (TupleDesc)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintTupleDescLeakWarning(res);
      }
      DecrTupleDescRefCount(res);
    }

                                       
    while (ResourceArrayGetAny(&(owner->snapshotarr), &foundres))
    {
      Snapshot res = (Snapshot)DatumGetPointer(foundres);

      if (isCommit)
      {
        PrintSnapshotLeakWarning(res);
      }
      UnregisterSnapshot(res);
    }

                                   
    while (ResourceArrayGetAny(&(owner->filearr), &foundres))
    {
      File res = DatumGetFile(foundres);

      if (isCommit)
      {
        PrintFileLeakWarning(res);
      }
      FileClose(res);
    }
  }

                                           
  for (item = ResourceRelease_callbacks; item; item = item->next)
  {
    item->callback(phase, isCommit, isTopLevel, item->arg);
  }

  CurrentResourceOwner = save;
}

   
                       
                                                
   
                                                                           
   
void
ResourceOwnerDelete(ResourceOwner owner)
{
                                                              
  Assert(owner != CurrentResourceOwner);

                                                   
  Assert(owner->bufferarr.nitems == 0);
  Assert(owner->catrefarr.nitems == 0);
  Assert(owner->catlistrefarr.nitems == 0);
  Assert(owner->relrefarr.nitems == 0);
  Assert(owner->planrefarr.nitems == 0);
  Assert(owner->tupdescarr.nitems == 0);
  Assert(owner->snapshotarr.nitems == 0);
  Assert(owner->filearr.nitems == 0);
  Assert(owner->dsmarr.nitems == 0);
  Assert(owner->jitarr.nitems == 0);
  Assert(owner->nlocks == 0 || owner->nlocks == MAX_RESOWNER_LOCKS + 1);

     
                                                                            
                                               
     
  while (owner->firstchild != NULL)
  {
    ResourceOwnerDelete(owner->firstchild);
  }

     
                                                                        
                                                                            
                                                  
     
  ResourceOwnerNewParent(owner, NULL);

                            
  ResourceArrayFree(&(owner->bufferarr));
  ResourceArrayFree(&(owner->catrefarr));
  ResourceArrayFree(&(owner->catlistrefarr));
  ResourceArrayFree(&(owner->relrefarr));
  ResourceArrayFree(&(owner->planrefarr));
  ResourceArrayFree(&(owner->tupdescarr));
  ResourceArrayFree(&(owner->snapshotarr));
  ResourceArrayFree(&(owner->filearr));
  ResourceArrayFree(&(owner->dsmarr));
  ResourceArrayFree(&(owner->jitarr));

  pfree(owner);
}

   
                                                                     
   
ResourceOwner
ResourceOwnerGetParent(ResourceOwner owner)
{
  return owner->parent;
}

   
                                                 
   
void
ResourceOwnerNewParent(ResourceOwner owner, ResourceOwner newparent)
{
  ResourceOwner oldparent = owner->parent;

  if (oldparent)
  {
    if (owner == oldparent->firstchild)
    {
      oldparent->firstchild = owner->nextchild;
    }
    else
    {
      ResourceOwner child;

      for (child = oldparent->firstchild; child; child = child->nextchild)
      {
        if (owner == child->nextchild)
        {
          child->nextchild = owner->nextchild;
          break;
        }
      }
    }
  }

  if (newparent)
  {
    Assert(owner != newparent);
    owner->parent = newparent;
    owner->nextchild = newparent->firstchild;
    newparent->firstchild = owner;
  }
  else
  {
    owner->parent = NULL;
    owner->nextchild = NULL;
  }
}

   
                                                                  
   
                                                                       
                                                                          
   
                                                                            
                                              
   
void
RegisterResourceReleaseCallback(ResourceReleaseCallback callback, void *arg)
{
  ResourceReleaseCallbackItem *item;

  item = (ResourceReleaseCallbackItem *)MemoryContextAlloc(TopMemoryContext, sizeof(ResourceReleaseCallbackItem));
  item->callback = callback;
  item->arg = arg;
  item->next = ResourceRelease_callbacks;
  ResourceRelease_callbacks = item;
}

void
UnregisterResourceReleaseCallback(ResourceReleaseCallback callback, void *arg)
{
  ResourceReleaseCallbackItem *item;
  ResourceReleaseCallbackItem *prev;

  prev = NULL;
  for (item = ResourceRelease_callbacks; item; prev = item, item = item->next)
  {
    if (item->callback == callback && item->arg == arg)
    {
      if (prev)
      {
        prev->next = item->next;
      }
      else
      {
        ResourceRelease_callbacks = item->next;
      }
      pfree(item);
      break;
    }
  }
}

   
                                                                 
   
void
CreateAuxProcessResourceOwner(void)
{
  Assert(AuxProcessResourceOwner == NULL);
  Assert(CurrentResourceOwner == NULL);
  AuxProcessResourceOwner = ResourceOwnerCreate(NULL, "AuxiliaryProcess");
  CurrentResourceOwner = AuxProcessResourceOwner;

     
                                                                        
                                                            
     
  on_shmem_exit(ReleaseAuxProcessResourcesCallback, 0);
}

   
                                                           
                                                                      
                                                    
   
void
ReleaseAuxProcessResources(bool isCommit)
{
     
                                                                         
                                                                   
     
  ResourceOwnerRelease(AuxProcessResourceOwner, RESOURCE_RELEASE_BEFORE_LOCKS, isCommit, true);
  ResourceOwnerRelease(AuxProcessResourceOwner, RESOURCE_RELEASE_LOCKS, isCommit, true);
  ResourceOwnerRelease(AuxProcessResourceOwner, RESOURCE_RELEASE_AFTER_LOCKS, isCommit, true);
}

   
                                     
                                                                         
   
static void
ReleaseAuxProcessResourcesCallback(int code, Datum arg)
{
  bool isCommit = (code == 0);

  ReleaseAuxProcessResources(isCommit);
}

   
                                                                            
                 
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeBuffers(ResourceOwner owner)
{
                                                                        
  Assert(owner != NULL);
  ResourceArrayEnlarge(&(owner->bufferarr));
}

   
                                                          
   
                                                                  
   
void
ResourceOwnerRememberBuffer(ResourceOwner owner, Buffer buffer)
{
  ResourceArrayAdd(&(owner->bufferarr), BufferGetDatum(buffer));
}

   
                                                        
   
void
ResourceOwnerForgetBuffer(ResourceOwner owner, Buffer buffer)
{
  if (!ResourceArrayRemove(&(owner->bufferarr), BufferGetDatum(buffer)))
  {
    elog(ERROR, "buffer %d is not owned by resource owner %s", buffer, owner->name);
  }
}

   
                                                          
   
                                                                           
                                                                              
                                                                                
                                                                             
                                                                              
              
   
void
ResourceOwnerRememberLock(ResourceOwner owner, LOCALLOCK *locallock)
{
  Assert(locallock != NULL);

  if (owner->nlocks > MAX_RESOWNER_LOCKS)
  {
    return;                                 
  }

  if (owner->nlocks < MAX_RESOWNER_LOCKS)
  {
    owner->locks[owner->nlocks] = locallock;
  }
  else
  {
                    
  }
  owner->nlocks++;
}

   
                                                        
   
void
ResourceOwnerForgetLock(ResourceOwner owner, LOCALLOCK *locallock)
{
  int i;

  if (owner->nlocks > MAX_RESOWNER_LOCKS)
  {
    return;                         
  }

  Assert(owner->nlocks > 0);
  for (i = owner->nlocks - 1; i >= 0; i--)
  {
    if (locallock == owner->locks[i])
    {
      owner->locks[i] = owner->locks[owner->nlocks - 1];
      owner->nlocks--;
      return;
    }
  }
  elog(ERROR, "lock reference %p is not owned by resource owner %s", locallock, owner->name);
}

   
                                                                            
                             
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeCatCacheRefs(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->catrefarr));
}

   
                                                                  
   
                                                                       
   
void
ResourceOwnerRememberCatCacheRef(ResourceOwner owner, HeapTuple tuple)
{
  ResourceArrayAdd(&(owner->catrefarr), PointerGetDatum(tuple));
}

   
                                                                
   
void
ResourceOwnerForgetCatCacheRef(ResourceOwner owner, HeapTuple tuple)
{
  if (!ResourceArrayRemove(&(owner->catrefarr), PointerGetDatum(tuple)))
  {
    elog(ERROR, "catcache reference %p is not owned by resource owner %s", tuple, owner->name);
  }
}

   
                                                                            
                                  
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeCatCacheListRefs(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->catlistrefarr));
}

   
                                                                       
   
                                                                           
   
void
ResourceOwnerRememberCatCacheListRef(ResourceOwner owner, CatCList *list)
{
  ResourceArrayAdd(&(owner->catlistrefarr), PointerGetDatum(list));
}

   
                                                                     
   
void
ResourceOwnerForgetCatCacheListRef(ResourceOwner owner, CatCList *list)
{
  if (!ResourceArrayRemove(&(owner->catlistrefarr), PointerGetDatum(list)))
  {
    elog(ERROR, "catcache list reference %p is not owned by resource owner %s", list, owner->name);
  }
}

   
                                                                            
                             
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeRelationRefs(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->relrefarr));
}

   
                                                                  
   
                                                                       
   
void
ResourceOwnerRememberRelationRef(ResourceOwner owner, Relation rel)
{
  ResourceArrayAdd(&(owner->relrefarr), PointerGetDatum(rel));
}

   
                                                                
   
void
ResourceOwnerForgetRelationRef(ResourceOwner owner, Relation rel)
{
  if (!ResourceArrayRemove(&(owner->relrefarr), PointerGetDatum(rel)))
  {
    elog(ERROR, "relcache reference %s is not owned by resource owner %s", RelationGetRelationName(rel), owner->name);
  }
}

   
                        
   
static void
PrintRelCacheLeakWarning(Relation rel)
{
  elog(WARNING, "relcache reference leak: relation \"%s\" not closed", RelationGetRelationName(rel));
}

   
                                                                            
                              
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargePlanCacheRefs(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->planrefarr));
}

   
                                                                   
   
                                                                        
   
void
ResourceOwnerRememberPlanCacheRef(ResourceOwner owner, CachedPlan *plan)
{
  ResourceArrayAdd(&(owner->planrefarr), PointerGetDatum(plan));
}

   
                                                                 
   
void
ResourceOwnerForgetPlanCacheRef(ResourceOwner owner, CachedPlan *plan)
{
  if (!ResourceArrayRemove(&(owner->planrefarr), PointerGetDatum(plan)))
  {
    elog(ERROR, "plancache reference %p is not owned by resource owner %s", plan, owner->name);
  }
}

   
                        
   
static void
PrintPlanCacheLeakWarning(CachedPlan *plan)
{
  elog(WARNING, "plancache reference leak: plan %p not closed", plan);
}

   
                                                                            
                            
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeTupleDescs(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->tupdescarr));
}

   
                                                                 
   
                                                                     
   
void
ResourceOwnerRememberTupleDesc(ResourceOwner owner, TupleDesc tupdesc)
{
  ResourceArrayAdd(&(owner->tupdescarr), PointerGetDatum(tupdesc));
}

   
                                                               
   
void
ResourceOwnerForgetTupleDesc(ResourceOwner owner, TupleDesc tupdesc)
{
  if (!ResourceArrayRemove(&(owner->tupdescarr), PointerGetDatum(tupdesc)))
  {
    elog(ERROR, "tupdesc reference %p is not owned by resource owner %s", tupdesc, owner->name);
  }
}

   
                        
   
static void
PrintTupleDescLeakWarning(TupleDesc tupdesc)
{
  elog(WARNING, "TupleDesc reference leak: TupleDesc %p (%u,%d) still referenced", tupdesc, tupdesc->tdtypeid, tupdesc->tdtypmod);
}

   
                                                                            
                             
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeSnapshots(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->snapshotarr));
}

   
                                                                  
   
                                                                    
   
void
ResourceOwnerRememberSnapshot(ResourceOwner owner, Snapshot snapshot)
{
  ResourceArrayAdd(&(owner->snapshotarr), PointerGetDatum(snapshot));
}

   
                                                                
   
void
ResourceOwnerForgetSnapshot(ResourceOwner owner, Snapshot snapshot)
{
  if (!ResourceArrayRemove(&(owner->snapshotarr), PointerGetDatum(snapshot)))
  {
    elog(ERROR, "snapshot reference %p is not owned by resource owner %s", snapshot, owner->name);
  }
}

   
                        
   
static void
PrintSnapshotLeakWarning(Snapshot snapshot)
{
  elog(WARNING, "Snapshot reference leak: Snapshot %p still referenced", snapshot);
}

   
                                                                            
                          
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeFiles(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->filearr));
}

   
                                                              
   
                                                                
   
void
ResourceOwnerRememberFile(ResourceOwner owner, File file)
{
  ResourceArrayAdd(&(owner->filearr), FileGetDatum(file));
}

   
                                                            
   
void
ResourceOwnerForgetFile(ResourceOwner owner, File file)
{
  if (!ResourceArrayRemove(&(owner->filearr), FileGetDatum(file)))
  {
    elog(ERROR, "temporary file %d is not owned by resource owner %s", file, owner->name);
  }
}

   
                        
   
static void
PrintFileLeakWarning(File file)
{
  elog(WARNING, "temporary file leak: File %d still referenced", file);
}

   
                                                                            
                                          
   
                                                                           
                                                                      
   
void
ResourceOwnerEnlargeDSMs(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->dsmarr));
}

   
                                                                     
   
                                                               
   
void
ResourceOwnerRememberDSM(ResourceOwner owner, dsm_segment *seg)
{
  ResourceArrayAdd(&(owner->dsmarr), PointerGetDatum(seg));
}

   
                                                                   
   
void
ResourceOwnerForgetDSM(ResourceOwner owner, dsm_segment *seg)
{
  if (!ResourceArrayRemove(&(owner->dsmarr), PointerGetDatum(seg)))
  {
    elog(ERROR, "dynamic shared memory segment %u is not owned by resource owner %s", dsm_segment_handle(seg), owner->name);
  }
}

   
                        
   
static void
PrintDSMLeakWarning(dsm_segment *seg)
{
  elog(WARNING, "dynamic shared memory leak: segment %u still referenced", dsm_segment_handle(seg));
}

   
                                                                            
                                
   
                                                                              
                                                                   
   
void
ResourceOwnerEnlargeJIT(ResourceOwner owner)
{
  ResourceArrayEnlarge(&(owner->jitarr));
}

   
                                                           
   
                                                              
   
void
ResourceOwnerRememberJIT(ResourceOwner owner, Datum handle)
{
  ResourceArrayAdd(&(owner->jitarr), handle);
}

   
                                                         
   
void
ResourceOwnerForgetJIT(ResourceOwner owner, Datum handle)
{
  if (!ResourceArrayRemove(&(owner->jitarr), handle))
  {
    elog(ERROR, "JIT context %p is not owned by resource owner %s", DatumGetPointer(handle), owner->name);
  }
}
