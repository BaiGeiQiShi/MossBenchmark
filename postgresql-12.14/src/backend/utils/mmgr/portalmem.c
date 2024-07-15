                                                                            
   
               
                                      
   
                                                                    
                                                                       
                                               
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/portalcmds.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

   
                                                                     
                                                                          
                                                                        
                                                                     
                                                            
   
#define PORTALS_PER_USER 16

                    
                 
                    
   

#define MAX_PORTALNAME_LEN NAMEDATALEN

typedef struct portalhashent
{
  char portalname[MAX_PORTALNAME_LEN];
  Portal portal;
} PortalHashEnt;

static HTAB *PortalHashTable = NULL;

#define PortalHashTableLookup(NAME, PORTAL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    PortalHashEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    hentry = (PortalHashEnt *)hash_search(PortalHashTable, (NAME), HASH_FIND, NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (hentry)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      PORTAL = hentry->portal;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      PORTAL = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

#define PortalHashTableInsert(PORTAL, NAME)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    PortalHashEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    bool found;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    hentry = (PortalHashEnt *)hash_search(PortalHashTable, (NAME), HASH_ENTER, &found);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    if (found)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      elog(ERROR, "duplicate portal name");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    hentry->portal = PORTAL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    PORTAL->name = hentry->portalname;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

#define PortalHashTableDelete(PORTAL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    PortalHashEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    hentry = (PortalHashEnt *)hash_search(PortalHashTable, PORTAL->name, HASH_REMOVE, NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (hentry == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      elog(WARNING, "trying to delete portal name that does not exist");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)

static MemoryContext TopPortalContext = NULL;

                                                                    
                                           
                                                                    
   

   
                       
                                                             
   
void
EnablePortalManager(void)
{
  HASHCTL ctl;

  Assert(TopPortalContext == NULL);

  TopPortalContext = AllocSetContextCreate(TopMemoryContext, "TopPortalContext", ALLOCSET_DEFAULT_SIZES);

  ctl.keysize = MAX_PORTALNAME_LEN;
  ctl.entrysize = sizeof(PortalHashEnt);

     
                                                                       
                       
     
  PortalHashTable = hash_create("Portal hash", PORTALS_PER_USER, &ctl, HASH_ELEM);
}

   
                   
                                                                     
   
Portal
GetPortalByName(const char *name)
{
  Portal portal;

  if (PointerIsValid(name))
  {
    PortalHashTableLookup(name, portal);
  }
  else
  {
    portal = NULL;
  }

  return portal;
}

   
                        
                                                                          
   
                                                                             
                                                                         
                                                          
   
PlannedStmt *
PortalGetPrimaryStmt(Portal portal)
{
  ListCell *lc;

  foreach (lc, portal->stmts)
  {
    PlannedStmt *stmt = lfirst_node(PlannedStmt, lc);

    if (stmt->canSetTag)
    {
      return stmt;
    }
  }
  return NULL;
}

   
                
                                       
   
                                                                        
                                             
   
                                                  
   
Portal
CreatePortal(const char *name, bool allowDup, bool dupSilent)
{
  Portal portal;

  AssertArg(PointerIsValid(name));

  portal = GetPortalByName(name);
  if (PortalIsValid(portal))
  {
    if (!allowDup)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_CURSOR), errmsg("cursor \"%s\" already exists", name)));
    }
    if (!dupSilent)
    {
      ereport(WARNING, (errcode(ERRCODE_DUPLICATE_CURSOR), errmsg("closing existing cursor \"%s\"", name)));
    }
    PortalDrop(portal, false);
  }

                                 
  portal = (Portal)MemoryContextAllocZero(TopPortalContext, sizeof *portal);

                                                                
  portal->portalContext = AllocSetContextCreate(TopPortalContext, "PortalContext", ALLOCSET_SMALL_SIZES);

                                              
  portal->resowner = ResourceOwnerCreate(CurTransactionResourceOwner, "Portal");

                                                          
  portal->status = PORTAL_NEW;
  portal->cleanup = PortalCleanup;
  portal->createSubid = GetCurrentSubTransactionId();
  portal->activeSubid = portal->createSubid;
  portal->createLevel = GetCurrentTransactionNestLevel();
  portal->strategy = PORTAL_MULTI_QUERY;
  portal->cursorOptions = CURSOR_OPT_NO_SCROLL;
  portal->atStart = true;
  portal->atEnd = true;                                          
  portal->visible = true;
  portal->creation_time = GetCurrentStatementStartTimestamp();

                                               
  PortalHashTableInsert(portal, name);

                               
  MemoryContextSetIdentifier(portal->portalContext, portal->name);

  return portal;
}

   
                   
                                                                    
   
