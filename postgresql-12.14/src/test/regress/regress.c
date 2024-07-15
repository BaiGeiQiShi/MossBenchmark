                                                                           
   
             
                                                                 
                      
   
                                                                    
   
                                                                         
                                                                        
   
                              
   
                                                                            
   

#include "postgres.h"

#include <math.h>
#include <signal.h>

#include "access/htup_details.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "commands/sequence.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "nodes/supportnodes.h"
#include "optimizer/optimizer.h"
#include "optimizer/plancat.h"
#include "port/atomics.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/geo_decls.h"
#include "utils/rel.h"
#include "utils/typcache.h"
#include "utils/memutils.h"

#define EXPECT_TRUE(expr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!(expr))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      elog(ERROR, "%s was unexpectedly false in file \"%s\" line %u", #expr, __FILE__, __LINE__);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define EXPECT_EQ_U32(result_expr, expected_expr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    uint32 result = (result_expr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    uint32 expected = (expected_expr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (result != expected)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      elog(ERROR, "%s yielded %u, expected %s in file \"%s\" line %u", #result_expr, result, #expected_expr, __FILE__, __LINE__);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define EXPECT_EQ_U64(result_expr, expected_expr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    uint64 result = (result_expr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    uint64 expected = (expected_expr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (result != expected)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      elog(ERROR, "%s yielded " UINT64_FORMAT ", expected %s in file \"%s\" line %u", #result_expr, result, #expected_expr, __FILE__, __LINE__);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

#define LDELIM '('
#define RDELIM ')'
#define DELIM ','

static void
regress_lseg_construct(LSEG *lseg, Point *pt1, Point *pt2);

PG_MODULE_MAGIC;

                                                                             
PG_FUNCTION_INFO_V1(interpt_pp);

Datum
interpt_pp(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);
  int i, j;
  LSEG seg1, seg2;
  bool found;                                   

  found = false;                           

  for (i = 0; i < p1->npts - 1 && !found; i++)
  {
    regress_lseg_construct(&seg1, &p1->p[i], &p1->p[i + 1]);
    for (j = 0; j < p2->npts - 1 && !found; j++)
    {
      regress_lseg_construct(&seg2, &p2->p[j], &p2->p[j + 1]);
      if (DatumGetBool(DirectFunctionCall2(lseg_intersect, LsegPGetDatum(&seg1), LsegPGetDatum(&seg2))))
      {
        found = true;
      }
    }
  }

  if (!found)
  {
    PG_RETURN_NULL();
  }

     
                                                                        
                                                                       
                         
     
  PG_RETURN_DATUM(DirectFunctionCall2(lseg_interpt, LsegPGetDatum(&seg1), LsegPGetDatum(&seg2)));
}

                                                             
static void
regress_lseg_construct(LSEG *lseg, Point *pt1, Point *pt2)
{
  lseg->p[0].x = pt1->x;
  lseg->p[0].y = pt1->y;
  lseg->p[1].x = pt2->x;
  lseg->p[1].y = pt2->y;
}

PG_FUNCTION_INFO_V1(overpaid);

Datum
overpaid(PG_FUNCTION_ARGS)
{
  HeapTupleHeader tuple = PG_GETARG_HEAPTUPLEHEADER(0);
  bool isnull;
  int32 salary;

  salary = DatumGetInt32(GetAttributeByName(tuple, "salary", &isnull));
  if (isnull)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(salary > 699);
}

                     
                                                             
                                                                   
   

typedef struct
{
  Point center;
  double radius;
} WIDGET;

PG_FUNCTION_INFO_V1(widget_in);
PG_FUNCTION_INFO_V1(widget_out);

#define NARGS 3

