                                                                            
   
               
                                                                          
   
                                                                         
                                                                        
   
                  
                                         
   
         
                                                                    
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "bootstrap/bootstrap.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_range.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

                                                          
get_attavgwidth_hook_type get_attavgwidth_hook = NULL;

                                                

   
                  
   
                                                                   
   
                                                                          
   
bool
op_in_opfamily(Oid opno, Oid opfamily)
{
  return SearchSysCacheExists3(AMOPOPID, ObjectIdGetDatum(opno), CharGetDatum(AMOP_SEARCH), ObjectIdGetDatum(opfamily));
}

   
                            
   
                                                                      
                                               
   
                                                                          
   
int
get_op_opfamily_strategy(Oid opno, Oid opfamily)
{
  HeapTuple tp;
  Form_pg_amop amop_tup;
  int result;

  tp = SearchSysCache3(AMOPOPID, ObjectIdGetDatum(opno), CharGetDatum(AMOP_SEARCH), ObjectIdGetDatum(opfamily));
  if (!HeapTupleIsValid(tp))
  {
    return 0;
  }
  amop_tup = (Form_pg_amop)GETSTRUCT(tp);
  result = amop_tup->amopstrategy;
  ReleaseSysCache(tp);
  return result;
}

   
                              
   
                                                                           
                                                           
   
Oid
get_op_opfamily_sortfamily(Oid opno, Oid opfamily)
{
  HeapTuple tp;
  Form_pg_amop amop_tup;
  Oid result;

  tp = SearchSysCache3(AMOPOPID, ObjectIdGetDatum(opno), CharGetDatum(AMOP_ORDER), ObjectIdGetDatum(opfamily));
  if (!HeapTupleIsValid(tp))
  {
    return InvalidOid;
  }
  amop_tup = (Form_pg_amop)GETSTRUCT(tp);
  result = amop_tup->amopsortfamily;
  ReleaseSysCache(tp);
  return result;
}

   
                              
   
                                                                     
                                   
   
                                                                          
                                                          
   
void
get_op_opfamily_properties(Oid opno, Oid opfamily, bool ordering_op, int *strategy, Oid *lefttype, Oid *righttype)
{
  HeapTuple tp;
  Form_pg_amop amop_tup;

  tp = SearchSysCache3(AMOPOPID, ObjectIdGetDatum(opno), CharGetDatum(ordering_op ? AMOP_ORDER : AMOP_SEARCH), ObjectIdGetDatum(opfamily));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "operator %u is not a member of opfamily %u", opno, opfamily);
  }
  amop_tup = (Form_pg_amop)GETSTRUCT(tp);
  *strategy = amop_tup->amopstrategy;
  *lefttype = amop_tup->amoplefttype;
  *righttype = amop_tup->amoprighttype;
  ReleaseSysCache(tp);
}

   
                       
                                                                       
                                                             
   
                                                                       
   
Oid
get_opfamily_member(Oid opfamily, Oid lefttype, Oid righttype, int16 strategy)
{
  HeapTuple tp;
  Form_pg_amop amop_tup;
  Oid result;

  tp = SearchSysCache4(AMOPSTRATEGY, ObjectIdGetDatum(opfamily), ObjectIdGetDatum(lefttype), ObjectIdGetDatum(righttype), Int16GetDatum(strategy));
  if (!HeapTupleIsValid(tp))
  {
    return InvalidOid;
  }
  amop_tup = (Form_pg_amop)GETSTRUCT(tp);
  result = amop_tup->amopopr;
  ReleaseSysCache(tp);
  return result;
}

   
                              
                                                                         
                                                                 
                                                                       
   
                                                                          
                                                                        
   
                                                                            
                                                                             
                                                                              
                                                                             
                                                                            
                                                                              
                                                                     
                                                                              
                                                                         
                                              
   
bool
get_ordering_op_properties(Oid opno, Oid *opfamily, Oid *opcintype, int16 *strategy)
{
  bool result = false;
  CatCList *catlist;
  int i;

                                                 
  *opfamily = InvalidOid;
  *opcintype = InvalidOid;
  *strategy = 0;

     
                                                                           
                                            
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple tuple = &catlist->members[i]->tuple;
    Form_pg_amop aform = (Form_pg_amop)GETSTRUCT(tuple);

                       
    if (aform->amopmethod != BTREE_AM_OID)
    {
      continue;
    }

    if (aform->amopstrategy == BTLessStrategyNumber || aform->amopstrategy == BTGreaterStrategyNumber)
    {
                                                           
      if (aform->amoplefttype == aform->amoprighttype)
      {
                                                    
        *opfamily = aform->amopfamily;
        *opcintype = aform->amoplefttype;
        *strategy = aform->amopstrategy;
        result = true;
        break;
      }
    }
  }

  ReleaseSysCacheList(catlist);

  return result;
}

   
                                   
                                                                 
                                                                  
   
                                                                               
                    
   
                                                                     
                                                                        
   
Oid
get_equality_op_for_ordering_op(Oid opno, bool *reverse)
{
  Oid result = InvalidOid;
  Oid opfamily;
  Oid opcintype;
  int16 strategy;

                                    
  if (get_ordering_op_properties(opno, &opfamily, &opcintype, &strategy))
  {
                                                                   
    result = get_opfamily_member(opfamily, opcintype, opcintype, BTEqualStrategyNumber);
    if (reverse)
    {
      *reverse = (strategy == BTGreaterStrategyNumber);
    }
  }

  return result;
}

   
                                   
                                                               
                                                                  
                                            
   
                                                                         
                                                                           
                                                                           
                                                                           
                                                                            
   
                                                                     
   
