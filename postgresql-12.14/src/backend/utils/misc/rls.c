                                                                            
   
         
                                     
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "access/transam.h"
#include "catalog/namespace.h"
#include "catalog/pg_class.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rls.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

   
                    
   
                                                                             
                                                                           
                                                                                 
                                                                              
                                                                                 
                                                                               
            
   
                                                                           
                                         
   
                                                                                
                                                                         
                                                                               
                                                                               
                                                                             
   
int
check_enable_rls(Oid relid, Oid checkAsUser, bool noError)
{
  Oid user_id = checkAsUser ? checkAsUser : GetUserId();
  HeapTuple tuple;
  Form_pg_class classform;
  bool relrowsecurity;
  bool relforcerowsecurity;
  bool amowner;

                                            
  if (relid < (Oid)FirstNormalObjectId)
  {
    return RLS_NONE;
  }

                                                                     
  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    return RLS_NONE;
  }
  classform = (Form_pg_class)GETSTRUCT(tuple);

  relrowsecurity = classform->relrowsecurity;
  relforcerowsecurity = classform->relforcerowsecurity;

  ReleaseSysCache(tuple);

                                                       
  if (!relrowsecurity)
  {
    return RLS_NONE;
  }

     
                                                                         
                                   
     
                                                                       
                                              
     
  if (has_bypassrls_privilege(user_id))
  {
    return RLS_NONE_ENV;
  }

     
                                                                             
                                                                    
                      
     
                                                                       
                                              
     
  amowner = pg_class_ownercheck(relid, user_id);
  if (amowner)
  {
       
                                                                        
                                                                         
                                                                           
                     
       
                                                                         
                                                                          
                                                                        
                                        
       
                                                                           
                                                                           
                         
       
    if (!relforcerowsecurity || InNoForceRLSOperation())
    {
      return RLS_NONE_ENV;
    }
  }

     
                                                                           
                                        
     
  if (!row_security && !noError)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("query would be affected by row-level security policy for table \"%s\"", get_rel_name(relid)), amowner ? errhint("To disable the policy for the table's owner, use ALTER TABLE NO FORCE ROW LEVEL SECURITY.") : 0));
  }

                                                      
  return RLS_ENABLED;
}

   
                       
   
                                                              
                                                            
   
Datum
row_security_active(PG_FUNCTION_ARGS)
{
              
  Oid tableoid = PG_GETARG_OID(0);
  int rls_status;

  rls_status = check_enable_rls(tableoid, InvalidOid, true);
  PG_RETURN_BOOL(rls_status == RLS_ENABLED);
}

Datum
row_security_active_name(PG_FUNCTION_ARGS)
{
                         
  text *tablename = PG_GETARG_TEXT_PP(0);
  RangeVar *tablerel;
  Oid tableoid;
  int rls_status;

                                                                          
  tablerel = makeRangeVarFromNameList(textToQualifiedNameList(tablename));
  tableoid = RangeVarGetRelid(tablerel, NoLock, false);

  rls_status = check_enable_rls(tableoid, InvalidOid, true);
  PG_RETURN_BOOL(rls_status == RLS_ENABLED);
}