Datum
widget_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  char *p, *coord[NARGS];
  int i;
  WIDGET *result;

  for (i = 0, p = str; *p && i < NARGS && *p != RDELIM; p++)
  {
    if (*p == DELIM || (*p == LDELIM && i == 0))
    {
      coord[i++] = p + 1;
    }
  }

  if (i < NARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "widget", str)));
  }

  result = (WIDGET *)palloc(sizeof(WIDGET));
  result->center.x = atof(coord[0]);
  result->center.y = atof(coord[1]);
  result->radius = atof(coord[2]);

  PG_RETURN_POINTER(result);
}

Datum
widget_out(PG_FUNCTION_ARGS)
{
  WIDGET *widget = (WIDGET *)PG_GETARG_POINTER(0);
  char *str = psprintf("(%g,%g,%g)", widget->center.x, widget->center.y, widget->radius);

  PG_RETURN_CSTRING(str);
}

PG_FUNCTION_INFO_V1(pt_in_widget);

Datum
pt_in_widget(PG_FUNCTION_ARGS)
{
  Point *point = PG_GETARG_POINT_P(0);
  WIDGET *widget = (WIDGET *)PG_GETARG_POINTER(1);
  float8 distance;

  distance = DatumGetFloat8(DirectFunctionCall2(point_distance, PointPGetDatum(point), PointPGetDatum(&widget->center)));

  PG_RETURN_BOOL(distance < widget->radius);
}

PG_FUNCTION_INFO_V1(reverse_name);

Datum
reverse_name(PG_FUNCTION_ARGS)
{
  char *string = PG_GETARG_CSTRING(0);
  int i;
  int len;
  char *new_string;

  new_string = palloc0(NAMEDATALEN);
  for (i = 0; i < NAMEDATALEN && string[i]; ++i)
    ;
  if (i == NAMEDATALEN || !string[i])
  {
    --i;
  }
  len = i;
  for (; i >= 0; --i)
  {
    new_string[len - i] = string[i];
  }
  PG_RETURN_CSTRING(new_string);
}

PG_FUNCTION_INFO_V1(trigger_return_old);

Datum
trigger_return_old(PG_FUNCTION_ARGS)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  HeapTuple tuple;

  if (!CALLED_AS_TRIGGER(fcinfo))
  {
    elog(ERROR, "trigger_return_old: not fired by trigger manager");
  }

  tuple = trigdata->tg_trigtuple;

  return PointerGetDatum(tuple);
}

#define TTDUMMY_INFINITY 999999

static SPIPlanPtr splan = NULL;
static bool ttoff = false;

PG_FUNCTION_INFO_V1(ttdummy);

