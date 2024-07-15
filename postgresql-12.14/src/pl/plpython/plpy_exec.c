   
                         
   
                               
   

#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/typcache.h"

#include "plpython.h"

#include "plpy_exec.h"

#include "plpy_elog.h"
#include "plpy_main.h"
#include "plpy_procedure.h"
#include "plpy_subxactobject.h"

                                              
typedef struct PLySRFState
{
  PyObject *iter;                                                        
  PLySavedArgs *savedargs;                                      
  MemoryContextCallback callback;                                        
} PLySRFState;

static PyObject *
PLy_function_build_args(FunctionCallInfo fcinfo, PLyProcedure *proc);
static PLySavedArgs *
PLy_function_save_args(PLyProcedure *proc);
static void
PLy_function_restore_args(PLyProcedure *proc, PLySavedArgs *savedargs);
static void
PLy_function_drop_args(PLySavedArgs *savedargs);
static void
PLy_global_args_push(PLyProcedure *proc);
static void
PLy_global_args_pop(PLyProcedure *proc);
static void
plpython_srf_cleanup_callback(void *arg);
static void
plpython_return_error_callback(void *arg);

static PyObject *
PLy_trigger_build_args(FunctionCallInfo fcinfo, PLyProcedure *proc, HeapTuple *rv);
static HeapTuple
PLy_modify_tuple(PLyProcedure *proc, PyObject *pltd, TriggerData *tdata, HeapTuple otup);
static void
plpython_trigger_error_callback(void *arg);