Oid
get_ordering_op_for_equality_op(Oid opno, bool use_lhs_type)
{
  Oid result = InvalidOid;
  CatCList *catlist;
  int i;

     
                                                                           
                                     
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple tuple = &catlist->members[i]->tuple;
    Form_pg_amop aform = (Form_pg_amop)GETSTRUCT(tuple);

                       
    if (aform->amopmethod != BTREE_AM_OID)
    {
      continue;
    }

    if (aform->amopstrategy == BTEqualStrategyNumber)
    {
                                                                     
      Oid typid;

      typid = use_lhs_type ? aform->amoplefttype : aform->amoprighttype;
      result = get_opfamily_member(aform->amopfamily, typid, typid, BTLessStrategyNumber);
      if (OidIsValid(result))
      {
        break;
      }
                                                                     
    }
  }

  ReleaseSysCacheList(catlist);

  return result;
}

   
                            
                                                                         
                                                             
   
                                                                             
                                                                             
                                                              
   
                                                                        
                                                                          
                                                                         
                                                                              
                                                                           
                                                                          
                                                                            
                                                                           
                                                          
   
List *
get_mergejoin_opfamilies(Oid opno)
{
  List *result = NIL;
  CatCList *catlist;
  int i;

     
                                                                           
                                     
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple tuple = &catlist->members[i]->tuple;
    Form_pg_amop aform = (Form_pg_amop)GETSTRUCT(tuple);

                                
    if (aform->amopmethod == BTREE_AM_OID && aform->amopstrategy == BTEqualStrategyNumber)
    {
      result = lappend_oid(result, aform->amopfamily);
    }
  }

  ReleaseSysCacheList(catlist);

  return result;
}

   
                                 
                                                                          
                                                            
   
                                                                         
                                                                           
                                                       
   
                                                                           
                                                                  
   
                                                                         
                                                                              
   
bool
get_compatible_hash_operators(Oid opno, Oid *lhs_opno, Oid *rhs_opno)
{
  bool result = false;
  CatCList *catlist;
  int i;

                                                     
  if (lhs_opno)
  {
    *lhs_opno = InvalidOid;
  }
  if (rhs_opno)
  {
    *rhs_opno = InvalidOid;
  }

     
                                                                           
                                                                      
                                                     
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple tuple = &catlist->members[i]->tuple;
    Form_pg_amop aform = (Form_pg_amop)GETSTRUCT(tuple);

    if (aform->amopmethod == HASH_AM_OID && aform->amopstrategy == HTEqualStrategyNumber)
    {
                                                                   
      if (aform->amoplefttype == aform->amoprighttype)
      {
        if (lhs_opno)
        {
          *lhs_opno = opno;
        }
        if (rhs_opno)
        {
          *rhs_opno = opno;
        }
        result = true;
        break;
      }

         
                                                                     
                                                                  
                                 
         
      if (lhs_opno)
      {
        *lhs_opno = get_opfamily_member(aform->amopfamily, aform->amoplefttype, aform->amoplefttype, HTEqualStrategyNumber);
        if (!OidIsValid(*lhs_opno))
        {
          continue;
        }
                                                                 
        if (!rhs_opno)
        {
          result = true;
          break;
        }
      }
      if (rhs_opno)
      {
        *rhs_opno = get_opfamily_member(aform->amopfamily, aform->amoprighttype, aform->amoprighttype, HTEqualStrategyNumber);
        if (!OidIsValid(*rhs_opno))
        {
                                                          
          if (lhs_opno)
          {
            *lhs_opno = InvalidOid;
          }
          continue;
        }
                                         
        result = true;
        break;
      }
    }
  }

  ReleaseSysCacheList(catlist);

  return result;
}

   
                         
                                                                            
                                                                              
   
                                                                          
                                                                            
                                                           
   
                                                                           
                                                                  
   
                                                                         
                                                                              
   
bool
get_op_hash_functions(Oid opno, RegProcedure *lhs_procno, RegProcedure *rhs_procno)
{
  bool result = false;
  CatCList *catlist;
  int i;

                                                     
  if (lhs_procno)
  {
    *lhs_procno = InvalidOid;
  }
  if (rhs_procno)
  {
    *rhs_procno = InvalidOid;
  }

     
                                                                           
                                                                      
                                                     
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple tuple = &catlist->members[i]->tuple;
    Form_pg_amop aform = (Form_pg_amop)GETSTRUCT(tuple);

    if (aform->amopmethod == HASH_AM_OID && aform->amopstrategy == HTEqualStrategyNumber)
    {
         
                                                                 
                                                                  
                                 
         
      if (lhs_procno)
      {
        *lhs_procno = get_opfamily_proc(aform->amopfamily, aform->amoplefttype, aform->amoplefttype, HASHSTANDARD_PROC);
        if (!OidIsValid(*lhs_procno))
        {
          continue;
        }
                                                                 
        if (!rhs_procno)
        {
          result = true;
          break;
        }
                                                                     
        if (aform->amoplefttype == aform->amoprighttype)
        {
          *rhs_procno = *lhs_procno;
          result = true;
          break;
        }
      }
      if (rhs_procno)
      {
        *rhs_procno = get_opfamily_proc(aform->amopfamily, aform->amoprighttype, aform->amoprighttype, HASHSTANDARD_PROC);
        if (!OidIsValid(*rhs_procno))
        {
                                                          
          if (lhs_procno)
          {
            *lhs_procno = InvalidOid;
          }
          continue;
        }
                                         
        result = true;
        break;
      }
    }
  }

  ReleaseSysCacheList(catlist);

  return result;
}

   
                               
                                                                            
                                                                          
                                                         
   
                                                                              
                                                                           
                                                                              
   