Datum
ttdummy(PG_FUNCTION_ARGS)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  Trigger *trigger;                          
  char **args;                     
  int attnum[2];                                        
  Datum oldon, oldoff;
  Datum newon, newoff;
  Datum *cvals;                     
  char *cnulls;                    
  char *relname;                              
  Relation rel;                          
  HeapTuple trigtuple;
  HeapTuple newtuple = NULL;
  HeapTuple rettuple;
  TupleDesc tupdesc;                        
  int natts;                              
  bool isnull;                                               
  int ret;
  int i;

  if (!CALLED_AS_TRIGGER(fcinfo))
  {
    elog(ERROR, "ttdummy: not fired by trigger manager");
  }
  if (!TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
  {
    elog(ERROR, "ttdummy: must be fired for row");
  }
  if (!TRIGGER_FIRED_BEFORE(trigdata->tg_event))
  {
    elog(ERROR, "ttdummy: must be fired before event");
  }
  if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
  {
    elog(ERROR, "ttdummy: cannot process INSERT event");
  }
  if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
  {
    newtuple = trigdata->tg_newtuple;
  }

  trigtuple = trigdata->tg_trigtuple;

  rel = trigdata->tg_relation;
  relname = SPI_getrelname(rel);

                                            
  if (ttoff)                          
  {
    pfree(relname);
    return PointerGetDatum((newtuple != NULL) ? newtuple : trigtuple);
  }

  trigger = trigdata->tg_trigger;

  if (trigger->tgnargs != 2)
  {
    elog(ERROR, "ttdummy (%s): invalid (!= 2) number of arguments %d", relname, trigger->tgnargs);
  }

  args = trigger->tgargs;
  tupdesc = rel->rd_att;
  natts = tupdesc->natts;

  for (i = 0; i < 2; i++)
  {
    attnum[i] = SPI_fnumber(tupdesc, args[i]);
    if (attnum[i] <= 0)
    {
      elog(ERROR, "ttdummy (%s): there is no attribute %s", relname, args[i]);
    }
    if (SPI_gettypeid(tupdesc, attnum[i]) != INT4OID)
    {
      elog(ERROR, "ttdummy (%s): attribute %s must be of integer type", relname, args[i]);
    }
  }

  oldon = SPI_getbinval(trigtuple, tupdesc, attnum[0], &isnull);
  if (isnull)
  {
    elog(ERROR, "ttdummy (%s): %s must be NOT NULL", relname, args[0]);
  }

  oldoff = SPI_getbinval(trigtuple, tupdesc, attnum[1], &isnull);
  if (isnull)
  {
    elog(ERROR, "ttdummy (%s): %s must be NOT NULL", relname, args[1]);
  }

  if (newtuple != NULL)             
  {
    newon = SPI_getbinval(newtuple, tupdesc, attnum[0], &isnull);
    if (isnull)
    {
      elog(ERROR, "ttdummy (%s): %s must be NOT NULL", relname, args[0]);
    }
    newoff = SPI_getbinval(newtuple, tupdesc, attnum[1], &isnull);
    if (isnull)
    {
      elog(ERROR, "ttdummy (%s): %s must be NOT NULL", relname, args[1]);
    }

    if (oldon != newon || oldoff != newoff)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ttdummy (%s): you cannot change %s and/or %s columns (use set_ttdummy)", relname, args[0], args[1])));
    }

    if (newoff != TTDUMMY_INFINITY)
    {
      pfree(relname);                                          
      return PointerGetDatum(NULL);
    }
  }
  else if (oldoff != TTDUMMY_INFINITY)             
  {
    pfree(relname);
    return PointerGetDatum(NULL);
  }

  newoff = DirectFunctionCall1(nextval, CStringGetTextDatum("ttdummy_seq"));
                                                       
  newoff = Int32GetDatum((int32)DatumGetInt64(newoff));

                              
  if ((ret = SPI_connect()) < 0)
  {
    elog(ERROR, "ttdummy (%s): SPI_connect returned %d", relname, ret);
  }

                                    
  cvals = (Datum *)palloc(natts * sizeof(Datum));
  cnulls = (char *)palloc(natts * sizeof(char));
  for (i = 0; i < natts; i++)
  {
    cvals[i] = SPI_getbinval((newtuple != NULL) ? newtuple : trigtuple, tupdesc, i + 1, &isnull);
    cnulls[i] = (isnull) ? 'n' : ' ';
  }

                             
  if (newtuple)             
  {
    cvals[attnum[0] - 1] = newoff;                                 
    cnulls[attnum[0] - 1] = ' ';
    cvals[attnum[1] - 1] = TTDUMMY_INFINITY;                            
    cnulls[attnum[1] - 1] = ' ';
  }
  else
              
  {
    cvals[attnum[1] - 1] = newoff;                                
    cnulls[attnum[1] - 1] = ' ';
  }

                               
  if (splan == NULL)
  {
    SPIPlanPtr pplan;
    Oid *ctypes;
    char *query;

                                       
    ctypes = (Oid *)palloc(natts * sizeof(Oid));
    query = (char *)palloc(100 + 16 * natts);

       
                                                                
       
    sprintf(query, "INSERT INTO %s VALUES (", relname);
    for (i = 1; i <= natts; i++)
    {
      sprintf(query + strlen(query), "$%d%s", i, (i < natts) ? ", " : ")");
      ctypes[i - 1] = SPI_gettypeid(tupdesc, i);
    }

                                
    pplan = SPI_prepare(query, natts, ctypes);
    if (pplan == NULL)
    {
      elog(ERROR, "ttdummy (%s): SPI_prepare returned %s", relname, SPI_result_code_string(SPI_result));
    }

    if (SPI_keepplan(pplan))
    {
      elog(ERROR, "ttdummy (%s): SPI_keepplan failed", relname);
    }

    splan = pplan;
  }

  ret = SPI_execp(splan, cvals, cnulls, 0);

  if (ret < 0)
  {
    elog(ERROR, "ttdummy (%s): SPI_execp returned %d", relname, ret);
  }

                                             
  if (newtuple)             
  {
    rettuple = SPI_modifytuple(rel, trigtuple, 1, &(attnum[1]), &newoff, NULL);
  }
  else             
  {
    rettuple = trigtuple;
  }

  SPI_finish();                                      

  pfree(relname);

  return PointerGetDatum(rettuple);
}

