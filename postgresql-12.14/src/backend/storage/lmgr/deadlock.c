                                                                            
   
              
                                      
   
                                                                         
                                        
   
   
                                                                         
                                                                        
   
   
                  
                                         
   
              
   
                   
                    
                            
                          
   
                                                                            
   
#include "postgres.h"

#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "utils/memutils.h"

   
                                    
   
                                                                               
                                                                             
                                                                           
                                                                            
                                                                               
                     
   
typedef struct
{
  PGPROC *waiter;                                            
  PGPROC *blocker;                                                
  LOCK *lock;                                     
  int pred;                                    
  int link;                                    
} EDGE;

                                                     
typedef struct
{
  LOCK *lock;                                                 
  PGPROC **procs;                                            
  int nProcs;
} WAIT_ORDER;

   
                                                                         
                                                       
   
                                                                       
                                                                            
                                                                 
   
typedef struct
{
  LOCKTAG locktag;                                  
  LOCKMODE lockmode;                                     
  int pid;                                       
} DEADLOCK_INFO;

static bool
DeadLockCheckRecurse(PGPROC *proc);
static int
TestConfiguration(PGPROC *startProc);
static bool
FindLockCycle(PGPROC *checkProc, EDGE *softEdges, int *nSoftEdges);
static bool
FindLockCycleRecurse(PGPROC *checkProc, int depth, EDGE *softEdges, int *nSoftEdges);
static bool
FindLockCycleRecurseMember(PGPROC *checkProc, PGPROC *checkProcLeader, int depth, EDGE *softEdges, int *nSoftEdges);
static bool
ExpandConstraints(EDGE *constraints, int nConstraints);
static bool
TopoSort(LOCK *lock, EDGE *constraints, int nConstraints, PGPROC **ordering);