static PyObject *
PLy_procedure_call(PLyProcedure *proc, const char *kargs, PyObject *vargs);
static void
PLy_abort_open_subtransactions(int save_subxact_level);

                         
Datum
PLy_exec_function(FunctionCallInfo fcinfo, PLyProcedure *proc)
{
  bool is_setof = proc->is_setof;
  Datum rv;
  PyObject *volatile plargs = NULL;
  PyObject *volatile plrv = NULL;
  FuncCallContext *volatile funcctx = NULL;
  PLySRFState *volatile srfstate = NULL;
  ErrorContextCallback plerrcontext;

     
                                                                     
                                                                           
                                                   
     
  PLy_global_args_push(proc);

  PG_TRY();
  {
    if (is_setof)
    {
                            
      if (SRF_IS_FIRSTCALL())
      {
        funcctx = SRF_FIRSTCALL_INIT();
        srfstate = (PLySRFState *)MemoryContextAllocZero(funcctx->multi_call_memory_ctx, sizeof(PLySRFState));
                                                   
        srfstate->callback.func = plpython_srf_cleanup_callback;
        srfstate->callback.arg = (void *)srfstate;
        MemoryContextRegisterResetCallback(funcctx->multi_call_memory_ctx, &srfstate->callback);
        funcctx->user_fctx = (void *)srfstate;
      }
                            
      funcctx = SRF_PERCALL_SETUP();
      Assert(funcctx != NULL);
      srfstate = (PLySRFState *)funcctx->user_fctx;
      Assert(srfstate != NULL);
    }

    if (srfstate == NULL || srfstate->iter == NULL)
    {
         
                                                                    
                                                   
         
      plargs = PLy_function_build_args(fcinfo, proc);
      plrv = PLy_procedure_call(proc, "args", plargs);
      Assert(plrv != NULL);
    }
    else
    {
         
                                                                         
                                                                      
                                                                      
                                                                       
                                                                      
                                                           
         
      if (srfstate->savedargs)
      {
        PLy_function_restore_args(proc, srfstate->savedargs);
      }
      srfstate->savedargs = NULL;                              
    }

       
                                                                           
                                                                          
                                                                  
       
    if (is_setof)
    {
      if (srfstate->iter == NULL)
      {
                                               
        ReturnSetInfo *rsi = (ReturnSetInfo *)fcinfo->resultinfo;

        if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_ValuePerCall) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unsupported set function return mode"), errdetail("PL/Python set-returning functions only support returning one value per call.")));
        }
        rsi->returnMode = SFRM_ValuePerCall;

                                                  
        srfstate->iter = PyObject_GetIter(plrv);

        Py_DECREF(plrv);
        plrv = NULL;

        if (srfstate->iter == NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("returned object cannot be iterated"), errdetail("PL/Python set-returning functions must return an iterable object.")));
        }
      }

                                    
      plrv = PyIter_Next(srfstate->iter);
      if (plrv == NULL)
      {
                                                     
        bool has_error = (PyErr_Occurred() != NULL);

        Py_DECREF(srfstate->iter);
        srfstate->iter = NULL;

        if (has_error)
        {
          PLy_elog(ERROR, "error fetching next item from iterator");
        }

                                                                
        Py_INCREF(Py_None);
        plrv = Py_None;
      }
      else
      {
           
                                                                    
                                                                       
                   
           
        srfstate->savedargs = PLy_function_save_args(proc);
      }
    }

       
                                                                           
                                                                    
                                                                         
            
       
    if (SPI_finish() != SPI_OK_FINISH)
    {
      elog(ERROR, "SPI_finish failed");
    }

    plerrcontext.callback = plpython_return_error_callback;
    plerrcontext.previous = error_context_stack;
    error_context_stack = &plerrcontext;

       
                                                                       
                                                                        
                                                                       
                                                               
       
    if (proc->result.typoid == VOIDOID)
    {
      if (plrv != Py_None)
      {
        if (proc->is_procedure)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("PL/Python procedure did not return None")));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("PL/Python function with return type \"void\" did not return None")));
        }
      }

      fcinfo->isnull = false;
      rv = (Datum)0;
    }
    else if (plrv == Py_None && srfstate && srfstate->iter == NULL)
    {
         
                                                                     
                                                                      
                   
         
      fcinfo->isnull = true;
      rv = (Datum)0;
    }
    else
    {
                                       
      rv = PLy_output_convert(&proc->result, plrv, &fcinfo->isnull);
    }
  }
  PG_CATCH();
  {
                                                                    
    PLy_global_args_pop(proc);

    Py_XDECREF(plargs);
    Py_XDECREF(plrv);

       
                                                                       
                                                                   
                                                                        
                                                                          
                                                              
       
    if (srfstate)
    {
      Py_XDECREF(srfstate->iter);
      srfstate->iter = NULL;
                                                       
      if (srfstate->savedargs)
      {
        PLy_function_drop_args(srfstate->savedargs);
      }
      srfstate->savedargs = NULL;
    }

    PG_RE_THROW();
  }
  PG_END_TRY();

  error_context_stack = plerrcontext.previous;

                                                                  
  PLy_global_args_pop(proc);

  Py_XDECREF(plargs);
  Py_DECREF(plrv);

  if (srfstate)
  {
                                            
    if (srfstate->iter == NULL)
    {
                                             
      SRF_RETURN_DONE(funcctx);
    }
    else if (fcinfo->isnull)
    {
      SRF_RETURN_NEXT_NULL(funcctx);
    }
    else
    {
      SRF_RETURN_NEXT(funcctx, rv);
    }
  }

                                                                   
  return rv;
}

                      
   
                                                                     
                                                                     
                                                                    
                                                                     
                                                                       
                                                                     
                                                                
   
