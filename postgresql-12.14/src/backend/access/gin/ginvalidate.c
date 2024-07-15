                                                                            
   
                 
                                
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/gin_private.h"
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/regproc.h"

   
                                
   
bool
ginvalidate(Oid opclassoid)
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
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains support function %s with different left and right input types", opfamilyname, "gin", format_procedure(procform->amproc))));
      result = false;
    }

       
                                                                           
                                                                
       
    if (procform->amproclefttype != opcintype)
    {
      continue;
    }

                                                         
    switch (procform->amprocnum)
    {
    case GIN_COMPARE_PROC:
      ok = check_amproc_signature(procform->amproc, INT4OID, false, 2, 2, opckeytype, opckeytype);
      break;
    case GIN_EXTRACTVALUE_PROC:
                                         
      ok = check_amproc_signature(procform->amproc, INTERNALOID, false, 2, 3, opcintype, INTERNALOID, INTERNALOID);
      break;
    case GIN_EXTRACTQUERY_PROC:
                                                        
      ok = check_amproc_signature(procform->amproc, INTERNALOID, false, 5, 7, opcintype, INTERNALOID, INT2OID, INTERNALOID, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    case GIN_CONSISTENT_PROC:
                                                       
      ok = check_amproc_signature(procform->amproc, BOOLOID, false, 6, 8, INTERNALOID, INT2OID, opcintype, INT4OID, INTERNALOID, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    case GIN_COMPARE_PARTIAL_PROC:
      ok = check_amproc_signature(procform->amproc, INT4OID, false, 4, 4, opckeytype, opckeytype, INT2OID, INTERNALOID);
      break;
    case GIN_TRICONSISTENT_PROC:
      ok = check_amproc_signature(procform->amproc, CHAROID, false, 7, 7, INTERNALOID, INT2OID, opcintype, INT4OID, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    default:
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with invalid support number %d", opfamilyname, "gin", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
      continue;                                    
    }

    if (!ok)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with wrong signature for support number %d", opfamilyname, "gin", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
    }
  }

                                  
  for (i = 0; i < oprlist->n_members; i++)
  {
    HeapTuple oprtup = &oprlist->members[i]->tuple;
    Form_pg_amop oprform = (Form_pg_amop)GETSTRUCT(oprtup);

                                                              
    if (oprform->amopstrategy < 1 || oprform->amopstrategy > 63)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with invalid strategy number %d", opfamilyname, "gin", format_operator(oprform->amopopr), oprform->amopstrategy)));
      result = false;
    }

                                                
    if (oprform->amoppurpose != AMOP_SEARCH || OidIsValid(oprform->amopsortfamily))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains invalid ORDER BY specification for operator %s", opfamilyname, "gin", format_operator(oprform->amopopr))));
      result = false;
    }

                                                                  
    if (!check_amop_signature(oprform->amopopr, BOOLOID, oprform->amoplefttype, oprform->amoprighttype))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with wrong signature", opfamilyname, "gin", format_operator(oprform->amopopr))));
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

                                                           
  for (i = 1; i <= GINNProcs; i++)
  {
    if (opclassgroup && (opclassgroup->functionset & (((uint64)1) << i)) != 0)
    {
      continue;             
    }
    if (i == GIN_COMPARE_PROC || i == GIN_COMPARE_PARTIAL_PROC)
    {
      continue;                      
    }
    if (i == GIN_CONSISTENT_PROC || i == GIN_TRICONSISTENT_PROC)
    {
      continue;                                            
    }
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing support function %d", opclassname, "gin", i)));
    result = false;
  }
  if (!opclassgroup || ((opclassgroup->functionset & (1 << GIN_CONSISTENT_PROC)) == 0 && (opclassgroup->functionset & (1 << GIN_TRICONSISTENT_PROC)) == 0))
  {
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing support function %d or %d", opclassname, "gin", GIN_CONSISTENT_PROC, GIN_TRICONSISTENT_PROC)));
    result = false;
  }

  ReleaseCatCacheList(proclist);
  ReleaseCatCacheList(oprlist);
  ReleaseSysCache(familytup);
  ReleaseSysCache(classtup);

  return result;
}