List *
get_op_btree_interpretation(Oid opno)
{
  List *result = NIL;
  OpBtreeInterpretation *thisresult;
  CatCList *catlist;
  int i;

     
                                                           
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple op_tuple = &catlist->members[i]->tuple;
    Form_pg_amop op_form = (Form_pg_amop)GETSTRUCT(op_tuple);
    StrategyNumber op_strategy;

                       
    if (op_form->amopmethod != BTREE_AM_OID)
    {
      continue;
    }

                                                  
    op_strategy = (StrategyNumber)op_form->amopstrategy;
    Assert(op_strategy >= 1 && op_strategy <= 5);

    thisresult = (OpBtreeInterpretation *)palloc(sizeof(OpBtreeInterpretation));
    thisresult->opfamily_id = op_form->amopfamily;
    thisresult->strategy = op_strategy;
    thisresult->oplefttype = op_form->amoplefttype;
    thisresult->oprighttype = op_form->amoprighttype;
    result = lappend(result, thisresult);
  }

  ReleaseSysCacheList(catlist);

     
                                                                           
                                                                           
     
  if (result == NIL)
  {
    Oid op_negator = get_negator(opno);

    if (OidIsValid(op_negator))
    {
      catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(op_negator));

      for (i = 0; i < catlist->n_members; i++)
      {
        HeapTuple op_tuple = &catlist->members[i]->tuple;
        Form_pg_amop op_form = (Form_pg_amop)GETSTRUCT(op_tuple);
        StrategyNumber op_strategy;

                           
        if (op_form->amopmethod != BTREE_AM_OID)
        {
          continue;
        }

                                                      
        op_strategy = (StrategyNumber)op_form->amopstrategy;
        Assert(op_strategy >= 1 && op_strategy <= 5);

                                               
        if (op_strategy != BTEqualStrategyNumber)
        {
          continue;
        }

                                                         
        thisresult = (OpBtreeInterpretation *)palloc(sizeof(OpBtreeInterpretation));
        thisresult->opfamily_id = op_form->amopfamily;
        thisresult->strategy = ROWCOMPARE_NE;
        thisresult->oplefttype = op_form->amoplefttype;
        thisresult->oprighttype = op_form->amoprighttype;
        result = lappend(result, thisresult);
      }

      ReleaseSysCacheList(catlist);
    }
  }

  return result;
}

   
                               
                                                                    
               
   
                                                                     
                                                                           
                                                                        
                                                                           
                                                              
   
bool
equality_ops_are_compatible(Oid opno1, Oid opno2)
{
  bool result;
  CatCList *catlist;
  int i;

                                         
  if (opno1 == opno2)
  {
    return true;
  }

     
                                                          
     
  catlist = SearchSysCacheList1(AMOPOPID, ObjectIdGetDatum(opno1));

  result = false;
  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple op_tuple = &catlist->members[i]->tuple;
    Form_pg_amop op_form = (Form_pg_amop)GETSTRUCT(op_tuple);

                               
    if (op_form->amopmethod == BTREE_AM_OID || op_form->amopmethod == HASH_AM_OID)
    {
      if (op_in_opfamily(opno2, op_form->amopfamily))
      {
        result = true;
        break;
      }
    }
  }

  ReleaseSysCacheList(catlist);

  return result;
}

                                                  

   
                     
                                                  
                                              
   
                                                                         
   
Oid
get_opfamily_proc(Oid opfamily, Oid lefttype, Oid righttype, int16 procnum)
{
  HeapTuple tp;
  Form_pg_amproc amproc_tup;
  RegProcedure result;

  tp = SearchSysCache4(AMPROCNUM, ObjectIdGetDatum(opfamily), ObjectIdGetDatum(lefttype), ObjectIdGetDatum(righttype), Int16GetDatum(procnum));
  if (!HeapTupleIsValid(tp))
  {
    return InvalidOid;
  }
  amproc_tup = (Form_pg_amproc)GETSTRUCT(tp);
  result = amproc_tup->amproc;
  ReleaseSysCache(tp);
  return result;
}

                                                    

   
               
                                                                         
                                                             
   
                                                                         
                                                                  
   
char *
get_attname(Oid relid, AttrNumber attnum, bool missing_ok)
{
  HeapTuple tp;

  tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_attribute att_tup = (Form_pg_attribute)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(att_tup->attname));
    ReleaseSysCache(tp);
    return result;
  }

  if (!missing_ok)
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
  }
  return NULL;
}

   
              
   
                                                  
                                                           
   
                                                                         
   
AttrNumber
get_attnum(Oid relid, const char *attname)
{
  HeapTuple tp;

  tp = SearchSysCacheAttName(relid, attname);
  if (HeapTupleIsValid(tp))
  {
    Form_pg_attribute att_tup = (Form_pg_attribute)GETSTRUCT(tp);
    AttrNumber result;

    result = att_tup->attnum;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidAttrNumber;
  }
}

   
                     
   
                                                    
                                                                  
   
                         
   
int
get_attstattarget(Oid relid, AttrNumber attnum)
{
  HeapTuple tp;
  Form_pg_attribute att_tup;
  int result;

  tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
  }
  att_tup = (Form_pg_attribute)GETSTRUCT(tp);
  result = att_tup->attstattarget;
  ReleaseSysCache(tp);
  return result;
}

   
                    
   
                                                    
                                                                 
   
                         
   
                                                                           
                  
   
char
get_attgenerated(Oid relid, AttrNumber attnum)
{
  HeapTuple tp;
  Form_pg_attribute att_tup;
  char result;

  tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
  }
  att_tup = (Form_pg_attribute)GETSTRUCT(tp);
  result = att_tup->attgenerated;
  ReleaseSysCache(tp);
  return result;
}

   
               
   
                                                                       
                                   
   
Oid
get_atttype(Oid relid, AttrNumber attnum)
{
  HeapTuple tp;

  tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_attribute att_tup = (Form_pg_attribute)GETSTRUCT(tp);
    Oid result;

    result = att_tup->atttypid;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                         
   
                                                                 
                                                                          
   
                                                          
                                                       
   
void
get_atttypetypmodcoll(Oid relid, AttrNumber attnum, Oid *typid, int32 *typmod, Oid *collid)
{
  HeapTuple tp;
  Form_pg_attribute att_tup;

  tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
  }
  att_tup = (Form_pg_attribute)GETSTRUCT(tp);

  *typid = att_tup->atttypid;
  *typmod = att_tup->atttypmod;
  *collid = att_tup->attcollation;
  ReleaseSysCache(tp);
}

                                                   

   
                      
                                                    
   
                                                                        
   
                                                                            
                                                 
   
char *
get_collation_name(Oid colloid)
{
  HeapTuple tp;

  tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(colloid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_collation colltup = (Form_pg_collation)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(colltup->collname));
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return NULL;
  }
}

bool
get_collation_isdeterministic(Oid colloid)
{
  HeapTuple tp;
  Form_pg_collation colltup;
  bool result;

  tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(colloid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for collation %u", colloid);
  }
  colltup = (Form_pg_collation)GETSTRUCT(tp);
  result = colltup->collisdeterministic;
  ReleaseSysCache(tp);
  return result;
}

                                                    

   
                       
                                                     
   
                                                                         
   
                                                                             
                                                 
   
char *
get_constraint_name(Oid conoid)
{
  HeapTuple tp;

  tp = SearchSysCache1(CONSTROID, ObjectIdGetDatum(conoid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_constraint contup = (Form_pg_constraint)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(contup->conname));
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return NULL;
  }
}

                                                  

char *
get_language_name(Oid langoid, bool missing_ok)
{
  HeapTuple tp;

  tp = SearchSysCache1(LANGOID, ObjectIdGetDatum(langoid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_language lantup = (Form_pg_language)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(lantup->lanname));
    ReleaseSysCache(tp);
    return result;
  }

  if (!missing_ok)
  {
    elog(ERROR, "cache lookup failed for language %u", langoid);
  }
  return NULL;
}

                                                  

   
                      
   
                                                                   
   
Oid
get_opclass_family(Oid opclass)
{
  HeapTuple tp;
  Form_pg_opclass cla_tup;
  Oid result;

  tp = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for opclass %u", opclass);
  }
  cla_tup = (Form_pg_opclass)GETSTRUCT(tp);

  result = cla_tup->opcfamily;
  ReleaseSysCache(tp);
  return result;
}

   
                          
   
                                                         
   
Oid
get_opclass_input_type(Oid opclass)
{
  HeapTuple tp;
  Form_pg_opclass cla_tup;
  Oid result;

  tp = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for opclass %u", opclass);
  }
  cla_tup = (Form_pg_opclass)GETSTRUCT(tp);

  result = cla_tup->opcintype;
  ReleaseSysCache(tp);
  return result;
}

   
                                       
   
                                                                   
                                                  
   