HeapTuple
PLy_exec_trigger(FunctionCallInfo fcinfo, PLyProcedure *proc)
{
  HeapTuple rv = NULL;
  PyObject *volatile plargs = NULL;
  PyObject *volatile plrv = NULL;
  TriggerData *tdata;
  TupleDesc rel_descr;

  Assert(CALLED_AS_TRIGGER(fcinfo));
  tdata = (TriggerData *)fcinfo->context;

     
                                                                        
                                                                           
                                                                             
                                                                        
                                                                  
                                                                           
     
  rel_descr = RelationGetDescr(tdata->tg_relation);
  if (proc->result.typoid != rel_descr->tdtypeid)
  {
    PLy_output_setup_func(&proc->result, proc->mcxt, rel_descr->tdtypeid, rel_descr->tdtypmod, proc);
  }
  if (proc->result_in.typoid != rel_descr->tdtypeid)
  {
    PLy_input_setup_func(&proc->result_in, proc->mcxt, rel_descr->tdtypeid, rel_descr->tdtypmod, proc);
  }
  PLy_output_setup_tuple(&proc->result, rel_descr, proc);
  PLy_input_setup_tuple(&proc->result_in, rel_descr, proc);

  PG_TRY();
  {
    int rc PG_USED_FOR_ASSERTS_ONLY;

    rc = SPI_register_trigger_data(tdata);
    Assert(rc >= 0);

    plargs = PLy_trigger_build_args(fcinfo, proc, &rv);
    plrv = PLy_procedure_call(proc, "TD", plargs);

    Assert(plrv != NULL);

       
                                   
       
    if (SPI_finish() != SPI_OK_FINISH)
    {
      elog(ERROR, "SPI_finish failed");
    }

       
                                                       
       
    if (plrv != Py_None)
    {
      char *srv;

      if (PyString_Check(plrv))
      {
        srv = PyString_AsString(plrv);
      }
      else if (PyUnicode_Check(plrv))
      {
        srv = PLyUnicode_AsString(plrv);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("unexpected return value from trigger procedure"), errdetail("Expected None or a string.")));
        srv = NULL;                          
      }

      if (pg_strcasecmp(srv, "SKIP") == 0)
      {
        rv = NULL;
      }
      else if (pg_strcasecmp(srv, "MODIFY") == 0)
      {
        TriggerData *tdata = (TriggerData *)fcinfo->context;

        if (TRIGGER_FIRED_BY_INSERT(tdata->tg_event) || TRIGGER_FIRED_BY_UPDATE(tdata->tg_event))
        {
          rv = PLy_modify_tuple(proc, plargs, tdata, rv);
        }
        else
        {
          ereport(WARNING, (errmsg("PL/Python trigger function returned \"MODIFY\" in a DELETE trigger -- ignored")));
        }
      }
      else if (pg_strcasecmp(srv, "OK") != 0)
      {
           
                                                                      
                 
           
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("unexpected return value from trigger procedure"), errdetail("Expected None, \"OK\", \"SKIP\", or \"MODIFY\".")));
      }
    }
  }
  PG_CATCH();
  {
    Py_XDECREF(plargs);
    Py_XDECREF(plrv);

    PG_RE_THROW();
  }
  PG_END_TRY();

  Py_DECREF(plargs);
  Py_DECREF(plrv);

  return rv;
}

                                                

static PyObject *
PLy_function_build_args(FunctionCallInfo fcinfo, PLyProcedure *proc)
{
  PyObject *volatile arg = NULL;
  PyObject *volatile args = NULL;
  int i;

  PG_TRY();
  {
    args = PyList_New(proc->nargs);
    if (!args)
    {
      return NULL;
    }

    for (i = 0; i < proc->nargs; i++)
    {
      PLyDatumToOb *arginfo = &proc->args[i];

      if (fcinfo->args[i].isnull)
      {
        arg = NULL;
      }
      else
      {
        arg = PLy_input_convert(arginfo, fcinfo->args[i].value);
      }

      if (arg == NULL)
      {
        Py_INCREF(Py_None);
        arg = Py_None;
      }

      if (PyList_SetItem(args, i, arg) == -1)
      {
        PLy_elog(ERROR, "PyList_SetItem() failed, while setting up arguments");
      }

      if (proc->argnames && proc->argnames[i] && PyDict_SetItemString(proc->globals, proc->argnames[i], arg) == -1)
      {
        PLy_elog(ERROR, "PyDict_SetItemString() failed, while setting up arguments");
      }
      arg = NULL;
    }

                                                                 
    if (proc->result.typoid == RECORDOID)
    {
      TupleDesc desc;

      if (get_call_result_type(fcinfo, NULL, &desc) != TYPEFUNC_COMPOSITE)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function returning record called in context "
                                                                       "that cannot accept type record")));
      }

                                                 
      PLy_output_setup_record(&proc->result, desc, proc);
    }
  }
  PG_CATCH();
  {
    Py_XDECREF(arg);
    Py_XDECREF(args);

    PG_RE_THROW();
  }
  PG_END_TRY();

  return args;
}

   
                                                                          
                                                                           
                                                                              
                           
   
                                                                           
                                                                           
   
static PLySavedArgs *
PLy_function_save_args(PLyProcedure *proc)
{
  PLySavedArgs *result;

                                                              
  result = (PLySavedArgs *)MemoryContextAllocZero(proc->mcxt, offsetof(PLySavedArgs, namedargs) + proc->nargs * sizeof(PyObject *));
  result->nargs = proc->nargs;

                             
  result->args = PyDict_GetItemString(proc->globals, "args");
  Py_XINCREF(result->args);

                                     
  if (proc->argnames)
  {
    int i;

    for (i = 0; i < result->nargs; i++)
    {
      if (proc->argnames[i])
      {
        result->namedargs[i] = PyDict_GetItemString(proc->globals, proc->argnames[i]);
        Py_XINCREF(result->namedargs[i]);
      }
    }
  }

  return result;
}

   
                                                             
                         
   