PG_FUNCTION_INFO_V1(set_ttdummy);

Datum
set_ttdummy(PG_FUNCTION_ARGS)
{
  int32 on = PG_GETARG_INT32(0);

  if (ttoff)                    
  {
    if (on == 0)
    {
      PG_RETURN_INT32(0);
    }

                 
    ttoff = false;
    PG_RETURN_INT32(0);
  }

                    
  if (on != 0)
  {
    PG_RETURN_INT32(1);
  }

                
  ttoff = true;

  PG_RETURN_INT32(1);
}

   
                                                                     
                                                                           
   

   
                                                          
   
                                                   
   
PG_FUNCTION_INFO_V1(int44in);

Datum
int44in(PG_FUNCTION_ARGS)
{
  char *input_string = PG_GETARG_CSTRING(0);
  int32 *result = (int32 *)palloc(4 * sizeof(int32));
  int i;

  i = sscanf(input_string, "%d, %d, %d, %d", &result[0], &result[1], &result[2], &result[3]);
  while (i < 4)
  {
    result[i++] = 0;
  }

  PG_RETURN_POINTER(result);
}

   
                                                          
   
PG_FUNCTION_INFO_V1(int44out);

Datum
int44out(PG_FUNCTION_ARGS)
{
  int32 *an_array = (int32 *)PG_GETARG_POINTER(0);
  char *result = (char *)palloc(16 * 4);

  snprintf(result, 16 * 4, "%d,%d,%d,%d", an_array[0], an_array[1], an_array[2], an_array[3]);

  PG_RETURN_CSTRING(result);
}