Portal
CreateNewPortal(void)
{
  static unsigned int unnamed_portal_count = 0;

  char portalname[MAX_PORTALNAME_LEN];

                                    
  for (;;)
  {
    unnamed_portal_count++;
    sprintf(portalname, "<unnamed portal %u>", unnamed_portal_count);
    if (GetPortalByName(portalname) == NULL)
    {
      break;
    }
  }

  return CreatePortal(portalname, false, false);
}

   
                     
                                                       
   
                                                                          
                                                                         
                                                                     
   
                                                                     
                                                                             
                                                              
   
                                                                            
                                                                            
                                                               
   
                                                                           
                                                                            
                                           
   
                                                                            
                                                        
   
                                                                          
                                                                         
                                                                           
                                                         
   
void
PortalDefineQuery(Portal portal, const char *prepStmtName, const char *sourceText, const char *commandTag, List *stmts, CachedPlan *cplan)
{
  AssertArg(PortalIsValid(portal));
  AssertState(portal->status == PORTAL_NEW);

  AssertArg(sourceText != NULL);
  AssertArg(commandTag != NULL || stmts == NIL);

  portal->prepStmtName = prepStmtName;
  portal->sourceText = sourceText;
  portal->commandTag = commandTag;
  portal->stmts = stmts;
  portal->cplan = cplan;
  portal->status = PORTAL_DEFINED;
}

   
                           
                                                             
   
static void
PortalReleaseCachedPlan(Portal portal)
{
  if (portal->cplan)
  {
    ReleaseCachedPlan(portal->cplan, false);
    portal->cplan = NULL;

       
                                                                          
                                                                          
                                        
       
    portal->stmts = NIL;
  }
}

   
                         
                                        
   
void
PortalCreateHoldStore(Portal portal)
{
  MemoryContext oldcxt;

  Assert(portal->holdContext == NULL);
  Assert(portal->holdStore == NULL);
  Assert(portal->holdSnapshot == NULL);

     
                                                                          
                                                             
     
  portal->holdContext = AllocSetContextCreate(TopPortalContext, "PortalHoldContext", ALLOCSET_DEFAULT_SIZES);

     
                                                                         
                                                               
     
                                                                   
     
  oldcxt = MemoryContextSwitchTo(portal->holdContext);

  portal->holdStore = tuplestore_begin_heap(portal->cursorOptions & CURSOR_OPT_SCROLL, true, work_mem);

  MemoryContextSwitchTo(oldcxt);
}

   
             
                                    
   
                                                                   
                         
   
void
PinPortal(Portal portal)
{
  if (portal->portalPinned)
  {
    elog(ERROR, "portal already pinned");
  }

  portal->portalPinned = true;
}

void
UnpinPortal(Portal portal)
{
  if (!portal->portalPinned)
  {
    elog(ERROR, "portal not pinned");
  }

  portal->portalPinned = false;
}

   
                    
                                                    
   
                                                                               
   
void
MarkPortalActive(Portal portal)
{
                                                             
  if (portal->status != PORTAL_READY)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("portal \"%s\" cannot be run", portal->name)));
  }
                                    
  portal->status = PORTAL_ACTIVE;
  portal->activeSubid = GetCurrentSubTransactionId();
}

   
                  
                                                   
   
                                                                             
   
void
MarkPortalDone(Portal portal)
{
                                    
  Assert(portal->status == PORTAL_ACTIVE);
  portal->status = PORTAL_DONE;

     
                                                                           
                                                                    
     
                                                                           
                                                                             
                                             
     
  if (PointerIsValid(portal->cleanup))
  {
    portal->cleanup(portal);
    portal->cleanup = NULL;
  }
}

   
                    
                                           
   
                                                                               
   
void
MarkPortalFailed(Portal portal)
{
                                    
  Assert(portal->status != PORTAL_DONE);
  portal->status = PORTAL_FAILED;

     
                                                                           
                                                                    
     
                                                                             
                                                                         
                       
     
  if (PointerIsValid(portal->cleanup))
  {
    portal->cleanup(portal);
    portal->cleanup = NULL;
  }
}

   
              
                        
   