static void
PLy_function_restore_args(PLyProcedure *proc, PLySavedArgs *savedargs)
{
                                                                    
  if (proc->argnames)
  {
    int i;

    for (i = 0; i < savedargs->nargs; i++)
    {
      if (proc->argnames[i] && savedargs->namedargs[i])
      {
        PyDict_SetItemString(proc->globals, proc->argnames[i], savedargs->namedargs[i]);
        Py_DECREF(savedargs->namedargs[i]);
      }
    }
  }

                                      
  if (savedargs->args)
  {
    PyDict_SetItemString(proc->globals, "args", savedargs->args);
    Py_DECREF(savedargs->args);
  }

                                        
  pfree(savedargs);
}

   
                                                            
   
static void
PLy_function_drop_args(PLySavedArgs *savedargs)
{
  int i;

                                      
  for (i = 0; i < savedargs->nargs; i++)
  {
    Py_XDECREF(savedargs->namedargs[i]);
  }

                                          
  Py_XDECREF(savedargs->args);

                                        
  pfree(savedargs);
}

   
                                                                            
                                                                           
                                    
   
                                                                          
                                                                            
                                                                           
                      
   
static void
PLy_global_args_push(PLyProcedure *proc)
{
                                                                      
  if (proc->calldepth > 0)
  {
    PLySavedArgs *node;

                                                           
    node = PLy_function_save_args(proc);

       
                                                                           
                                                                      
                                                
       
    node->next = proc->argstack;
    proc->argstack = node;
  }
  proc->calldepth++;
}

   
                                                    
   
                                                                            
                                                                          
                                                                          
              
   
static void
PLy_global_args_pop(PLyProcedure *proc)
{
  Assert(proc->calldepth > 0);
                                                                      
  if (proc->calldepth > 1)
  {
    PLySavedArgs *ptr = proc->argstack;

                           
    Assert(ptr != NULL);
    proc->argstack = ptr->next;
    proc->calldepth--;

                                                
    PLy_function_restore_args(proc, ptr);
  }
  else
  {
                              
    Assert(proc->argstack == NULL);
    proc->calldepth--;

       
                                                                       
                                                                       
                                                                          
                                                                    
                                                                 
       
  }
}

   
                                                                   
                                                                  
                                                                   
   
static void
plpython_srf_cleanup_callback(void *arg)
{
  PLySRFState *srfstate = (PLySRFState *)arg;

                                                          
  Py_XDECREF(srfstate->iter);
  srfstate->iter = NULL;
                                                   
  if (srfstate->savedargs)
  {
    PLy_function_drop_args(srfstate->savedargs);
  }
  srfstate->savedargs = NULL;
}

static void
plpython_return_error_callback(void *arg)
{
  PLyExecutionContext *exec_ctx = PLy_current_execution_context();

  if (exec_ctx->curr_proc && !exec_ctx->curr_proc->is_procedure)
  {
    errcontext("while creating return value");
  }
}

