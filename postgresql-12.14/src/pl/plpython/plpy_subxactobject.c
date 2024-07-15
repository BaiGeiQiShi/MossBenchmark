   
                               
   
                                        
   

#include "postgres.h"

#include "access/xact.h"
#include "utils/memutils.h"

#include "plpython.h"

#include "plpy_subxactobject.h"

#include "plpy_elog.h"

List *explicit_subtransactions = NIL;

static void
PLy_subtransaction_dealloc(PyObject *subxact);
static PyObject *
PLy_subtransaction_enter(PyObject *self, PyObject *unused);
static PyObject *
PLy_subtransaction_exit(PyObject *self, PyObject *args);

static char PLy_subtransaction_doc[] = {"PostgreSQL subtransaction context manager"};

static PyMethodDef PLy_subtransaction_methods[] = {{"__enter__", PLy_subtransaction_enter, METH_VARARGS, NULL}, {"__exit__", PLy_subtransaction_exit, METH_VARARGS, NULL},
                                             
    {"enter", PLy_subtransaction_enter, METH_VARARGS, NULL}, {"exit", PLy_subtransaction_exit, METH_VARARGS, NULL}, {NULL, NULL, 0, NULL}};

static PyTypeObject PLy_SubtransactionType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "PLySubtransaction",
    .tp_basicsize = sizeof(PLySubtransactionObject),
    .tp_dealloc = PLy_subtransaction_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = PLy_subtransaction_doc,
    .tp_methods = PLy_subtransaction_methods,
};

void
PLy_subtransaction_init_type(void)
{
  if (PyType_Ready(&PLy_SubtransactionType) < 0)
  {
    elog(ERROR, "could not initialize PLy_SubtransactionType");
  }
}

                               
PyObject *
PLy_subtransaction_new(PyObject *self, PyObject *unused)
{
  PLySubtransactionObject *ob;

  ob = PyObject_New(PLySubtransactionObject, &PLy_SubtransactionType);

  if (ob == NULL)
  {
    return NULL;
  }

  ob->started = false;
  ob->exited = false;

  return (PyObject *)ob;
}

                                                      
static void
PLy_subtransaction_dealloc(PyObject *subxact)
{
}

   
                                          
   
                                                                   
                                                                    
                                                                    
                      
   
static PyObject *
PLy_subtransaction_enter(PyObject *self, PyObject *unused)
{
  PLySubtransactionData *subxactdata;
  MemoryContext oldcontext;
  PLySubtransactionObject *subxact = (PLySubtransactionObject *)self;

  if (subxact->started)
  {
    PLy_exception_set(PyExc_ValueError, "this subtransaction has already been entered");
    return NULL;
  }

  if (subxact->exited)
  {
    PLy_exception_set(PyExc_ValueError, "this subtransaction has already been exited");
    return NULL;
  }

  subxact->started = true;
  oldcontext = CurrentMemoryContext;

  subxactdata = (PLySubtransactionData *)MemoryContextAlloc(TopTransactionContext, sizeof(PLySubtransactionData));

  subxactdata->oldcontext = oldcontext;
  subxactdata->oldowner = CurrentResourceOwner;

  BeginInternalSubTransaction(NULL);

                                                                          
  MemoryContextSwitchTo(TopTransactionContext);
  explicit_subtransactions = lcons(subxactdata, explicit_subtransactions);

                                                       
  MemoryContextSwitchTo(oldcontext);

  Py_INCREF(self);
  return self;
}

   
                                                                          
   
                                                                       
                                                                       
                                                
   
                                                                     
                                               
                                               
   
static PyObject *
PLy_subtransaction_exit(PyObject *self, PyObject *args)
{
  PyObject *type;
  PyObject *value;
  PyObject *traceback;
  PLySubtransactionData *subxactdata;
  PLySubtransactionObject *subxact = (PLySubtransactionObject *)self;

  if (!PyArg_ParseTuple(args, "OOO", &type, &value, &traceback))
  {
    return NULL;
  }

  if (!subxact->started)
  {
    PLy_exception_set(PyExc_ValueError, "this subtransaction has not been entered");
    return NULL;
  }

  if (subxact->exited)
  {
    PLy_exception_set(PyExc_ValueError, "this subtransaction has already been exited");
    return NULL;
  }

  if (explicit_subtransactions == NIL)
  {
    PLy_exception_set(PyExc_ValueError, "there is no subtransaction to exit from");
    return NULL;
  }

  subxact->exited = true;

  if (type != Py_None)
  {
                                     
    RollbackAndReleaseCurrentSubTransaction();
  }
  else
  {
    ReleaseCurrentSubTransaction();
  }

  subxactdata = (PLySubtransactionData *)linitial(explicit_subtransactions);
  explicit_subtransactions = list_delete_first(explicit_subtransactions);

  MemoryContextSwitchTo(subxactdata->oldcontext);
  CurrentResourceOwner = subxactdata->oldowner;
  pfree(subxactdata);

  Py_RETURN_NONE;
}