#ifdef DEBUG_DEADLOCK
static void
PrintLockQueue(LOCK *lock, const char *info);
#endif

   
                                           
   

                                 
static PGPROC **visitedProcs;                             
static int nVisitedProcs;

                            
static PGPROC **topoProcs;                                        
static int *beforeConstraints;                                             
static int *afterConstraints;                                       

                                       
static WAIT_ORDER *waitOrders;                                             
static int nWaitOrders;
static PGPROC **waitOrderProcs;                                          

                                                  
static EDGE *curConstraints;
static int nCurConstraints;
static int maxCurConstraints;

                                                  
static EDGE *possibleConstraints;
static int nPossibleConstraints;
static int maxPossibleConstraints;
static DEADLOCK_INFO *deadlockDetails;
static int nDeadlockDetails;

                                                            
static PGPROC *blocking_autovacuum_proc = NULL;

   
                                                                              
   
                                                                            
                                                                           
                                                                     
                                                                          
                                                                           
                                                                             
                                                                          
   
void
InitDeadLockChecking(void)
{
  MemoryContext oldcxt;

                                           
  oldcxt = MemoryContextSwitchTo(TopMemoryContext);

     
                                                                           
                        
     
  visitedProcs = (PGPROC **)palloc(MaxBackends * sizeof(PGPROC *));
  deadlockDetails = (DEADLOCK_INFO *)palloc(MaxBackends * sizeof(DEADLOCK_INFO));

     
                                                                            
                                                     
     
  topoProcs = visitedProcs;                        
  beforeConstraints = (int *)palloc(MaxBackends * sizeof(int));
  afterConstraints = (int *)palloc(MaxBackends * sizeof(int));

     
                                                                       
                                                                             
                                                                      
                                
     
  waitOrders = (WAIT_ORDER *)palloc((MaxBackends / 2) * sizeof(WAIT_ORDER));
  waitOrderProcs = (PGPROC **)palloc(MaxBackends * sizeof(PGPROC *));

     
                                                                            
                                                                            
                                                                            
                                                                         
                                                                           
                                                                  
     
  maxCurConstraints = MaxBackends;
  curConstraints = (EDGE *)palloc(maxCurConstraints * sizeof(EDGE));

     
                                                                         
                                                                           
                                                                
                                                                             
                                                                       
                                         
     
  maxPossibleConstraints = MaxBackends * 4;
  possibleConstraints = (EDGE *)palloc(maxPossibleConstraints * sizeof(EDGE));

  MemoryContextSwitchTo(oldcxt);
}

   
                                                             
   
                                                                      
                                                                    
                                                                       
                                                                      
   
                                                                      
   
                                                                      
                                                                       
                                                                       
                                                             
   
DeadLockState
DeadLockCheck(PGPROC *proc)
{
  int i, j;

                                      
  nCurConstraints = 0;
  nPossibleConstraints = 0;
  nWaitOrders = 0;

                                                         
  blocking_autovacuum_proc = NULL;

                                               
  if (DeadLockCheckRecurse(proc))
  {
       
                                                               
                                                                     
       
    int nSoftEdges;

    TRACE_POSTGRESQL_DEADLOCK_FOUND();

    nWaitOrders = 0;
    if (!FindLockCycle(proc, possibleConstraints, &nSoftEdges))
    {
      elog(FATAL, "deadlock seems to have disappeared");
    }

    return DS_HARD_DEADLOCK;                                         
  }

                                                      
  for (i = 0; i < nWaitOrders; i++)
  {
    LOCK *lock = waitOrders[i].lock;
    PGPROC **procs = waitOrders[i].procs;
    int nProcs = waitOrders[i].nProcs;
    PROC_QUEUE *waitQueue = &(lock->waitProcs);

    Assert(nProcs == waitQueue->size);

#ifdef DEBUG_DEADLOCK
    PrintLockQueue(lock, "DeadLockCheck:");
#endif

                                                               
    ProcQueueInit(waitQueue);
    for (j = 0; j < nProcs; j++)
    {
      SHMQueueInsertBefore(&(waitQueue->links), &(procs[j]->links));
      waitQueue->size++;
    }

#ifdef DEBUG_DEADLOCK
    PrintLockQueue(lock, "rearranged to:");
#endif

                                                             
    ProcLockWakeup(GetLocksMethodTable(lock), lock);
  }

                                                                      
  if (nWaitOrders > 0)
  {
    return DS_SOFT_DEADLOCK;
  }
  else if (blocking_autovacuum_proc != NULL)
  {
    return DS_BLOCKED_BY_AUTOVACUUM;
  }
  else
  {
    return DS_NO_DEADLOCK;
  }
}

   
                                                                  
   
                                                          
   
PGPROC *
GetBlockingAutoVacuumPgproc(void)
{
  PGPROC *ptr;

  ptr = blocking_autovacuum_proc;
  blocking_autovacuum_proc = NULL;

  return ptr;
}

   
                                                                  
   
                                                                          
                                                                       
                                                    
   
                                                                         
                                                                      
                                                
   
static bool
DeadLockCheckRecurse(PGPROC *proc)
{
  int nEdges;
  int oldPossibleConstraints;
  bool savedList;
  int i;

  nEdges = TestConfiguration(proc);
  if (nEdges < 0)
  {
    return true;                                    
  }
  if (nEdges == 0)
  {
    return false;                               
  }
  if (nCurConstraints >= maxCurConstraints)
  {
    return true;                                          
  }
  oldPossibleConstraints = nPossibleConstraints;
  if (nPossibleConstraints + nEdges + MaxBackends <= maxPossibleConstraints)
  {
                                                            
    nPossibleConstraints += nEdges;
    savedList = true;
  }
  else
  {
                                                                
    savedList = false;
  }

     
                                                                       
     
  for (i = 0; i < nEdges; i++)
  {
    if (!savedList && i > 0)
    {
                                                             
      if (nEdges != TestConfiguration(proc))
      {
        elog(FATAL, "inconsistent results during deadlock check");
      }
    }
    curConstraints[nCurConstraints] = possibleConstraints[oldPossibleConstraints + i];
    nCurConstraints++;
    if (!DeadLockCheckRecurse(proc))
    {
      return false;                              
    }
                                                     
    nCurConstraints--;
  }
  nPossibleConstraints = oldPossibleConstraints;
  return true;                        
}

                       
                                                                   
   
            
                                                
                                                                          
                                                         
   
                                                                           
                                                         
                                                                      
                         
                       
   