void
PortalDrop(Portal portal, bool isTopCommit)
{
  AssertArg(PortalIsValid(portal));

     
                                                                        
                
     
  if (portal->portalPinned)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cannot drop pinned portal \"%s\"", portal->name)));
  }

     
                                                                     
     
  if (portal->status == PORTAL_ACTIVE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cannot drop active portal \"%s\"", portal->name)));
  }

     
                                                                            
                                                                             
                                                                        
                                                                             
                                                                        
                               
     
                                                                         
                                                                  
     
  if (PointerIsValid(portal->cleanup))
  {
    portal->cleanup(portal);
    portal->cleanup = NULL;
  }

                                                                         
  Assert(portal->portalSnapshot == NULL || !isTopCommit);

     
                                                                          
                                                                             
                                                                           
                                   
     
  PortalHashTableDelete(portal);

                                          
  PortalReleaseCachedPlan(portal);

     
                                                                             
                                                                           
                                                                       
                                                           
     
  if (portal->holdSnapshot)
  {
    if (portal->resowner)
    {
      UnregisterSnapshotFromOwner(portal->holdSnapshot, portal->resowner);
    }
    portal->holdSnapshot = NULL;
  }

     
                                                                            
                               
     
                                                                           
                                                                     
                                                                            
                                                                          
                                                               
     
                                                                          
                                           
     
                                                                    
                                                                       
                                      
     
                                                                           
                                                                           
                                                                        
                                                                             
                      
     
  if (portal->resowner && (!isTopCommit || portal->status == PORTAL_FAILED))
  {
    bool isCommit = (portal->status != PORTAL_FAILED);

    ResourceOwnerRelease(portal->resowner, RESOURCE_RELEASE_BEFORE_LOCKS, isCommit, false);
    ResourceOwnerRelease(portal->resowner, RESOURCE_RELEASE_LOCKS, isCommit, false);
    ResourceOwnerRelease(portal->resowner, RESOURCE_RELEASE_AFTER_LOCKS, isCommit, false);
    ResourceOwnerDelete(portal->resowner);
  }
  portal->resowner = NULL;

     
                                                                       
                                                                   
                                                                        
     
  if (portal->holdStore)
  {
    MemoryContext oldcontext;

    oldcontext = MemoryContextSwitchTo(portal->holdContext);
    tuplestore_end(portal->holdStore);
    MemoryContextSwitchTo(oldcontext);
    portal->holdStore = NULL;
  }

                                         
  if (portal->holdContext)
  {
    MemoryContextDelete(portal->holdContext);
  }

                                  
  MemoryContextDelete(portal->portalContext);

                                                        
  pfree(portal);
}

   
                                
   
                                            
   
void
PortalHashTableDeleteAll(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  if (PortalHashTable == NULL)
  {
    return;
  }

  hash_seq_init(&status, PortalHashTable);
  while ((hentry = hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

                                                                     
    if (portal->status == PORTAL_ACTIVE)
    {
      continue;
    }

    PortalDrop(portal, false);

                                                               
    hash_seq_term(&status);
    hash_seq_init(&status, PortalHashTable);
  }
}

   
                                                                  
   
static void
HoldPortal(Portal portal)
{
     
                                                                          
                                                            
     
  PortalCreateHoldStore(portal);
  PersistHoldablePortal(portal);

                                          
  PortalReleaseCachedPlan(portal);

     
                                                                            
                                                                      
                
     
  portal->resowner = NULL;

     
                                                                      
                                    
     
  portal->createSubid = InvalidSubTransactionId;
  portal->activeSubid = InvalidSubTransactionId;
  portal->createLevel = 0;
}

   
                                      
   
                                                                        
                                                                        
                                                                        
                                                                        
                   
   
                                                                            
                                  
   
bool
PreCommit_Portals(bool isPrepare)
{
  bool result = false;
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

       
                                                                      
                                                                         
                                     
       
    if (portal->portalPinned && !portal->autoHeld)
    {
      elog(ERROR, "cannot commit while a portal is pinned");
    }

       
                                                                           
                                                                           
                    
       
                                                                         
                                                                        
                                                                    
                                                           
       
    if (portal->status == PORTAL_ACTIVE)
    {
      if (portal->holdSnapshot)
      {
        if (portal->resowner)
        {
          UnregisterSnapshotFromOwner(portal->holdSnapshot, portal->resowner);
        }
        portal->holdSnapshot = NULL;
      }
      portal->resowner = NULL;
                                                     
      portal->portalSnapshot = NULL;
      continue;
    }

                                                              
    if ((portal->cursorOptions & CURSOR_OPT_HOLD) && portal->createSubid != InvalidSubTransactionId && portal->status == PORTAL_READY)
    {
         
                                                                        
                                                                        
                       
         
                                                                     
                                                                    
         
      if (isPrepare)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE a transaction that has created a cursor WITH HOLD")));
      }

      HoldPortal(portal);

                                   
      result = true;
    }
    else if (portal->createSubid == InvalidSubTransactionId)
    {
         
                                                                     
                                                                         
         
      continue;
    }
    else
    {
                                        
      PortalDrop(portal, true);

                                   
      result = true;
    }

       
                                                                          
                                                                       
                                                           
       
    hash_seq_term(&status);
    hash_seq_init(&status, PortalHashTable);
  }

  return result;
}

   
                                 
   
                                                                              
                                           
   
