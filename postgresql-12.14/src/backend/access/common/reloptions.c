                                                                            
   
                
                                                             
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include <float.h>

#include "access/gist_private.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/spgist.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/tablespace.h"
#include "commands/view.h"
#include "nodes/makefuncs.h"
#include "postmaster/postmaster.h"
#include "utils/array.h"
#include "utils/attoptcache.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                   
   
                     
   
                                                                            
                                                                              
            
                                                          
                                                                         
                                                            
                       
                                                                    
                                            
   
                                                                        
                                                                        
                                                                           
                                                           
                                                                       
                                                                             
                                                                        
                                               
   
                                                                                
                                     
   
                                                                        
                                                                      
                        
   
                                                                               
                                          
   
                                                                          
                                                                        
                                                                            
                                                                              
   
                                                                               
                                                                             
                                                                               
                                                                            
                                                                           
                                                                       
   
                                                               
                                                                             
                                     
   
                                                                     
                                                                      
                                                                         
                                                                            
   

static relopt_bool boolRelOpts[] = {{{"autosummarize", "Enables automatic summarization on this BRIN index", RELOPT_KIND_BRIN, AccessExclusiveLock}, false}, {{"autovacuum_enabled", "Enables autovacuum in this relation", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, true}, {{"user_catalog_table", "Declare a table as an additional catalog table, e.g. for the purpose of logical replication", RELOPT_KIND_HEAP, AccessExclusiveLock}, false}, {{"fastupdate", "Enables \"fast update\" feature for this GIN index", RELOPT_KIND_GIN, AccessExclusiveLock}, true}, {{"security_barrier", "View acts as a row security barrier", RELOPT_KIND_VIEW, AccessExclusiveLock}, false}, {{"vacuum_index_cleanup", "Enables index vacuuming and index cleanup", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, true}, {{"vacuum_truncate", "Enables vacuum to truncate empty pages at the end of this table", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, true},
                         
    {{NULL}}};

static relopt_int intRelOpts[] = {{{
                                       "fillfactor", "Packs table pages only to this percentage", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock                                   
                                                                                                                                                          
                                   },
                                      HEAP_DEFAULT_FILLFACTOR, HEAP_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs btree index pages only to this percentage", RELOPT_KIND_BTREE, ShareUpdateExclusiveLock                                   
                                                                                                                                   
     },
        BTREE_DEFAULT_FILLFACTOR, BTREE_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs hash index pages only to this percentage", RELOPT_KIND_HASH, ShareUpdateExclusiveLock                                   
                                                                                                                                 
     },
        HASH_DEFAULT_FILLFACTOR, HASH_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs gist index pages only to this percentage", RELOPT_KIND_GIST, ShareUpdateExclusiveLock                                   
                                                                                                                                 
     },
        GIST_DEFAULT_FILLFACTOR, GIST_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs spgist index pages only to this percentage", RELOPT_KIND_SPGIST, ShareUpdateExclusiveLock                                   
                                                                                                                                     
     },
        SPGIST_DEFAULT_FILLFACTOR, SPGIST_MIN_FILLFACTOR, 100},
    {{"autovacuum_vacuum_threshold", "Minimum number of tuple updates or deletes prior to vacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, INT_MAX}, {{"autovacuum_analyze_threshold", "Minimum number of tuple inserts, updates or deletes prior to analyze", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, -1, 0, INT_MAX}, {{"autovacuum_vacuum_cost_limit", "Vacuum cost amount available before napping, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 1, 10000}, {{"autovacuum_freeze_min_age", "Minimum age at which VACUUM should freeze a table row, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 1000000000}, {{"autovacuum_multixact_freeze_min_age", "Minimum multixact age at which VACUUM should freeze a row multixact's, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 1000000000},
    {{"autovacuum_freeze_max_age", "Age at which to autovacuum a table to prevent transaction ID wraparound", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 100000, 2000000000}, {{"autovacuum_multixact_freeze_max_age", "Multixact age at which to autovacuum a table to prevent multixact wraparound", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 10000, 2000000000}, {{"autovacuum_freeze_table_age", "Age at which VACUUM should perform a full table sweep to freeze row versions", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 2000000000}, {{"autovacuum_multixact_freeze_table_age", "Age of multixact at which VACUUM should perform a full table sweep to freeze row versions", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 2000000000},
    {{"log_autovacuum_min_duration", "Sets the minimum execution time above which autovacuum actions will be logged", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, -1, INT_MAX}, {{"toast_tuple_target", "Sets the target tuple length at which external columns will be toasted", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, TOAST_TUPLE_TARGET, 128, TOAST_TUPLE_TARGET_MAIN}, {{"pages_per_range", "Number of pages that each page range covers in a BRIN index", RELOPT_KIND_BRIN, AccessExclusiveLock}, 128, 1, 131072}, {{"gin_pending_list_limit", "Maximum size of the pending list for this GIN index, in kilobytes.", RELOPT_KIND_GIN, AccessExclusiveLock}, -1, 64, MAX_KILOBYTES},
    {{"effective_io_concurrency", "Number of simultaneous requests that can be handled efficiently by the disk subsystem.", RELOPT_KIND_TABLESPACE, ShareUpdateExclusiveLock},
#ifdef USE_PREFETCH
        -1, 0, MAX_IO_CONCURRENCY
#else
        0, 0, 0
#endif
    },
    {{"parallel_workers", "Number of parallel processes that can be used per executor node for this relation.", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, -1, 0, 1024},

                         
    {{NULL}}};

static relopt_real realRelOpts[] = {{{"autovacuum_vacuum_cost_delay", "Vacuum cost delay in milliseconds, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0.0, 100.0}, {{"autovacuum_vacuum_scale_factor", "Number of tuple updates or deletes prior to vacuum as a fraction of reltuples", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0.0, 100.0}, {{"autovacuum_analyze_scale_factor", "Number of tuple inserts, updates or deletes prior to analyze as a fraction of reltuples", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, -1, 0.0, 100.0}, {{"seq_page_cost", "Sets the planner's estimate of the cost of a sequentially fetched disk page.", RELOPT_KIND_TABLESPACE, ShareUpdateExclusiveLock}, -1, 0.0, DBL_MAX}, {{"random_page_cost", "Sets the planner's estimate of the cost of a nonsequentially fetched disk page.", RELOPT_KIND_TABLESPACE, ShareUpdateExclusiveLock}, -1, 0.0, DBL_MAX},
    {{"n_distinct", "Sets the planner's estimate of the number of distinct values appearing in a column (excluding child relations).", RELOPT_KIND_ATTRIBUTE, ShareUpdateExclusiveLock}, 0, -1.0, DBL_MAX}, {{"n_distinct_inherited", "Sets the planner's estimate of the number of distinct values appearing in a column (including child relations).", RELOPT_KIND_ATTRIBUTE, ShareUpdateExclusiveLock}, 0, -1.0, DBL_MAX}, {{"vacuum_cleanup_index_scale_factor", "Number of tuple inserts prior to index cleanup as a fraction of reltuples.", RELOPT_KIND_BTREE, ShareUpdateExclusiveLock}, -1, 0.0, 1e10},
                         
    {{NULL}}};

static relopt_string stringRelOpts[] = {{{"buffering", "Enables buffering build for this GiST index", RELOPT_KIND_GIST, AccessExclusiveLock}, 4, false, gistValidateBufferingOption, "auto"}, {{"check_option", "View has WITH CHECK OPTION defined (local or cascaded).", RELOPT_KIND_VIEW, AccessExclusiveLock}, 0, true, validateWithCheckOption, NULL},
                         
    {{NULL}}};

static relopt_gen **relOpts = NULL;
static bits32 last_assigned_kind = RELOPT_KIND_LAST_DEFAULT;

static int num_custom_options = 0;
static relopt_gen **custom_options = NULL;
static bool need_initialization = true;

static void
initialize_reloptions(void);
static void
parse_one_reloption(relopt_value *option, char *text_str, int text_len, bool validate);

   
                         
                                                          
   
                                                                               
   
static void
initialize_reloptions(void)
{
  int i;
  int j;

  j = 0;
  for (i = 0; boolRelOpts[i].gen.name; i++)
  {
    Assert(DoLockModesConflict(boolRelOpts[i].gen.lockmode, boolRelOpts[i].gen.lockmode));
    j++;
  }
  for (i = 0; intRelOpts[i].gen.name; i++)
  {
    Assert(DoLockModesConflict(intRelOpts[i].gen.lockmode, intRelOpts[i].gen.lockmode));
    j++;
  }
  for (i = 0; realRelOpts[i].gen.name; i++)
  {
    Assert(DoLockModesConflict(realRelOpts[i].gen.lockmode, realRelOpts[i].gen.lockmode));
    j++;
  }
  for (i = 0; stringRelOpts[i].gen.name; i++)
  {
    Assert(DoLockModesConflict(stringRelOpts[i].gen.lockmode, stringRelOpts[i].gen.lockmode));
    j++;
  }
  j += num_custom_options;

  if (relOpts)
  {
    pfree(relOpts);
  }
  relOpts = MemoryContextAlloc(TopMemoryContext, (j + 1) * sizeof(relopt_gen *));

  j = 0;
  for (i = 0; boolRelOpts[i].gen.name; i++)
  {
    relOpts[j] = &boolRelOpts[i].gen;
    relOpts[j]->type = RELOPT_TYPE_BOOL;
    relOpts[j]->namelen = strlen(relOpts[j]->name);
    j++;
  }

  for (i = 0; intRelOpts[i].gen.name; i++)
  {
    relOpts[j] = &intRelOpts[i].gen;
    relOpts[j]->type = RELOPT_TYPE_INT;
    relOpts[j]->namelen = strlen(relOpts[j]->name);
    j++;
  }

  for (i = 0; realRelOpts[i].gen.name; i++)
  {
    relOpts[j] = &realRelOpts[i].gen;
    relOpts[j]->type = RELOPT_TYPE_REAL;
    relOpts[j]->namelen = strlen(relOpts[j]->name);
    j++;
  }

  for (i = 0; stringRelOpts[i].gen.name; i++)
  {
    relOpts[j] = &stringRelOpts[i].gen;
    relOpts[j]->type = RELOPT_TYPE_STRING;
    relOpts[j]->namelen = strlen(relOpts[j]->name);
    j++;
  }

  for (i = 0; i < num_custom_options; i++)
  {
    relOpts[j] = custom_options[i];
    j++;
  }

                             
  relOpts[j] = NULL;

                                 
  need_initialization = false;
}

   
                      
                                                                       
                      
   
relopt_kind
add_reloption_kind(void)
{
                                                                           
  if (last_assigned_kind >= RELOPT_KIND_MAX)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("user-defined relation parameter types limit exceeded")));
  }
  last_assigned_kind <<= 1;
  return (relopt_kind)last_assigned_kind;
}

   
                 
                                                                           
                       
   
static void
add_reloption(relopt_gen *newoption)
{
  static int max_custom_options = 0;

  if (num_custom_options >= max_custom_options)
  {
    MemoryContext oldcxt;

    oldcxt = MemoryContextSwitchTo(TopMemoryContext);

    if (max_custom_options == 0)
    {
      max_custom_options = 8;
      custom_options = palloc(max_custom_options * sizeof(relopt_gen *));
    }
    else
    {
      max_custom_options *= 2;
      custom_options = repalloc(custom_options, max_custom_options * sizeof(relopt_gen *));
    }
    MemoryContextSwitchTo(oldcxt);
  }
  custom_options[num_custom_options++] = newoption;

  need_initialization = true;
}

   
                      
                                                                     
                                  
   
static relopt_gen *
allocate_reloption(bits32 kinds, int type, const char *name, const char *desc)
{
  MemoryContext oldcxt;
  size_t size;
  relopt_gen *newoption;

  oldcxt = MemoryContextSwitchTo(TopMemoryContext);

  switch (type)
  {
  case RELOPT_TYPE_BOOL:
    size = sizeof(relopt_bool);
    break;
  case RELOPT_TYPE_INT:
    size = sizeof(relopt_int);
    break;
  case RELOPT_TYPE_REAL:
    size = sizeof(relopt_real);
    break;
  case RELOPT_TYPE_STRING:
    size = sizeof(relopt_string);
    break;
  default:
    elog(ERROR, "unsupported reloption type %d", type);
    return NULL;                          
  }

  newoption = palloc(size);

  newoption->name = pstrdup(name);
  if (desc)
  {
    newoption->desc = pstrdup(desc);
  }
  else
  {
    newoption->desc = NULL;
  }
  newoption->kinds = kinds;
  newoption->namelen = strlen(name);
  newoption->type = type;

     
                                                                        
                                                                         
                                                                 
     
  newoption->lockmode = AccessExclusiveLock;

  MemoryContextSwitchTo(oldcxt);

  return newoption;
}

   
                      
                                
   
void
add_bool_reloption(bits32 kinds, const char *name, const char *desc, bool default_val)
{
  relopt_bool *newoption;

  newoption = (relopt_bool *)allocate_reloption(kinds, RELOPT_TYPE_BOOL, name, desc);
  newoption->default_val = default_val;

  add_reloption((relopt_gen *)newoption);
}

   
                     
                                
   
void
add_int_reloption(bits32 kinds, const char *name, const char *desc, int default_val, int min_val, int max_val)
{
  relopt_int *newoption;

  newoption = (relopt_int *)allocate_reloption(kinds, RELOPT_TYPE_INT, name, desc);
  newoption->default_val = default_val;
  newoption->min = min_val;
  newoption->max = max_val;

  add_reloption((relopt_gen *)newoption);
}

   
                      
                              
   
void
add_real_reloption(bits32 kinds, const char *name, const char *desc, double default_val, double min_val, double max_val)
{
  relopt_real *newoption;

  newoption = (relopt_real *)allocate_reloption(kinds, RELOPT_TYPE_REAL, name, desc);
  newoption->default_val = default_val;
  newoption->min = min_val;
  newoption->max = max_val;

  add_reloption((relopt_gen *)newoption);
}

   
                        
                               
   
                                                                            
                                                                            
                                                                           
                   
   
void
add_string_reloption(bits32 kinds, const char *name, const char *desc, const char *default_val, validate_string_relopt validator)
{
  relopt_string *newoption;

                                                           
  if (validator)
  {
    (validator)(default_val);
  }

  newoption = (relopt_string *)allocate_reloption(kinds, RELOPT_TYPE_STRING, name, desc);
  newoption->validate_cb = validator;
  if (default_val)
  {
    newoption->default_val = MemoryContextStrdup(TopMemoryContext, default_val);
    newoption->default_len = strlen(default_val);
    newoption->default_isnull = false;
  }
  else
  {
    newoption->default_val = "";
    newoption->default_len = 0;
    newoption->default_isnull = true;
  }

  add_reloption((relopt_gen *)newoption);
}

   
                                                                           
                                                                            
                                                                           
              
   
                                                                          
                                                                      
                                                                      
              
   
                                                                              
                                                          
   
                                                                         
                                                                              
                                                                            
                                                                       
                       
   
                                                                           
                                                                             
   
Datum
transformRelOptions(Datum oldOptions, List *defList, const char *namspace, char *validnsps[], bool acceptOidsOff, bool isReset)
{
  Datum result;
  ArrayBuildState *astate;
  ListCell *cell;

                               
  if (defList == NIL)
  {
    return oldOptions;
  }

                                                 
  astate = NULL;

                                                      
  if (PointerIsValid(DatumGetPointer(oldOptions)))
  {
    ArrayType *array = DatumGetArrayTypeP(oldOptions);
    Datum *oldoptions;
    int noldoptions;
    int i;

    deconstruct_array(array, TEXTOID, -1, false, 'i', &oldoptions, NULL, &noldoptions);

    for (i = 0; i < noldoptions; i++)
    {
      char *text_str = VARDATA(oldoptions[i]);
      int text_len = VARSIZE(oldoptions[i]) - VARHDRSZ;

                                         
      foreach (cell, defList)
      {
        DefElem *def = (DefElem *)lfirst(cell);
        int kw_len;

                                                 
        if (namspace == NULL)
        {
          if (def->defnamespace != NULL)
          {
            continue;
          }
        }
        else if (def->defnamespace == NULL)
        {
          continue;
        }
        else if (strcmp(def->defnamespace, namspace) != 0)
        {
          continue;
        }

        kw_len = strlen(def->defname);
        if (text_len > kw_len && text_str[kw_len] == '=' && strncmp(text_str, def->defname, kw_len) == 0)
        {
          break;
        }
      }
      if (!cell)
      {
                                          
        astate = accumArrayResult(astate, oldoptions[i], false, TEXTOID, CurrentMemoryContext);
      }
    }
  }

     
                                                                            
                                                                            
                          
     
  foreach (cell, defList)
  {
    DefElem *def = (DefElem *)lfirst(cell);

    if (isReset)
    {
      if (def->arg != NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("RESET must not include values for parameters")));
      }
    }
    else
    {
      text *t;
      const char *value;
      Size len;

         
                                                                       
                       
         
      if (def->defnamespace != NULL)
      {
        bool valid = false;
        int i;

        if (validnsps)
        {
          for (i = 0; validnsps[i]; i++)
          {
            if (strcmp(def->defnamespace, validnsps[i]) == 0)
            {
              valid = true;
              break;
            }
          }
        }

        if (!valid)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized parameter namespace \"%s\"", def->defnamespace)));
        }
      }

                                               
      if (namspace == NULL)
      {
        if (def->defnamespace != NULL)
        {
          continue;
        }
      }
      else if (def->defnamespace == NULL)
      {
        continue;
      }
      else if (strcmp(def->defnamespace, namspace) != 0)
      {
        continue;
      }

         
                                                                       
                                                                   
                                  
         
      if (def->arg != NULL)
      {
        value = defGetString(def);
      }
      else
      {
        value = "true";
      }

         
                                                                       
                                                                    
                                                                       
                         
         
      if (acceptOidsOff && def->defnamespace == NULL && strcmp(def->defname, "oids") == 0)
      {
        if (defGetBoolean(def))
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tables declared WITH OIDS are not supported")));
        }
                                                                    
        continue;
      }

      len = VARHDRSZ + strlen(def->defname) + 1 + strlen(value);
                                                      
      t = (text *)palloc(len + 1);
      SET_VARSIZE(t, len);
      sprintf(VARDATA(t), "%s=%s", def->defname, value);

      astate = accumArrayResult(astate, PointerGetDatum(t), false, TEXTOID, CurrentMemoryContext);
    }
  }

  if (astate)
  {
    result = makeArrayResult(astate, CurrentMemoryContext);
  }
  else
  {
    result = (Datum)0;
  }

  return result;
}

   
                                                                       
                                                 
   
List *
untransformRelOptions(Datum options)
{
  List *result = NIL;
  ArrayType *array;
  Datum *optiondatums;
  int noptions;
  int i;

                                   
  if (!PointerIsValid(DatumGetPointer(options)))
  {
    return result;
  }

  array = DatumGetArrayTypeP(options);

  deconstruct_array(array, TEXTOID, -1, false, 'i', &optiondatums, NULL, &noptions);

  for (i = 0; i < noptions; i++)
  {
    char *s;
    char *p;
    Node *val = NULL;

    s = TextDatumGetCString(optiondatums[i]);
    p = strchr(s, '=');
    if (p)
    {
      *p++ = '\0';
      val = (Node *)makeString(pstrdup(p));
    }
    result = lappend(result, makeDefElem(pstrdup(s), val, -1));
  }

  return result;
}

   
                                                       
   
                                                                         
                                                                             
                                                                                
            
   
                                                                               
                                                                           
                             
   
bytea *
extractRelOptions(HeapTuple tuple, TupleDesc tupdesc, amoptions_function amoptions)
{
  bytea *options;
  bool isnull;
  Datum datum;
  Form_pg_class classForm;

  datum = fastgetattr(tuple, Anum_pg_class_reloptions, tupdesc, &isnull);
  if (isnull)
  {
    return NULL;
  }

  classForm = (Form_pg_class)GETSTRUCT(tuple);

                                                           
  switch (classForm->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_TOASTVALUE:
  case RELKIND_MATVIEW:
  case RELKIND_PARTITIONED_TABLE:
    options = heap_reloptions(classForm->relkind, datum, false);
    break;
  case RELKIND_VIEW:
    options = view_reloptions(datum, false);
    break;
  case RELKIND_INDEX:
  case RELKIND_PARTITIONED_INDEX:
    options = index_reloptions(amoptions, datum, false);
    break;
  case RELKIND_FOREIGN_TABLE:
    options = NULL;
    break;
  default:
    Assert(false);                      
    options = NULL;                          
    break;
  }

  return options;
}

   
                                                             
   
                                                                            
                                                         
   
                                                                            
                                                                            
                                                                              
                                                                           
   
                                                                              
                                                                         
                                            
   
                                                                        
                                                                            
                           
   
relopt_value *
parseRelOptions(Datum options, bool validate, relopt_kind kind, int *numrelopts)
{
  relopt_value *reloptions = NULL;
  int numoptions = 0;
  int i;
  int j;

  if (need_initialization)
  {
    initialize_reloptions();
  }

                                                       

  for (i = 0; relOpts[i]; i++)
  {
    if (relOpts[i]->kinds & kind)
    {
      numoptions++;
    }
  }

  if (numoptions > 0)
  {
    reloptions = palloc(numoptions * sizeof(relopt_value));

    for (i = 0, j = 0; relOpts[i]; i++)
    {
      if (relOpts[i]->kinds & kind)
      {
        reloptions[j].gen = relOpts[i];
        reloptions[j].isset = false;
        j++;
      }
    }
  }

                          
  if (PointerIsValid(DatumGetPointer(options)))
  {
    ArrayType *array = DatumGetArrayTypeP(options);
    Datum *optiondatums;
    int noptions;

    deconstruct_array(array, TEXTOID, -1, false, 'i', &optiondatums, NULL, &noptions);

    for (i = 0; i < noptions; i++)
    {
      char *text_str = VARDATA(optiondatums[i]);
      int text_len = VARSIZE(optiondatums[i]) - VARHDRSZ;
      int j;

                                            
      for (j = 0; j < numoptions; j++)
      {
        int kw_len = reloptions[j].gen->namelen;

        if (text_len > kw_len && text_str[kw_len] == '=' && strncmp(text_str, reloptions[j].gen->name, kw_len) == 0)
        {
          parse_one_reloption(&reloptions[j], text_str, text_len, validate);
          break;
        }
      }

      if (j >= numoptions && validate)
      {
        char *s;
        char *p;

        s = TextDatumGetCString(optiondatums[i]);
        p = strchr(s, '=');
        if (p)
        {
          *p = '\0';
        }
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized parameter \"%s\"", s)));
      }
    }

                                                           
    pfree(optiondatums);
    if (((void *)array) != DatumGetPointer(options))
    {
      pfree(array);
    }
  }

  *numrelopts = numoptions;
  return reloptions;
}

   
                                                                           
         
   
