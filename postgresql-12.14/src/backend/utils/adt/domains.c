                                                                            
   
             
                                     
   
                                                                          
                                                                       
                                                                         
                                                                      
                                   
   
                                                                              
                                                                             
                                                      
                                                                             
                                                                       
                                                             
                                                                         
                         
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/expandeddatum.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

   
                                                  
   
typedef struct DomainIOData
{
  Oid domain_type;
                                                      
  Oid typiofunc;
  Oid typioparam;
  int32 typtypmod;
  FmgrInfo proc;
                                                             
  DomainConstraintRef constraint_ref;
                                                   
  ExprContext *econtext;
                                       
  MemoryContext mcxt;
} DomainIOData;

   
                                                                    
   
                                                                      
                                                                     
                                                                     
                                                 
   
static DomainIOData *
domain_state_setup(Oid domainType, bool binary, MemoryContext mcxt)
{
  DomainIOData *my_extra;
  TypeCacheEntry *typentry;
  Oid baseType;

  my_extra = (DomainIOData *)MemoryContextAlloc(mcxt, sizeof(DomainIOData));

     
                                                                           
                                                                            
                                                                         
                                                                             
                                                 
     
  typentry = lookup_type_cache(domainType, TYPECACHE_DOMAIN_BASE_INFO);
  if (typentry->typtype != TYPTYPE_DOMAIN)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type %s is not a domain", format_type_be(domainType))));
  }

                              
  baseType = typentry->domainBaseType;
  my_extra->typtypmod = typentry->domainBaseTypmod;

                                       
  if (binary)
  {
    getTypeBinaryInputInfo(baseType, &my_extra->typiofunc, &my_extra->typioparam);
  }
  else
  {
    getTypeInputInfo(baseType, &my_extra->typiofunc, &my_extra->typioparam);
  }
  fmgr_info_cxt(my_extra->typiofunc, &my_extra->proc, mcxt);

                                      
  InitDomainConstraintRef(domainType, &my_extra->constraint_ref, mcxt, true);

                                                 
  my_extra->econtext = NULL;
  my_extra->mcxt = mcxt;

                        
  my_extra->domain_type = domainType;

  return my_extra;
}

   
                                                 
   
                                                                      
                                                                       
                                                      
   
static void
domain_check_input(Datum value, bool isnull, DomainIOData *my_extra)
{
  ExprContext *econtext = my_extra->econtext;
  ListCell *l;

                                                
  UpdateDomainConstraintRef(&my_extra->constraint_ref);

  foreach (l, my_extra->constraint_ref.constraints)
  {
    DomainConstraintState *con = (DomainConstraintState *)lfirst(l);

    switch (con->constrainttype)
    {
    case DOM_CONSTRAINT_NOTNULL:
      if (isnull)
      {
        ereport(ERROR, (errcode(ERRCODE_NOT_NULL_VIOLATION), errmsg("domain %s does not allow null values", format_type_be(my_extra->domain_type)), errdatatype(my_extra->domain_type)));
      }
      break;
    case DOM_CONSTRAINT_CHECK:
    {
                                                  
      if (econtext == NULL)
      {
        MemoryContext oldcontext;

        oldcontext = MemoryContextSwitchTo(my_extra->mcxt);
        econtext = CreateStandaloneExprContext();
        MemoryContextSwitchTo(oldcontext);
        my_extra->econtext = econtext;
      }

         
                                                            
                                                             
                                                               
                                                             
                                                              
                                                           
                                                           
                          
         
      econtext->domainValue_datum = MakeExpandedObjectReadOnly(value, isnull, my_extra->constraint_ref.tcache->typlen);
      econtext->domainValue_isNull = isnull;

      if (!ExecCheck(con->check_exprstate, econtext))
      {
        ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("value for domain %s violates check constraint \"%s\"", format_type_be(my_extra->domain_type), con->name), errdomainconstraint(my_extra->domain_type, con->name)));
      }
      break;
    }
    default:
      elog(ERROR, "unrecognized constraint type: %d", (int)con->constrainttype);
      break;
    }
  }

     
                                                                      
                                                                     
                                            
     
  if (econtext)
  {
    ReScanExprContext(econtext);
  }
}

   
                                                   
   
Datum
domain_in(PG_FUNCTION_ARGS)
{
  char *string;
  Oid domainType;
  DomainIOData *my_extra;
  Datum value;

     
                                                                          
                                                                             
                                                                       
     
  if (PG_ARGISNULL(0))
  {
    string = NULL;
  }
  else
  {
    string = PG_GETARG_CSTRING(0);
  }
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }
  domainType = PG_GETARG_OID(1);

     
                                                                          
                                                                         
                                             
     
  my_extra = (DomainIOData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->domain_type != domainType)
  {
    my_extra = domain_state_setup(domainType, false, fcinfo->flinfo->fn_mcxt);
    fcinfo->flinfo->fn_extra = (void *)my_extra;
  }

     
                                                                    
     
  value = InputFunctionCall(&my_extra->proc, string, my_extra->typioparam, my_extra->typtypmod);

     
                                                                  
     
  domain_check_input(value, (string == NULL), my_extra);

  if (string == NULL)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_DATUM(value);
  }
}

   
                                                            
   
Datum
domain_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf;
  Oid domainType;
  DomainIOData *my_extra;
  Datum value;

     
                                                                            
                                                                             
                                                                       
     
  if (PG_ARGISNULL(0))
  {
    buf = NULL;
  }
  else
  {
    buf = (StringInfo)PG_GETARG_POINTER(0);
  }
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }
  domainType = PG_GETARG_OID(1);

     
                                                                          
                                                                         
                                             
     
  my_extra = (DomainIOData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->domain_type != domainType)
  {
    my_extra = domain_state_setup(domainType, true, fcinfo->flinfo->fn_mcxt);
    fcinfo->flinfo->fn_extra = (void *)my_extra;
  }

     
                                                                      
     
  value = ReceiveFunctionCall(&my_extra->proc, buf, my_extra->typioparam, my_extra->typtypmod);

     
                                                                  
     
  domain_check_input(value, (buf == NULL), my_extra);

  if (buf == NULL)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_DATUM(value);
  }
}

   
                                                                    
                                                                     
                                                                     
                                    
   
