                                                                         
   
                
                                                               
   
                                                                         
                                                                        
   
                                         
                                                                         
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

   
                     
                                                                
                                                                  
                    
   
const TableAmRoutine *
GetTableAmRoutine(Oid amhandler)
{
  Datum datum;
  const TableAmRoutine *routine;

  datum = OidFunctionCall0(amhandler);
  routine = (TableAmRoutine *)DatumGetPointer(datum);

  if (routine == NULL || !IsA(routine, TableAmRoutine))
  {
    elog(ERROR, "table access method handler %u did not return a TableAmRoutine struct", amhandler);
  }

     
                                                                         
                                                                            
                    
     
  Assert(routine->scan_begin != NULL);
  Assert(routine->scan_end != NULL);
  Assert(routine->scan_rescan != NULL);
  Assert(routine->scan_getnextslot != NULL);

  Assert(routine->parallelscan_estimate != NULL);
  Assert(routine->parallelscan_initialize != NULL);
  Assert(routine->parallelscan_reinitialize != NULL);

  Assert(routine->index_fetch_begin != NULL);
  Assert(routine->index_fetch_reset != NULL);
  Assert(routine->index_fetch_end != NULL);
  Assert(routine->index_fetch_tuple != NULL);

  Assert(routine->tuple_fetch_row_version != NULL);
  Assert(routine->tuple_tid_valid != NULL);
  Assert(routine->tuple_get_latest_tid != NULL);
  Assert(routine->tuple_satisfies_snapshot != NULL);
  Assert(routine->compute_xid_horizon_for_tuples != NULL);

  Assert(routine->tuple_insert != NULL);

     
                                                                     
                     
     
  Assert(routine->tuple_insert_speculative != NULL);
  Assert(routine->tuple_complete_speculative != NULL);

  Assert(routine->multi_insert != NULL);
  Assert(routine->tuple_delete != NULL);
  Assert(routine->tuple_update != NULL);
  Assert(routine->tuple_lock != NULL);

  Assert(routine->relation_set_new_filenode != NULL);
  Assert(routine->relation_nontransactional_truncate != NULL);
  Assert(routine->relation_copy_data != NULL);
  Assert(routine->relation_copy_for_cluster != NULL);
  Assert(routine->relation_vacuum != NULL);
  Assert(routine->scan_analyze_next_block != NULL);
  Assert(routine->scan_analyze_next_tuple != NULL);
  Assert(routine->index_build_range_scan != NULL);
  Assert(routine->index_validate_scan != NULL);

  Assert(routine->relation_size != NULL);
  Assert(routine->relation_needs_toast_table != NULL);

  Assert(routine->relation_estimate_size != NULL);

                                                                
  Assert((routine->scan_bitmap_next_block == NULL) == (routine->scan_bitmap_next_tuple == NULL));
  Assert(routine->scan_sample_next_block != NULL);
  Assert(routine->scan_sample_next_tuple != NULL);

  return routine;
}

                                                          
bool
check_default_table_access_method(char **newval, void **extra, GucSource source)
{
  if (**newval == '\0')
  {
    GUC_check_errdetail("%s cannot be empty.", "default_table_access_method");
    return false;
  }

  if (strlen(*newval) >= NAMEDATALEN)
  {
    GUC_check_errdetail("%s is too long (maximum %d characters).", "default_table_access_method", NAMEDATALEN - 1);
    return false;
  }

     
                                                                           
                                                                        
                                
     
  if (IsTransactionState() && MyDatabaseId != InvalidOid)
  {
    if (!OidIsValid(get_table_am_oid(*newval, true)))
    {
         
                                                                   
                                                                         
                
         
      if (source == PGC_S_TEST)
      {
        ereport(NOTICE, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("table access method \"%s\" does not exist", *newval)));
      }
      else
      {
        GUC_check_errdetail("Table access method \"%s\" does not exist.", *newval);
        return false;
      }
    }
  }

  return true;
}
