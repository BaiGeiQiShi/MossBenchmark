   
                                                        
   
                                 
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#include "plpython.h"

#include "plpy_typeio.h"

#include "plpy_elog.h"
#include "plpy_main.h"

                                              
static PyObject *
PLyBool_FromBool(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyFloat_FromFloat4(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyFloat_FromFloat8(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyDecimal_FromNumeric(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyInt_FromInt16(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyInt_FromInt32(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyLong_FromInt64(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyLong_FromOid(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyBytes_FromBytea(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyString_FromScalar(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyObject_FromTransform(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyList_FromArray(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyList_FromArray_recurse(PLyDatumToOb *elm, int *dims, int ndim, int dim, char **dataptr_p, bits8 **bitmap_p, int *bitmask_p);
static PyObject *
PLyDict_FromComposite(PLyDatumToOb *arg, Datum d);
static PyObject *
PLyDict_FromTuple(PLyDatumToOb *arg, HeapTuple tuple, TupleDesc desc, bool include_generated);

                                              
static Datum
PLyObject_ToBool(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static Datum
PLyObject_ToBytea(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static Datum
PLyObject_ToComposite(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static Datum
PLyObject_ToScalar(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static Datum
PLyObject_ToDomain(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static Datum
PLyObject_ToTransform(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static Datum
PLySequence_ToArray(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray);
static void
PLySequence_ToArray_recurse(PLyObToDatum *elm, PyObject *list, int *dims, int ndim, int dim, Datum *elems, bool *nulls, int *currelem);

                                                        
static Datum
PLyString_ToComposite(PLyObToDatum *arg, PyObject *string, bool inarray);
static Datum
PLyMapping_ToComposite(PLyObToDatum *arg, TupleDesc desc, PyObject *mapping);
static Datum
PLySequence_ToComposite(PLyObToDatum *arg, TupleDesc desc, PyObject *sequence);
static Datum
PLyGenericObject_ToComposite(PLyObToDatum *arg, TupleDesc desc, PyObject *object, bool inarray);

   
                                                                  
                               
   

   
                                                                       
   
                                                                              
                                                            
   
PyObject *
PLy_input_convert(PLyDatumToOb *arg, Datum val)
{
  PyObject *result;
  PLyExecutionContext *exec_ctx = PLy_current_execution_context();
  MemoryContext scratch_context = PLy_get_scratch_context(exec_ctx);
  MemoryContext oldcontext;

     
                                                                         
                                                                       
                                                                            
                                                                         
                                             
     
                                                                            
                                                                        
                                                               
     
  MemoryContextReset(scratch_context);

  oldcontext = MemoryContextSwitchTo(scratch_context);

  result = arg->func(arg, val);

  MemoryContextSwitchTo(oldcontext);

  return result;
}

   
                                                                        
   
                                                                               
                                                            
   
                                                                        
                                                               
   
Datum
PLy_output_convert(PLyObToDatum *arg, PyObject *val, bool *isnull)
{
                                                               
  return arg->func(arg, val, isnull, false);
}

   
                                                
   
                                                                       
                                                                      
                                                                 
   
PyObject *
PLy_input_from_tuple(PLyDatumToOb *arg, HeapTuple tuple, TupleDesc desc, bool include_generated)
{
  PyObject *dict;
  PLyExecutionContext *exec_ctx = PLy_current_execution_context();
  MemoryContext scratch_context = PLy_get_scratch_context(exec_ctx);
  MemoryContext oldcontext;

     
                                                                  
     
  MemoryContextReset(scratch_context);

  oldcontext = MemoryContextSwitchTo(scratch_context);

  dict = PLyDict_FromTuple(arg, tuple, desc, include_generated);

  MemoryContextSwitchTo(oldcontext);

  return dict;
}

   
                                                                             
   
                                                                           
                                                                        
                                                                       
                                                                         
                                                   
   
void
PLy_input_setup_tuple(PLyDatumToOb *arg, TupleDesc desc, PLyProcedure *proc)
{
  int i;

                                                          
  Assert(arg->func == PLyDict_FromComposite);

                                                                             
  if (arg->typoid == RECORDOID && arg->typmod < 0)
  {
    arg->u.tuple.recdesc = desc;
  }

                                         
  if (arg->u.tuple.natts != desc->natts)
  {
    if (arg->u.tuple.atts)
    {
      pfree(arg->u.tuple.atts);
    }
    arg->u.tuple.natts = desc->natts;
    arg->u.tuple.atts = (PLyDatumToOb *)MemoryContextAllocZero(arg->mcxt, desc->natts * sizeof(PLyDatumToOb));
  }

                                                         
  for (i = 0; i < desc->natts; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(desc, i);
    PLyDatumToOb *att = &arg->u.tuple.atts[i];

    if (attr->attisdropped)
    {
      continue;
    }

    if (att->typoid == attr->atttypid && att->typmod == attr->atttypmod)
    {
      continue;                                
    }

    PLy_input_setup_func(att, arg->mcxt, attr->atttypid, attr->atttypmod, proc);
  }
}

   
                                                                              
   
                                                                            
                                                                        
                                                                       
                                                                         
                                                   
   
void
PLy_output_setup_tuple(PLyObToDatum *arg, TupleDesc desc, PLyProcedure *proc)
{
  int i;

                                                          
  Assert(arg->func == PLyObject_ToComposite);

                                                                             
  if (arg->typoid == RECORDOID && arg->typmod < 0)
  {
    arg->u.tuple.recdesc = desc;
  }

                                         
  if (arg->u.tuple.natts != desc->natts)
  {
    if (arg->u.tuple.atts)
    {
      pfree(arg->u.tuple.atts);
    }
    arg->u.tuple.natts = desc->natts;
    arg->u.tuple.atts = (PLyObToDatum *)MemoryContextAllocZero(arg->mcxt, desc->natts * sizeof(PLyObToDatum));
  }

                                                         
  for (i = 0; i < desc->natts; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(desc, i);
    PLyObToDatum *att = &arg->u.tuple.atts[i];

    if (attr->attisdropped)
    {
      continue;
    }

    if (att->typoid == attr->atttypid && att->typmod == attr->atttypmod)
    {
      continue;                                
    }

    PLy_output_setup_func(att, arg->mcxt, attr->atttypid, attr->atttypmod, proc);
  }
}

   
                                                                 
   
                                                          
   
void
PLy_output_setup_record(PLyObToDatum *arg, TupleDesc desc, PLyProcedure *proc)
{
                                    
  Assert(arg->typoid == RECORDOID);
  Assert(desc->tdtypeid == RECORDOID);

     
                                                                             
                                                                          
                                      
     
  BlessTupleDesc(desc);

     
                                                                         
                                                                          
                          
     
  arg->typmod = desc->tdtypmod;
  if (arg->u.tuple.recdesc && arg->u.tuple.recdesc->tdtypmod != arg->typmod)
  {
    arg->u.tuple.recdesc = NULL;
  }

                                        
  PLy_output_setup_tuple(arg, desc, proc);
}

   
                                                                            
                                                                    
                                                                               
                 
                                                
   
void
PLy_output_setup_func(PLyObToDatum *arg, MemoryContext arg_mcxt, Oid typeOid, int32 typmod, PLyProcedure *proc)
{
  TypeCacheEntry *typentry;
  char typtype;
  Oid trfuncid;
  Oid typinput;

                                                                             
  check_stack_depth();

  arg->typoid = typeOid;
  arg->typmod = typmod;
  arg->mcxt = arg_mcxt;

     
                                                                        
                                                                             
                                                
     
  if (typeOid != RECORDOID)
  {
    typentry = lookup_type_cache(typeOid, TYPECACHE_DOMAIN_BASE_INFO);
    typtype = typentry->typtype;
    arg->typbyval = typentry->typbyval;
    arg->typlen = typentry->typlen;
    arg->typalign = typentry->typalign;
  }
  else
  {
    typentry = NULL;
    typtype = TYPTYPE_COMPOSITE;
                                                 
    arg->typbyval = false;
    arg->typlen = -1;
    arg->typalign = 'd';
  }

     
                                                                          
                                                                             
                                                                            
                                                                             
                                        
     
  if (typtype == TYPTYPE_DOMAIN)
  {
                
    arg->func = PLyObject_ToDomain;
    arg->u.domain.domain_info = NULL;
                                                                 
    arg->u.domain.base = (PLyObToDatum *)MemoryContextAllocZero(arg_mcxt, sizeof(PLyObToDatum));
    PLy_output_setup_func(arg->u.domain.base, arg_mcxt, typentry->domainBaseType, typentry->domainBaseTypmod, proc);
  }
  else if (typentry && OidIsValid(typentry->typelem) && typentry->typlen == -1)
  {
                                                       
    arg->func = PLySequence_ToArray;
                                                            
                                                                       
    arg->u.array.elmbasetype = getBaseType(typentry->typelem);
                                                                 
    arg->u.array.elm = (PLyObToDatum *)MemoryContextAllocZero(arg_mcxt, sizeof(PLyObToDatum));
    PLy_output_setup_func(arg->u.array.elm, arg_mcxt, typentry->typelem, typmod, proc);
  }
  else if ((trfuncid = get_transform_tosql(typeOid, proc->langid, proc->trftypes)))
  {
    arg->func = PLyObject_ToTransform;
    fmgr_info_cxt(trfuncid, &arg->u.transform.typtransform, arg_mcxt);
  }
  else if (typtype == TYPTYPE_COMPOSITE)
  {
                                         
    arg->func = PLyObject_ToComposite;
                                               
    arg->u.tuple.recdesc = NULL;
    arg->u.tuple.typentry = typentry;
    arg->u.tuple.tupdescid = INVALID_TUPLEDESC_IDENTIFIER;
    arg->u.tuple.atts = NULL;
    arg->u.tuple.natts = 0;
                                            
    arg->u.tuple.recinfunc.fn_oid = InvalidOid;
  }
  else
  {
                                                            
    switch (typeOid)
    {
    case BOOLOID:
      arg->func = PLyObject_ToBool;
      break;
    case BYTEAOID:
      arg->func = PLyObject_ToBytea;
      break;
    default:
      arg->func = PLyObject_ToScalar;
      getTypeInputInfo(typeOid, &typinput, &arg->u.scalar.typioparam);
      fmgr_info_cxt(typinput, &arg->u.scalar.typfunc, arg_mcxt);
      break;
    }
  }
}

   
                                                                            
                                                                    
                                                                               
                 
                                                
   
void
PLy_input_setup_func(PLyDatumToOb *arg, MemoryContext arg_mcxt, Oid typeOid, int32 typmod, PLyProcedure *proc)
{
  TypeCacheEntry *typentry;
  char typtype;
  Oid trfuncid;
  Oid typoutput;
  bool typisvarlena;

                                                                             
  check_stack_depth();

  arg->typoid = typeOid;
  arg->typmod = typmod;
  arg->mcxt = arg_mcxt;

     
                                                                        
                                                                             
                                                
     
  if (typeOid != RECORDOID)
  {
    typentry = lookup_type_cache(typeOid, TYPECACHE_DOMAIN_BASE_INFO);
    typtype = typentry->typtype;
    arg->typbyval = typentry->typbyval;
    arg->typlen = typentry->typlen;
    arg->typalign = typentry->typalign;
  }
  else
  {
    typentry = NULL;
    typtype = TYPTYPE_COMPOSITE;
                                                 
    arg->typbyval = false;
    arg->typlen = -1;
    arg->typalign = 'd';
  }

     
                                                                          
                                                                             
                                                                            
                                                                             
                                        
     
  if (typtype == TYPTYPE_DOMAIN)
  {
                                                                      
    PLy_input_setup_func(arg, arg_mcxt, typentry->domainBaseType, typentry->domainBaseTypmod, proc);
  }
  else if (typentry && OidIsValid(typentry->typelem) && typentry->typlen == -1)
  {
                                                       
    arg->func = PLyList_FromArray;
                                                                 
    arg->u.array.elm = (PLyDatumToOb *)MemoryContextAllocZero(arg_mcxt, sizeof(PLyDatumToOb));
    PLy_input_setup_func(arg->u.array.elm, arg_mcxt, typentry->typelem, typmod, proc);
  }
  else if ((trfuncid = get_transform_fromsql(typeOid, proc->langid, proc->trftypes)))
  {
    arg->func = PLyObject_FromTransform;
    fmgr_info_cxt(trfuncid, &arg->u.transform.typtransform, arg_mcxt);
  }
  else if (typtype == TYPTYPE_COMPOSITE)
  {
                                         
    arg->func = PLyDict_FromComposite;
                                               
    arg->u.tuple.recdesc = NULL;
    arg->u.tuple.typentry = typentry;
    arg->u.tuple.tupdescid = INVALID_TUPLEDESC_IDENTIFIER;
    arg->u.tuple.atts = NULL;
    arg->u.tuple.natts = 0;
  }
  else
  {
                                                            
    switch (typeOid)
    {
    case BOOLOID:
      arg->func = PLyBool_FromBool;
      break;
    case FLOAT4OID:
      arg->func = PLyFloat_FromFloat4;
      break;
    case FLOAT8OID:
      arg->func = PLyFloat_FromFloat8;
      break;
    case NUMERICOID:
      arg->func = PLyDecimal_FromNumeric;
      break;
    case INT2OID:
      arg->func = PLyInt_FromInt16;
      break;
    case INT4OID:
      arg->func = PLyInt_FromInt32;
      break;
    case INT8OID:
      arg->func = PLyLong_FromInt64;
      break;
    case OIDOID:
      arg->func = PLyLong_FromOid;
      break;
    case BYTEAOID:
      arg->func = PLyBytes_FromBytea;
      break;
    default:
      arg->func = PLyString_FromScalar;
      getTypeOutputInfo(typeOid, &typoutput, &typisvarlena);
      fmgr_info_cxt(typoutput, &arg->u.scalar.typfunc, arg_mcxt);
      break;
    }
  }
}

   
                                     
   

static PyObject *
PLyBool_FromBool(PLyDatumToOb *arg, Datum d)
{
  if (DatumGetBool(d))
  {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

static PyObject *
PLyFloat_FromFloat4(PLyDatumToOb *arg, Datum d)
{
  return PyFloat_FromDouble(DatumGetFloat4(d));
}

static PyObject *
PLyFloat_FromFloat8(PLyDatumToOb *arg, Datum d)
{
  return PyFloat_FromDouble(DatumGetFloat8(d));
}

static PyObject *
PLyDecimal_FromNumeric(PLyDatumToOb *arg, Datum d)
{
  static PyObject *decimal_constructor;
  char *str;
  PyObject *pyvalue;

                                                                           
  if (!decimal_constructor)
  {
    PyObject *decimal_module;

    decimal_module = PyImport_ImportModule("cdecimal");
    if (!decimal_module)
    {
      PyErr_Clear();
      decimal_module = PyImport_ImportModule("decimal");
    }
    if (!decimal_module)
    {
      PLy_elog(ERROR, "could not import a module for Decimal constructor");
    }

    decimal_constructor = PyObject_GetAttrString(decimal_module, "Decimal");
    if (!decimal_constructor)
    {
      PLy_elog(ERROR, "no Decimal attribute in module");
    }
  }

  str = DatumGetCString(DirectFunctionCall1(numeric_out, d));
  pyvalue = PyObject_CallFunction(decimal_constructor, "s", str);
  if (!pyvalue)
  {
    PLy_elog(ERROR, "conversion from numeric to Decimal failed");
  }

  return pyvalue;
}

static PyObject *
PLyInt_FromInt16(PLyDatumToOb *arg, Datum d)
{
  return PyInt_FromLong(DatumGetInt16(d));
}

static PyObject *
PLyInt_FromInt32(PLyDatumToOb *arg, Datum d)
{
  return PyInt_FromLong(DatumGetInt32(d));
}

static PyObject *
PLyLong_FromInt64(PLyDatumToOb *arg, Datum d)
{
  return PyLong_FromLongLong(DatumGetInt64(d));
}

static PyObject *
PLyLong_FromOid(PLyDatumToOb *arg, Datum d)
{
  return PyLong_FromUnsignedLong(DatumGetObjectId(d));
}

static PyObject *
PLyBytes_FromBytea(PLyDatumToOb *arg, Datum d)
{
  text *txt = DatumGetByteaPP(d);
  char *str = VARDATA_ANY(txt);
  size_t size = VARSIZE_ANY_EXHDR(txt);

  return PyBytes_FromStringAndSize(str, size);
}

   
                                                                
   
static PyObject *
PLyString_FromScalar(PLyDatumToOb *arg, Datum d)
{
  char *x = OutputFunctionCall(&arg->u.scalar.typfunc, d);
  PyObject *r = PyString_FromString(x);

  pfree(x);
  return r;
}

   
                                                
   
static PyObject *
PLyObject_FromTransform(PLyDatumToOb *arg, Datum d)
{
  Datum t;

  t = FunctionCall1(&arg->u.transform.typtransform, d);
  return (PyObject *)DatumGetPointer(t);
}

   
                                         
   
static PyObject *
PLyList_FromArray(PLyDatumToOb *arg, Datum d)
{
  ArrayType *array = DatumGetArrayTypeP(d);
  PLyDatumToOb *elm = arg->u.array.elm;
  int ndim;
  int *dims;
  char *dataptr;
  bits8 *bitmap;
  int bitmask;

  if (ARR_NDIM(array) == 0)
  {
    return PyList_New(0);
  }

                                        
  ndim = ARR_NDIM(array);
  dims = ARR_DIMS(array);
  Assert(ndim <= MAXDIM);

     
                                                                       
                                                                          
                                                                            
                                                                     
                         
     
                                                                           
                                                                             
                                                                           
                                                                     
                                                           
     
  dataptr = ARR_DATA_PTR(array);
  bitmap = ARR_NULLBITMAP(array);
  bitmask = 1;

  return PLyList_FromArray_recurse(elm, dims, ndim, 0, &dataptr, &bitmap, &bitmask);
}

static PyObject *
PLyList_FromArray_recurse(PLyDatumToOb *elm, int *dims, int ndim, int dim, char **dataptr_p, bits8 **bitmap_p, int *bitmask_p)
{
  int i;
  PyObject *list;

  list = PyList_New(dims[dim]);
  if (!list)
  {
    return NULL;
  }

  if (dim < ndim - 1)
  {
                                                        
    for (i = 0; i < dims[dim]; i++)
    {
      PyObject *sublist;

      sublist = PLyList_FromArray_recurse(elm, dims, ndim, dim + 1, dataptr_p, bitmap_p, bitmask_p);
      PyList_SET_ITEM(list, i, sublist);
    }
  }
  else
  {
       
                                                                         
                       
       
    char *dataptr = *dataptr_p;
    bits8 *bitmap = *bitmap_p;
    int bitmask = *bitmask_p;

    for (i = 0; i < dims[dim]; i++)
    {
                             
      if (bitmap && (*bitmap & bitmask) == 0)
      {
        Py_INCREF(Py_None);
        PyList_SET_ITEM(list, i, Py_None);
      }
      else
      {
        Datum itemvalue;

        itemvalue = fetch_att(dataptr, elm->typbyval, elm->typlen);
        PyList_SET_ITEM(list, i, elm->func(elm, itemvalue));
        dataptr = att_addlength_pointer(dataptr, elm->typlen, dataptr);
        dataptr = (char *)att_align_nominal(dataptr, elm->typalign);
      }

                                         
      if (bitmap)
      {
        bitmask <<= 1;
        if (bitmask == 0x100             )
        {
          bitmap++;
          bitmask = 1;
        }
      }
    }

    *dataptr_p = dataptr;
    *bitmap_p = bitmap;
    *bitmask_p = bitmask;
  }

  return list;
}

   
                                                   
   
static PyObject *
PLyDict_FromComposite(PLyDatumToOb *arg, Datum d)
{
  PyObject *dict;
  HeapTupleHeader td;
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupdesc;
  HeapTupleData tmptup;

  td = DatumGetHeapTupleHeader(d);
                                               
  tupType = HeapTupleHeaderGetTypeId(td);
  tupTypmod = HeapTupleHeaderGetTypMod(td);
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

                                        
  PLy_input_setup_tuple(arg, tupdesc, PLy_current_execution_context()->curr_proc);

                                                     
  tmptup.t_len = HeapTupleHeaderGetDatumLength(td);
  tmptup.t_data = td;

  dict = PLyDict_FromTuple(arg, &tmptup, tupdesc, true);

  ReleaseTupleDesc(tupdesc);

  return dict;
}

   
                                                
   
static PyObject *
PLyDict_FromTuple(PLyDatumToOb *arg, HeapTuple tuple, TupleDesc desc, bool include_generated)
{
  PyObject *volatile dict;

                                             
  Assert(desc->natts == arg->u.tuple.natts);

  dict = PyDict_New();
  if (dict == NULL)
  {
    return NULL;
  }

  PG_TRY();
  {
    int i;

    for (i = 0; i < arg->u.tuple.natts; i++)
    {
      PLyDatumToOb *att = &arg->u.tuple.atts[i];
      Form_pg_attribute attr = TupleDescAttr(desc, i);
      char *key;
      Datum vattr;
      bool is_null;
      PyObject *value;

      if (attr->attisdropped)
      {
        continue;
      }

      if (attr->attgenerated)
      {
                                            
        if (!include_generated)
        {
          continue;
        }
      }

      key = NameStr(attr->attname);
      vattr = heap_getattr(tuple, (i + 1), desc, &is_null);

      if (is_null)
      {
        PyDict_SetItemString(dict, key, Py_None);
      }
      else
      {
        value = att->func(att, vattr);
        PyDict_SetItemString(dict, key, value);
        Py_DECREF(value);
      }
    }
  }
  PG_CATCH();
  {
    Py_DECREF(dict);
    PG_RE_THROW();
  }
  PG_END_TRY();

  return dict;
}

   
                                                                      
                                                                      
                                                                     
                   
   
static Datum
PLyObject_ToBool(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  if (plrv == Py_None)
  {
    *isnull = true;
    return (Datum)0;
  }
  *isnull = false;
  return BoolGetDatum(PyObject_IsTrue(plrv));
}

   
                                                                      
                                                                     
                                                   
   
static Datum
PLyObject_ToBytea(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  PyObject *volatile plrv_so = NULL;
  Datum rv;

  if (plrv == Py_None)
  {
    *isnull = true;
    return (Datum)0;
  }
  *isnull = false;

  plrv_so = PyObject_Bytes(plrv);
  if (!plrv_so)
  {
    PLy_elog(ERROR, "could not create bytes representation of Python object");
  }

  PG_TRY();
  {
    char *plrv_sc = PyBytes_AsString(plrv_so);
    size_t len = PyBytes_Size(plrv_so);
    size_t size = len + VARHDRSZ;
    bytea *result = palloc(size);

    SET_VARSIZE(result, size);
    memcpy(VARDATA(result), plrv_sc, len);
    rv = PointerGetDatum(result);
  }
  PG_CATCH();
  {
    Py_XDECREF(plrv_so);
    PG_RE_THROW();
  }
  PG_END_TRY();

  Py_XDECREF(plrv_so);

  return rv;
}

   
                                                                         
                                                                             
                                    
   
static Datum
PLyObject_ToComposite(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  Datum rv;
  TupleDesc desc;

  if (plrv == Py_None)
  {
    *isnull = true;
    return (Datum)0;
  }
  *isnull = false;

     
                                                                         
                                                                   
     
  if (PyString_Check(plrv) || PyUnicode_Check(plrv))
  {
    return PLyString_ToComposite(arg, plrv, inarray);
  }

     
                                                                       
                                                                          
                                                                           
                                                           
     
  if (arg->typoid != RECORDOID)
  {
    desc = lookup_rowtype_tupdesc(arg->typoid, arg->typmod);
                                                                    
    Assert(desc == arg->u.tuple.typentry->tupDesc);
                                                             
    if (arg->u.tuple.tupdescid != arg->u.tuple.typentry->tupDesc_identifier)
    {
      PLy_output_setup_tuple(arg, desc, PLy_current_execution_context()->curr_proc);
      arg->u.tuple.tupdescid = arg->u.tuple.typentry->tupDesc_identifier;
    }
  }
  else
  {
    desc = arg->u.tuple.recdesc;
    if (desc == NULL)
    {
      desc = lookup_rowtype_tupdesc(arg->typoid, arg->typmod);
      arg->u.tuple.recdesc = desc;
    }
    else
    {
                                               
      PinTupleDesc(desc);
    }
  }

                                          
  Assert(desc->natts == arg->u.tuple.natts);

     
                                                                        
                             
     
  if (PySequence_Check(plrv))
  {
                                                      
    rv = PLySequence_ToComposite(arg, desc, plrv);
  }
  else if (PyMapping_Check(plrv))
  {
                                                         
    rv = PLyMapping_ToComposite(arg, desc, plrv);
  }
  else
  {
                                                                 
    rv = PLyGenericObject_ToComposite(arg, desc, plrv, inarray);
  }

  ReleaseTupleDesc(desc);

  return rv;
}

   
                                                         
   
                                                               
   
char *
PLyObject_AsString(PyObject *plrv)
{
  PyObject *plrv_bo;
  char *plrv_sc;
  size_t plen;
  size_t slen;

  if (PyUnicode_Check(plrv))
  {
    plrv_bo = PLyUnicode_Bytes(plrv);
  }
  else if (PyFloat_Check(plrv))
  {
                                               
#if PY_MAJOR_VERSION >= 3
    PyObject *s = PyObject_Repr(plrv);

    plrv_bo = PLyUnicode_Bytes(s);
    Py_XDECREF(s);
#else
    plrv_bo = PyObject_Repr(plrv);
#endif
  }
  else
  {
#if PY_MAJOR_VERSION >= 3
    PyObject *s = PyObject_Str(plrv);

    plrv_bo = PLyUnicode_Bytes(s);
    Py_XDECREF(s);
#else
    plrv_bo = PyObject_Str(plrv);
#endif
  }
  if (!plrv_bo)
  {
    PLy_elog(ERROR, "could not create string representation of Python object");
  }

  plrv_sc = pstrdup(PyBytes_AsString(plrv_bo));
  plen = PyBytes_Size(plrv_bo);
  slen = strlen(plrv_sc);

  Py_XDECREF(plrv_bo);

  if (slen < plen)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("could not convert Python object into cstring: Python string representation appears to contain null bytes")));
  }
  else if (slen > plen)
  {
    elog(ERROR, "could not convert Python object into cstring: Python string longer than reported length");
  }
  pg_verifymbstr(plrv_sc, slen, false);

  return plrv_sc;
}

   
                                                                       
                                 
   
static Datum
PLyObject_ToScalar(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  char *str;

  if (plrv == Py_None)
  {
    *isnull = true;
    return (Datum)0;
  }
  *isnull = false;

  str = PLyObject_AsString(plrv);

  return InputFunctionCall(&arg->u.scalar.typfunc, str, arg->u.scalar.typioparam, arg->typmod);
}

   
                             
   
static Datum
PLyObject_ToDomain(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  Datum result;
  PLyObToDatum *base = arg->u.domain.base;

  result = base->func(base, plrv, isnull, inarray);
  domain_check(result, *isnull, arg->typoid, &arg->u.domain.domain_info, arg->mcxt);
  return result;
}

   
                                              
   
static Datum
PLyObject_ToTransform(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  if (plrv == Py_None)
  {
    *isnull = true;
    return (Datum)0;
  }
  *isnull = false;
  return FunctionCall1(&arg->u.transform.typtransform, PointerGetDatum(plrv));
}

   
                                         
   
static Datum
PLySequence_ToArray(PLyObToDatum *arg, PyObject *plrv, bool *isnull, bool inarray)
{
  ArrayType *array;
  int i;
  Datum *elems;
  bool *nulls;
  int64 len;
  int ndim;
  int dims[MAXDIM];
  int lbs[MAXDIM];
  int currelem;
  PyObject *pyptr = plrv;
  PyObject *next;

  if (plrv == Py_None)
  {
    *isnull = true;
    return (Datum)0;
  }
  *isnull = false;

     
                                                          
     
  ndim = 0;
  len = 1;

  Py_INCREF(plrv);

  for (;;)
  {
    if (!PyList_Check(pyptr))
    {
      break;
    }

    if (ndim == MAXDIM)
    {
      PLy_elog(ERROR, "number of array dimensions exceeds the maximum allowed (%d)", MAXDIM);
    }

    dims[ndim] = PySequence_Length(pyptr);
    if (dims[ndim] < 0)
    {
      PLy_elog(ERROR, "could not determine sequence length for function return value");
    }

    if (dims[ndim] > MaxAllocSize)
    {
      PLy_elog(ERROR, "array size exceeds the maximum allowed");
    }

    len *= dims[ndim];
    if (len > MaxAllocSize)
    {
      PLy_elog(ERROR, "array size exceeds the maximum allowed");
    }

    if (dims[ndim] == 0)
    {
                          
      break;
    }

    ndim++;

    next = PySequence_GetItem(pyptr, 0);
    Py_XDECREF(pyptr);
    pyptr = next;
  }
  Py_XDECREF(pyptr);

     
                                                                           
                                                                           
                                                                        
                                                                            
                                                
     
  if (ndim == 0)
  {
    if (!PySequence_Check(plrv))
    {
      PLy_elog(ERROR, "return value of function with array return type is not a Python sequence");
    }

    ndim = 1;
    len = dims[0] = PySequence_Length(plrv);
  }

     
                                                                          
                                                               
     
  elems = palloc(sizeof(Datum) * len);
  nulls = palloc(sizeof(bool) * len);
  currelem = 0;
  PLySequence_ToArray_recurse(arg->u.array.elm, plrv, dims, ndim, 0, elems, nulls, &currelem);

  for (i = 0; i < ndim; i++)
  {
    lbs[i] = 1;
  }

  array = construct_md_array(elems, nulls, ndim, dims, lbs, arg->u.array.elmbasetype, arg->u.array.elm->typlen, arg->u.array.elm->typbyval, arg->u.array.elm->typalign);

  return PointerGetDatum(array);
}

   
                                                                               
                                                       
   
static void
PLySequence_ToArray_recurse(PLyObToDatum *elm, PyObject *list, int *dims, int ndim, int dim, Datum *elems, bool *nulls, int *currelem)
{
  int i;

  if (PySequence_Length(list) != dims[dim])
  {
    ereport(ERROR, (errmsg("wrong length of inner sequence: has length %d, but %d was expected", (int)PySequence_Length(list), dims[dim]), (errdetail("To construct a multidimensional array, the inner sequences must all have the same length."))));
  }

  if (dim < ndim - 1)
  {
    for (i = 0; i < dims[dim]; i++)
    {
      PyObject *sublist = PySequence_GetItem(list, i);

      PLySequence_ToArray_recurse(elm, sublist, dims, ndim, dim + 1, elems, nulls, currelem);
      Py_XDECREF(sublist);
    }
  }
  else
  {
    for (i = 0; i < dims[dim]; i++)
    {
      PyObject *obj = PySequence_GetItem(list, i);

      elems[*currelem] = elm->func(elm, obj, &nulls[*currelem], true);
      Py_XDECREF(obj);
      (*currelem)++;
    }
  }
}

   
                                                          
   
static Datum
PLyString_ToComposite(PLyObToDatum *arg, PyObject *string, bool inarray)
{
  char *str;

     
                                                                           
                                                                        
     
  if (!OidIsValid(arg->u.tuple.recinfunc.fn_oid))
  {
    fmgr_info_cxt(F_RECORD_IN, &arg->u.tuple.recinfunc, arg->mcxt);
  }

  str = PLyObject_AsString(string);

     
                                                                        
                                                                           
                         
     
                                                                            
                        
     
                                                                        
                                                                          
                                                                  
                                                                      
                                                                          
              
     
                                                                       
                                                                          
                                                                          
                                                                           
                                                                        
                                                                       
     
                                                                             
                                       
     
  if (inarray)
  {
    char *ptr = str;

                                  
    while (*ptr && isspace((unsigned char)*ptr))
    {
      ptr++;
    }
    if (*ptr++ != '(')
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", str), errdetail("Missing left parenthesis."), errhint("To return a composite type in an array, return the composite type as a Python tuple, e.g., \"[('foo',)]\".")));
    }
  }

  return InputFunctionCall(&arg->u.tuple.recinfunc, str, arg->typoid, arg->typmod);
}

static Datum
PLyMapping_ToComposite(PLyObToDatum *arg, TupleDesc desc, PyObject *mapping)
{
  Datum result;
  HeapTuple tuple;
  Datum *values;
  bool *nulls;
  volatile int i;

  Assert(PyMapping_Check(mapping));

                   
  values = palloc(sizeof(Datum) * desc->natts);
  nulls = palloc(sizeof(bool) * desc->natts);
  for (i = 0; i < desc->natts; ++i)
  {
    char *key;
    PyObject *volatile value;
    PLyObToDatum *att;
    Form_pg_attribute attr = TupleDescAttr(desc, i);

    if (attr->attisdropped)
    {
      values[i] = (Datum)0;
      nulls[i] = true;
      continue;
    }

    key = NameStr(attr->attname);
    value = NULL;
    att = &arg->u.tuple.atts[i];
    PG_TRY();
    {
      value = PyMapping_GetItemString(mapping, key);
      if (!value)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("key \"%s\" not found in mapping", key),
                           errhint("To return null in a column, "
                                   "add the value None to the mapping with the key named after the column.")));
      }

      values[i] = att->func(att, value, &nulls[i], false);

      Py_XDECREF(value);
      value = NULL;
    }
    PG_CATCH();
    {
      Py_XDECREF(value);
      PG_RE_THROW();
    }
    PG_END_TRY();
  }

  tuple = heap_form_tuple(desc, values, nulls);
  result = heap_copy_tuple_as_datum(tuple, desc);
  heap_freetuple(tuple);

  pfree(values);
  pfree(nulls);

  return result;
}

static Datum
PLySequence_ToComposite(PLyObToDatum *arg, TupleDesc desc, PyObject *sequence)
{
  Datum result;
  HeapTuple tuple;
  Datum *values;
  bool *nulls;
  volatile int idx;
  volatile int i;

  Assert(PySequence_Check(sequence));

     
                                                                           
                                                                            
                                                    
     
  idx = 0;
  for (i = 0; i < desc->natts; i++)
  {
    if (!TupleDescAttr(desc, i)->attisdropped)
    {
      idx++;
    }
  }
  if (PySequence_Length(sequence) != idx)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("length of returned sequence did not match number of columns in row")));
  }

                   
  values = palloc(sizeof(Datum) * desc->natts);
  nulls = palloc(sizeof(bool) * desc->natts);
  idx = 0;
  for (i = 0; i < desc->natts; ++i)
  {
    PyObject *volatile value;
    PLyObToDatum *att;

    if (TupleDescAttr(desc, i)->attisdropped)
    {
      values[i] = (Datum)0;
      nulls[i] = true;
      continue;
    }

    value = NULL;
    att = &arg->u.tuple.atts[i];
    PG_TRY();
    {
      value = PySequence_GetItem(sequence, idx);
      Assert(value);

      values[i] = att->func(att, value, &nulls[i], false);

      Py_XDECREF(value);
      value = NULL;
    }
    PG_CATCH();
    {
      Py_XDECREF(value);
      PG_RE_THROW();
    }
    PG_END_TRY();

    idx++;
  }

  tuple = heap_form_tuple(desc, values, nulls);
  result = heap_copy_tuple_as_datum(tuple, desc);
  heap_freetuple(tuple);

  pfree(values);
  pfree(nulls);

  return result;
}

static Datum
PLyGenericObject_ToComposite(PLyObToDatum *arg, TupleDesc desc, PyObject *object, bool inarray)
{
  Datum result;
  HeapTuple tuple;
  Datum *values;
  bool *nulls;
  volatile int i;

                   
  values = palloc(sizeof(Datum) * desc->natts);
  nulls = palloc(sizeof(bool) * desc->natts);
  for (i = 0; i < desc->natts; ++i)
  {
    char *key;
    PyObject *volatile value;
    PLyObToDatum *att;
    Form_pg_attribute attr = TupleDescAttr(desc, i);

    if (attr->attisdropped)
    {
      values[i] = (Datum)0;
      nulls[i] = true;
      continue;
    }

    key = NameStr(attr->attname);
    value = NULL;
    att = &arg->u.tuple.atts[i];
    PG_TRY();
    {
      value = PyObject_GetAttrString(object, key);
      if (!value)
      {
           
                                                       
           
                                                                    
                                                                       
                                                                      
                                                                    
                                                                      
                                                                   
                                              
           
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("attribute \"%s\" does not exist in Python object", key), inarray ? errhint("To return a composite type in an array, return the composite type as a Python tuple, e.g., \"[('foo',)]\".") : errhint("To return null in a column, let the returned object have an attribute named after column with value None.")));
      }

      values[i] = att->func(att, value, &nulls[i], false);

      Py_XDECREF(value);
      value = NULL;
    }
    PG_CATCH();
    {
      Py_XDECREF(value);
      PG_RE_THROW();
    }
    PG_END_TRY();
  }

  tuple = heap_form_tuple(desc, values, nulls);
  result = heap_copy_tuple_as_datum(tuple, desc);
  heap_freetuple(tuple);

  pfree(values);
  pfree(nulls);

  return result;
}