void
domain_check(Datum value, bool isnull, Oid domainType, void **extra, MemoryContext mcxt)
{
  DomainIOData *my_extra = NULL;

  if (mcxt == NULL)
  {
    mcxt = CurrentMemoryContext;
  }

     
                                                                          
                                                                         
                                             
     
  if (extra)
  {
    my_extra = (DomainIOData *)*extra;
  }
  if (my_extra == NULL || my_extra->domain_type != domainType)
  {
    my_extra = domain_state_setup(domainType, true, mcxt);
    if (extra)
    {
      *extra = (void *)my_extra;
    }
  }

     
                                                                  
     
  domain_check_input(value, isnull, my_extra);
}

   
                                                                      
                                 
   
int
errdatatype(Oid datatypeOid)
{
  HeapTuple tup;
  Form_pg_type typtup;

  tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(datatypeOid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for type %u", datatypeOid);
  }
  typtup = (Form_pg_type)GETSTRUCT(tup);

  err_generic_string(PG_DIAG_SCHEMA_NAME, get_namespace_name(typtup->typnamespace));
  err_generic_string(PG_DIAG_DATATYPE_NAME, NameStr(typtup->typname));

  ReleaseSysCache(tup);

  return 0;                                   
}

   
                                                                 
                                                                                
   
int
errdomainconstraint(Oid datatypeOid, const char *conname)
{
  errdatatype(datatypeOid);
  err_generic_string(PG_DIAG_CONSTRAINT_NAME, conname);

  return 0;                                   
}