PG_FUNCTION_INFO_V1(make_tuple_indirect);
Datum
make_tuple_indirect(PG_FUNCTION_ARGS)
{
  HeapTupleHeader rec = PG_GETARG_HEAPTUPLEHEADER(0);
  HeapTupleData tuple;
  int ncolumns;
  Datum *values;
  bool *nulls;

  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupdesc;

  HeapTuple newtup;

  int i;

  MemoryContext old_context;

                                               
  tupType = HeapTupleHeaderGetTypeId(rec);
  tupTypmod = HeapTupleHeaderGetTypMod(rec);
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
  ncolumns = tupdesc->natts;

                                                     
  tuple.t_len = HeapTupleHeaderGetDatumLength(rec);
  ItemPointerSetInvalid(&(tuple.t_self));
  tuple.t_tableOid = InvalidOid;
  tuple.t_data = rec;

  values = (Datum *)palloc(ncolumns * sizeof(Datum));
  nulls = (bool *)palloc(ncolumns * sizeof(bool));

  heap_deform_tuple(&tuple, tupdesc, values, nulls);

  old_context = MemoryContextSwitchTo(TopTransactionContext);

  for (i = 0; i < ncolumns; i++)
  {
    struct varlena *attr;
    struct varlena *new_attr;
    struct varatt_indirect redirect_pointer;

                                                  
    if (TupleDescAttr(tupdesc, i)->attisdropped || nulls[i] || TupleDescAttr(tupdesc, i)->attlen != -1)
    {
      continue;
    }

    attr = (struct varlena *)DatumGetPointer(values[i]);

                                    
    if (VARATT_IS_EXTERNAL_INDIRECT(attr))
    {
      continue;
    }

                                             
    if (VARATT_IS_EXTERNAL_ONDISK(attr))
    {
      attr = heap_tuple_fetch_attr(attr);
    }
    else
    {
      struct varlena *oldattr = attr;

      attr = palloc0(VARSIZE_ANY(oldattr));
      memcpy(attr, oldattr, VARSIZE_ANY(oldattr));
    }

                                 
    new_attr = (struct varlena *)palloc0(INDIRECT_POINTER_SIZE);
    redirect_pointer.pointer = attr;
    SET_VARTAG_EXTERNAL(new_attr, VARTAG_INDIRECT);
    memcpy(VARDATA_EXTERNAL(new_attr), &redirect_pointer, sizeof(redirect_pointer));

    values[i] = PointerGetDatum(new_attr);
  }

  newtup = heap_form_tuple(tupdesc, values, nulls);
  pfree(values);
  pfree(nulls);
  ReleaseTupleDesc(tupdesc);

  MemoryContextSwitchTo(old_context);

     
                                                                             
                                                                        
                                                                          
                                                                          
                                                                         
                                                                         
                                                                          
     
  PG_RETURN_POINTER(newtup->t_data);
}

PG_FUNCTION_INFO_V1(regress_putenv);

Datum
regress_putenv(PG_FUNCTION_ARGS)
{
  MemoryContext oldcontext;
  char *envbuf;

  if (!superuser())
  {
    elog(ERROR, "must be superuser to change environment variables");
  }

  oldcontext = MemoryContextSwitchTo(TopMemoryContext);
  envbuf = text_to_cstring((text *)PG_GETARG_POINTER(0));
  MemoryContextSwitchTo(oldcontext);

  if (putenv(envbuf) != 0)
  {
    elog(ERROR, "could not set environment variable: %m");
  }

  PG_RETURN_VOID();
}

                                             
PG_FUNCTION_INFO_V1(wait_pid);

Datum
wait_pid(PG_FUNCTION_ARGS)
{
  int pid = PG_GETARG_INT32(0);

  if (!superuser())
  {
    elog(ERROR, "must be superuser to check PID liveness");
  }

  while (kill(pid, 0) == 0)
  {
    CHECK_FOR_INTERRUPTS();
    pg_usleep(50000);
  }

  if (errno != ESRCH)
  {
    elog(ERROR, "could not check PID %d liveness: %m", pid);
  }

  PG_RETURN_VOID();
}

static void
test_atomic_flag(void)
{
  pg_atomic_flag flag;

  pg_atomic_init_flag(&flag);
  EXPECT_TRUE(pg_atomic_unlocked_test_flag(&flag));
  EXPECT_TRUE(pg_atomic_test_set_flag(&flag));
  EXPECT_TRUE(!pg_atomic_unlocked_test_flag(&flag));
  EXPECT_TRUE(!pg_atomic_test_set_flag(&flag));
  pg_atomic_clear_flag(&flag);
  EXPECT_TRUE(pg_atomic_unlocked_test_flag(&flag));
  EXPECT_TRUE(pg_atomic_test_set_flag(&flag));
  pg_atomic_clear_flag(&flag);
}