static int
TestConfiguration(PGPROC *startProc)
{
  int softFound = 0;
  EDGE *softEdges = possibleConstraints + nPossibleConstraints;
  int nSoftEdges;
  int i;

     
                                                        
     
  if (nPossibleConstraints + MaxBackends > maxPossibleConstraints)
  {
    return -1;
  }

     
                                                                     
                                            
     
  if (!ExpandConstraints(curConstraints, nCurConstraints))
  {
    return -1;
  }

     
                                                                           
                                                                          
                                                              
     
  for (i = 0; i < nCurConstraints; i++)
  {
    if (FindLockCycle(curConstraints[i].waiter, softEdges, &nSoftEdges))
    {
      if (nSoftEdges == 0)
      {
        return -1;                             
      }
      softFound = nSoftEdges;
    }
    if (FindLockCycle(curConstraints[i].blocker, softEdges, &nSoftEdges))
    {
      if (nSoftEdges == 0)
      {
        return -1;                             
      }
      softFound = nSoftEdges;
    }
  }
  if (FindLockCycle(startProc, softEdges, &nSoftEdges))
  {
    if (nSoftEdges == 0)
    {
      return -1;                             
    }
    softFound = nSoftEdges;
  }
  return softFound;
}

   
                                                    
   
                                                                      
                                                                    
                                                                   
                                                                       
                                                                       
                                                                          
                                                                        
                          
   
                                                                            
                                                                           
                                                                          
                                                                           
   
static bool
FindLockCycle(PGPROC *checkProc, EDGE *softEdges,                      
    int *nSoftEdges)                                                   
{
  nVisitedProcs = 0;
  nDeadlockDetails = 0;
  *nSoftEdges = 0;
  return FindLockCycleRecurse(checkProc, 0, softEdges, nSoftEdges);
}

static bool
FindLockCycleRecurse(PGPROC *checkProc, int depth, EDGE *softEdges,                      
    int *nSoftEdges)                                                                     
{
  int i;
  dlist_iter iter;

     
                                                                             
                                                                  
     
  if (checkProc->lockGroupLeader != NULL)
  {
    checkProc = checkProc->lockGroupLeader;
  }

     
                                     
     
  for (i = 0; i < nVisitedProcs; i++)
  {
    if (visitedProcs[i] == checkProc)
    {
                                                                    
      if (i == 0)
      {
           
                                                                       
                             
           
        Assert(depth <= MaxBackends);
        nDeadlockDetails = depth;

        return true;
      }

         
                                                                      
                                      
         
      return false;
    }
  }
                         
  Assert(nVisitedProcs < MaxBackends);
  visitedProcs[nVisitedProcs++] = checkProc;

     
                                                                            
                             
     
  if (checkProc->links.next != NULL && checkProc->waitLock != NULL && FindLockCycleRecurseMember(checkProc, checkProc, depth, softEdges, nSoftEdges))
  {
    return true;
  }

     
                                                                            
                                                                            
                                                                          
                                                                           
                                                                             
     
  dlist_foreach(iter, &checkProc->lockGroupMembers)
  {
    PGPROC *memberProc;

    memberProc = dlist_container(PGPROC, lockGroupLink, iter.cur);

    if (memberProc->links.next != NULL && memberProc->waitLock != NULL && memberProc != checkProc && FindLockCycleRecurseMember(memberProc, checkProc, depth, softEdges, nSoftEdges))
    {
      return true;
    }
  }

  return false;
}