static PyObject *
PLy_trigger_build_args(FunctionCallInfo fcinfo, PLyProcedure *proc, HeapTuple *rv)
{
  TriggerData *tdata = (TriggerData *)fcinfo->context;
  TupleDesc rel_descr = RelationGetDescr(tdata->tg_relation);
  PyObject *pltname, *pltevent, *pltwhen, *pltlevel, *pltrelid, *plttablename, *plttableschema;
  PyObject *pltargs, *pytnew, *pytold;
  PyObject *volatile pltdata = NULL;
  char *stroid;

  PG_TRY();
  {
    pltdata = PyDict_New();
    if (!pltdata)
    {
      return NULL;
    }

    pltname = PyString_FromString(tdata->tg_trigger->tgname);
    PyDict_SetItemString(pltdata, "name", pltname);
    Py_DECREF(pltname);

    stroid = DatumGetCString(DirectFunctionCall1(oidout, ObjectIdGetDatum(tdata->tg_relation->rd_id)));
    pltrelid = PyString_FromString(stroid);
    PyDict_SetItemString(pltdata, "relid", pltrelid);
    Py_DECREF(pltrelid);
    pfree(stroid);

    stroid = SPI_getrelname(tdata->tg_relation);
    plttablename = PyString_FromString(stroid);
    PyDict_SetItemString(pltdata, "table_name", plttablename);
    Py_DECREF(plttablename);
    pfree(stroid);

    stroid = SPI_getnspname(tdata->tg_relation);
    plttableschema = PyString_FromString(stroid);
    PyDict_SetItemString(pltdata, "table_schema", plttableschema);
    Py_DECREF(plttableschema);
    pfree(stroid);

    if (TRIGGER_FIRED_BEFORE(tdata->tg_event))
    {
      pltwhen = PyString_FromString("BEFORE");
    }
    else if (TRIGGER_FIRED_AFTER(tdata->tg_event))
    {
      pltwhen = PyString_FromString("AFTER");
    }
    else if (TRIGGER_FIRED_INSTEAD(tdata->tg_event))
    {
      pltwhen = PyString_FromString("INSTEAD OF");
    }
    else
    {
      elog(ERROR, "unrecognized WHEN tg_event: %u", tdata->tg_event);
      pltwhen = NULL;                          
    }
    PyDict_SetItemString(pltdata, "when", pltwhen);
    Py_DECREF(pltwhen);

    if (TRIGGER_FIRED_FOR_ROW(tdata->tg_event))
    {
      pltlevel = PyString_FromString("ROW");
      PyDict_SetItemString(pltdata, "level", pltlevel);
      Py_DECREF(pltlevel);

         
                                                                   
                                                                 
         

      if (TRIGGER_FIRED_BY_INSERT(tdata->tg_event))
      {
        pltevent = PyString_FromString("INSERT");

        PyDict_SetItemString(pltdata, "old", Py_None);
        pytnew = PLy_input_from_tuple(&proc->result_in, tdata->tg_trigtuple, rel_descr, !TRIGGER_FIRED_BEFORE(tdata->tg_event));
        PyDict_SetItemString(pltdata, "new", pytnew);
        Py_DECREF(pytnew);
        *rv = tdata->tg_trigtuple;
      }
      else if (TRIGGER_FIRED_BY_DELETE(tdata->tg_event))
      {
        pltevent = PyString_FromString("DELETE");

        PyDict_SetItemString(pltdata, "new", Py_None);
        pytold = PLy_input_from_tuple(&proc->result_in, tdata->tg_trigtuple, rel_descr, true);
        PyDict_SetItemString(pltdata, "old", pytold);
        Py_DECREF(pytold);
        *rv = tdata->tg_trigtuple;
      }
      else if (TRIGGER_FIRED_BY_UPDATE(tdata->tg_event))
      {
        pltevent = PyString_FromString("UPDATE");

        pytnew = PLy_input_from_tuple(&proc->result_in, tdata->tg_newtuple, rel_descr, !TRIGGER_FIRED_BEFORE(tdata->tg_event));
        PyDict_SetItemString(pltdata, "new", pytnew);
        Py_DECREF(pytnew);
        pytold = PLy_input_from_tuple(&proc->result_in, tdata->tg_trigtuple, rel_descr, true);
        PyDict_SetItemString(pltdata, "old", pytold);
        Py_DECREF(pytold);
        *rv = tdata->tg_newtuple;
      }
      else
      {
        elog(ERROR, "unrecognized OP tg_event: %u", tdata->tg_event);
        pltevent = NULL;                          
      }

      PyDict_SetItemString(pltdata, "event", pltevent);
      Py_DECREF(pltevent);
    }
    else if (TRIGGER_FIRED_FOR_STATEMENT(tdata->tg_event))
    {
      pltlevel = PyString_FromString("STATEMENT");
      PyDict_SetItemString(pltdata, "level", pltlevel);
      Py_DECREF(pltlevel);

      PyDict_SetItemString(pltdata, "old", Py_None);
      PyDict_SetItemString(pltdata, "new", Py_None);
      *rv = NULL;

      if (TRIGGER_FIRED_BY_INSERT(tdata->tg_event))
      {
        pltevent = PyString_FromString("INSERT");
      }
      else if (TRIGGER_FIRED_BY_DELETE(tdata->tg_event))
      {
        pltevent = PyString_FromString("DELETE");
      }
      else if (TRIGGER_FIRED_BY_UPDATE(tdata->tg_event))
      {
        pltevent = PyString_FromString("UPDATE");
      }
      else if (TRIGGER_FIRED_BY_TRUNCATE(tdata->tg_event))
      {
        pltevent = PyString_FromString("TRUNCATE");
      }
      else
      {
        elog(ERROR, "unrecognized OP tg_event: %u", tdata->tg_event);
        pltevent = NULL;                          
      }

      PyDict_SetItemString(pltdata, "event", pltevent);
      Py_DECREF(pltevent);
    }
    else
    {
      elog(ERROR, "unrecognized LEVEL tg_event: %u", tdata->tg_event);
    }

    if (tdata->tg_trigger->tgnargs)
    {
         
                        
         
      int i;
      PyObject *pltarg;

      pltargs = PyList_New(tdata->tg_trigger->tgnargs);
      if (!pltargs)
      {
        Py_DECREF(pltdata);
        return NULL;
      }
      for (i = 0; i < tdata->tg_trigger->tgnargs; i++)
      {
        pltarg = PyString_FromString(tdata->tg_trigger->tgargs[i]);

           
                                   
           
        PyList_SetItem(pltargs, i, pltarg);
      }
    }
    else
    {
      Py_INCREF(Py_None);
      pltargs = Py_None;
    }
    PyDict_SetItemString(pltdata, "args", pltargs);
    Py_DECREF(pltargs);
  }
  PG_CATCH();
  {
    Py_XDECREF(pltdata);
    PG_RE_THROW();
  }
  PG_END_TRY();

  return pltdata;
}

   
                                                                       
   
static HeapTuple
PLy_modify_tuple(PLyProcedure *proc, PyObject *pltd, TriggerData *tdata, HeapTuple otup)
{
  HeapTuple rtup;
  PyObject *volatile plntup;
  PyObject *volatile plkeys;
  PyObject *volatile plval;
  Datum *volatile modvalues;
  bool *volatile modnulls;
  bool *volatile modrepls;
  ErrorContextCallback plerrcontext;

  plerrcontext.callback = plpython_trigger_error_callback;
  plerrcontext.previous = error_context_stack;
  error_context_stack = &plerrcontext;

  plntup = plkeys = plval = NULL;
  modvalues = NULL;
  modnulls = NULL;
  modrepls = NULL;

  PG_TRY();
  {
    TupleDesc tupdesc;
    int nkeys, i;

    if ((plntup = PyDict_GetItemString(pltd, "new")) == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("TD[\"new\"] deleted, cannot modify row")));
    }
    Py_INCREF(plntup);
    if (!PyDict_Check(plntup))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("TD[\"new\"] is not a dictionary")));
    }

    plkeys = PyDict_Keys(plntup);
    nkeys = PyList_Size(plkeys);

    tupdesc = RelationGetDescr(tdata->tg_relation);

    modvalues = (Datum *)palloc0(tupdesc->natts * sizeof(Datum));
    modnulls = (bool *)palloc0(tupdesc->natts * sizeof(bool));
    modrepls = (bool *)palloc0(tupdesc->natts * sizeof(bool));

    for (i = 0; i < nkeys; i++)
    {
      PyObject *platt;
      char *plattstr;
      int attn;
      PLyObToDatum *att;

      platt = PyList_GetItem(plkeys, i);
      if (PyString_Check(platt))
      {
        plattstr = PyString_AsString(platt);
      }
      else if (PyUnicode_Check(platt))
      {
        plattstr = PLyUnicode_AsString(platt);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("TD[\"new\"] dictionary key at ordinal position %d is not a string", i)));
        plattstr = NULL;                          
      }
      attn = SPI_fnumber(tupdesc, plattstr);
      if (attn == SPI_ERROR_NOATTRIBUTE)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("key \"%s\" found in TD[\"new\"] does not exist as a column in the triggering row", plattstr)));
      }
      if (attn <= 0)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot set system attribute \"%s\"", plattstr)));
      }
      if (TupleDescAttr(tupdesc, attn - 1)->attgenerated)
      {
        ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("cannot set generated column \"%s\"", plattstr)));
      }

      plval = PyDict_GetItem(plntup, platt);
      if (plval == NULL)
      {
        elog(FATAL, "Python interpreter is probably corrupted");
      }

      Py_INCREF(plval);

                                                                       
      att = &proc->result.u.tuple.atts[attn - 1];

      modvalues[attn - 1] = PLy_output_convert(att, plval, &modnulls[attn - 1]);
      modrepls[attn - 1] = true;

      Py_DECREF(plval);
      plval = NULL;
    }

    rtup = heap_modify_tuple(otup, tupdesc, modvalues, modnulls, modrepls);
  }
  PG_CATCH();
  {
    Py_XDECREF(plntup);
    Py_XDECREF(plkeys);
    Py_XDECREF(plval);

    if (modvalues)
    {
      pfree(modvalues);
    }
    if (modnulls)
    {
      pfree(modnulls);
    }
    if (modrepls)
    {
      pfree(modrepls);
    }

    PG_RE_THROW();
  }
  PG_END_TRY();

  Py_DECREF(plntup);
  Py_DECREF(plkeys);

  pfree(modvalues);
  pfree(modnulls);
  pfree(modrepls);

  error_context_stack = plerrcontext.previous;

  return rtup;
}

