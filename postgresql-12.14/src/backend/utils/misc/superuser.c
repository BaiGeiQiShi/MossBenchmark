                                                                            
   
               
                                                                            
   
                                                                 
                                                              
                                                                      
                               
   
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_authid.h"
#include "utils/inval.h"
#include "utils/syscache.h"
#include "miscadmin.h"

   
                                                                        
                                                                       
                                                                      
                                                             
   
static Oid last_roleid = InvalidOid;                                    
static bool last_roleid_is_super = false;
static bool roleid_callback_registered = false;

static void
RoleidCallback(Datum arg, int cacheid, uint32 hashvalue);

   
                                                                            
   
bool
superuser(void)
{
  return superuser_arg(GetUserId());
}

   
                                                        
   
bool
superuser_arg(Oid roleid)
{
  bool result;
  HeapTuple rtup;

                               
  if (OidIsValid(last_roleid) && last_roleid == roleid)
  {
    return last_roleid_is_super;
  }

                                                               
  if (!IsUnderPostmaster && roleid == BOOTSTRAP_SUPERUSERID)
  {
    return true;
  }

                                                
  rtup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (HeapTupleIsValid(rtup))
  {
    result = ((Form_pg_authid)GETSTRUCT(rtup))->rolsuper;
    ReleaseSysCache(rtup);
  }
  else
  {
                                                    
    result = false;
  }

                                                                
  if (!roleid_callback_registered)
  {
    CacheRegisterSyscacheCallback(AUTHOID, RoleidCallback, (Datum)0);
    roleid_callback_registered = true;
  }

                                      
  last_roleid = roleid;
  last_roleid_is_super = result;

  return result;
}

   
                  
                                     
   
static void
RoleidCallback(Datum arg, int cacheid, uint32 hashvalue)
{
                                                                       
  last_roleid = InvalidOid;
}
