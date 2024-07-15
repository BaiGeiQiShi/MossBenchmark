                                                                            
   
                  
                                 
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "parser/parse_coerce.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/regproc.h"
#include "utils/syscache.h"

static bool
check_hash_func_signature(Oid funcid, int16 amprocnum, Oid argtype);

   
                                 
   
                                                                            
                                                                              
                                                                            
                                  
   
bool
hashvalidate(Oid opclassoid)
{
  bool result = true;
  HeapTuple classtup;
  Form_pg_opclass classform;
  Oid opfamilyoid;
  Oid opcintype;
  char *opclassname;
  HeapTuple familytup;
  Form_pg_opfamily familyform;
  char *opfamilyname;
  CatCList *proclist, *oprlist;
  List *grouplist;
  OpFamilyOpFuncGroup *opclassgroup;
  List *hashabletypes = NIL;
  int i;
  ListCell *lc;

                                 
  classtup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclassoid));
  if (!HeapTupleIsValid(classtup))
  {
    elog(ERROR, "cache lookup failed for operator class %u", opclassoid);
  }
  classform = (Form_pg_opclass)GETSTRUCT(classtup);

  opfamilyoid = classform->opcfamily;
  opcintype = classform->opcintype;
  opclassname = NameStr(classform->opcname);

                                  
  familytup = SearchSysCache1(OPFAMILYOID, ObjectIdGetDatum(opfamilyoid));
  if (!HeapTupleIsValid(familytup))
  {
    elog(ERROR, "cache lookup failed for operator family %u", opfamilyoid);
  }
  familyform = (Form_pg_opfamily)GETSTRUCT(familytup);

  opfamilyname = NameStr(familyform->opfname);

                                                                 
  oprlist = SearchSysCacheList1(AMOPSTRATEGY, ObjectIdGetDatum(opfamilyoid));
  proclist = SearchSysCacheList1(AMPROCNUM, ObjectIdGetDatum(opfamilyoid));

                                          
  for (i = 0; i < proclist->n_members; i++)
  {
    HeapTuple proctup = &proclist->members[i]->tuple;
    Form_pg_amproc procform = (Form_pg_amproc)GETSTRUCT(proctup);

       
                                                                        
             
       
    if (procform->amproclefttype != procform->amprocrighttype)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains support function %s with different left and right input types", opfamilyname, "hash", format_procedure(procform->amproc))));
      result = false;
    }

                                                         
    switch (procform->amprocnum)
    {
    case HASHSTANDARD_PROC:
    case HASHEXTENDED_PROC:
      if (!check_hash_func_signature(procform->amproc, procform->amprocnum, procform->amproclefttype))
      {
        ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with wrong signature for support number %d", opfamilyname, "hash", format_procedure(procform->amproc), procform->amprocnum)));
        result = false;
      }
      else
      {
                                              
        hashabletypes = list_append_unique_oid(hashabletypes, procform->amproclefttype);
      }
      break;
    default:
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with invalid support number %d", opfamilyname, "hash", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
      break;
    }
  }

                                  
  for (i = 0; i < oprlist->n_members; i++)
  {
    HeapTuple oprtup = &oprlist->members[i]->tuple;
    Form_pg_amop oprform = (Form_pg_amop)GETSTRUCT(oprtup);

                                                        
    if (oprform->amopstrategy < 1 || oprform->amopstrategy > HTMaxStrategyNumber)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with invalid strategy number %d", opfamilyname, "hash", format_operator(oprform->amopopr), oprform->amopstrategy)));
      result = false;
    }

                                                 
    if (oprform->amoppurpose != AMOP_SEARCH || OidIsValid(oprform->amopsortfamily))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains invalid ORDER BY specification for operator %s", opfamilyname, "hash", format_operator(oprform->amopopr))));
      result = false;
    }

                                                                   
    if (!check_amop_signature(oprform->amopopr, BOOLOID, oprform->amoplefttype, oprform->amoprighttype))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with wrong signature", opfamilyname, "hash", format_operator(oprform->amopopr))));
      result = false;
    }

                                                                   
    if (!list_member_oid(hashabletypes, oprform->amoplefttype) || !list_member_oid(hashabletypes, oprform->amoprighttype))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s lacks support function for operator %s", opfamilyname, "hash", format_operator(oprform->amopopr))));
      result = false;
    }
  }

                                                                
  grouplist = identify_opfamily_groups(oprlist, proclist);
  opclassgroup = NULL;
  foreach (lc, grouplist)
  {
    OpFamilyOpFuncGroup *thisgroup = (OpFamilyOpFuncGroup *)lfirst(lc);

                                                              
    if (thisgroup->lefttype == opcintype && thisgroup->righttype == opcintype)
    {
      opclassgroup = thisgroup;
    }

       
                                                                        
                                                                        
                  
       
    if (thisgroup->operatorset != (1 << HTEqualStrategyNumber))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing operator(s) for types %s and %s", opfamilyname, "hash", format_type_be(thisgroup->lefttype), format_type_be(thisgroup->righttype))));
      result = false;
    }
  }

                                                            
                                                                   
  if (!opclassgroup)
  {
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing operator(s)", opclassname, "hash")));
    result = false;
  }

     
                                                                    
                                                                        
                                                                     
                                           
     
  if (list_length(grouplist) != list_length(hashabletypes) * list_length(hashabletypes))
  {
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing cross-type operator(s)", opfamilyname, "hash")));
    result = false;
  }

  ReleaseCatCacheList(proclist);
  ReleaseCatCacheList(oprlist);
  ReleaseSysCache(familytup);
  ReleaseSysCache(classtup);

  return result;
}

   
                                                                          
                                               
   
static bool
check_hash_func_signature(Oid funcid, int16 amprocnum, Oid argtype)
{
  bool result = true;
  Oid restype;
  int16 nargs;
  HeapTuple tp;
  Form_pg_proc procform;

  switch (amprocnum)
  {
  case HASHSTANDARD_PROC:
    restype = INT4OID;
    nargs = 1;
    break;

  case HASHEXTENDED_PROC:
    restype = INT8OID;
    nargs = 2;
    break;

  default:
    elog(ERROR, "invalid amprocnum");
  }

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(tp);

  if (procform->prorettype != restype || procform->proretset || procform->pronargs != nargs)
  {
    result = false;
  }

  if (!IsBinaryCoercible(argtype, procform->proargtypes.values[0]))
  {
       
                                                                         
                                                                          
                                                                          
                                                                          
                                                                        
                                                                      
                                                                
       
    if ((funcid == F_HASHINT4 || funcid == F_HASHINT4EXTENDED) && (argtype == DATEOID || argtype == XIDOID || argtype == CIDOID))
                                           ;
    else if ((funcid == F_TIMESTAMP_HASH || funcid == F_TIMESTAMP_HASH_EXTENDED) && argtype == TIMESTAMPTZOID)
                                                 ;
    else if ((funcid == F_HASHCHAR || funcid == F_HASHCHAREXTENDED) && argtype == BOOLOID)
                                           ;
    else if ((funcid == F_HASHVARLENA || funcid == F_HASHVARLENAEXTENDED) && argtype == BYTEAOID)
                                              ;
    else
    {
      result = false;
    }
  }

                                                                          
  if (nargs == 2 && procform->proargtypes.values[1] != INT8OID)
  {
    result = false;
  }

  ReleaseSysCache(tp);
  return result;
}