static void
test_atomic_uint32(void)
{
  pg_atomic_uint32 var;
  uint32 expected;
  int i;

  pg_atomic_init_u32(&var, 0);
  EXPECT_EQ_U32(pg_atomic_read_u32(&var), 0);
  pg_atomic_write_u32(&var, 3);
  EXPECT_EQ_U32(pg_atomic_read_u32(&var), 3);
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, pg_atomic_read_u32(&var) - 2), 3);
  EXPECT_EQ_U32(pg_atomic_fetch_sub_u32(&var, 1), 4);
  EXPECT_EQ_U32(pg_atomic_sub_fetch_u32(&var, 3), 0);
  EXPECT_EQ_U32(pg_atomic_add_fetch_u32(&var, 10), 10);
  EXPECT_EQ_U32(pg_atomic_exchange_u32(&var, 5), 10);
  EXPECT_EQ_U32(pg_atomic_exchange_u32(&var, 0), 5);

                                    
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, INT_MAX), 0);
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, INT_MAX), INT_MAX);
  pg_atomic_fetch_add_u32(&var, 2);                
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, PG_INT16_MAX), 0);
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, PG_INT16_MAX + 1), PG_INT16_MAX);
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, PG_INT16_MIN), 2 * PG_INT16_MAX + 1);
  EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&var, PG_INT16_MIN - 1), PG_INT16_MAX);
  pg_atomic_fetch_add_u32(&var, 1);                         
  EXPECT_EQ_U32(pg_atomic_read_u32(&var), UINT_MAX);
  EXPECT_EQ_U32(pg_atomic_fetch_sub_u32(&var, INT_MAX), UINT_MAX);
  EXPECT_EQ_U32(pg_atomic_read_u32(&var), (uint32)INT_MAX + 1);
  EXPECT_EQ_U32(pg_atomic_sub_fetch_u32(&var, INT_MAX), 1);
  pg_atomic_sub_fetch_u32(&var, 1);

                                             
  expected = 10;
  EXPECT_TRUE(!pg_atomic_compare_exchange_u32(&var, &expected, 1));

                                                                       
  for (i = 0; i < 1000; i++)
  {
    expected = 0;
    if (!pg_atomic_compare_exchange_u32(&var, &expected, 1))
    {
      break;
    }
  }
  if (i == 1000)
  {
    elog(ERROR, "atomic_compare_exchange_u32() never succeeded");
  }
  EXPECT_EQ_U32(pg_atomic_read_u32(&var), 1);
  pg_atomic_write_u32(&var, 0);

                            
  EXPECT_TRUE(!(pg_atomic_fetch_or_u32(&var, 1) & 1));
  EXPECT_TRUE(pg_atomic_fetch_or_u32(&var, 2) & 1);
  EXPECT_EQ_U32(pg_atomic_read_u32(&var), 3);
                             
  EXPECT_EQ_U32(pg_atomic_fetch_and_u32(&var, ~2) & 3, 3);
  EXPECT_EQ_U32(pg_atomic_fetch_and_u32(&var, ~1), 1);
                           
  EXPECT_EQ_U32(pg_atomic_fetch_and_u32(&var, ~0), 0);
}

