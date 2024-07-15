                                                                            
   
              
                                                               
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

   
                                                     
   
typedef struct ColumnIOData
{
  Oid column_type;
  Oid typiofunc;
  Oid typioparam;
  bool typisvarlena;
  FmgrInfo proc;
} ColumnIOData;

typedef struct RecordIOData
{
  Oid record_type;
  int32 record_typmod;
  int ncolumns;
  ColumnIOData columns[FLEXIBLE_ARRAY_MEMBER];
} RecordIOData;

   
                                                            
   
typedef struct ColumnCompareData
{
  TypeCacheEntry *typentry;                                       
} ColumnCompareData;

typedef struct RecordCompareData
{
  int ncolumns;                                    
  Oid record1_type;
  int32 record1_typmod;
  Oid record2_type;
  int32 record2_typmod;
  ColumnCompareData columns[FLEXIBLE_ARRAY_MEMBER];
} RecordCompareData;

   
                                                      
   
Datum
record_in(PG_FUNCTION_ARGS)
{
  char *string = PG_GETARG_CSTRING(0);
  Oid tupType = PG_GETARG_OID(1);
  int32 tupTypmod = PG_GETARG_INT32(2);
  HeapTupleHeader result;
  TupleDesc tupdesc;
  HeapTuple tuple;
  RecordIOData *my_extra;
  bool needComma = false;
  int ncolumns;
  int i;
  char *ptr;
  Datum *values;
  bool *nulls;
  StringInfoData buf;

  check_stack_depth();                                       

     
                                                                             
                                                                             
                                                                             
                                                                            
                                                                             
                                                                            
     
  if (tupType == RECORDOID && tupTypmod < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("input of anonymous composite types is not implemented")));
  }

     
                                                                             
                                                                     
                                   
     
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
  ncolumns = tupdesc->natts;

     
                                                                       
                                                                   
     
  my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns != ncolumns)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
    my_extra->record_type = InvalidOid;
    my_extra->record_typmod = 0;
  }

  if (my_extra->record_type != tupType || my_extra->record_typmod != tupTypmod)
  {
    MemSet(my_extra, 0, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra->record_type = tupType;
    my_extra->record_typmod = tupTypmod;
    my_extra->ncolumns = ncolumns;
  }

  values = (Datum *)palloc(ncolumns * sizeof(Datum));
  nulls = (bool *)palloc(ncolumns * sizeof(bool));

     
                                                                         
                                                                        
     
  ptr = string;
                                
  while (*ptr && isspace((unsigned char)*ptr))
  {
    ptr++;
  }
  if (*ptr++ != '(')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", string), errdetail("Missing left parenthesis.")));
  }

  initStringInfo(&buf);

  for (i = 0; i < ncolumns; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);
    ColumnIOData *column_info = &my_extra->columns[i];
    Oid column_type = att->atttypid;
    char *column_data;

                                                                 
    if (att->attisdropped)
    {
      values[i] = (Datum)0;
      nulls[i] = true;
      continue;
    }

    if (needComma)
    {
                                                               
      if (*ptr == ',')
      {
        ptr++;
      }
      else
      {
                              
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", string), errdetail("Too few columns.")));
      }
    }

                                                           
    if (*ptr == ',' || *ptr == ')')
    {
      column_data = NULL;
      nulls[i] = true;
    }
    else
    {
                                          
      bool inquote = false;

      resetStringInfo(&buf);
      while (inquote || !(*ptr == ',' || *ptr == ')'))
      {
        char ch = *ptr++;

        if (ch == '\0')
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", string), errdetail("Unexpected end of input.")));
        }
        if (ch == '\\')
        {
          if (*ptr == '\0')
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", string), errdetail("Unexpected end of input.")));
          }
          appendStringInfoChar(&buf, *ptr++);
        }
        else if (ch == '"')
        {
          if (!inquote)
          {
            inquote = true;
          }
          else if (*ptr == '"')
          {
                                                     
            appendStringInfoChar(&buf, *ptr++);
          }
          else
          {
            inquote = false;
          }
        }
        else
        {
          appendStringInfoChar(&buf, ch);
        }
      }

      column_data = buf.data;
      nulls[i] = false;
    }

       
                                
       
    if (column_info->column_type != column_type)
    {
      getTypeInputInfo(column_type, &column_info->typiofunc, &column_info->typioparam);
      fmgr_info_cxt(column_info->typiofunc, &column_info->proc, fcinfo->flinfo->fn_mcxt);
      column_info->column_type = column_type;
    }

    values[i] = InputFunctionCall(&column_info->proc, column_data, column_info->typioparam, att->atttypmod);

       
                            
       
    needComma = true;
  }

  if (*ptr++ != ')')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", string), errdetail("Too many columns.")));
  }
                                 
  while (*ptr && isspace((unsigned char)*ptr))
  {
    ptr++;
  }
  if (*ptr)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed record literal: \"%s\"", string), errdetail("Junk after right parenthesis.")));
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);

     
                                                                            
                                                                           
                                                                 
     
  result = (HeapTupleHeader)palloc(tuple->t_len);
  memcpy(result, tuple->t_data, tuple->t_len);

  heap_freetuple(tuple);
  pfree(buf.data);
  pfree(values);
  pfree(nulls);
  ReleaseTupleDesc(tupdesc);

  PG_RETURN_HEAPTUPLEHEADER(result);
}

   
                                                        
   
Datum
record_out(PG_FUNCTION_ARGS)
{
  HeapTupleHeader rec = PG_GETARG_HEAPTUPLEHEADER(0);
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupdesc;
  HeapTupleData tuple;
  RecordIOData *my_extra;
  bool needComma = false;
  int ncolumns;
  int i;
  Datum *values;
  bool *nulls;
  StringInfoData buf;

  check_stack_depth();                                       

                                               
  tupType = HeapTupleHeaderGetTypeId(rec);
  tupTypmod = HeapTupleHeaderGetTypMod(rec);
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
  ncolumns = tupdesc->natts;

                                                     
  tuple.t_len = HeapTupleHeaderGetDatumLength(rec);
  ItemPointerSetInvalid(&(tuple.t_self));
  tuple.t_tableOid = InvalidOid;
  tuple.t_data = rec;

     
                                                                       
                                                                   
     
  my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns != ncolumns)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
    my_extra->record_type = InvalidOid;
    my_extra->record_typmod = 0;
  }

  if (my_extra->record_type != tupType || my_extra->record_typmod != tupTypmod)
  {
    MemSet(my_extra, 0, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra->record_type = tupType;
    my_extra->record_typmod = tupTypmod;
    my_extra->ncolumns = ncolumns;
  }

  values = (Datum *)palloc(ncolumns * sizeof(Datum));
  nulls = (bool *)palloc(ncolumns * sizeof(bool));

                                        
  heap_deform_tuple(&tuple, tupdesc, values, nulls);

                                   
  initStringInfo(&buf);

  appendStringInfoChar(&buf, '(');

  for (i = 0; i < ncolumns; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);
    ColumnIOData *column_info = &my_extra->columns[i];
    Oid column_type = att->atttypid;
    Datum attr;
    char *value;
    char *tmp;
    bool nq;

                                            
    if (att->attisdropped)
    {
      continue;
    }

    if (needComma)
    {
      appendStringInfoChar(&buf, ',');
    }
    needComma = true;

    if (nulls[i])
    {
                           
      continue;
    }

       
                                        
       
    if (column_info->column_type != column_type)
    {
      getTypeOutputInfo(column_type, &column_info->typiofunc, &column_info->typisvarlena);
      fmgr_info_cxt(column_info->typiofunc, &column_info->proc, fcinfo->flinfo->fn_mcxt);
      column_info->column_type = column_type;
    }

    attr = values[i];
    value = OutputFunctionCall(&column_info->proc, attr);

                                                             
    nq = (value[0] == '\0');                                    
    for (tmp = value; *tmp; tmp++)
    {
      char ch = *tmp;

      if (ch == '"' || ch == '\\' || ch == '(' || ch == ')' || ch == ',' || isspace((unsigned char)ch))
      {
        nq = true;
        break;
      }
    }

                             
    if (nq)
    {
      appendStringInfoCharMacro(&buf, '"');
    }
    for (tmp = value; *tmp; tmp++)
    {
      char ch = *tmp;

      if (ch == '"' || ch == '\\')
      {
        appendStringInfoCharMacro(&buf, ch);
      }
      appendStringInfoCharMacro(&buf, ch);
    }
    if (nq)
    {
      appendStringInfoCharMacro(&buf, '"');
    }
  }

  appendStringInfoChar(&buf, ')');

  pfree(values);
  pfree(nulls);
  ReleaseTupleDesc(tupdesc);

  PG_RETURN_CSTRING(buf.data);
}

   
                                                               
   
Datum
record_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  Oid tupType = PG_GETARG_OID(1);
  int32 tupTypmod = PG_GETARG_INT32(2);
  HeapTupleHeader result;
  TupleDesc tupdesc;
  HeapTuple tuple;
  RecordIOData *my_extra;
  int ncolumns;
  int usercols;
  int validcols;
  int i;
  Datum *values;
  bool *nulls;

  check_stack_depth();                                       

     
                                                                             
                                                                             
                                                                             
                                                                            
                                                                             
                                                                            
     
  if (tupType == RECORDOID && tupTypmod < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("input of anonymous composite types is not implemented")));
  }

  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
  ncolumns = tupdesc->natts;

     
                                                                       
                                                                   
     
  my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns != ncolumns)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
    my_extra->record_type = InvalidOid;
    my_extra->record_typmod = 0;
  }

  if (my_extra->record_type != tupType || my_extra->record_typmod != tupTypmod)
  {
    MemSet(my_extra, 0, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra->record_type = tupType;
    my_extra->record_typmod = tupTypmod;
    my_extra->ncolumns = ncolumns;
  }

  values = (Datum *)palloc(ncolumns * sizeof(Datum));
  nulls = (bool *)palloc(ncolumns * sizeof(bool));

                                                  
  usercols = pq_getmsgint(buf, 4);

                                                
  validcols = 0;
  for (i = 0; i < ncolumns; i++)
  {
    if (!TupleDescAttr(tupdesc, i)->attisdropped)
    {
      validcols++;
    }
  }
  if (usercols != validcols)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("wrong number of columns: %d, expected %d", usercols, validcols)));
  }

                           
  for (i = 0; i < ncolumns; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);
    ColumnIOData *column_info = &my_extra->columns[i];
    Oid column_type = att->atttypid;
    Oid coltypoid;
    int itemlen;
    StringInfoData item_buf;
    StringInfo bufptr;
    char csave;

                                                                 
    if (att->attisdropped)
    {
      values[i] = (Datum)0;
      nulls[i] = true;
      continue;
    }

                                
    coltypoid = pq_getmsgint(buf, sizeof(Oid));
    if (coltypoid != column_type)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("wrong data type: %u, expected %u", coltypoid, column_type)));
    }

                                       
    itemlen = pq_getmsgint(buf, 4);
    if (itemlen < -1 || itemlen > (buf->len - buf->cursor))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("insufficient data left in message")));
    }

    if (itemlen == -1)
    {
                                
      bufptr = NULL;
      nulls[i] = true;
      csave = 0;                          
    }
    else
    {
         
                                                                 
                                                                         
                                                                         
                                                               
         
      item_buf.data = &buf->data[buf->cursor];
      item_buf.maxlen = itemlen + 1;
      item_buf.len = itemlen;
      item_buf.cursor = 0;

      buf->cursor += itemlen;

      csave = buf->data[buf->cursor];
      buf->data[buf->cursor] = '\0';

      bufptr = &item_buf;
      nulls[i] = false;
    }

                                           
    if (column_info->column_type != column_type)
    {
      getTypeBinaryInputInfo(column_type, &column_info->typiofunc, &column_info->typioparam);
      fmgr_info_cxt(column_info->typiofunc, &column_info->proc, fcinfo->flinfo->fn_mcxt);
      column_info->column_type = column_type;
    }

    values[i] = ReceiveFunctionCall(&column_info->proc, bufptr, column_info->typioparam, att->atttypmod);

    if (bufptr)
    {
                                                     
      if (item_buf.cursor != itemlen)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("improper binary format in record column %d", i + 1)));
      }

      buf->data[buf->cursor] = csave;
    }
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);

     
                                                                            
                                                                           
                                                                 
     
  result = (HeapTupleHeader)palloc(tuple->t_len);
  memcpy(result, tuple->t_data, tuple->t_len);

  heap_freetuple(tuple);
  pfree(values);
  pfree(nulls);
  ReleaseTupleDesc(tupdesc);

  PG_RETURN_HEAPTUPLEHEADER(result);
}

   
                                                                
   
Datum
record_send(PG_FUNCTION_ARGS)
{
  HeapTupleHeader rec = PG_GETARG_HEAPTUPLEHEADER(0);
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupdesc;
  HeapTupleData tuple;
  RecordIOData *my_extra;
  int ncolumns;
  int validcols;
  int i;
  Datum *values;
  bool *nulls;
  StringInfoData buf;

  check_stack_depth();                                       

                                               
  tupType = HeapTupleHeaderGetTypeId(rec);
  tupTypmod = HeapTupleHeaderGetTypMod(rec);
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
  ncolumns = tupdesc->natts;

                                                     
  tuple.t_len = HeapTupleHeaderGetDatumLength(rec);
  ItemPointerSetInvalid(&(tuple.t_self));
  tuple.t_tableOid = InvalidOid;
  tuple.t_data = rec;

     
                                                                       
                                                                   
     
  my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns != ncolumns)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra = (RecordIOData *)fcinfo->flinfo->fn_extra;
    my_extra->record_type = InvalidOid;
    my_extra->record_typmod = 0;
  }

  if (my_extra->record_type != tupType || my_extra->record_typmod != tupTypmod)
  {
    MemSet(my_extra, 0, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    my_extra->record_type = tupType;
    my_extra->record_typmod = tupTypmod;
    my_extra->ncolumns = ncolumns;
  }

  values = (Datum *)palloc(ncolumns * sizeof(Datum));
  nulls = (bool *)palloc(ncolumns * sizeof(bool));

                                        
  heap_deform_tuple(&tuple, tupdesc, values, nulls);

                                   
  pq_begintypsend(&buf);

                                                
  validcols = 0;
  for (i = 0; i < ncolumns; i++)
  {
    if (!TupleDescAttr(tupdesc, i)->attisdropped)
    {
      validcols++;
    }
  }
  pq_sendint32(&buf, validcols);

  for (i = 0; i < ncolumns; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);
    ColumnIOData *column_info = &my_extra->columns[i];
    Oid column_type = att->atttypid;
    Datum attr;
    bytea *outputbytes;

                                            
    if (att->attisdropped)
    {
      continue;
    }

    pq_sendint32(&buf, column_type);

    if (nulls[i])
    {
                                                 
      pq_sendint32(&buf, -1);
      continue;
    }

       
                                          
       
    if (column_info->column_type != column_type)
    {
      getTypeBinaryOutputInfo(column_type, &column_info->typiofunc, &column_info->typisvarlena);
      fmgr_info_cxt(column_info->typiofunc, &column_info->proc, fcinfo->flinfo->fn_mcxt);
      column_info->column_type = column_type;
    }

    attr = values[i];
    outputbytes = SendFunctionCall(&column_info->proc, attr);
    pq_sendint32(&buf, VARSIZE(outputbytes) - VARHDRSZ);
    pq_sendbytes(&buf, VARDATA(outputbytes), VARSIZE(outputbytes) - VARHDRSZ);
  }

  pfree(values);
  pfree(nulls);
  ReleaseTupleDesc(tupdesc);

  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                
                                             
   
                      
   
                                                                       
                                                                             
                                                                               
                                             
   
