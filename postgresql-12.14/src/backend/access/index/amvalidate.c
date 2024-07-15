                                                                            
   
                
                                                                      
   
                                                                
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/htup_details.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "parser/parse_coerce.h"
#include "utils/syscache.h"

   
                                                                             
                                                                          
                                                                          
                                                                              
                                                                             
                                                                          
   
                                                                           
                                                                       
                                                                             
   
List *
identify_opfamily_groups(CatCList *oprlist, CatCList *proclist)
{
  List *result = NIL;
  OpFamilyOpFuncGroup *thisgroup;
  Form_pg_amop oprform;
  Form_pg_amproc procform;
  int io, ip;

                                                                           
  if (!oprlist->ordered || !proclist->ordered)
  {
    elog(ERROR, "cannot validate operator family without ordered data");
  }

     
                                                                         
                                                                     
                    
     
  thisgroup = NULL;
  io = ip = 0;
  if (io < oprlist->n_members)
  {
    oprform = (Form_pg_amop)GETSTRUCT(&oprlist->members[io]->tuple);
    io++;
  }
  else
  {
    oprform = NULL;
  }
  if (ip < proclist->n_members)
  {
    procform = (Form_pg_amproc)GETSTRUCT(&proclist->members[ip]->tuple);
    ip++;
  }
  else
  {
    procform = NULL;
  }

  while (oprform || procform)
  {
    if (oprform && thisgroup && oprform->amoplefttype == thisgroup->lefttype && oprform->amoprighttype == thisgroup->righttype)
    {
                                                                     

                                                           
      if (oprform->amopstrategy > 0 && oprform->amopstrategy < 64)
      {
        thisgroup->operatorset |= ((uint64)1) << oprform->amopstrategy;
      }

      if (io < oprlist->n_members)
      {
        oprform = (Form_pg_amop)GETSTRUCT(&oprlist->members[io]->tuple);
        io++;
      }
      else
      {
        oprform = NULL;
      }
      continue;
    }

    if (procform && thisgroup && procform->amproclefttype == thisgroup->lefttype && procform->amprocrighttype == thisgroup->righttype)
    {
                                                                      

                                                           
      if (procform->amprocnum > 0 && procform->amprocnum < 64)
      {
        thisgroup->functionset |= ((uint64)1) << procform->amprocnum;
      }

      if (ip < proclist->n_members)
      {
        procform = (Form_pg_amproc)GETSTRUCT(&proclist->members[ip]->tuple);
        ip++;
      }
      else
      {
        procform = NULL;
      }
      continue;
    }

                              
    thisgroup = (OpFamilyOpFuncGroup *)palloc(sizeof(OpFamilyOpFuncGroup));
    if (oprform && (!procform || (oprform->amoplefttype < procform->amproclefttype || (oprform->amoplefttype == procform->amproclefttype && oprform->amoprighttype < procform->amprocrighttype))))
    {
      thisgroup->lefttype = oprform->amoplefttype;
      thisgroup->righttype = oprform->amoprighttype;
    }
    else
    {
      thisgroup->lefttype = procform->amproclefttype;
      thisgroup->righttype = procform->amprocrighttype;
    }
    thisgroup->operatorset = thisgroup->functionset = 0;
    result = lappend(result, thisgroup);
  }

  return result;
}

   
                                                                            
                                               
   
                                                                              
                                                                          
                                                                    
   
bool
check_amproc_signature(Oid funcid, Oid restype, bool exact, int minargs, int maxargs, ...)
{
  bool result = true;
  HeapTuple tp;
  Form_pg_proc procform;
  va_list ap;
  int i;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(tp);

  if (procform->prorettype != restype || procform->proretset || procform->pronargs < minargs || procform->pronargs > maxargs)
  {
    result = false;
  }

  va_start(ap, maxargs);
  for (i = 0; i < maxargs; i++)
  {
    Oid argtype = va_arg(ap, Oid);

    if (i >= procform->pronargs)
    {
      continue;
    }
    if (exact ? (argtype != procform->proargtypes.values[i]) : !IsBinaryCoercible(argtype, procform->proargtypes.values[i]))
    {
      result = false;
    }
  }
  va_end(ap);

  ReleaseSysCache(tp);
  return result;
}

   
                                                                              
                                    
   
                                                                               
                                                                           
                                                                   
   
bool
check_amop_signature(Oid opno, Oid restype, Oid lefttype, Oid righttype)
{
  bool result = true;
  HeapTuple tp;
  Form_pg_operator opform;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (!HeapTupleIsValid(tp))                       
  {
    elog(ERROR, "cache lookup failed for operator %u", opno);
  }
  opform = (Form_pg_operator)GETSTRUCT(tp);

  if (opform->oprresult != restype || opform->oprkind != 'b' || opform->oprleft != lefttype || opform->oprright != righttype)
  {
    result = false;
  }

  ReleaseSysCache(tp);
  return result;
}

   
                                                                   
   
bool
opfamily_can_sort_type(Oid opfamilyoid, Oid datatypeoid)
{
  bool result = false;
  CatCList *opclist;
  int i;

     
                                                                             
                                                                            
                                                                  
     
  opclist = SearchSysCacheList1(CLAAMNAMENSP, ObjectIdGetDatum(BTREE_AM_OID));

  for (i = 0; i < opclist->n_members; i++)
  {
    HeapTuple classtup = &opclist->members[i]->tuple;
    Form_pg_opclass classform = (Form_pg_opclass)GETSTRUCT(classtup);

    if (classform->opcfamily == opfamilyoid && classform->opcintype == datatypeoid)
    {
      result = true;
      break;
    }
  }

  ReleaseCatCacheList(opclist);

  return result;
}
