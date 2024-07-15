   
                        
                                                                        
   
                                                                         
                                                                        
   
                  
                                             
   
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_db_role_setting.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

void
AlterSetting(Oid databaseid, Oid roleid, VariableSetStmt *setstmt)
{
  char *valuestr;
  HeapTuple tuple;
  Relation rel;
  ScanKeyData scankey[2];
  SysScanDesc scan;

  valuestr = ExtractSetVariableArgs(setstmt);

                                  

  rel = table_open(DbRoleSettingRelationId, RowExclusiveLock);
  ScanKeyInit(&scankey[0], Anum_pg_db_role_setting_setdatabase, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(databaseid));
  ScanKeyInit(&scankey[1], Anum_pg_db_role_setting_setrole, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));
  scan = systable_beginscan(rel, DbRoleSettingDatidRolidIndexId, true, NULL, 2, scankey);
  tuple = systable_getnext(scan);

     
                            
     
                                                                            
                                                           
     
                                                                           
                                        
     
                                                                         
                          
     
  if (setstmt->kind == VAR_RESET_ALL)
  {
    if (HeapTupleIsValid(tuple))
    {
      ArrayType *new = NULL;
      Datum datum;
      bool isnull;

      datum = heap_getattr(tuple, Anum_pg_db_role_setting_setconfig, RelationGetDescr(rel), &isnull);

      if (!isnull)
      {
        new = GUCArrayReset(DatumGetArrayTypeP(datum));
      }

      if (new)
      {
        Datum repl_val[Natts_pg_db_role_setting];
        bool repl_null[Natts_pg_db_role_setting];
        bool repl_repl[Natts_pg_db_role_setting];
        HeapTuple newtuple;

        memset(repl_repl, false, sizeof(repl_repl));

        repl_val[Anum_pg_db_role_setting_setconfig - 1] = PointerGetDatum(new);
        repl_repl[Anum_pg_db_role_setting_setconfig - 1] = true;
        repl_null[Anum_pg_db_role_setting_setconfig - 1] = false;

        newtuple = heap_modify_tuple(tuple, RelationGetDescr(rel), repl_val, repl_null, repl_repl);
        CatalogTupleUpdate(rel, &tuple->t_self, newtuple);
      }
      else
      {
        CatalogTupleDelete(rel, &tuple->t_self);
      }
    }
  }
  else if (HeapTupleIsValid(tuple))
  {
    Datum repl_val[Natts_pg_db_role_setting];
    bool repl_null[Natts_pg_db_role_setting];
    bool repl_repl[Natts_pg_db_role_setting];
    HeapTuple newtuple;
    Datum datum;
    bool isnull;
    ArrayType *a;

    memset(repl_repl, false, sizeof(repl_repl));
    repl_repl[Anum_pg_db_role_setting_setconfig - 1] = true;
    repl_null[Anum_pg_db_role_setting_setconfig - 1] = false;

                                        
    datum = heap_getattr(tuple, Anum_pg_db_role_setting_setconfig, RelationGetDescr(rel), &isnull);
    a = isnull ? NULL : DatumGetArrayTypeP(datum);

                                                  
    if (valuestr)
    {
      a = GUCArrayAdd(a, setstmt->name, valuestr);
    }
    else
    {
      a = GUCArrayDelete(a, setstmt->name);
    }

    if (a)
    {
      repl_val[Anum_pg_db_role_setting_setconfig - 1] = PointerGetDatum(a);

      newtuple = heap_modify_tuple(tuple, RelationGetDescr(rel), repl_val, repl_null, repl_repl);
      CatalogTupleUpdate(rel, &tuple->t_self, newtuple);
    }
    else
    {
      CatalogTupleDelete(rel, &tuple->t_self);
    }
  }
  else if (valuestr)
  {
                                                                       
    HeapTuple newtuple;
    Datum values[Natts_pg_db_role_setting];
    bool nulls[Natts_pg_db_role_setting];
    ArrayType *a;

    memset(nulls, false, sizeof(nulls));

    a = GUCArrayAdd(NULL, setstmt->name, valuestr);

    values[Anum_pg_db_role_setting_setdatabase - 1] = ObjectIdGetDatum(databaseid);
    values[Anum_pg_db_role_setting_setrole - 1] = ObjectIdGetDatum(roleid);
    values[Anum_pg_db_role_setting_setconfig - 1] = PointerGetDatum(a);
    newtuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);

    CatalogTupleInsert(rel, newtuple);
  }

  InvokeObjectPostAlterHookArg(DbRoleSettingRelationId, databaseid, 0, roleid, false);

  systable_endscan(scan);

                                                           
  table_close(rel, NoLock);
}

   
                                                                       
                                                                             
                                                     
   
void
DropSetting(Oid databaseid, Oid roleid)
{
  Relation relsetting;
  TableScanDesc scan;
  ScanKeyData keys[2];
  HeapTuple tup;
  int numkeys = 0;

  relsetting = table_open(DbRoleSettingRelationId, RowExclusiveLock);

  if (OidIsValid(databaseid))
  {
    ScanKeyInit(&keys[numkeys], Anum_pg_db_role_setting_setdatabase, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(databaseid));
    numkeys++;
  }
  if (OidIsValid(roleid))
  {
    ScanKeyInit(&keys[numkeys], Anum_pg_db_role_setting_setrole, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));
    numkeys++;
  }

  scan = table_beginscan_catalog(relsetting, numkeys, keys);
  while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
  {
    CatalogTupleDelete(relsetting, &tup->t_self);
  }
  table_endscan(scan);

  table_close(relsetting, RowExclusiveLock);
}

   
                                                                             
                        
   
                                                                
   
                                                                               
                                                                              
                      
   
void
ApplySetting(Snapshot snapshot, Oid databaseid, Oid roleid, Relation relsetting, GucSource source)
{
  SysScanDesc scan;
  ScanKeyData keys[2];
  HeapTuple tup;

  ScanKeyInit(&keys[0], Anum_pg_db_role_setting_setdatabase, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(databaseid));
  ScanKeyInit(&keys[1], Anum_pg_db_role_setting_setrole, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));

  scan = systable_beginscan(relsetting, DbRoleSettingDatidRolidIndexId, true, snapshot, 2, keys);
  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    bool isnull;
    Datum datum;

    datum = heap_getattr(tup, Anum_pg_db_role_setting_setconfig, RelationGetDescr(relsetting), &isnull);
    if (!isnull)
    {
      ArrayType *a = DatumGetArrayTypeP(datum);

         
                                                                        
                                                                       
                               
         
      ProcessGUCArray(a, PGC_SUSET, source, GUC_ACTION_SET);
    }
  }

  systable_endscan(scan);
}