static void
parse_one_reloption(relopt_value *option, char *text_str, int text_len, bool validate)
{
  char *value;
  int value_len;
  bool parsed;
  bool nofree = false;

  if (option->isset && validate)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("parameter \"%s\" specified more than once", option->gen->name)));
  }

  value_len = text_len - option->gen->namelen - 1;
  value = (char *)palloc(value_len + 1);
  memcpy(value, text_str + option->gen->namelen + 1, value_len);
  value[value_len] = '\0';

  switch (option->gen->type)
  {
  case RELOPT_TYPE_BOOL:
  {
    parsed = parse_bool(value, &option->values.bool_val);
    if (validate && !parsed)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for boolean option \"%s\": %s", option->gen->name, value)));
    }
  }
  break;
  case RELOPT_TYPE_INT:
  {
    relopt_int *optint = (relopt_int *)option->gen;

    parsed = parse_int(value, &option->values.int_val, 0, NULL);
    if (validate && !parsed)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for integer option \"%s\": %s", option->gen->name, value)));
    }
    if (validate && (option->values.int_val < optint->min || option->values.int_val > optint->max))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("value %s out of bounds for option \"%s\"", value, option->gen->name), errdetail("Valid values are between \"%d\" and \"%d\".", optint->min, optint->max)));
    }
  }
  break;
  case RELOPT_TYPE_REAL:
  {
    relopt_real *optreal = (relopt_real *)option->gen;

    parsed = parse_real(value, &option->values.real_val, 0, NULL);
    if (validate && !parsed)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for floating point option \"%s\": %s", option->gen->name, value)));
    }
    if (validate && (option->values.real_val < optreal->min || option->values.real_val > optreal->max))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("value %s out of bounds for option \"%s\"", value, option->gen->name), errdetail("Valid values are between \"%f\" and \"%f\".", optreal->min, optreal->max)));
    }
  }
  break;
  case RELOPT_TYPE_STRING:
  {
    relopt_string *optstring = (relopt_string *)option->gen;

    option->values.string_val = value;
    nofree = true;
    if (validate && optstring->validate_cb)
    {
      (optstring->validate_cb)(value);
    }
    parsed = true;
  }
  break;
  default:
    elog(ERROR, "unsupported reloption type %d", option->gen->type);
    parsed = true;                     
    break;
  }

  if (parsed)
  {
    option->isset = true;
  }
  if (!nofree)
  {
    pfree(value);
  }
}

   
                                                                          
                                                                                
   
                                                                             
                
   
void *
allocateReloptStruct(Size base, relopt_value *options, int numoptions)
{
  Size size = base;
  int i;

  for (i = 0; i < numoptions; i++)
  {
    if (options[i].gen->type == RELOPT_TYPE_STRING)
    {
      size += GET_STRING_RELOPTION_LEN(options[i]) + 1;
    }
  }

  return palloc0(size);
}

   
                                                                        
                                                                           
           
   
                                                               
                                                                           
                                                              
                                                                           
                                                                           
   
void
fillRelOptions(void *rdopts, Size basesize, relopt_value *options, int numoptions, bool validate, const relopt_parse_elt *elems, int numelems)
{
  int i;
  int offset = basesize;

  for (i = 0; i < numoptions; i++)
  {
    int j;
    bool found = false;

    for (j = 0; j < numelems; j++)
    {
      if (strcmp(options[i].gen->name, elems[j].optname) == 0)
      {
        relopt_string *optstring;
        char *itempos = ((char *)rdopts) + elems[j].offset;
        char *string_val;

        switch (options[i].gen->type)
        {
        case RELOPT_TYPE_BOOL:
          *(bool *)itempos = options[i].isset ? options[i].values.bool_val : ((relopt_bool *)options[i].gen)->default_val;
          break;
        case RELOPT_TYPE_INT:
          *(int *)itempos = options[i].isset ? options[i].values.int_val : ((relopt_int *)options[i].gen)->default_val;
          break;
        case RELOPT_TYPE_REAL:
          *(double *)itempos = options[i].isset ? options[i].values.real_val : ((relopt_real *)options[i].gen)->default_val;
          break;
        case RELOPT_TYPE_STRING:
          optstring = (relopt_string *)options[i].gen;
          if (options[i].isset)
          {
            string_val = options[i].values.string_val;
          }
          else if (!optstring->default_isnull)
          {
            string_val = optstring->default_val;
          }
          else
          {
            string_val = NULL;
          }

          if (string_val == NULL)
          {
            *(int *)itempos = 0;
          }
          else
          {
            strcpy((char *)rdopts + offset, string_val);
            *(int *)itempos = offset;
            offset += strlen(string_val) + 1;
          }
          break;
        default:
          elog(ERROR, "unsupported reloption type %d", options[i].gen->type);
          break;
        }
        found = true;
        break;
      }
    }
    if (validate && !found)
    {
      elog(ERROR, "reloption \"%s\" not found in parse table", options[i].gen->name);
    }
  }
  SET_VARSIZE(rdopts, offset);
}

   
                                                      
   
bytea *
default_reloptions(Datum reloptions, bool validate, relopt_kind kind)
{
  relopt_value *options;
  StdRdOptions *rdopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"fillfactor", RELOPT_TYPE_INT, offsetof(StdRdOptions, fillfactor)}, {"autovacuum_enabled", RELOPT_TYPE_BOOL, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, enabled)}, {"autovacuum_vacuum_threshold", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, vacuum_threshold)}, {"autovacuum_analyze_threshold", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, analyze_threshold)}, {"autovacuum_vacuum_cost_limit", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, vacuum_cost_limit)}, {"autovacuum_freeze_min_age", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, freeze_min_age)}, {"autovacuum_freeze_max_age", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, freeze_max_age)}, {"autovacuum_freeze_table_age", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, freeze_table_age)},
      {"autovacuum_multixact_freeze_min_age", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, multixact_freeze_min_age)}, {"autovacuum_multixact_freeze_max_age", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, multixact_freeze_max_age)}, {"autovacuum_multixact_freeze_table_age", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, multixact_freeze_table_age)}, {"log_autovacuum_min_duration", RELOPT_TYPE_INT, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, log_min_duration)}, {"toast_tuple_target", RELOPT_TYPE_INT, offsetof(StdRdOptions, toast_tuple_target)}, {"autovacuum_vacuum_cost_delay", RELOPT_TYPE_REAL, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, vacuum_cost_delay)}, {"autovacuum_vacuum_scale_factor", RELOPT_TYPE_REAL, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, vacuum_scale_factor)},
      {"autovacuum_analyze_scale_factor", RELOPT_TYPE_REAL, offsetof(StdRdOptions, autovacuum) + offsetof(AutoVacOpts, analyze_scale_factor)}, {"user_catalog_table", RELOPT_TYPE_BOOL, offsetof(StdRdOptions, user_catalog_table)}, {"parallel_workers", RELOPT_TYPE_INT, offsetof(StdRdOptions, parallel_workers)}, {"vacuum_cleanup_index_scale_factor", RELOPT_TYPE_REAL, offsetof(StdRdOptions, vacuum_cleanup_index_scale_factor)}, {"vacuum_index_cleanup", RELOPT_TYPE_BOOL, offsetof(StdRdOptions, vacuum_index_cleanup)}, {"vacuum_truncate", RELOPT_TYPE_BOOL, offsetof(StdRdOptions, vacuum_truncate)}};

  options = parseRelOptions(reloptions, validate, kind, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  rdopts = allocateReloptStruct(sizeof(StdRdOptions), options, numoptions);

  fillRelOptions((void *)rdopts, sizeof(StdRdOptions), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)rdopts;
}

   
                           
   
bytea *
view_reloptions(Datum reloptions, bool validate)
{
  relopt_value *options;
  ViewOptions *vopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"security_barrier", RELOPT_TYPE_BOOL, offsetof(ViewOptions, security_barrier)}, {"check_option", RELOPT_TYPE_STRING, offsetof(ViewOptions, check_option_offset)}};

  options = parseRelOptions(reloptions, validate, RELOPT_KIND_VIEW, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  vopts = allocateReloptStruct(sizeof(ViewOptions), options, numoptions);

  fillRelOptions((void *)vopts, sizeof(ViewOptions), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)vopts;
}

   
                                                    
   
bytea *
heap_reloptions(char relkind, Datum reloptions, bool validate)
{
  StdRdOptions *rdopts;

  switch (relkind)
  {
  case RELKIND_TOASTVALUE:
    rdopts = (StdRdOptions *)default_reloptions(reloptions, validate, RELOPT_KIND_TOAST);
    if (rdopts != NULL)
    {
                                                              
      rdopts->fillfactor = 100;
      rdopts->autovacuum.analyze_threshold = -1;
      rdopts->autovacuum.analyze_scale_factor = -1;
    }
    return (bytea *)rdopts;
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
    return default_reloptions(reloptions, validate, RELOPT_KIND_HEAP);
  case RELKIND_PARTITIONED_TABLE:
    return default_reloptions(reloptions, validate, RELOPT_KIND_PARTITIONED);
  default:
                                          
    return NULL;
  }
}

   
                              
   
                                               
                                      
                       
   
bytea *
index_reloptions(amoptions_function amoptions, Datum reloptions, bool validate)
{
  Assert(amoptions != NULL);

                                 
  if (!PointerIsValid(DatumGetPointer(reloptions)))
  {
    return NULL;
  }

  return amoptions(reloptions, validate);
}

   
                                          
   
bytea *
attribute_reloptions(Datum reloptions, bool validate)
{
  relopt_value *options;
  AttributeOpts *aopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"n_distinct", RELOPT_TYPE_REAL, offsetof(AttributeOpts, n_distinct)}, {"n_distinct_inherited", RELOPT_TYPE_REAL, offsetof(AttributeOpts, n_distinct_inherited)}};

  options = parseRelOptions(reloptions, validate, RELOPT_KIND_ATTRIBUTE, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  aopts = allocateReloptStruct(sizeof(AttributeOpts), options, numoptions);

  fillRelOptions((void *)aopts, sizeof(AttributeOpts), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)aopts;
}

   
                                           
   
bytea *
tablespace_reloptions(Datum reloptions, bool validate)
{
  relopt_value *options;
  TableSpaceOpts *tsopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"random_page_cost", RELOPT_TYPE_REAL, offsetof(TableSpaceOpts, random_page_cost)}, {"seq_page_cost", RELOPT_TYPE_REAL, offsetof(TableSpaceOpts, seq_page_cost)}, {"effective_io_concurrency", RELOPT_TYPE_INT, offsetof(TableSpaceOpts, effective_io_concurrency)}};

  options = parseRelOptions(reloptions, validate, RELOPT_KIND_TABLESPACE, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  tsopts = allocateReloptStruct(sizeof(TableSpaceOpts), options, numoptions);

  fillRelOptions((void *)tsopts, sizeof(TableSpaceOpts), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)tsopts;
}

   
                                                        
   
                                                           
                                               
   
LOCKMODE
AlterTableGetRelOptionsLockLevel(List *defList)
{
  LOCKMODE lockmode = NoLock;
  ListCell *cell;

  if (defList == NIL)
  {
    return AccessExclusiveLock;
  }

  if (need_initialization)
  {
    initialize_reloptions();
  }

  foreach (cell, defList)
  {
    DefElem *def = (DefElem *)lfirst(cell);
    int i;

    for (i = 0; relOpts[i]; i++)
    {
      if (strncmp(relOpts[i]->name, def->defname, relOpts[i]->namelen + 1) == 0)
      {
        if (lockmode < relOpts[i]->lockmode)
        {
          lockmode = relOpts[i]->lockmode;
        }
      }
    }
  }

  return lockmode;
}
