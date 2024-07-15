                                                                            
   
                   
                                 
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/brin_internal.h"
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/regproc.h"

   
                                 
   
                                                                            
                                                                              
                                                                            
                                  
   
bool
brinvalidate(Oid opclassoid)
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
  uint64 allfuncs = 0;
  uint64 allops = 0;
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

                                                         
    switch (procform->amprocnum)
    {
    case BRIN_PROCNUM_OPCINFO:
      ok = check_amproc_signature(procform->amproc, INTERNALOID, true, 1, 1, INTERNALOID);
      break;
    case BRIN_PROCNUM_ADDVALUE:
      ok = check_amproc_signature(procform->amproc, BOOLOID, true, 4, 4, INTERNALOID, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    case BRIN_PROCNUM_CONSISTENT:
      ok = check_amproc_signature(procform->amproc, BOOLOID, true, 3, 3, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    case BRIN_PROCNUM_UNION:
      ok = check_amproc_signature(procform->amproc, BOOLOID, true, 3, 3, INTERNALOID, INTERNALOID, INTERNALOID);
      break;
    default:
                                                             
      if (procform->amprocnum < BRIN_FIRST_OPTIONAL_PROCNUM || procform->amprocnum > BRIN_LAST_OPTIONAL_PROCNUM)
      {
        ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with invalid support number %d", opfamilyname, "brin", format_procedure(procform->amproc), procform->amprocnum)));
        result = false;
        continue;                                          
      }
                                                                  
      ok = true;
      break;
    }

    if (!ok)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with wrong signature for support number %d", opfamilyname, "brin", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
    }

                                                            
    allfuncs |= ((uint64)1) << procform->amprocnum;
  }

                                  
  for (i = 0; i < oprlist->n_members; i++)
  {
    HeapTuple oprtup = &oprlist->members[i]->tuple;
    Form_pg_amop oprform = (Form_pg_amop)GETSTRUCT(oprtup);

                                                        
    if (oprform->amopstrategy < 1 || oprform->amopstrategy > 63)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with invalid strategy number %d", opfamilyname, "brin", format_operator(oprform->amopopr), oprform->amopstrategy)));
      result = false;
    }
    else
    {
         
                                                                      
                                                                       
                                                                         
                                                                      
                                                                   
                                                                     
                                                                
                                                                     
                                                            
         
      if (oprform->amoplefttype == oprform->amoprighttype)
      {
        allops |= ((uint64)1) << oprform->amopstrategy;
      }
    }

                                                 
    if (oprform->amoppurpose != AMOP_SEARCH || OidIsValid(oprform->amopsortfamily))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains invalid ORDER BY specification for operator %s", opfamilyname, "brin", format_operator(oprform->amopopr))));
      result = false;
    }

                                                                   
    if (!check_amop_signature(oprform->amopopr, BOOLOID, oprform->amoplefttype, oprform->amoprighttype))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with wrong signature", opfamilyname, "brin", format_operator(oprform->amopopr))));
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

       
                                                                          
                                                                        
                                                                           
                                                                        
                                       
       
    if (thisgroup->functionset == 0 && thisgroup->lefttype != thisgroup->righttype)
    {
      continue;
    }

       
                                                                      
                                                              
       
    if (thisgroup->operatorset != allops)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing operator(s) for types %s and %s", opfamilyname, "brin", format_type_be(thisgroup->lefttype), format_type_be(thisgroup->righttype))));
      result = false;
    }
    if (thisgroup->functionset != allfuncs)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing support function(s) for types %s and %s", opfamilyname, "brin", format_type_be(thisgroup->lefttype), format_type_be(thisgroup->righttype))));
      result = false;
    }
  }

                                                           
  if (!opclassgroup || opclassgroup->operatorset != allops)
  {
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing operator(s)", opclassname, "brin")));
    result = false;
  }
  for (i = 1; i <= BRIN_MANDATORY_NPROCS; i++)
  {
    if (opclassgroup && (opclassgroup->functionset & (((int64)1) << i)) != 0)
    {
      continue;             
    }
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing support function %d", opclassname, "brin", i)));
    result = false;
  }

  ReleaseCatCacheList(proclist);
  ReleaseCatCacheList(oprlist);
  ReleaseSysCache(familytup);
  ReleaseSysCache(classtup);

  return result;
}