bool
get_opclass_opfamily_and_input_type(Oid opclass, Oid *opfamily, Oid *opcintype)
{
  HeapTuple tp;
  Form_pg_opclass cla_tup;

  tp = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
  if (!HeapTupleIsValid(tp))
  {
    return false;
  }

  cla_tup = (Form_pg_opclass)GETSTRUCT(tp);

  *opfamily = cla_tup->opcfamily;
  *opcintype = cla_tup->opcintype;

  ReleaseSysCache(tp);

  return true;
}

                                                  

   
              
   
                                                               
                                     
   
RegProcedure
get_opcode(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    RegProcedure result;

    result = optup->oprcode;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return (RegProcedure)InvalidOid;
  }
}

   
              
                                                          
   
                                                                             
   
char *
get_opname(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(optup->oprname));
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return NULL;
  }
}

   
                  
                                                           
   
Oid
get_op_rettype(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    Oid result;

    result = optup->oprresult;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                  
   
                                                               
                                  
   
void
op_input_types(Oid opno, Oid *lefttype, Oid *righttype)
{
  HeapTuple tp;
  Form_pg_operator optup;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (!HeapTupleIsValid(tp))                       
  {
    elog(ERROR, "cache lookup failed for operator %u", opno);
  }
  optup = (Form_pg_operator)GETSTRUCT(tp);
  *lefttype = optup->oprleft;
  *righttype = optup->oprright;
  ReleaseSysCache(tp);
}

   
                    
   
                                                                            
                                                                         
                                                                               
                                                                       
   
                                                                           
                                                                           
                                                                               
                                                                               
   
bool
op_mergejoinable(Oid opno, Oid inputtype)
{
  bool result = false;
  HeapTuple tp;
  TypeCacheEntry *typentry;

     
                                                                          
                                                                             
                                                                         
                           
     
  if (opno == ARRAY_EQ_OP)
  {
    typentry = lookup_type_cache(inputtype, TYPECACHE_CMP_PROC);
    if (typentry->cmp_proc == F_BTARRAYCMP)
    {
      result = true;
    }
  }
  else if (opno == RECORD_EQ_OP)
  {
    typentry = lookup_type_cache(inputtype, TYPECACHE_CMP_PROC);
    if (typentry->cmp_proc == F_BTRECORDCMP)
    {
      result = true;
    }
  }
  else
  {
                                                                  
    tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
    if (HeapTupleIsValid(tp))
    {
      Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);

      result = optup->oprcanmerge;
      ReleaseSysCache(tp);
    }
  }
  return result;
}

   
                   
   
                                                                            
                                                              
   
                                                                           
                                                                         
                                                                             
                                                                     
   
bool
op_hashjoinable(Oid opno, Oid inputtype)
{
  bool result = false;
  HeapTuple tp;
  TypeCacheEntry *typentry;

                                                                      
                                                              
  if (opno == ARRAY_EQ_OP)
  {
    typentry = lookup_type_cache(inputtype, TYPECACHE_HASH_PROC);
    if (typentry->hash_proc == F_HASH_ARRAY)
    {
      result = true;
    }
  }
  else
  {
                                                                 
    tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
    if (HeapTupleIsValid(tp))
    {
      Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);

      result = optup->oprcanhash;
      ReleaseSysCache(tp);
    }
  }
  return result;
}

   
             
   
                                                                    
   
bool
op_strict(Oid opno)
{
  RegProcedure funcid = get_opcode(opno);

  if (funcid == (RegProcedure)InvalidOid)
  {
    elog(ERROR, "operator %u does not exist", opno);
  }

  return func_strict((Oid)funcid);
}

   
               
   
                                                                    
   
char
op_volatile(Oid opno)
{
  RegProcedure funcid = get_opcode(opno);

  if (funcid == (RegProcedure)InvalidOid)
  {
    elog(ERROR, "operator %u does not exist", opno);
  }

  return func_volatile((Oid)funcid);
}

   
                  
   
                                                         
   
Oid
get_commutator(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    Oid result;

    result = optup->oprcom;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
               
   
                                                      
   
Oid
get_negator(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    Oid result;

    result = optup->oprnegate;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
               
   
                                                                   
   
RegProcedure
get_oprrest(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    RegProcedure result;

    result = optup->oprrest;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return (RegProcedure)InvalidOid;
  }
}

   
               
   
                                                              
   
RegProcedure
get_oprjoin(Oid opno)
{
  HeapTuple tp;

  tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_operator optup = (Form_pg_operator)GETSTRUCT(tp);
    RegProcedure result;

    result = optup->oprjoin;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return (RegProcedure)InvalidOid;
  }
}

                                                  

   
                 
                                                            
   
                                                                             
   
char *
get_func_name(Oid funcid)
{
  HeapTuple tp;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_proc functup = (Form_pg_proc)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(functup->proname));
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return NULL;
  }
}

   
                      
   
                                                                   
   
Oid
get_func_namespace(Oid funcid)
{
  HeapTuple tp;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_proc functup = (Form_pg_proc)GETSTRUCT(tp);
    Oid result;

    result = functup->pronamespace;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                    
                                                           
   
Oid
get_func_rettype(Oid funcid)
{
  HeapTuple tp;
  Oid result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->prorettype;
  ReleaseSysCache(tp);
  return result;
}

   
                  
                                                        
   
int
get_func_nargs(Oid funcid)
{
  HeapTuple tp;
  int result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->pronargs;
  ReleaseSysCache(tp);
  return result;
}

   
                      
                                                                         
                                           
   
                                                   
   
Oid
get_func_signature(Oid funcid, Oid **argtypes, int *nargs)
{
  HeapTuple tp;
  Form_pg_proc procstruct;
  Oid result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  procstruct = (Form_pg_proc)GETSTRUCT(tp);

  result = procstruct->prorettype;
  *nargs = (int)procstruct->pronargs;
  Assert(*nargs == procstruct->proargtypes.dim1);
  *argtypes = (Oid *)palloc(*nargs * sizeof(Oid));
  memcpy(*argtypes, procstruct->proargtypes.values, *nargs * sizeof(Oid));

  ReleaseSysCache(tp);
  return result;
}

   
                         
                                                                 
   
Oid
get_func_variadictype(Oid funcid)
{
  HeapTuple tp;
  Oid result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->provariadic;
  ReleaseSysCache(tp);
  return result;
}

   
                   
                                                              
   
bool
get_func_retset(Oid funcid)
{
  HeapTuple tp;
  bool result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->proretset;
  ReleaseSysCache(tp);
  return result;
}

   
               
                                                                
   
bool
func_strict(Oid funcid)
{
  HeapTuple tp;
  bool result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->proisstrict;
  ReleaseSysCache(tp);
  return result;
}

   
                 
                                                                
   
char
func_volatile(Oid funcid)
{
  HeapTuple tp;
  char result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->provolatile;
  ReleaseSysCache(tp);
  return result;
}

   
                 
                                                                
   
char
func_parallel(Oid funcid)
{
  HeapTuple tp;
  char result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->proparallel;
  ReleaseSysCache(tp);
  return result;
}

   
                    
                                                   
   
char
get_func_prokind(Oid funcid)
{
  HeapTuple tp;
  char result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->prokind;
  ReleaseSysCache(tp);
  return result;
}

   
                      
                                                                 
   
bool
get_func_leakproof(Oid funcid)
{
  HeapTuple tp;
  bool result;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }

  result = ((Form_pg_proc)GETSTRUCT(tp))->proleakproof;
  ReleaseSysCache(tp);
  return result;
}

   
                    
   
                                                                       
                                    
   