void
AtAbort_Portals(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

       
                                                                         
                                                                          
       
    if (portal->status == PORTAL_ACTIVE && shmem_exit_inprogress)
    {
      MarkPortalFailed(portal);
    }

       
                                                                         
       
    if (portal->createSubid == InvalidSubTransactionId)
    {
      continue;
    }

       
                                                                          
                                                                         
                                                                         
       
    if (portal->autoHeld)
    {
      continue;
    }

       
                                                                        
                                                                    
                                                           
                           
       
    if (portal->status == PORTAL_READY)
    {
      MarkPortalFailed(portal);
    }

       
                                                                      
                        
       
    if (PointerIsValid(portal->cleanup))
    {
      portal->cleanup(portal);
      portal->cleanup = NULL;
    }

                                            
    PortalReleaseCachedPlan(portal);

       
                                                                     
                                                                          
                   
       
    portal->resowner = NULL;

       
                                                                         
                                                                          
                                                                       
                                               
       
    if (portal->status != PORTAL_ACTIVE)
    {
      MemoryContextDeleteChildren(portal->portalContext);
    }
  }
}

   
                                   
   
                                                                
void
AtCleanup_Portals(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

       
                                                                           
                                    
       
    if (portal->status == PORTAL_ACTIVE)
    {
      continue;
    }

       
                                                                      
                       
       
    if (portal->createSubid == InvalidSubTransactionId || portal->autoHeld)
    {
      Assert(portal->status != PORTAL_ACTIVE);
      Assert(portal->resowner == NULL);
      continue;
    }

       
                                                                           
                                                                       
                                                                     
       
    if (portal->portalPinned)
    {
      portal->portalPinned = false;
    }

       
                                                                          
                                                                          
       
    if (PointerIsValid(portal->cleanup))
    {
      elog(WARNING, "skipping cleanup for portal \"%s\"", portal->name);
      portal->cleanup = NULL;
    }

                 
    PortalDrop(portal, false);
  }
}

   
                                                                    
   
                                                                               
                                                         
   
void
PortalErrorCleanup(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

    if (portal->autoHeld)
    {
      portal->portalPinned = false;
      PortalDrop(portal, false);
    }
  }
}

   
                                         
   
                                                                         
                          
   
void
AtSubCommit_Portals(SubTransactionId mySubid, SubTransactionId parentSubid, int parentLevel, ResourceOwner parentXactOwner)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

    if (portal->createSubid == mySubid)
    {
      portal->createSubid = parentSubid;
      portal->createLevel = parentLevel;
      if (portal->resowner)
      {
        ResourceOwnerNewParent(portal->resowner, parentXactOwner);
      }
    }
    if (portal->activeSubid == mySubid)
    {
      portal->activeSubid = parentSubid;
    }
  }
}

   
                                              
   
                                                                        
                                                                           
                                             
   
                                                                           
   
void
AtSubAbort_Portals(SubTransactionId mySubid, SubTransactionId parentSubid, ResourceOwner myXactOwner, ResourceOwner parentXactOwner)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

                                                
    if (portal->createSubid != mySubid)
    {
                                                             
      if (portal->activeSubid == mySubid)
      {
                                                              
        portal->activeSubid = parentSubid;

           
                                                                    
                                                                       
                                                                       
                                    
           
                                                                       
                                                                      
                                                               
                                                                    
                                                                      
                                                                       
                                                                       
                                                                     
                                               
           
        if (portal->status == PORTAL_ACTIVE)
        {
          MarkPortalFailed(portal);
        }

           
                                                                   
                                                                  
                                                                    
                                                                    
                                                                       
                                                                      
                                                                
                                                                  
           
        if (portal->status == PORTAL_FAILED && portal->resowner)
        {
          ResourceOwnerNewParent(portal->resowner, myXactOwner);
          portal->resowner = NULL;
        }
      }
                                                            
      continue;
    }

       
                                                                          
                                                                         
                                                                       
                                                                     
                                                                         
                                                                         
                                                                         
                                         
       
    if (portal->status == PORTAL_READY || portal->status == PORTAL_ACTIVE)
    {
      MarkPortalFailed(portal);
    }

       
                                                                      
                        
       
    if (PointerIsValid(portal->cleanup))
    {
      portal->cleanup(portal);
      portal->cleanup = NULL;
    }

                                            
    PortalReleaseCachedPlan(portal);

       
                                                                     
                                                                          
                   
       
    portal->resowner = NULL;

       
                                                                         
                                                                          
                                                                       
              
       
    MemoryContextDeleteChildren(portal->portalContext);
  }
}

   
                                      
   
                                                                        
                                                                   
   
