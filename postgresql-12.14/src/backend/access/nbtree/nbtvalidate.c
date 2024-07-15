                                                                            
   
                 
                                  
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/regproc.h"
#include "utils/syscache.h"

   
                                  
   
                                                                            
                                                                              
                                                                            
                                  
   
bool
btvalidate(Oid opclassoid)
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
  List *familytypes;
  int usefulgroups;
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
    case BTORDER_PROC:
      ok = check_amproc_signature(procform->amproc, INT4OID, true, 2, 2, procform->amproclefttype, procform->amprocrighttype);
      break;
    case BTSORTSUPPORT_PROC:
      ok = check_amproc_signature(procform->amproc, VOIDOID, true, 1, 1, INTERNALOID);
      break;
    case BTINRANGE_PROC:
      ok = check_amproc_signature(procform->amproc, BOOLOID, true, 5, 5, procform->amproclefttype, procform->amproclefttype, procform->amprocrighttype, BOOLOID, BOOLOID);
      break;
    default:
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with invalid support number %d", opfamilyname, "btree", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
      continue;                                    
    }

    if (!ok)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains function %s with wrong signature for support number %d", opfamilyname, "btree", format_procedure(procform->amproc), procform->amprocnum)));
      result = false;
    }
  }

                                  
  for (i = 0; i < oprlist->n_members; i++)
  {
    HeapTuple oprtup = &oprlist->members[i]->tuple;
    Form_pg_amop oprform = (Form_pg_amop)GETSTRUCT(oprtup);

                                                        
    if (oprform->amopstrategy < 1 || oprform->amopstrategy > BTMaxStrategyNumber)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with invalid strategy number %d", opfamilyname, "btree", format_operator(oprform->amopopr), oprform->amopstrategy)));
      result = false;
    }

                                                  
    if (oprform->amoppurpose != AMOP_SEARCH || OidIsValid(oprform->amopsortfamily))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains invalid ORDER BY specification for operator %s", opfamilyname, "btree", format_operator(oprform->amopopr))));
      result = false;
    }

                                                                    
    if (!check_amop_signature(oprform->amopopr, BOOLOID, oprform->amoplefttype, oprform->amoprighttype))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s contains operator %s with wrong signature", opfamilyname, "btree", format_operator(oprform->amopopr))));
      result = false;
    }
  }

                                                                
  grouplist = identify_opfamily_groups(oprlist, proclist);
  usefulgroups = 0;
  opclassgroup = NULL;
  familytypes = NIL;
  foreach (lc, grouplist)
  {
    OpFamilyOpFuncGroup *thisgroup = (OpFamilyOpFuncGroup *)lfirst(lc);

       
                                                                          
                                                                          
                                                                       
                                                                      
                                                                    
                        
       
    if (thisgroup->operatorset == 0 && thisgroup->functionset == (1 << BTINRANGE_PROC))
    {
      continue;
    }

                                           
    usefulgroups++;

                                                              
    if (thisgroup->lefttype == opcintype && thisgroup->righttype == opcintype)
    {
      opclassgroup = thisgroup;
    }

       
                                                                        
                                                                      
                                             
       
    familytypes = list_append_unique_oid(familytypes, thisgroup->lefttype);
    familytypes = list_append_unique_oid(familytypes, thisgroup->righttype);

       
                                                                           
                                                                     
                                                                       
       
    if (thisgroup->operatorset != ((1 << BTLessStrategyNumber) | (1 << BTLessEqualStrategyNumber) | (1 << BTEqualStrategyNumber) | (1 << BTGreaterEqualStrategyNumber) | (1 << BTGreaterStrategyNumber)))
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing operator(s) for types %s and %s", opfamilyname, "btree", format_type_be(thisgroup->lefttype), format_type_be(thisgroup->righttype))));
      result = false;
    }
    if ((thisgroup->functionset & (1 << BTORDER_PROC)) == 0)
    {
      ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing support function for types %s and %s", opfamilyname, "btree", format_type_be(thisgroup->lefttype), format_type_be(thisgroup->righttype))));
      result = false;
    }
  }

                                                            
                                                                   
  if (!opclassgroup)
  {
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing operator(s)", opclassname, "btree")));
    result = false;
  }

     
                                                                    
                                                                        
                                                                            
                                                                   
                                                                          
     
  if (usefulgroups != (list_length(familytypes) * list_length(familytypes)))
  {
    ereport(INFO, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator family \"%s\" of access method %s is missing cross-type operator(s)", opfamilyname, "btree")));
    result = false;
  }

  ReleaseCatCacheList(proclist);
  ReleaseCatCacheList(oprlist);
  ReleaseSysCache(familytup);
  ReleaseSysCache(classtup);

  return result;
}
