                                                                            
   
                  
                                 
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/gist_private.h"
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/regproc.h"
#include "utils/syscache.h"

   
                                 
   
bool
gistvalidate(Oid opclassoid)
{
  bool result = true;
  HeapTuple classtup;
  Form_pg_opclass classform;
  Oid opfamilyoid;
  Oid opcintype;
  Oid opckeytype;
  char *opclassname;
  HeapTuple familytup;
  Form_pg_opfamily familyform;
  char *opfamilyname;
  CatCList *proclist, *oprlist;
  List *grouplist;
  OpFamilyOpFuncGroup *opclassgroup;
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
  opckeytype = classform->opckeytype;
  if (!OidIsValid(opckeytype))
  {
    opckeytype = opcintype;
  }
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
    bool ok;

       
                                                                     
                        
       
    if (procform->amproclefttype != procform->amprocrighttype)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains support function %s with different left and right input types", opfamilyname, "gist", format_procedure(procform->amproc))));
      result = false;
    }

       
                                                                           
                                                                
       
    if (procform->amproclefttype != opcintype)
    {
      continue;
    }

                                                         
    switch (procform->amprocnum)
    {
    case GIST_CONSISTENT_PROC:
      ok = check_amproc_signature(procform->amproc, BOOLOID, false, 5, 5, INTERNALOID, opcintype, INT2OID, OIDOID, INTERNALOID);
      break;
    case GIST_UNION_PROC:
      ok = check_amproc_signature(procform->amproc, opckeytype, false, 2, 2, INTERNALOID, INTERNALOID);
      break;
    case GIST_COMPRESS_PROC:
    case GIST_DECOMPRESS_PROC:
    case GIST_FETCH_PROC:
      ok = check_amproc_signature(procform->amproc, INTERNALOID, true, 1, 1, INTERNALOID);
      break;
    case GIST_PENALTY_PROC:
      ok = check_amproc_signature(procform->amproc, INTERNALOID, true, 3, 3, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    case GIST_PICKSPLIT_PROC:
      ok = check_amproc_signature(procform->amproc, INTERNALOID, true, 2, 2, INTERNALOID, INTERNALOID);
      break;
    case GIST_EQUAL_PROC:
      ok = check_amproc_signature(procform->amproc, INTERNALOID, false, 3, 3, opckeytype, opckeytype, INTERNALOID);
      break;
    case GIST_DISTANCE_PROC:
      ok = check_amproc_signature(procform->amproc, FLOAT8OID, false, 5, 5, INTERNALOID, opcintype, INT2OID, OIDOID, INTERNALOID);
      break;
    default:
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with invalid support number %d", opfamilyname, "gist", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
      continue;                                    
    }

    if (!ok)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with wrong signature for support number %d", opfamilyname, "gist", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
    }
  }

                                  
  for (i = 0; i < oprlist->n_members; i++)
  {
    HeapTuple oprtup = &oprlist->members[i]->tuple;
    Form_pg_amop oprform = (Form_pg_amop)GETSTRUCT(oprtup);
    Oid op_rettype;

                                                              
    if (oprform->amopstrategy < 1)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with invalid strategy number %d", opfamilyname, "gist", format_operator(oprform->amopopr), oprform->amopstrategy)));
      result = false;
    }

                                          
    if (oprform->amoppurpose != AMOP_SEARCH)
    {
                                                    
      if (!OidIsValid(get_opfamily_proc(opfamilyoid, oprform->amoplefttype, oprform->amoplefttype, GIST_DISTANCE_PROC)))
      {
        ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains unsupported ORDER BY specification for operator %s", opfamilyname, "gist", format_operator(oprform->amopopr))));
        result = false;
      }
                                                                         
      op_rettype = get_op_rettype(oprform->amopopr);
      if (!opfamily_can_sort_type(oprform->amopsortfamily, op_rettype))
      {
        ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains incorrect ORDER BY opfamily specification for operator %s", opfamilyname, "gist", format_operator(oprform->amopopr))));
        result = false;
      }
    }
    else
    {
                                                    
      op_rettype = BOOLOID;
    }

                                  
    if (!check_amop_signature(oprform->amopopr, op_rettype, oprform->amoplefttype, oprform->amoprighttype))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with wrong signature", opfamilyname, "gist", format_operator(oprform->amopopr))));
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

       
                                                                           
                                                                        
                                                                           
                                                                           
                                                                         
                            
       
  }

                                                           
  for (i = 1; i <= GISTNProcs; i++)
  {
    if (opclassgroup && (opclassgroup->functionset & (((uint64)1) << i)) != 0)
    {
      continue;             
    }
    if (i == GIST_DISTANCE_PROC || i == GIST_FETCH_PROC || i == GIST_COMPRESS_PROC || i == GIST_DECOMPRESS_PROC)
    {
      continue;                       
    }
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing support function %d", opclassname, "gist", i)));
    result = false;
  }

  ReleaseCatCacheList(proclist);
  ReleaseCatCacheList(oprlist);
  ReleaseSysCache(familytup);
  ReleaseSysCache(classtup);

  return result;
}