static bool
FindLockCycleRecurseMember(PGPROC *checkProc, PGPROC *checkProcLeader, int depth, EDGE *softEdges,                      
    int *nSoftEdges)                                                                                                    
{
  PGPROC *proc;
  LOCK *lock = checkProc->waitLock;
  PGXACT *pgxact;
  PROCLOCK *proclock;
  SHM_QUEUE *procLocks;
  LockMethod lockMethodTable;
  PROC_QUEUE *waitQueue;
  int queue_size;
  int conflictMask;
  int i;
  int numLockModes, lm;

  lockMethodTable = GetLocksMethodTable(lock);
  numLockModes = lockMethodTable->numLockModes;
  conflictMask = lockMethodTable->conflictTab[checkProc->waitLockMode];

     
                                                                           
                                   
     
  procLocks = &(lock->procLocks);

  proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, lockLink));

  while (proclock)
  {
    PGPROC *leader;

    proc = proclock->tag.myProc;
    pgxact = &ProcGlobal->allPgXact[proc->pgprocno];
    leader = proc->lockGroupLeader == NULL ? proc : proc->lockGroupLeader;

                                                                   
    if (leader != checkProcLeader)
    {
      for (lm = 1; lm <= numLockModes; lm++)
      {
        if ((proclock->holdMask & LOCKBIT_ON(lm)) && (conflictMask & LOCKBIT_ON(lm)))
        {
                                               
          if (FindLockCycleRecurse(proc, depth + 1, softEdges, nSoftEdges))
          {
                                        
            DEADLOCK_INFO *info = &deadlockDetails[depth];

            info->locktag = lock->tag;
            info->lockmode = checkProc->waitLockMode;
            info->pid = checkProc->pid;

            return true;
          }

             
                                                                     
                                                                  
                                                                   
                                                                   
                                                                  
             
                                                                     
                                                                     
                                                                    
                                                                 
                                            
             
                                                                    
                                                               
                                                                 
                                                                    
                                                                  
                                                                  
                                                               
                            
             
          if (checkProc == MyProc && pgxact->vacuumFlags & PROC_IS_AUTOVACUUM)
          {
            blocking_autovacuum_proc = proc;
          }

                                                   
          break;
        }
      }
    }

    proclock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->lockLink, offsetof(PROCLOCK, lockLink));
  }

     
                                                                         
                                                                             
                                                                            
                                                           
     
                                                                           
                                         
     
  for (i = 0; i < nWaitOrders; i++)
  {
    if (waitOrders[i].lock == lock)
    {
      break;
    }
  }

  if (i < nWaitOrders)
  {
                                                     
    PGPROC **procs = waitOrders[i].procs;

    queue_size = waitOrders[i].nProcs;

    for (i = 0; i < queue_size; i++)
    {
      PGPROC *leader;

      proc = procs[i];
      leader = proc->lockGroupLeader == NULL ? proc : proc->lockGroupLeader;

         
                                                                    
                                                                
                                                                      
                                                                        
                                                                       
         
      if (leader == checkProcLeader)
      {
        break;
      }

                                                        
      if ((LOCKBIT_ON(proc->waitLockMode) & conflictMask) != 0)
      {
                                             
        if (FindLockCycleRecurse(proc, depth + 1, softEdges, nSoftEdges))
        {
                                      
          DEADLOCK_INFO *info = &deadlockDetails[depth];

          info->locktag = lock->tag;
          info->lockmode = checkProc->waitLockMode;
          info->pid = checkProc->pid;

             
                                                                  
             
          Assert(*nSoftEdges < MaxBackends);
          softEdges[*nSoftEdges].waiter = checkProcLeader;
          softEdges[*nSoftEdges].blocker = leader;
          softEdges[*nSoftEdges].lock = lock;
          (*nSoftEdges)++;
          return true;
        }
      }
    }
  }
  else
  {
    PGPROC *lastGroupMember = NULL;

                                            
    waitQueue = &(lock->waitProcs);

       
                                                                          
                                                                         
                                                                           
                                                                        
                                               
       
    if (checkProc->lockGroupLeader == NULL)
    {
      lastGroupMember = checkProc;
    }
    else
    {
      proc = (PGPROC *)waitQueue->links.next;
      queue_size = waitQueue->size;
      while (queue_size-- > 0)
      {
        if (proc->lockGroupLeader == checkProcLeader)
        {
          lastGroupMember = proc;
        }
        proc = (PGPROC *)proc->links.next;
      }
      Assert(lastGroupMember != NULL);
    }

       
                                                                          
       
    queue_size = waitQueue->size;
    proc = (PGPROC *)waitQueue->links.next;
    while (queue_size-- > 0)
    {
      PGPROC *leader;

      leader = proc->lockGroupLeader == NULL ? proc : proc->lockGroupLeader;

                                              
      if (proc == lastGroupMember)
      {
        break;
      }

                                                        
      if ((LOCKBIT_ON(proc->waitLockMode) & conflictMask) != 0 && leader != checkProcLeader)
      {
                                             
        if (FindLockCycleRecurse(proc, depth + 1, softEdges, nSoftEdges))
        {
                                      
          DEADLOCK_INFO *info = &deadlockDetails[depth];

          info->locktag = lock->tag;
          info->lockmode = checkProc->waitLockMode;
          info->pid = checkProc->pid;

             
                                                                  
             
          Assert(*nSoftEdges < MaxBackends);
          softEdges[*nSoftEdges].waiter = checkProcLeader;
          softEdges[*nSoftEdges].blocker = leader;
          softEdges[*nSoftEdges].lock = lock;
          (*nSoftEdges)++;
          return true;
        }
      }

      proc = (PGPROC *)proc->links.next;
    }
  }

     
                                
     
  return false;
}

   
                                                                   
                                                    
   
                                                                       
                                                                        
                                  
   
                                                                    
                                                                    
   
static bool
ExpandConstraints(EDGE *constraints, int nConstraints)
{
  int nWaitOrderProcs = 0;
  int i, j;

  nWaitOrders = 0;

     
                                                                     
                                                                           
                              
     
  for (i = nConstraints; --i >= 0;)
  {
    LOCK *lock = constraints[i].lock;

                                                   
    for (j = nWaitOrders; --j >= 0;)
    {
      if (waitOrders[j].lock == lock)
      {
        break;
      }
    }
    if (j >= 0)
    {
      continue;
    }
                                    
    waitOrders[nWaitOrders].lock = lock;
    waitOrders[nWaitOrders].procs = waitOrderProcs + nWaitOrderProcs;
    waitOrders[nWaitOrders].nProcs = lock->waitProcs.size;
    nWaitOrderProcs += lock->waitProcs.size;
    Assert(nWaitOrderProcs <= MaxBackends);

       
                                                                           
                                                    
       
    if (!TopoSort(lock, constraints, i + 1, waitOrders[nWaitOrders].procs))
    {
      return false;
    }
    nWaitOrders++;
  }
  return true;
}

   
                                                
   
                                                                      
                                                                            
                                                                          
                                               
   
                                                                            
                                                                         
                                                                         
                                                                              
                                                  
   
                                                                            
                                                                            
                                                                           
                                                                          
                                                                             
                                                                       
                                                               
   
                                                                    
                                                                    
   
static bool
TopoSort(LOCK *lock, EDGE *constraints, int nConstraints, PGPROC **ordering)                      
{
  PROC_QUEUE *waitQueue = &(lock->waitProcs);
  int queue_size = waitQueue->size;
  PGPROC *proc;
  int i, j, jj, k, kk, last;

                                                                           
  proc = (PGPROC *)waitQueue->links.next;
  for (i = 0; i < queue_size; i++)
  {
    topoProcs[i] = proc;
    proc = (PGPROC *)proc->links.next;
  }

     
                                                                            
                                                                             
                                                                        
                                                                          
                                                                       
                                                                           
                                                                           
                                                      
     
                                                                           
                                                                             
                                                                        
                                                                         
                                                                          
                                                                         
                                                                     
                                                                             
               
     
  MemSet(beforeConstraints, 0, queue_size * sizeof(int));
  MemSet(afterConstraints, 0, queue_size * sizeof(int));
  for (i = 0; i < nConstraints; i++)
  {
       
                                                                           
                                                                         
                                                                           
                                                                
                                                                       
                                                                
       
                                                                          
                                                                          
                                                                       
                                                                      
                                                                
       
    proc = constraints[i].waiter;
    Assert(proc != NULL);
    jj = -1;
    for (j = queue_size; --j >= 0;)
    {
      PGPROC *waiter = topoProcs[j];

      if (waiter == proc || waiter->lockGroupLeader == proc)
      {
        Assert(waiter->waitLock == lock);
        if (jj == -1)
        {
          jj = j;
        }
        else
        {
          Assert(beforeConstraints[j] <= 0);
          beforeConstraints[j] = -1;
        }
      }
    }

                                                                         
    if (jj < 0)
    {
      continue;
    }

       
                                                                          
                                                                          
                                       
       
    proc = constraints[i].blocker;
    Assert(proc != NULL);
    kk = -1;
    for (k = queue_size; --k >= 0;)
    {
      PGPROC *blocker = topoProcs[k];

      if (blocker == proc || blocker->lockGroupLeader == proc)
      {
        Assert(blocker->waitLock == lock);
        if (kk == -1)
        {
          kk = k;
        }
        else
        {
          Assert(beforeConstraints[k] <= 0);
          beforeConstraints[k] = -1;
        }
      }
    }

                                                                          
    if (kk < 0)
    {
      continue;
    }

    Assert(beforeConstraints[jj] >= 0);
    beforeConstraints[jj]++;                              
                                                                      
    constraints[i].pred = jj;
    constraints[i].link = afterConstraints[kk];
    afterConstraints[kk] = i + 1;
  }

                         
                                                                       
                                                                       
                                                                         
                                                            
                                                               
                                      
                                                      
                                                                        
                         
     
  last = queue_size - 1;
  for (i = queue_size - 1; i >= 0;)
  {
    int c;
    int nmatches = 0;

                                       
    while (topoProcs[last] == NULL)
    {
      last--;
    }
    for (j = last; j >= 0; j--)
    {
      if (topoProcs[j] != NULL && beforeConstraints[j] == 0)
      {
        break;
      }
    }

                                                           
    if (j < 0)
    {
      return false;
    }

       
                                                                 
                                                                           
                                                                          
                                                                        
                                                                          
                                                                      
                                                
       
    proc = topoProcs[j];
    if (proc->lockGroupLeader != NULL)
    {
      proc = proc->lockGroupLeader;
    }
    Assert(proc != NULL);
    for (c = 0; c <= last; ++c)
    {
      if (topoProcs[c] == proc || (topoProcs[c] != NULL && topoProcs[c]->lockGroupLeader == proc))
      {
        ordering[i - nmatches] = topoProcs[c];
        topoProcs[c] = NULL;
        ++nmatches;
      }
    }
    Assert(nmatches > 0);
    i -= nmatches;

                                                             
    for (k = afterConstraints[j]; k > 0; k = constraints[k - 1].link)
    {
      beforeConstraints[constraints[k - 1].pred]--;
    }
  }

            
  return true;
}