static void
test_atomic_uint64(void)
{
  pg_atomic_uint64 var;
  uint64 expected;
  int i;

  pg_atomic_init_u64(&var, 0);
  EXPECT_EQ_U64(pg_atomic_read_u64(&var), 0);
  pg_atomic_write_u64(&var, 3);
  EXPECT_EQ_U64(pg_atomic_read_u64(&var), 3);
  EXPECT_EQ_U64(pg_atomic_fetch_add_u64(&var, pg_atomic_read_u64(&var) - 2), 3);
  EXPECT_EQ_U64(pg_atomic_fetch_sub_u64(&var, 1), 4);
  EXPECT_EQ_U64(pg_atomic_sub_fetch_u64(&var, 3), 0);
  EXPECT_EQ_U64(pg_atomic_add_fetch_u64(&var, 10), 10);
  EXPECT_EQ_U64(pg_atomic_exchange_u64(&var, 5), 10);
  EXPECT_EQ_U64(pg_atomic_exchange_u64(&var, 0), 5);

                                             
  expected = 10;
  EXPECT_TRUE(!pg_atomic_compare_exchange_u64(&var, &expected, 1));

                                                                       
  for (i = 0; i < 100; i++)
  {
    expected = 0;
    if (!pg_atomic_compare_exchange_u64(&var, &expected, 1))
    {
      break;
    }
  }
  if (i == 100)
  {
    elog(ERROR, "atomic_compare_exchange_u64() never succeeded");
  }
  EXPECT_EQ_U64(pg_atomic_read_u64(&var), 1);

  pg_atomic_write_u64(&var, 0);

                            
  EXPECT_TRUE(!(pg_atomic_fetch_or_u64(&var, 1) & 1));
  EXPECT_TRUE(pg_atomic_fetch_or_u64(&var, 2) & 1);
  EXPECT_EQ_U64(pg_atomic_read_u64(&var), 3);
                             
  EXPECT_EQ_U64((pg_atomic_fetch_and_u64(&var, ~2) & 3), 3);
  EXPECT_EQ_U64(pg_atomic_fetch_and_u64(&var, ~1), 1);
                           
  EXPECT_EQ_U64(pg_atomic_fetch_and_u64(&var, ~0), 0);
}

   
                                                                    
   
                                                                           
                                                        
   
static void
test_spinlock(void)
{
     
                                                                      
     
                                                                           
                                                        
     
  {
    struct test_lock_struct
    {
      char data_before[4];
      slock_t lock;
      char data_after[4];
    } struct_w_lock;

    memcpy(struct_w_lock.data_before, "abcd", 4);
    memcpy(struct_w_lock.data_after, "ef12", 4);

                                                     
    SpinLockInit(&struct_w_lock.lock);
    SpinLockAcquire(&struct_w_lock.lock);
    SpinLockRelease(&struct_w_lock.lock);

                                                      
    S_INIT_LOCK(&struct_w_lock.lock);
    S_LOCK(&struct_w_lock.lock);
    S_UNLOCK(&struct_w_lock.lock);

                                                
    s_lock(&struct_w_lock.lock, "testfile", 17, "testfunc");
    S_UNLOCK(&struct_w_lock.lock);

       
                                                                         
                                                
       
#ifdef TAS
    S_LOCK(&struct_w_lock.lock);

    if (!TAS(&struct_w_lock.lock))
    {
      elog(ERROR, "acquired already held spinlock");
    }

#ifdef TAS_SPIN
    if (!TAS_SPIN(&struct_w_lock.lock))
    {
      elog(ERROR, "acquired already held spinlock");
    }
#endif                        

    S_UNLOCK(&struct_w_lock.lock);
#endif                   

       
                                                                     
                
       
    if (memcmp(struct_w_lock.data_before, "abcd", 4) != 0)
    {
      elog(ERROR, "padding before spinlock modified");
    }
    if (memcmp(struct_w_lock.data_after, "ef12", 4) != 0)
    {
      elog(ERROR, "padding after spinlock modified");
    }
  }

     
                                                                   
                                                                           
                                                                     
     
#ifndef HAVE_SPINLOCKS
  {
       
                                                               
                                                                           
                                                                         
                                                 
       
    for (uint32 i = 0; i < INT32_MAX - 100000; i++)
    {
      slock_t lock;

      SpinLockInit(&lock);
    }

    for (uint32 i = 0; i < 200000; i++)
    {
      slock_t lock;

      SpinLockInit(&lock);

      SpinLockAcquire(&lock);
      SpinLockRelease(&lock);
      SpinLockAcquire(&lock);
      SpinLockRelease(&lock);
    }
  }
#endif
}

   
                                                               
                                                                      
                                                                             
                        
   
                                                                             
                                                                          
                                                                 
   
                                                        
                                                                   
                
   
