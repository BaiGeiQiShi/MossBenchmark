                                                                            
   
             
                                                     
   
                                                                
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/amapi.h"
#include "access/htup_details.h"
#include "catalog/pg_class.h"
#include "catalog/pg_index.h"
#include "utils/builtins.h"
#include "utils/syscache.h"

                                                          
struct am_propname
{
  const char *name;
  IndexAMProperty prop;
};

static const struct am_propname am_propnames[] = {
    {"asc", AMPROP_ASC},
    {"desc", AMPROP_DESC},
    {"nulls_first", AMPROP_NULLS_FIRST},
    {"nulls_last", AMPROP_NULLS_LAST},
    {"orderable", AMPROP_ORDERABLE},
    {"distance_orderable", AMPROP_DISTANCE_ORDERABLE},
    {"returnable", AMPROP_RETURNABLE},
    {"search_array", AMPROP_SEARCH_ARRAY},
    {"search_nulls", AMPROP_SEARCH_NULLS},
    {"clusterable", AMPROP_CLUSTERABLE},
    {"index_scan", AMPROP_INDEX_SCAN},
    {"bitmap_scan", AMPROP_BITMAP_SCAN},
    {"backward_scan", AMPROP_BACKWARD_SCAN},
    {"can_order", AMPROP_CAN_ORDER},
    {"can_unique", AMPROP_CAN_UNIQUE},
    {"can_multi_col", AMPROP_CAN_MULTI_COL},
    {"can_exclude", AMPROP_CAN_EXCLUDE},
    {"can_include", AMPROP_CAN_INCLUDE},
};

static IndexAMProperty
lookup_prop_name(const char *name)
{
  int i;

  for (i = 0; i < lengthof(am_propnames); i++)
  {
    if (pg_strcasecmp(am_propnames[i].name, name) == 0)
    {
      return am_propnames[i].prop;
    }
  }

                                                                             
  return AMPROP_UNKNOWN;
}

   
                                                                     
   
                                 
                                                               
                                                                             
                                                  
                                                                      
   
                                                                         
                                                       
   
static bool
test_indoption(HeapTuple tuple, int attno, bool guard, int16 iopt_mask, int16 iopt_expect, bool *res)
{
  Datum datum;
  bool isnull;
  int2vector *indoption;
  int16 indoption_val;

  if (!guard)
  {
    *res = false;
    return true;
  }

  datum = SysCacheGetAttr(INDEXRELID, tuple, Anum_pg_index_indoption, &isnull);
  Assert(!isnull);

  indoption = ((int2vector *)DatumGetPointer(datum));
  indoption_val = indoption->values[attno - 1];

  *res = (indoption_val & iopt_mask) == iopt_expect;

  return true;
}

   
                                                         
   
                                                                       
                                                                              
                                                                              
               
   