static void
plpython_trigger_error_callback(void *arg)
{
  PLyExecutionContext *exec_ctx = PLy_current_execution_context();

  if (exec_ctx->curr_proc)
  {
    errcontext("while modifying trigger row");
  }
}

                                                                 
static PyObject *
PLy_procedure_call(PLyProcedure *proc, const char *kargs, PyObject *vargs)
{
  PyObject *rv;
  int volatile save_subxact_level = list_length(explicit_subtransactions);

  PyDict_SetItemString(proc->globals, kargs, vargs);

  PG_TRY();
  {
#if PY_VERSION_HEX >= 0x03020000
    rv = PyEval_EvalCode(proc->code, proc->globals, proc->globals);
#else
    rv = PyEval_EvalCode((PyCodeObject *)proc->code, proc->globals, proc->globals);
#endif

       
                                                                   
                                                                      
                        
       
    Assert(list_length(explicit_subtransactions) >= save_subxact_level);
  }
  PG_CATCH();
  {
    PLy_abort_open_subtransactions(save_subxact_level);
    PG_RE_THROW();
  }
  PG_END_TRY();

  PLy_abort_open_subtransactions(save_subxact_level);

                                                          
  if (rv == NULL)
  {
    PLy_elog(ERROR, NULL);
  }

  return rv;
}

   
                                                                     
                                                             
   
static void
PLy_abort_open_subtransactions(int save_subxact_level)
{
  Assert(save_subxact_level >= 0);

  while (list_length(explicit_subtransactions) > save_subxact_level)
  {
    PLySubtransactionData *subtransactiondata;

    Assert(explicit_subtransactions != NIL);

    ereport(WARNING, (errmsg("forcibly aborting a subtransaction that has not been exited")));

    RollbackAndReleaseCurrentSubTransaction();

    subtransactiondata = (PLySubtransactionData *)linitial(explicit_subtransactions);
    explicit_subtransactions = list_delete_first(explicit_subtransactions);

    MemoryContextSwitchTo(subtransactiondata->oldcontext);
    CurrentResourceOwner = subtransactiondata->oldowner;
    pfree(subtransactiondata);
  }
}