RegProcedure
get_func_support(Oid funcid)
{
  HeapTuple tp;

  tp = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_proc functup = (Form_pg_proc)GETSTRUCT(tp);
    RegProcedure result;

    result = functup->prosupport;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return (RegProcedure)InvalidOid;
  }
}

                                                  

   
                     
                                                             
   
                                                    
   
Oid
get_relname_relid(const char *relname, Oid relnamespace)
{
  return GetSysCacheOid2(RELNAMENSP, Anum_pg_class_oid, PointerGetDatum(relname), ObjectIdGetDatum(relnamespace));
}

#ifdef NOT_USED
   
                
   
                                                           
   
int
get_relnatts(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    int result;

    result = reltup->relnatts;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidAttrNumber;
  }
}
#endif

   
                
                                          
   
                                                                       
   
                                                                           
                                                 
   
char *
get_rel_name(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(reltup->relname));
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return NULL;
  }
}

   
                     
   
                                                                   
   
Oid
get_rel_namespace(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    Oid result;

    result = reltup->relnamespace;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                   
   
                                                              
   
                                                                      
                                           
   
Oid
get_rel_type_id(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    Oid result;

    result = reltup->reltype;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                   
   
                                                          
   
char
get_rel_relkind(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    char result;

    result = reltup->relkind;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return '\0';
  }
}

   
                          
   
                                                                      
   
bool
get_rel_relispartition(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    bool result;

    result = reltup->relispartition;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return false;
  }
}

   
                      
   
                                                                    
   
                                                                          
                                                       
   
Oid
get_rel_tablespace(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    Oid result;

    result = reltup->reltablespace;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                       
   
                                                                 
   
char
get_rel_persistence(Oid relid)
{
  HeapTuple tp;
  Form_pg_class reltup;
  char result;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  reltup = (Form_pg_class)GETSTRUCT(tp);
  result = reltup->relpersistence;
  ReleaseSysCache(tp);

  return result;
}

                                                    

Oid
get_transform_fromsql(Oid typid, Oid langid, List *trftypes)
{
  HeapTuple tup;

  if (!list_member_oid(trftypes, typid))
  {
    return InvalidOid;
  }

  tup = SearchSysCache2(TRFTYPELANG, typid, langid);
  if (HeapTupleIsValid(tup))
  {
    Oid funcid;

    funcid = ((Form_pg_transform)GETSTRUCT(tup))->trffromsql;
    ReleaseSysCache(tup);
    return funcid;
  }
  else
  {
    return InvalidOid;
  }
}

Oid
get_transform_tosql(Oid typid, Oid langid, List *trftypes)
{
  HeapTuple tup;

  if (!list_member_oid(trftypes, typid))
  {
    return InvalidOid;
  }

  tup = SearchSysCache2(TRFTYPELANG, typid, langid);
  if (HeapTupleIsValid(tup))
  {
    Oid funcid;

    funcid = ((Form_pg_transform)GETSTRUCT(tup))->trftosql;
    ReleaseSysCache(tup);
    return funcid;
  }
  else
  {
    return InvalidOid;
  }
}

                                               

   
                    
   
                                                              
                                 
   
bool
get_typisdefined(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    bool result;

    result = typtup->typisdefined;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return false;
  }
}

   
              
   
                                                       
   
int16
get_typlen(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    int16 result;

    result = typtup->typlen;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return 0;
  }
}

   
                
   
                                                                           
                                                           
   
bool
get_typbyval(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    bool result;

    result = typtup->typbyval;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return false;
  }
}

   
                   
   
                                                                    
   
                                                                      
                                                                         
                                                                   
                                                       
   
void
get_typlenbyval(Oid typid, int16 *typlen, bool *typbyval)
{
  HeapTuple tp;
  Form_pg_type typtup;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typtup = (Form_pg_type)GETSTRUCT(tp);
  *typlen = typtup->typlen;
  *typbyval = typtup->typbyval;
  ReleaseSysCache(tp);
}

   
                        
   
                                                                        
   
void
get_typlenbyvalalign(Oid typid, int16 *typlen, bool *typbyval, char *typalign)
{
  HeapTuple tp;
  Form_pg_type typtup;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typtup = (Form_pg_type)GETSTRUCT(tp);
  *typlen = typtup->typlen;
  *typbyval = typtup->typbyval;
  *typalign = typtup->typalign;
  ReleaseSysCache(tp);
}

   
                  
                                                                      
   
                                                                           
                                                                      
                                                                           
                                                                           
                                                                    
                                                                      
   
                                                                        
                                                                        
                                                           
   
Oid
getTypeIOParam(HeapTuple typeTuple)
{
  Form_pg_type typeStruct = (Form_pg_type)GETSTRUCT(typeTuple);

     
                                                                           
                                
     
  if (OidIsValid(typeStruct->typelem))
  {
    return typeStruct->typelem;
  }
  else
  {
    return typeStruct->oid;
  }
}

   
                    
   
                                                                      
                                                                  
                                                
   
void
get_type_io_data(Oid typid, IOFuncSelector which_func, int16 *typlen, bool *typbyval, char *typalign, char *typdelim, Oid *typioparam, Oid *func)
{
  HeapTuple typeTuple;
  Form_pg_type typeStruct;

     
                                                                            
                                                  
     
  if (IsBootstrapProcessingMode())
  {
    Oid typinput;
    Oid typoutput;

    boot_get_type_io_data(typid, typlen, typbyval, typalign, typdelim, typioparam, &typinput, &typoutput);
    switch (which_func)
    {
    case IOFunc_input:
      *func = typinput;
      break;
    case IOFunc_output:
      *func = typoutput;
      break;
    default:
      elog(ERROR, "binary I/O not supported during bootstrap");
      break;
    }
    return;
  }

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typeStruct = (Form_pg_type)GETSTRUCT(typeTuple);

  *typlen = typeStruct->typlen;
  *typbyval = typeStruct->typbyval;
  *typalign = typeStruct->typalign;
  *typdelim = typeStruct->typdelim;
  *typioparam = getTypeIOParam(typeTuple);
  switch (which_func)
  {
  case IOFunc_input:
    *func = typeStruct->typinput;
    break;
  case IOFunc_output:
    *func = typeStruct->typoutput;
    break;
  case IOFunc_receive:
    *func = typeStruct->typreceive;
    break;
  case IOFunc_send:
    *func = typeStruct->typsend;
    break;
  }
  ReleaseSysCache(typeTuple);
}