static Datum
indexam_property(FunctionCallInfo fcinfo, const char *propname, Oid amoid, Oid index_oid, int attno)
{
  bool res = false;
  bool isnull = false;
  int natts = 0;
  IndexAMProperty prop;
  IndexAmRoutine *routine;

                                                                    
  prop = lookup_prop_name(propname);

                                                                         
  if (OidIsValid(index_oid))
  {
    HeapTuple tuple;
    Form_pg_class rd_rel;

    Assert(!OidIsValid(amoid));
    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(index_oid));
    if (!HeapTupleIsValid(tuple))
    {
      PG_RETURN_NULL();
    }
    rd_rel = (Form_pg_class)GETSTRUCT(tuple);
    if (rd_rel->relkind != RELKIND_INDEX && rd_rel->relkind != RELKIND_PARTITIONED_INDEX)
    {
      ReleaseSysCache(tuple);
      PG_RETURN_NULL();
    }
    amoid = rd_rel->relam;
    natts = rd_rel->relnatts;
    ReleaseSysCache(tuple);
  }

     
                                                                         
                                                                         
                                                                           
            
     
  if (attno < 0 || attno > natts)
  {
    PG_RETURN_NULL();
  }

     
                                                                        
     
  routine = GetIndexAmRoutineByAmId(amoid, true);
  if (routine == NULL)
  {
    PG_RETURN_NULL();
  }

     
                                                                         
                                                  
     
  if (routine->amproperty && routine->amproperty(index_oid, attno, prop, propname, &res, &isnull))
  {
    if (isnull)
    {
      PG_RETURN_NULL();
    }
    PG_RETURN_BOOL(res);
  }

  if (attno > 0)
  {
    HeapTuple tuple;
    Form_pg_index rd_index;
    bool iskey = true;

       
                                                                           
                                                                        
                   
       
    tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(index_oid));
    if (!HeapTupleIsValid(tuple))
    {
      PG_RETURN_NULL();
    }
    rd_index = (Form_pg_index)GETSTRUCT(tuple);

    Assert(index_oid == rd_index->indexrelid);
    Assert(attno > 0 && attno <= rd_index->indnatts);

    isnull = true;

       
                                                                     
                                                                          
             
       
    if (routine->amcaninclude && attno > rd_index->indnkeyatts)
    {
      iskey = false;
    }

    switch (prop)
    {
    case AMPROP_ASC:
      if (iskey && test_indoption(tuple, attno, routine->amcanorder, INDOPTION_DESC, 0, &res))
      {
        isnull = false;
      }
      break;

    case AMPROP_DESC:
      if (iskey && test_indoption(tuple, attno, routine->amcanorder, INDOPTION_DESC, INDOPTION_DESC, &res))
      {
        isnull = false;
      }
      break;

    case AMPROP_NULLS_FIRST:
      if (iskey && test_indoption(tuple, attno, routine->amcanorder, INDOPTION_NULLS_FIRST, INDOPTION_NULLS_FIRST, &res))
      {
        isnull = false;
      }
      break;

    case AMPROP_NULLS_LAST:
      if (iskey && test_indoption(tuple, attno, routine->amcanorder, INDOPTION_NULLS_FIRST, 0, &res))
      {
        isnull = false;
      }
      break;

    case AMPROP_ORDERABLE:

         
                                                                     
         
      res = iskey ? routine->amcanorder : false;
      isnull = false;
      break;

    case AMPROP_DISTANCE_ORDERABLE:

         
                                                                   
                                                                
                                                                    
                                                                
                                                                    
                                                                    
                                                                 
                                                                    
                                                               
                
         
      if (!iskey || !routine->amcanorderbyop)
      {
        res = false;
        isnull = false;
      }
      break;

    case AMPROP_RETURNABLE:

                                                       

      isnull = false;
      res = false;

      if (routine->amcanreturn)
      {
           
                                                              
                                                                 
                                                   
           
        Relation indexrel = index_open(index_oid, AccessShareLock);

        res = index_can_return(indexrel, attno);
        index_close(indexrel, AccessShareLock);
      }
      break;

    case AMPROP_SEARCH_ARRAY:
      if (iskey)
      {
        res = routine->amsearcharray;
        isnull = false;
      }
      break;

    case AMPROP_SEARCH_NULLS:
      if (iskey)
      {
        res = routine->amsearchnulls;
        isnull = false;
      }
      break;

    default:
      break;
    }

    ReleaseSysCache(tuple);

    if (!isnull)
    {
      PG_RETURN_BOOL(res);
    }
    PG_RETURN_NULL();
  }

  if (OidIsValid(index_oid))
  {
       
                                                                           
                                                                        
                             
       
    switch (prop)
    {
    case AMPROP_CLUSTERABLE:
      PG_RETURN_BOOL(routine->amclusterable);

    case AMPROP_INDEX_SCAN:
      PG_RETURN_BOOL(routine->amgettuple ? true : false);

    case AMPROP_BITMAP_SCAN:
      PG_RETURN_BOOL(routine->amgetbitmap ? true : false);

    case AMPROP_BACKWARD_SCAN:
      PG_RETURN_BOOL(routine->amcanbackward);

    default:
      PG_RETURN_NULL();
    }
  }

     
                                                                        
                    
     
  switch (prop)
  {
  case AMPROP_CAN_ORDER:
    PG_RETURN_BOOL(routine->amcanorder);

  case AMPROP_CAN_UNIQUE:
    PG_RETURN_BOOL(routine->amcanunique);

  case AMPROP_CAN_MULTI_COL:
    PG_RETURN_BOOL(routine->amcanmulticol);

  case AMPROP_CAN_EXCLUDE:
    PG_RETURN_BOOL(routine->amgettuple ? true : false);

  case AMPROP_CAN_INCLUDE:
    PG_RETURN_BOOL(routine->amcaninclude);

  default:
    PG_RETURN_NULL();
  }
}

   
                                              
   
Datum
pg_indexam_has_property(PG_FUNCTION_ARGS)
{
  Oid amoid = PG_GETARG_OID(0);
  char *propname = text_to_cstring(PG_GETARG_TEXT_PP(1));

  return indexam_property(fcinfo, propname, amoid, InvalidOid, 0);
}

   
                                                    
   
Datum
pg_index_has_property(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  char *propname = text_to_cstring(PG_GETARG_TEXT_PP(1));

  return indexam_property(fcinfo, propname, InvalidOid, relid, 0);
}

   
                                                                             
   
Datum
pg_index_column_has_property(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  int32 attno = PG_GETARG_INT32(1);
  char *propname = text_to_cstring(PG_GETARG_TEXT_PP(2));

                                                                          
  if (attno <= 0)
  {
    PG_RETURN_NULL();
  }

  return indexam_property(fcinfo, propname, InvalidOid, relid, attno);
}

   
                                                                             
             
   
Datum
pg_indexam_progress_phasename(PG_FUNCTION_ARGS)
{
  Oid amoid = PG_GETARG_OID(0);
  int32 phasenum = PG_GETARG_INT32(1);
  IndexAmRoutine *routine;
  char *name;

  routine = GetIndexAmRoutineByAmId(amoid, true);
  if (routine == NULL || !routine->ambuildphasename)
  {
    PG_RETURN_NULL();
  }

  name = routine->ambuildphasename(phasenum);
  if (!name)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(CStringGetTextDatum(name));
}