static int
record_cmp(FunctionCallInfo fcinfo)
{
  HeapTupleHeader record1 = PG_GETARG_HEAPTUPLEHEADER(0);
  HeapTupleHeader record2 = PG_GETARG_HEAPTUPLEHEADER(1);
  int result = 0;
  Oid tupType1;
  Oid tupType2;
  int32 tupTypmod1;
  int32 tupTypmod2;
  TupleDesc tupdesc1;
  TupleDesc tupdesc2;
  HeapTupleData tuple1;
  HeapTupleData tuple2;
  int ncolumns1;
  int ncolumns2;
  RecordCompareData *my_extra;
  int ncols;
  Datum *values1;
  Datum *values2;
  bool *nulls1;
  bool *nulls2;
  int i1;
  int i2;
  int j;

  check_stack_depth();                                       

                                         
  tupType1 = HeapTupleHeaderGetTypeId(record1);
  tupTypmod1 = HeapTupleHeaderGetTypMod(record1);
  tupdesc1 = lookup_rowtype_tupdesc(tupType1, tupTypmod1);
  ncolumns1 = tupdesc1->natts;
  tupType2 = HeapTupleHeaderGetTypeId(record2);
  tupTypmod2 = HeapTupleHeaderGetTypMod(record2);
  tupdesc2 = lookup_rowtype_tupdesc(tupType2, tupTypmod2);
  ncolumns2 = tupdesc2->natts;

                                                    
  tuple1.t_len = HeapTupleHeaderGetDatumLength(record1);
  ItemPointerSetInvalid(&(tuple1.t_self));
  tuple1.t_tableOid = InvalidOid;
  tuple1.t_data = record1;
  tuple2.t_len = HeapTupleHeaderGetDatumLength(record2);
  ItemPointerSetInvalid(&(tuple2.t_self));
  tuple2.t_tableOid = InvalidOid;
  tuple2.t_data = record2;

     
                                                                           
                                                                     
     
  ncols = Max(ncolumns1, ncolumns2);
  my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns < ncols)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordCompareData, columns) + ncols * sizeof(ColumnCompareData));
    my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
    my_extra->ncolumns = ncols;
    my_extra->record1_type = InvalidOid;
    my_extra->record1_typmod = 0;
    my_extra->record2_type = InvalidOid;
    my_extra->record2_typmod = 0;
  }

  if (my_extra->record1_type != tupType1 || my_extra->record1_typmod != tupTypmod1 || my_extra->record2_type != tupType2 || my_extra->record2_typmod != tupTypmod2)
  {
    MemSet(my_extra->columns, 0, ncols * sizeof(ColumnCompareData));
    my_extra->record1_type = tupType1;
    my_extra->record1_typmod = tupTypmod1;
    my_extra->record2_type = tupType2;
    my_extra->record2_typmod = tupTypmod2;
  }

                                         
  values1 = (Datum *)palloc(ncolumns1 * sizeof(Datum));
  nulls1 = (bool *)palloc(ncolumns1 * sizeof(bool));
  heap_deform_tuple(&tuple1, tupdesc1, values1, nulls1);
  values2 = (Datum *)palloc(ncolumns2 * sizeof(Datum));
  nulls2 = (bool *)palloc(ncolumns2 * sizeof(bool));
  heap_deform_tuple(&tuple2, tupdesc2, values2, nulls2);

     
                                                                           
                                                                          
                               
     
  i1 = i2 = j = 0;
  while (i1 < ncolumns1 || i2 < ncolumns2)
  {
    Form_pg_attribute att1;
    Form_pg_attribute att2;
    TypeCacheEntry *typentry;
    Oid collation;

       
                            
       
    if (i1 < ncolumns1 && TupleDescAttr(tupdesc1, i1)->attisdropped)
    {
      i1++;
      continue;
    }
    if (i2 < ncolumns2 && TupleDescAttr(tupdesc2, i2)->attisdropped)
    {
      i2++;
      continue;
    }
    if (i1 >= ncolumns1 || i2 >= ncolumns2)
    {
      break;                                          
    }

    att1 = TupleDescAttr(tupdesc1, i1);
    att2 = TupleDescAttr(tupdesc2, i2);

       
                                                         
       
    if (att1->atttypid != att2->atttypid)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare dissimilar column types %s and %s at record column %d", format_type_be(att1->atttypid), format_type_be(att2->atttypid), j + 1)));
    }

       
                                                                      
                                  
       
    collation = att1->attcollation;
    if (collation != att2->attcollation)
    {
      collation = InvalidOid;
    }

       
                                                          
       
    typentry = my_extra->columns[j].typentry;
    if (typentry == NULL || typentry->type_id != att1->atttypid)
    {
      typentry = lookup_type_cache(att1->atttypid, TYPECACHE_CMP_PROC_FINFO);
      if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify a comparison function for type %s", format_type_be(typentry->type_id))));
      }
      my_extra->columns[j].typentry = typentry;
    }

       
                                                     
       
    if (!nulls1[i1] || !nulls2[i2])
    {
      LOCAL_FCINFO(locfcinfo, 2);
      int32 cmpresult;

      if (nulls1[i1])
      {
                                       
        result = 1;
        break;
      }
      if (nulls2[i2])
      {
                                    
        result = -1;
        break;
      }

                                        
      InitFunctionCallInfoData(*locfcinfo, &typentry->cmp_proc_finfo, 2, collation, NULL, NULL);
      locfcinfo->args[0].value = values1[i1];
      locfcinfo->args[0].isnull = false;
      locfcinfo->args[1].value = values2[i2];
      locfcinfo->args[1].isnull = false;
      locfcinfo->isnull = false;
      cmpresult = DatumGetInt32(FunctionCallInvoke(locfcinfo));

      if (cmpresult < 0)
      {
                                    
        result = -1;
        break;
      }
      else if (cmpresult > 0)
      {
                                       
        result = 1;
        break;
      }
    }

                                           
    i1++, i2++, j++;
  }

     
                                                                      
                                                                           
                                          
     
  if (result == 0)
  {
    if (i1 != ncolumns1 || i2 != ncolumns2)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare record types with different numbers of columns")));
    }
  }

  pfree(values1);
  pfree(nulls1);
  pfree(values2);
  pfree(nulls2);
  ReleaseTupleDesc(tupdesc1);
  ReleaseTupleDesc(tupdesc2);

                                                       
  PG_FREE_IF_COPY(record1, 0);
  PG_FREE_IF_COPY(record2, 1);

  return result;
}

   
               
                                        
            
                                                              
   
                                                                            
                                                                            
   
Datum
record_eq(PG_FUNCTION_ARGS)
{
  HeapTupleHeader record1 = PG_GETARG_HEAPTUPLEHEADER(0);
  HeapTupleHeader record2 = PG_GETARG_HEAPTUPLEHEADER(1);
  bool result = true;
  Oid tupType1;
  Oid tupType2;
  int32 tupTypmod1;
  int32 tupTypmod2;
  TupleDesc tupdesc1;
  TupleDesc tupdesc2;
  HeapTupleData tuple1;
  HeapTupleData tuple2;
  int ncolumns1;
  int ncolumns2;
  RecordCompareData *my_extra;
  int ncols;
  Datum *values1;
  Datum *values2;
  bool *nulls1;
  bool *nulls2;
  int i1;
  int i2;
  int j;

  check_stack_depth();                                       

                                         
  tupType1 = HeapTupleHeaderGetTypeId(record1);
  tupTypmod1 = HeapTupleHeaderGetTypMod(record1);
  tupdesc1 = lookup_rowtype_tupdesc(tupType1, tupTypmod1);
  ncolumns1 = tupdesc1->natts;
  tupType2 = HeapTupleHeaderGetTypeId(record2);
  tupTypmod2 = HeapTupleHeaderGetTypMod(record2);
  tupdesc2 = lookup_rowtype_tupdesc(tupType2, tupTypmod2);
  ncolumns2 = tupdesc2->natts;

                                                    
  tuple1.t_len = HeapTupleHeaderGetDatumLength(record1);
  ItemPointerSetInvalid(&(tuple1.t_self));
  tuple1.t_tableOid = InvalidOid;
  tuple1.t_data = record1;
  tuple2.t_len = HeapTupleHeaderGetDatumLength(record2);
  ItemPointerSetInvalid(&(tuple2.t_self));
  tuple2.t_tableOid = InvalidOid;
  tuple2.t_data = record2;

     
                                                                           
                                                                     
     
  ncols = Max(ncolumns1, ncolumns2);
  my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns < ncols)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordCompareData, columns) + ncols * sizeof(ColumnCompareData));
    my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
    my_extra->ncolumns = ncols;
    my_extra->record1_type = InvalidOid;
    my_extra->record1_typmod = 0;
    my_extra->record2_type = InvalidOid;
    my_extra->record2_typmod = 0;
  }

  if (my_extra->record1_type != tupType1 || my_extra->record1_typmod != tupTypmod1 || my_extra->record2_type != tupType2 || my_extra->record2_typmod != tupTypmod2)
  {
    MemSet(my_extra->columns, 0, ncols * sizeof(ColumnCompareData));
    my_extra->record1_type = tupType1;
    my_extra->record1_typmod = tupTypmod1;
    my_extra->record2_type = tupType2;
    my_extra->record2_typmod = tupTypmod2;
  }

                                         
  values1 = (Datum *)palloc(ncolumns1 * sizeof(Datum));
  nulls1 = (bool *)palloc(ncolumns1 * sizeof(bool));
  heap_deform_tuple(&tuple1, tupdesc1, values1, nulls1);
  values2 = (Datum *)palloc(ncolumns2 * sizeof(Datum));
  nulls2 = (bool *)palloc(ncolumns2 * sizeof(bool));
  heap_deform_tuple(&tuple2, tupdesc2, values2, nulls2);

     
                                                                           
                                                                          
                               
     
  i1 = i2 = j = 0;
  while (i1 < ncolumns1 || i2 < ncolumns2)
  {
    LOCAL_FCINFO(locfcinfo, 2);
    Form_pg_attribute att1;
    Form_pg_attribute att2;
    TypeCacheEntry *typentry;
    Oid collation;
    bool oprresult;

       
                            
       
    if (i1 < ncolumns1 && TupleDescAttr(tupdesc1, i1)->attisdropped)
    {
      i1++;
      continue;
    }
    if (i2 < ncolumns2 && TupleDescAttr(tupdesc2, i2)->attisdropped)
    {
      i2++;
      continue;
    }
    if (i1 >= ncolumns1 || i2 >= ncolumns2)
    {
      break;                                          
    }

    att1 = TupleDescAttr(tupdesc1, i1);
    att2 = TupleDescAttr(tupdesc2, i2);

       
                                                         
       
    if (att1->atttypid != att2->atttypid)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare dissimilar column types %s and %s at record column %d", format_type_be(att1->atttypid), format_type_be(att2->atttypid), j + 1)));
    }

       
                                                                      
                                
       
    collation = att1->attcollation;
    if (collation != att2->attcollation)
    {
      collation = InvalidOid;
    }

       
                                                        
       
    typentry = my_extra->columns[j].typentry;
    if (typentry == NULL || typentry->type_id != att1->atttypid)
    {
      typentry = lookup_type_cache(att1->atttypid, TYPECACHE_EQ_OPR_FINFO);
      if (!OidIsValid(typentry->eq_opr_finfo.fn_oid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify an equality operator for type %s", format_type_be(typentry->type_id))));
      }
      my_extra->columns[j].typentry = typentry;
    }

       
                                                     
       
    if (!nulls1[i1] || !nulls2[i2])
    {
      if (nulls1[i1] || nulls2[i2])
      {
        result = false;
        break;
      }

                                        
      InitFunctionCallInfoData(*locfcinfo, &typentry->eq_opr_finfo, 2, collation, NULL, NULL);
      locfcinfo->args[0].value = values1[i1];
      locfcinfo->args[0].isnull = false;
      locfcinfo->args[1].value = values2[i2];
      locfcinfo->args[1].isnull = false;
      locfcinfo->isnull = false;
      oprresult = DatumGetBool(FunctionCallInvoke(locfcinfo));
      if (!oprresult)
      {
        result = false;
        break;
      }
    }

                                           
    i1++, i2++, j++;
  }

     
                                                                      
                                                                           
                                          
     
  if (result)
  {
    if (i1 != ncolumns1 || i2 != ncolumns2)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare record types with different numbers of columns")));
    }
  }

  pfree(values1);
  pfree(nulls1);
  pfree(values2);
  pfree(nulls2);
  ReleaseTupleDesc(tupdesc1);
  ReleaseTupleDesc(tupdesc2);

                                                       
  PG_FREE_IF_COPY(record1, 0);
  PG_FREE_IF_COPY(record2, 1);

  PG_RETURN_BOOL(result);
}