void
AtSubCleanup_Portals(SubTransactionId mySubid)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

    if (portal->createSubid != mySubid)
    {
      continue;
    }

       
                                                                           
                                                                       
                                                                     
       
    if (portal->portalPinned)
    {
      portal->portalPinned = false;
    }

       
                                                                          
                                                                          
       
    if (PointerIsValid(portal->cleanup))
    {
      elog(WARNING, "skipping cleanup for portal \"%s\"", portal->name);
      portal->cleanup = NULL;
    }

                 
    PortalDrop(portal, false);
  }
}

                                
Datum
pg_cursor(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  HASH_SEQ_STATUS hash_seq;
  PortalHashEnt *hentry;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not "
                                                                   "allowed in this context")));
  }

                                                 
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

     
                                                                            
                                         
     
  tupdesc = CreateTemplateTupleDesc(6);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "name", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "statement", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)3, "is_holdable", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)4, "is_binary", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)5, "is_scrollable", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)6, "creation_time", TIMESTAMPTZOID, -1, 0);

     
                                                                           
                                                                             
     
  tupstore = tuplestore_begin_heap(rsinfo->allowedModes & SFRM_Materialize_Random, false, work_mem);

                                           
  MemoryContextSwitchTo(oldcontext);

  hash_seq_init(&hash_seq, PortalHashTable);
  while ((hentry = hash_seq_search(&hash_seq)) != NULL)
  {
    Portal portal = hentry->portal;
    Datum values[6];
    bool nulls[6];

                                       
    if (!portal->visible)
    {
      continue;
    }

    MemSet(nulls, 0, sizeof(nulls));

    values[0] = CStringGetTextDatum(portal->name);
    values[1] = CStringGetTextDatum(portal->sourceText);
    values[2] = BoolGetDatum(portal->cursorOptions & CURSOR_OPT_HOLD);
    values[3] = BoolGetDatum(portal->cursorOptions & CURSOR_OPT_BINARY);
    values[4] = BoolGetDatum(portal->cursorOptions & CURSOR_OPT_SCROLL);
    values[5] = TimestampTzGetDatum(portal->creation_time);

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

                                          
  tuplestore_donestoring(tupstore);

  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  return (Datum)0;
}

bool
ThereAreNoReadyPortals(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

    if (portal->status == PORTAL_READY)
    {
      return false;
    }
  }

  return true;
}

   
                            
   
                                                                         
                                                                            
                                                                           
                                                                            
                                                                        
                                                                       
                                                                 
   
                                                                             
                                                                              
                                                                              
                                                                           
            
   
void
HoldPinnedPortals(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;

  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

    if (portal->portalPinned && !portal->autoHeld)
    {
         
                                                                      
                                                                  
                                                            
                                                                      
                                                                       
                                                                        
              
         
      if (portal->strategy != PORTAL_ONE_SELECT)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot perform transaction commands inside a cursor loop that is not read-only")));
      }

                                                      
      if (portal->status != PORTAL_READY)
      {
        elog(ERROR, "pinned portal is not ready to be auto-held");
      }

      HoldPortal(portal);
      portal->autoHeld = true;
    }
  }
}

   
                                                                         
                  
   
                                                                           
                                                                            
                                                                          
   
                                                                         
                                                                            
   
void
ForgetPortalSnapshots(void)
{
  HASH_SEQ_STATUS status;
  PortalHashEnt *hentry;
  int numPortalSnaps = 0;
  int numActiveSnaps = 0;

                                                                   
  hash_seq_init(&status, PortalHashTable);

  while ((hentry = (PortalHashEnt *)hash_seq_search(&status)) != NULL)
  {
    Portal portal = hentry->portal;

    if (portal->portalSnapshot != NULL)
    {
      portal->portalSnapshot = NULL;
      numPortalSnaps++;
    }
                                                                      
  }

     
                                                                             
                                                                        
                                                                       
                                                 
     
  while (ActiveSnapshotSet())
  {
    PopActiveSnapshot();
    numActiveSnaps++;
  }

  if (numPortalSnaps != numActiveSnaps)
  {
    elog(ERROR, "portal snapshots (%d) did not account for all active snapshots (%d)", numPortalSnaps, numActiveSnaps);
  }
}
