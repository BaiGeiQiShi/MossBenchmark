                                                                            
   
             
                                               
   
                                                                
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/xact.h"
#include "catalog/namespace.h"
#include "commands/async.h"
#include "commands/discard.h"
#include "commands/prepare.h"
#include "commands/sequence.h"
#include "utils/guc.h"
#include "utils/portal.h"

static void
DiscardAll(bool isTopLevel);

   
                                              
   
void
DiscardCommand(DiscardStmt *stmt, bool isTopLevel)
{
  switch (stmt->target)
  {
  case DISCARD_ALL:
    DiscardAll(isTopLevel);
    break;

  case DISCARD_PLANS:
    ResetPlanCache();
    break;

  case DISCARD_SEQUENCES:
    ResetSequenceCaches();
    break;

  case DISCARD_TEMP:
    ResetTempTableNamespace();
    break;

  default:
    elog(ERROR, "unrecognized DISCARD target: %d", stmt->target);
  }
}

static void
DiscardAll(bool isTopLevel)
{
     
                                                                   
                                                                         
                                                                            
                                                                        
                        
     
  PreventInTransactionBlock(isTopLevel, "DISCARD ALL");

                                                                      
  PortalHashTableDeleteAll();
  SetPGVariable("session_authorization", NIL, false);
  ResetAllOptions();
  DropAllPreparedStatements();
  Async_UnlistenAll();
  LockReleaseAll(USER_LOCKMETHOD, true);
  ResetPlanCache();
  ResetTempTableNamespace();
  ResetSequenceCaches();
}