Datum
record_ne(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(!DatumGetBool(record_eq(fcinfo)));
}

Datum
record_lt(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_cmp(fcinfo) < 0);
}

Datum
record_gt(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_cmp(fcinfo) > 0);
}

Datum
record_le(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_cmp(fcinfo) <= 0);
}

Datum
record_ge(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_cmp(fcinfo) >= 0);
}

Datum
btrecordcmp(PG_FUNCTION_ARGS)
{
  PG_RETURN_INT32(record_cmp(fcinfo));
}

   
                      
                                                           
   
                      
   
                                                                        
                                                                            
                                                                             
                           
   
static int
record_image_cmp(FunctionCallInfo fcinfo)
{
  HeapTupleHeader record1 = PG_GETARG_HEAPTUPLEHEADER(0);
  HeapTupleHeader record2 = PG_GETARG_HEAPTUPLEHEADER(1);
  int result = 0;
  Oid tupType1;
  Oid tupType2;
  int32 tupTypmod1;
  int32 tupTypmod2;
  TupleDesc tupdesc1;
  TupleDesc tupdesc2;
  HeapTupleData tuple1;
  HeapTupleData tuple2;
  int ncolumns1;
  int ncolumns2;
  RecordCompareData *my_extra;
  int ncols;
  Datum *values1;
  Datum *values2;
  bool *nulls1;
  bool *nulls2;
  int i1;
  int i2;
  int j;

                                         
  tupType1 = HeapTupleHeaderGetTypeId(record1);
  tupTypmod1 = HeapTupleHeaderGetTypMod(record1);
  tupdesc1 = lookup_rowtype_tupdesc(tupType1, tupTypmod1);
  ncolumns1 = tupdesc1->natts;
  tupType2 = HeapTupleHeaderGetTypeId(record2);
  tupTypmod2 = HeapTupleHeaderGetTypMod(record2);
  tupdesc2 = lookup_rowtype_tupdesc(tupType2, tupTypmod2);
  ncolumns2 = tupdesc2->natts;

                                                    
  tuple1.t_len = HeapTupleHeaderGetDatumLength(record1);
  ItemPointerSetInvalid(&(tuple1.t_self));
  tuple1.t_tableOid = InvalidOid;
  tuple1.t_data = record1;
  tuple2.t_len = HeapTupleHeaderGetDatumLength(record2);
  ItemPointerSetInvalid(&(tuple2.t_self));
  tuple2.t_tableOid = InvalidOid;
  tuple2.t_data = record2;

     
                                                                           
                                                                     
     
  ncols = Max(ncolumns1, ncolumns2);
  my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns < ncols)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordCompareData, columns) + ncols * sizeof(ColumnCompareData));
    my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
    my_extra->ncolumns = ncols;
    my_extra->record1_type = InvalidOid;
    my_extra->record1_typmod = 0;
    my_extra->record2_type = InvalidOid;
    my_extra->record2_typmod = 0;
  }

  if (my_extra->record1_type != tupType1 || my_extra->record1_typmod != tupTypmod1 || my_extra->record2_type != tupType2 || my_extra->record2_typmod != tupTypmod2)
  {
    MemSet(my_extra->columns, 0, ncols * sizeof(ColumnCompareData));
    my_extra->record1_type = tupType1;
    my_extra->record1_typmod = tupTypmod1;
    my_extra->record2_type = tupType2;
    my_extra->record2_typmod = tupTypmod2;
  }

                                         
  values1 = (Datum *)palloc(ncolumns1 * sizeof(Datum));
  nulls1 = (bool *)palloc(ncolumns1 * sizeof(bool));
  heap_deform_tuple(&tuple1, tupdesc1, values1, nulls1);
  values2 = (Datum *)palloc(ncolumns2 * sizeof(Datum));
  nulls2 = (bool *)palloc(ncolumns2 * sizeof(bool));
  heap_deform_tuple(&tuple2, tupdesc2, values2, nulls2);

     
                                                                           
                                                                          
                               
     
  i1 = i2 = j = 0;
  while (i1 < ncolumns1 || i2 < ncolumns2)
  {
    Form_pg_attribute att1;
    Form_pg_attribute att2;

       
                            
       
    if (i1 < ncolumns1 && TupleDescAttr(tupdesc1, i1)->attisdropped)
    {
      i1++;
      continue;
    }
    if (i2 < ncolumns2 && TupleDescAttr(tupdesc2, i2)->attisdropped)
    {
      i2++;
      continue;
    }
    if (i1 >= ncolumns1 || i2 >= ncolumns2)
    {
      break;                                          
    }

    att1 = TupleDescAttr(tupdesc1, i1);
    att2 = TupleDescAttr(tupdesc2, i2);

       
                                                         
       
    if (att1->atttypid != att2->atttypid)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare dissimilar column types %s and %s at record column %d", format_type_be(att1->atttypid), format_type_be(att2->atttypid), j + 1)));
    }

       
                                                                    
                  
       
    Assert(att1->attlen == att2->attlen);

       
                                                     
       
    if (!nulls1[i1] || !nulls2[i2])
    {
      int cmpresult = 0;

      if (nulls1[i1])
      {
                                       
        result = 1;
        break;
      }
      if (nulls2[i2])
      {
                                    
        result = -1;
        break;
      }

                                        
      if (att1->attbyval)
      {
        if (values1[i1] != values2[i2])
        {
          cmpresult = (values1[i1] < values2[i2]) ? -1 : 1;
        }
      }
      else if (att1->attlen > 0)
      {
        cmpresult = memcmp(DatumGetPointer(values1[i1]), DatumGetPointer(values2[i2]), att1->attlen);
      }
      else if (att1->attlen == -1)
      {
        Size len1, len2;
        struct varlena *arg1val;
        struct varlena *arg2val;

        len1 = toast_raw_datum_size(values1[i1]);
        len2 = toast_raw_datum_size(values2[i2]);
        arg1val = PG_DETOAST_DATUM_PACKED(values1[i1]);
        arg2val = PG_DETOAST_DATUM_PACKED(values2[i2]);

        cmpresult = memcmp(VARDATA_ANY(arg1val), VARDATA_ANY(arg2val), Min(len1, len2) - VARHDRSZ);
        if ((cmpresult == 0) && (len1 != len2))
        {
          cmpresult = (len1 < len2) ? -1 : 1;
        }

        if ((Pointer)arg1val != (Pointer)values1[i1])
        {
          pfree(arg1val);
        }
        if ((Pointer)arg2val != (Pointer)values2[i2])
        {
          pfree(arg2val);
        }
      }
      else
      {
        elog(ERROR, "unexpected attlen: %d", att1->attlen);
      }

      if (cmpresult < 0)
      {
                                    
        result = -1;
        break;
      }
      else if (cmpresult > 0)
      {
                                       
        result = 1;
        break;
      }
    }

                                           
    i1++, i2++, j++;
  }

     
                                                                      
                                                                           
                                          
     
  if (result == 0)
  {
    if (i1 != ncolumns1 || i2 != ncolumns2)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare record types with different numbers of columns")));
    }
  }

  pfree(values1);
  pfree(nulls1);
  pfree(values2);
  pfree(nulls2);
  ReleaseTupleDesc(tupdesc1);
  ReleaseTupleDesc(tupdesc2);

                                                       
  PG_FREE_IF_COPY(record1, 0);
  PG_FREE_IF_COPY(record2, 1);

  return result;
}

   
                     
                                                                        
            
                                                                  
   
                                                                 
                                             
   
Datum
record_image_eq(PG_FUNCTION_ARGS)
{
  HeapTupleHeader record1 = PG_GETARG_HEAPTUPLEHEADER(0);
  HeapTupleHeader record2 = PG_GETARG_HEAPTUPLEHEADER(1);
  bool result = true;
  Oid tupType1;
  Oid tupType2;
  int32 tupTypmod1;
  int32 tupTypmod2;
  TupleDesc tupdesc1;
  TupleDesc tupdesc2;
  HeapTupleData tuple1;
  HeapTupleData tuple2;
  int ncolumns1;
  int ncolumns2;
  RecordCompareData *my_extra;
  int ncols;
  Datum *values1;
  Datum *values2;
  bool *nulls1;
  bool *nulls2;
  int i1;
  int i2;
  int j;

                                         
  tupType1 = HeapTupleHeaderGetTypeId(record1);
  tupTypmod1 = HeapTupleHeaderGetTypMod(record1);
  tupdesc1 = lookup_rowtype_tupdesc(tupType1, tupTypmod1);
  ncolumns1 = tupdesc1->natts;
  tupType2 = HeapTupleHeaderGetTypeId(record2);
  tupTypmod2 = HeapTupleHeaderGetTypMod(record2);
  tupdesc2 = lookup_rowtype_tupdesc(tupType2, tupTypmod2);
  ncolumns2 = tupdesc2->natts;

                                                    
  tuple1.t_len = HeapTupleHeaderGetDatumLength(record1);
  ItemPointerSetInvalid(&(tuple1.t_self));
  tuple1.t_tableOid = InvalidOid;
  tuple1.t_data = record1;
  tuple2.t_len = HeapTupleHeaderGetDatumLength(record2);
  ItemPointerSetInvalid(&(tuple2.t_self));
  tuple2.t_tableOid = InvalidOid;
  tuple2.t_data = record2;

     
                                                                           
                                                                     
     
  ncols = Max(ncolumns1, ncolumns2);
  my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->ncolumns < ncols)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, offsetof(RecordCompareData, columns) + ncols * sizeof(ColumnCompareData));
    my_extra = (RecordCompareData *)fcinfo->flinfo->fn_extra;
    my_extra->ncolumns = ncols;
    my_extra->record1_type = InvalidOid;
    my_extra->record1_typmod = 0;
    my_extra->record2_type = InvalidOid;
    my_extra->record2_typmod = 0;
  }

  if (my_extra->record1_type != tupType1 || my_extra->record1_typmod != tupTypmod1 || my_extra->record2_type != tupType2 || my_extra->record2_typmod != tupTypmod2)
  {
    MemSet(my_extra->columns, 0, ncols * sizeof(ColumnCompareData));
    my_extra->record1_type = tupType1;
    my_extra->record1_typmod = tupTypmod1;
    my_extra->record2_type = tupType2;
    my_extra->record2_typmod = tupTypmod2;
  }

                                         
  values1 = (Datum *)palloc(ncolumns1 * sizeof(Datum));
  nulls1 = (bool *)palloc(ncolumns1 * sizeof(bool));
  heap_deform_tuple(&tuple1, tupdesc1, values1, nulls1);
  values2 = (Datum *)palloc(ncolumns2 * sizeof(Datum));
  nulls2 = (bool *)palloc(ncolumns2 * sizeof(bool));
  heap_deform_tuple(&tuple2, tupdesc2, values2, nulls2);

     
                                                                           
                                                                          
                               
     
  i1 = i2 = j = 0;
  while (i1 < ncolumns1 || i2 < ncolumns2)
  {
    Form_pg_attribute att1;
    Form_pg_attribute att2;

       
                            
       
    if (i1 < ncolumns1 && TupleDescAttr(tupdesc1, i1)->attisdropped)
    {
      i1++;
      continue;
    }
    if (i2 < ncolumns2 && TupleDescAttr(tupdesc2, i2)->attisdropped)
    {
      i2++;
      continue;
    }
    if (i1 >= ncolumns1 || i2 >= ncolumns2)
    {
      break;                                          
    }

    att1 = TupleDescAttr(tupdesc1, i1);
    att2 = TupleDescAttr(tupdesc2, i2);

       
                                                         
       
    if (att1->atttypid != att2->atttypid)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare dissimilar column types %s and %s at record column %d", format_type_be(att1->atttypid), format_type_be(att2->atttypid), j + 1)));
    }

       
                                                     
       
    if (!nulls1[i1] || !nulls2[i2])
    {
      if (nulls1[i1] || nulls2[i2])
      {
        result = false;
        break;
      }

                                        
      result = datum_image_eq(values1[i1], values2[i2], att1->attbyval, att2->attlen);
      if (!result)
      {
        break;
      }
    }

                                           
    i1++, i2++, j++;
  }

     
                                                                      
                                                                           
                                          
     
  if (result)
  {
    if (i1 != ncolumns1 || i2 != ncolumns2)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot compare record types with different numbers of columns")));
    }
  }

  pfree(values1);
  pfree(nulls1);
  pfree(values2);
  pfree(nulls2);
  ReleaseTupleDesc(tupdesc1);
  ReleaseTupleDesc(tupdesc2);

                                                       
  PG_FREE_IF_COPY(record1, 0);
  PG_FREE_IF_COPY(record2, 1);

  PG_RETURN_BOOL(result);
}

Datum
record_image_ne(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(!DatumGetBool(record_image_eq(fcinfo)));
}

Datum
record_image_lt(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_image_cmp(fcinfo) < 0);
}

Datum
record_image_gt(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_image_cmp(fcinfo) > 0);
}

Datum
record_image_le(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_image_cmp(fcinfo) <= 0);
}

Datum
record_image_ge(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(record_image_cmp(fcinfo) >= 0);
}

Datum
btrecordimagecmp(PG_FUNCTION_ARGS)
{
  PG_RETURN_INT32(record_image_cmp(fcinfo));
}