#ifdef DEBUG_DEADLOCK
static void
PrintLockQueue(LOCK *lock, const char *info)
{
  PROC_QUEUE *waitQueue = &(lock->waitProcs);
  int queue_size = waitQueue->size;
  PGPROC *proc;
  int i;

  printf("%s lock %p queue ", info, lock);
  proc = (PGPROC *)waitQueue->links.next;
  for (i = 0; i < queue_size; i++)
  {
    printf(" %d", proc->pid);
    proc = (PGPROC *)proc->links.next;
  }
  printf("\n");
  fflush(stdout);
}
#endif

   
                                                       
   
void
DeadLockReport(void)
{
  StringInfoData clientbuf;                           
  StringInfoData logbuf;                                  
  StringInfoData locktagbuf;
  int i;

  initStringInfo(&clientbuf);
  initStringInfo(&logbuf);
  initStringInfo(&locktagbuf);

                                                         
  for (i = 0; i < nDeadlockDetails; i++)
  {
    DEADLOCK_INFO *info = &deadlockDetails[i];
    int nextpid;

                                                  
    if (i < nDeadlockDetails - 1)
    {
      nextpid = info[1].pid;
    }
    else
    {
      nextpid = deadlockDetails[0].pid;
    }

                                                          
    resetStringInfo(&locktagbuf);

    DescribeLockTag(&locktagbuf, &info->locktag);

    if (i > 0)
    {
      appendStringInfoChar(&clientbuf, '\n');
    }

    appendStringInfo(&clientbuf, _("Process %d waits for %s on %s; blocked by process %d."), info->pid, GetLockmodeName(info->locktag.locktag_lockmethodid, info->lockmode), locktagbuf.data, nextpid);
  }

                                                  
  appendStringInfoString(&logbuf, clientbuf.data);

                                            
  for (i = 0; i < nDeadlockDetails; i++)
  {
    DEADLOCK_INFO *info = &deadlockDetails[i];

    appendStringInfoChar(&logbuf, '\n');

    appendStringInfo(&logbuf, _("Process %d: %s"), info->pid, pgstat_get_backend_current_activity(info->pid, false));
  }

  pgstat_report_deadlock();

  ereport(ERROR, (errcode(ERRCODE_T_R_DEADLOCK_DETECTED), errmsg("deadlock detected"), errdetail_internal("%s", clientbuf.data), errdetail_log("%s", logbuf.data), errhint("See server log for query details.")));
}

   
                                                                         
                                                                            
                                                                        
   
void
RememberSimpleDeadLock(PGPROC *proc1, LOCKMODE lockmode, LOCK *lock, PGPROC *proc2)
{
  DEADLOCK_INFO *info = &deadlockDetails[0];

  info->locktag = lock->tag;
  info->lockmode = lockmode;
  info->pid = proc1->pid;
  info++;
  info->locktag = proc2->waitLock->tag;
  info->lockmode = proc2->waitLockMode;
  info->pid = proc2->pid;
  nDeadlockDetails = 2;
}