static void
test_atomic_spin_nest(void)
{
  slock_t lock;
#define NUM_TEST_ATOMICS (NUM_SPINLOCK_SEMAPHORES + NUM_ATOMICS_SEMAPHORES + 27)
  pg_atomic_uint32 atomics32[NUM_TEST_ATOMICS];
  pg_atomic_uint64 atomics64[NUM_TEST_ATOMICS];

  SpinLockInit(&lock);

  for (int i = 0; i < NUM_TEST_ATOMICS; i++)
  {
    pg_atomic_init_u32(&atomics32[i], 0);
    pg_atomic_init_u64(&atomics64[i], 0);
  }

                                   
  for (int i = 0; i < NUM_TEST_ATOMICS; i++)
  {
    EXPECT_EQ_U32(pg_atomic_fetch_add_u32(&atomics32[i], i), 0);
    EXPECT_EQ_U64(pg_atomic_fetch_add_u64(&atomics64[i], i), 0);
  }

                                                       
  SpinLockAcquire(&lock);
  for (int i = 0; i < NUM_TEST_ATOMICS; i++)
  {
    EXPECT_EQ_U32(pg_atomic_fetch_sub_u32(&atomics32[i], i), i);
    EXPECT_EQ_U32(pg_atomic_read_u32(&atomics32[i]), 0);
    EXPECT_EQ_U64(pg_atomic_fetch_sub_u64(&atomics64[i], i), i);
    EXPECT_EQ_U64(pg_atomic_read_u64(&atomics64[i]), 0);
  }
  SpinLockRelease(&lock);
}
#undef NUM_TEST_ATOMICS

PG_FUNCTION_INFO_V1(test_atomic_ops);
Datum
test_atomic_ops(PG_FUNCTION_ARGS)
{
  test_atomic_flag();

  test_atomic_uint32();

  test_atomic_uint64();

     
                                                                          
                                                        
     
  test_spinlock();

  test_atomic_spin_nest();

  PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(test_fdw_handler);
Datum
test_fdw_handler(PG_FUNCTION_ARGS)
{
  elog(ERROR, "test_fdw_handler is not implemented");
  PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(test_support_func);
Datum
test_support_func(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSelectivity))
  {
       
                                                                         
                                                            
       
    SupportRequestSelectivity *req = (SupportRequestSelectivity *)rawreq;
    Selectivity s1;

    if (req->is_join)
    {
      s1 = join_selectivity(req->root, Int4EqualOperator, req->args, req->inputcollid, req->jointype, req->sjinfo);
    }
    else
    {
      s1 = restriction_selectivity(req->root, Int4EqualOperator, req->args, req->inputcollid, req->varRelid);
    }

    req->selectivity = s1;
    ret = (Node *)req;
  }

  if (IsA(rawreq, SupportRequestCost))
  {
                                       
    SupportRequestCost *req = (SupportRequestCost *)rawreq;

    req->startup = 0;
    req->per_tuple = 2 * cpu_operator_cost;
    ret = (Node *)req;
  }

  if (IsA(rawreq, SupportRequestRows))
  {
       
                                                                           
                                                                    
       
    SupportRequestRows *req = (SupportRequestRows *)rawreq;

    if (req->node && IsA(req->node, FuncExpr))                  
    {
      List *args = ((FuncExpr *)req->node)->args;
      Node *arg1 = linitial(args);
      Node *arg2 = lsecond(args);

      if (IsA(arg1, Const) && !((Const *)arg1)->constisnull && IsA(arg2, Const) && !((Const *)arg2)->constisnull)
      {
        int32 val1 = DatumGetInt32(((Const *)arg1)->constvalue);
        int32 val2 = DatumGetInt32(((Const *)arg2)->constvalue);

        req->rows = val2 - val1 + 1;
        ret = (Node *)req;
      }
    }
  }

  PG_RETURN_POINTER(ret);
}