#ifdef NOT_USED
char
get_typalign(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    char result;

    result = typtup->typalign;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return 'i';
  }
}
#endif

char
get_typstorage(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    char result;

    result = typtup->typstorage;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return 'p';
  }
}

   
                  
                                                                
   
                                                                     
                                             
   
                                                                       
                                                                           
   
Node *
get_typdefault(Oid typid)
{
  HeapTuple typeTuple;
  Form_pg_type type;
  Datum datum;
  bool isNull;
  Node *expr;

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  type = (Form_pg_type)GETSTRUCT(typeTuple);

     
                                                                        
                                                               
                      
     
  datum = SysCacheGetAttr(TYPEOID, typeTuple, Anum_pg_type_typdefaultbin, &isNull);

  if (!isNull)
  {
                                       
    expr = stringToNode(TextDatumGetCString(datum));
  }
  else
  {
                                                 
    datum = SysCacheGetAttr(TYPEOID, typeTuple, Anum_pg_type_typdefault, &isNull);

    if (!isNull)
    {
      char *strDefaultVal;

                                          
      strDefaultVal = TextDatumGetCString(datum);
                                                         
      datum = OidInputFunctionCall(type->typinput, strDefaultVal, getTypeIOParam(typeTuple), -1);
                                                   
      expr = (Node *)makeConst(typid, -1, type->typcollation, type->typlen, datum, false, type->typbyval);
      pfree(strDefaultVal);
    }
    else
    {
                      
      expr = NULL;
    }
  }

  ReleaseSysCache(typeTuple);

  return expr;
}

   
               
                                                         
                                         
   
Oid
getBaseType(Oid typid)
{
  int32 typmod = -1;

  return getBaseTypeAndTypmod(typid, &typmod);
}

   
                        
                                                                    
                                                                      
   
                                                                      
                                                                     
                                   
   
Oid
getBaseTypeAndTypmod(Oid typid, int32 *typmod)
{
     
                                                                 
     
  for (;;)
  {
    HeapTuple tup;
    Form_pg_type typTup;

    tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for type %u", typid);
    }
    typTup = (Form_pg_type)GETSTRUCT(tup);
    if (typTup->typtype != TYPTYPE_DOMAIN)
    {
                                 
      ReleaseSysCache(tup);
      break;
    }

    Assert(*typmod == -1);
    typid = typTup->typbasetype;
    *typmod = typTup->typtypmod;

    ReleaseSysCache(tup);
  }

  return typid;
}

   
                   
   
                                                                         
                                                                        
                                                                    
                                                                
   
int32
get_typavgwidth(Oid typid, int32 typmod)
{
  int typlen = get_typlen(typid);
  int32 maxwidth;

     
                                     
     
  if (typlen > 0)
  {
    return typlen;
  }

     
                                                                        
                                          
     
  maxwidth = type_maximum_size(typid, typmod);
  if (maxwidth > 0)
  {
       
                                                                       
                                                                           
                                                           
       
    if (typid == BPCHAROID)
    {
      return maxwidth;
    }
    if (maxwidth <= 32)
    {
      return maxwidth;                        
    }
    if (maxwidth < 1000)
    {
      return 32 + (maxwidth - 32) / 2;                 
    }

       
                                                           
                                                                          
                             
       
    return 32 + (1000 - 32) / 2;
  }

     
                                                
     
  return 32;
}

   
               
   
                                                                         
                                                          
   
char
get_typtype(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    char result;

    result = typtup->typtype;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return '\0';
  }
}

   
                   
   
                                                                    
                                                                 
                                                      
   
bool
type_is_rowtype(Oid typid)
{
  if (typid == RECORDOID)
  {
    return true;                
  }
  switch (get_typtype(typid))
  {
  case TYPTYPE_COMPOSITE:
    return true;
  case TYPTYPE_DOMAIN:
    if (get_typtype(getBaseType(typid)) == TYPTYPE_COMPOSITE)
    {
      return true;
    }
    break;
  default:
    break;
  }
  return false;
}

   
                
                                                     
   
bool
type_is_enum(Oid typid)
{
  return (get_typtype(typid) == TYPTYPE_ENUM);
}

   
                 
                                                     
   
bool
type_is_range(Oid typid)
{
  return (get_typtype(typid) == TYPTYPE_RANGE);
}

   
                               
   
                                                                      
                             
   
void
get_type_category_preferred(Oid typid, char *typcategory, bool *typispreferred)
{
  HeapTuple tp;
  Form_pg_type typtup;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typtup = (Form_pg_type)GETSTRUCT(tp);
  *typcategory = typtup->typcategory;
  *typispreferred = typtup->typispreferred;
  ReleaseSysCache(tp);
}

   
                    
   
                                                                      
           
   
