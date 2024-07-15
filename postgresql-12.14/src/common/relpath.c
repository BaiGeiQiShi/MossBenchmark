                                                                            
             
                                                                        
   
                                                                    
   
                                                                         
                                                                        
   
                  
                          
   
                                                                            
   
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "catalog/pg_tablespace_d.h"
#include "common/relpath.h"
#include "storage/backendid.h"

   
                                             
   
                                                             
                                                                     
                       
   
const char *const forkNames[] = {
    "main",                   
    "fsm",                   
    "vm",                              
    "init"                    
};

   
                                                    
   
                                                                    
                             
   
ForkNumber
forkname_to_number(const char *forkName)
{
  ForkNumber forkNum;

  for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
  {
    if (strcmp(forkNames[forkNum], forkName) == 0)
    {
      return forkNum;
    }
  }

#ifndef FRONTEND
  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid fork name"),
                     errhint("Valid fork names are \"main\", \"fsm\", "
                             "\"vm\", and \"init\".")));
#endif

  return InvalidForkNumber;
}

   
                  
                                                                     
                                                                     
                                                                     
                                                                       
                                                                          
   
                                                                           
                                     
   
int
forkname_chars(const char *str, ForkNumber *fork)
{
  ForkNumber forkNum;

  for (forkNum = 1; forkNum <= MAX_FORKNUM; forkNum++)
  {
    int len = strlen(forkNames[forkNum]);

    if (strncmp(forkNames[forkNum], str, len) == 0)
    {
      if (fork)
      {
        *fork = forkNum;
      }
      return len;
    }
  }
  if (fork)
  {
    *fork = InvalidForkNumber;
  }
  return 0;
}

   
                                                            
   
                                
   
                                               
   
char *
GetDatabasePath(Oid dbNode, Oid spcNode)
{
  if (spcNode == GLOBALTABLESPACE_OID)
  {
                                                          
    Assert(dbNode == 0);
    return pstrdup("global");
  }
  else if (spcNode == DEFAULTTABLESPACE_OID)
  {
                                                  
    return psprintf("base/%u", dbNode);
  }
  else
  {
                                                         
    return psprintf("pg_tblspc/%u/%s/%u", spcNode, TABLESPACE_VERSION_DIRECTORY, dbNode);
  }
}

   
                                                         
   
                                
   
                                                                               
                                                                              
                                                         
   
char *
GetRelationPath(Oid dbNode, Oid spcNode, Oid relNode, int backendId, ForkNumber forkNumber)
{
  char *path;

  if (spcNode == GLOBALTABLESPACE_OID)
  {
                                                          
    Assert(dbNode == 0);
    Assert(backendId == InvalidBackendId);
    if (forkNumber != MAIN_FORKNUM)
    {
      path = psprintf("global/%u_%s", relNode, forkNames[forkNumber]);
    }
    else
    {
      path = psprintf("global/%u", relNode);
    }
  }
  else if (spcNode == DEFAULTTABLESPACE_OID)
  {
                                                  
    if (backendId == InvalidBackendId)
    {
      if (forkNumber != MAIN_FORKNUM)
      {
        path = psprintf("base/%u/%u_%s", dbNode, relNode, forkNames[forkNumber]);
      }
      else
      {
        path = psprintf("base/%u/%u", dbNode, relNode);
      }
    }
    else
    {
      if (forkNumber != MAIN_FORKNUM)
      {
        path = psprintf("base/%u/t%d_%u_%s", dbNode, backendId, relNode, forkNames[forkNumber]);
      }
      else
      {
        path = psprintf("base/%u/t%d_%u", dbNode, backendId, relNode);
      }
    }
  }
  else
  {
                                                         
    if (backendId == InvalidBackendId)
    {
      if (forkNumber != MAIN_FORKNUM)
      {
        path = psprintf("pg_tblspc/%u/%s/%u/%u_%s", spcNode, TABLESPACE_VERSION_DIRECTORY, dbNode, relNode, forkNames[forkNumber]);
      }
      else
      {
        path = psprintf("pg_tblspc/%u/%s/%u/%u", spcNode, TABLESPACE_VERSION_DIRECTORY, dbNode, relNode);
      }
    }
    else
    {
      if (forkNumber != MAIN_FORKNUM)
      {
        path = psprintf("pg_tblspc/%u/%s/%u/t%d_%u_%s", spcNode, TABLESPACE_VERSION_DIRECTORY, dbNode, backendId, relNode, forkNames[forkNumber]);
      }
      else
      {
        path = psprintf("pg_tblspc/%u/%s/%u/t%d_%u", spcNode, TABLESPACE_VERSION_DIRECTORY, dbNode, backendId, relNode);
      }
    }
  }
  return path;
}