Oid
get_typ_typrelid(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    Oid result;

    result = typtup->typrelid;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                    
   
                                                                           
   
                                                                           
                                                       
   
Oid
get_element_type(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    Oid result;

    if (typtup->typlen == -1)
    {
      result = typtup->typelem;
    }
    else
    {
      result = InvalidOid;
    }
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                  
   
                                                                 
                                                      
   
Oid
get_array_type(Oid typid)
{
  HeapTuple tp;
  Oid result = InvalidOid;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    result = ((Form_pg_type)GETSTRUCT(tp))->typarray;
    ReleaseSysCache(tp);
  }
  return result;
}

   
                           
   
                                                                    
                                                                   
                                                               
                                                                  
                                                     
   
Oid
get_promoted_array_type(Oid typid)
{
  Oid array_type = get_array_type(typid);

  if (OidIsValid(array_type))
  {
    return array_type;
  }
  if (OidIsValid(get_element_type(typid)))
  {
    return typid;
  }
  return InvalidOid;
}

   
                         
                                                                      
                                  
   
                                                                          
                                                                         
                                  
   
Oid
get_base_element_type(Oid typid)
{
     
                                                                 
     
  for (;;)
  {
    HeapTuple tup;
    Form_pg_type typTup;

    tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
    if (!HeapTupleIsValid(tup))
    {
      break;
    }
    typTup = (Form_pg_type)GETSTRUCT(tup);
    if (typTup->typtype != TYPTYPE_DOMAIN)
    {
                                            
      Oid result;

                                                 
      if (typTup->typlen == -1)
      {
        result = typTup->typelem;
      }
      else
      {
        result = InvalidOid;
      }
      ReleaseSysCache(tup);
      return result;
    }

    typid = typTup->typbasetype;
    ReleaseSysCache(tup);
  }

                                                                         
  return InvalidOid;
}

   
                    
   
                                                                     
   
void
getTypeInputInfo(Oid type, Oid *typInput, Oid *typIOParam)
{
  HeapTuple typeTuple;
  Form_pg_type pt;

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", type);
  }
  pt = (Form_pg_type)GETSTRUCT(typeTuple);

  if (!pt->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type %s is only a shell", format_type_be(type))));
  }
  if (!OidIsValid(pt->typinput))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("no input function available for type %s", format_type_be(type))));
  }

  *typInput = pt->typinput;
  *typIOParam = getTypeIOParam(typeTuple);

  ReleaseSysCache(typeTuple);
}

   
                     
   
                                                  
   
void
getTypeOutputInfo(Oid type, Oid *typOutput, bool *typIsVarlena)
{
  HeapTuple typeTuple;
  Form_pg_type pt;

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", type);
  }
  pt = (Form_pg_type)GETSTRUCT(typeTuple);

  if (!pt->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type %s is only a shell", format_type_be(type))));
  }
  if (!OidIsValid(pt->typoutput))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("no output function available for type %s", format_type_be(type))));
  }

  *typOutput = pt->typoutput;
  *typIsVarlena = (!pt->typbyval) && (pt->typlen == -1);

  ReleaseSysCache(typeTuple);
}

   
                          
   
                                                         
   
void
getTypeBinaryInputInfo(Oid type, Oid *typReceive, Oid *typIOParam)
{
  HeapTuple typeTuple;
  Form_pg_type pt;

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", type);
  }
  pt = (Form_pg_type)GETSTRUCT(typeTuple);

  if (!pt->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type %s is only a shell", format_type_be(type))));
  }
  if (!OidIsValid(pt->typreceive))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("no binary input function available for type %s", format_type_be(type))));
  }

  *typReceive = pt->typreceive;
  *typIOParam = getTypeIOParam(typeTuple);

  ReleaseSysCache(typeTuple);
}

   
                           
   
                                                          
   
void
getTypeBinaryOutputInfo(Oid type, Oid *typSend, bool *typIsVarlena)
{
  HeapTuple typeTuple;
  Form_pg_type pt;

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", type);
  }
  pt = (Form_pg_type)GETSTRUCT(typeTuple);

  if (!pt->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type %s is only a shell", format_type_be(type))));
  }
  if (!OidIsValid(pt->typsend))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("no binary output function available for type %s", format_type_be(type))));
  }

  *typSend = pt->typsend;
  *typIsVarlena = (!pt->typbyval) && (pt->typlen == -1);

  ReleaseSysCache(typeTuple);
}

   
                
   
                                                                      
   
Oid
get_typmodin(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    Oid result;

    result = typtup->typmodin;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

#ifdef NOT_USED
   
                 
   
                                                                       
   
Oid
get_typmodout(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    Oid result;

    result = typtup->typmodout;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}
#endif               

   
                    
   
                                                                  
   
Oid
get_typcollation(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    Oid result;

    result = typtup->typcollation;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                      
   
                                                   
   
bool
type_is_collatable(Oid typid)
{
  return OidIsValid(get_typcollation(typid));
}

                                                    

   
                   
   
                                                                       
                                                                        
   
                                                                               
                                               
   
                                                                        
                                                                       
                                   
   
int32
get_attavgwidth(Oid relid, AttrNumber attnum)
{
  HeapTuple tp;
  int32 stawidth;

  if (get_attavgwidth_hook)
  {
    stawidth = (*get_attavgwidth_hook)(relid, attnum);
    if (stawidth > 0)
    {
      return stawidth;
    }
  }
  tp = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(relid), Int16GetDatum(attnum), BoolGetDatum(false));
  if (HeapTupleIsValid(tp))
  {
    stawidth = ((Form_pg_statistic)GETSTRUCT(tp))->stawidth;
    ReleaseSysCache(tp);
    if (stawidth > 0)
    {
      return stawidth;
    }
  }
  return 0;
}

   
                    
   
                                                              
                                                               
   
                                                                  
                                                                        
                                                                        
                                                                      
                                                                        
                                                                       
                 
   
                                                                              
                                                  
                                                           
                                                           
                                                                      
   
                                                                             
                                           
                                               
                                                                     
                                                                 
                                          
                                                                               
                                            
   
                                                                         
                                                               
                                          
   
                                                                          
   
                                                                         
                                                                            
                                                                            
   
                                                                       
                                                                       
                                                                         
   
                                                                           
                                                                     
   
bool
get_attstatsslot(AttStatsSlot *sslot, HeapTuple statstuple, int reqkind, Oid reqop, int flags)
{
  Form_pg_statistic stats = (Form_pg_statistic)GETSTRUCT(statstuple);
  int i;
  Datum val;
  bool isnull;
  ArrayType *statarray;
  Oid arrayelemtype;
  int narrayelem;
  HeapTuple typeTuple;
  Form_pg_type typeForm;

                                  
  memset(sslot, 0, sizeof(AttStatsSlot));

  for (i = 0; i < STATISTIC_NUM_SLOTS; i++)
  {
    if ((&stats->stakind1)[i] == reqkind && (reqop == InvalidOid || (&stats->staop1)[i] == reqop))
    {
      break;
    }
  }
  if (i >= STATISTIC_NUM_SLOTS)
  {
    return false;                
  }

  sslot->staop = (&stats->staop1)[i];
  sslot->stacoll = (&stats->stacoll1)[i];

     
                                                                            
                                                                           
                                                                           
                                                                             
                                                                            
                                                                          
                                                                          
                                                      
     
  if (sslot->stacoll == InvalidOid)
  {
    sslot->stacoll = DEFAULT_COLLATION_OID;
  }

  if (flags & ATTSTATSSLOT_VALUES)
  {
    val = SysCacheGetAttr(STATRELATTINH, statstuple, Anum_pg_statistic_stavalues1 + i, &isnull);
    if (isnull)
    {
      elog(ERROR, "stavalues is null");
    }

       
                                                                       
                                           
       
    statarray = DatumGetArrayTypePCopy(val);

       
                                                                           
                        
       
    sslot->valuetype = arrayelemtype = ARR_ELEMTYPE(statarray);

                                      
    typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(arrayelemtype));
    if (!HeapTupleIsValid(typeTuple))
    {
      elog(ERROR, "cache lookup failed for type %u", arrayelemtype);
    }
    typeForm = (Form_pg_type)GETSTRUCT(typeTuple);

                                                                   
    deconstruct_array(statarray, arrayelemtype, typeForm->typlen, typeForm->typbyval, typeForm->typalign, &sslot->values, NULL, &sslot->nvalues);

       
                                                                        
                                                                       
                                                                           
                                                                     
       
    if (!typeForm->typbyval)
    {
      sslot->values_arr = statarray;
    }
    else
    {
      pfree(statarray);
    }

    ReleaseSysCache(typeTuple);
  }

  if (flags & ATTSTATSSLOT_NUMBERS)
  {
    val = SysCacheGetAttr(STATRELATTINH, statstuple, Anum_pg_statistic_stanumbers1 + i, &isnull);
    if (isnull)
    {
      elog(ERROR, "stanumbers is null");
    }

       
                                                                       
                                           
       
    statarray = DatumGetArrayTypePCopy(val);

       
                                                                           
                                                                          
                                                
       
    narrayelem = ARR_DIMS(statarray)[0];
    if (ARR_NDIM(statarray) != 1 || narrayelem <= 0 || ARR_HASNULL(statarray) || ARR_ELEMTYPE(statarray) != FLOAT4OID)
    {
      elog(ERROR, "stanumbers is not a 1-D float4 array");
    }

                                                           
    sslot->numbers = (float4 *)ARR_DATA_PTR(statarray);
    sslot->nnumbers = narrayelem;

                                                       
    sslot->numbers_arr = statarray;
  }

  return true;
}

   
                     
                                            
   
void
free_attstatsslot(AttStatsSlot *sslot)
{
                                                                       
  if (sslot->values)
  {
    pfree(sslot->values);
  }
                                                                    
                                                
  if (sslot->values_arr)
  {
    pfree(sslot->values_arr);
  }
  if (sslot->numbers_arr)
  {
    pfree(sslot->numbers_arr);
  }
}

                                                     

   
                      
                                          
   
                                                                        
   
char *
get_namespace_name(Oid nspid)
{
  HeapTuple tp;

  tp = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(nspid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_namespace nsptup = (Form_pg_namespace)GETSTRUCT(tp);
    char *result;

    result = pstrdup(NameStr(nsptup->nspname));
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return NULL;
  }
}

   
                              
                                                                      
                       
   
char *
get_namespace_name_or_temp(Oid nspid)
{
  if (isTempNamespace(nspid))
  {
    return "pg_temp";
  }
  else
  {
    return get_namespace_name(nspid);
  }
}

                                                 

   
                     
                                              
   
                                                       
   
Oid
get_range_subtype(Oid rangeOid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RANGETYPE, ObjectIdGetDatum(rangeOid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_range rngtup = (Form_pg_range)GETSTRUCT(tp);
    Oid result;

    result = rngtup->rngsubtype;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

   
                       
                                                
   
                                                       
                                        
   
Oid
get_range_collation(Oid rangeOid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RANGETYPE, ObjectIdGetDatum(rangeOid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_range rngtup = (Form_pg_range)GETSTRUCT(tp);
    Oid result;

    result = rngtup->rngcollation;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}

                                                 

   
                            
   
                                           
                                       
                                              
                                
   
Oid
get_index_column_opclass(Oid index_oid, int attno)
{
  HeapTuple tuple;
  Form_pg_index rd_index PG_USED_FOR_ASSERTS_ONLY;
  Datum datum;
  bool isnull;
  oidvector *indclass;
  Oid opclass;

                                                   

  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(index_oid));
  if (!HeapTupleIsValid(tuple))
  {
    return InvalidOid;
  }

  rd_index = (Form_pg_index)GETSTRUCT(tuple);

                                            
  Assert(attno > 0 && attno <= rd_index->indnatts);

                                                
  if (attno > rd_index->indnkeyatts)
  {
    ReleaseSysCache(tuple);
    return InvalidOid;
  }

  datum = SysCacheGetAttr(INDEXRELID, tuple, Anum_pg_index_indclass, &isnull);
  Assert(!isnull);

  indclass = ((oidvector *)DatumGetPointer(datum));

  Assert(attno <= indclass->dim1);
  opclass = indclass->values[attno - 1];

  ReleaseSysCache(tuple);

  return opclass;
}

   
                         
   
                                                         
   
bool
get_index_isreplident(Oid index_oid)
{
  HeapTuple tuple;
  Form_pg_index rd_index;
  bool result;

  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(index_oid));
  if (!HeapTupleIsValid(tuple))
  {
    return false;
  }

  rd_index = (Form_pg_index)GETSTRUCT(tuple);
  result = rd_index->indisreplident;
  ReleaseSysCache(tuple);

  return result;
}

   
                     
   
                                                     
   
bool
get_index_isvalid(Oid index_oid)
{
  bool isvalid;
  HeapTuple tuple;
  Form_pg_index rd_index;

  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(index_oid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for index %u", index_oid);
  }

  rd_index = (Form_pg_index)GETSTRUCT(tuple);
  isvalid = rd_index->indisvalid;
  ReleaseSysCache(tuple);

  return isvalid;
}

   
                         
   
                                                         
   
bool
get_index_isclustered(Oid index_oid)
{
  bool isclustered;
  HeapTuple tuple;
  Form_pg_index rd_index;

  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(index_oid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for index %u", index_oid);
  }

  rd_index = (Form_pg_index)GETSTRUCT(tuple);
  isclustered = rd_index->indisclustered;
  ReleaseSysCache(tuple);

  return isclustered;
}
