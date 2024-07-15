                                                                            
   
              
                                                            
   
                                                                         
                                                                        
   
   
                  
                                 
   
         
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_proc.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "port/pg_bswap.h"
#include "tcop/fastpath.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

   
                                                                     
                                                                     
                                                                      
                                                                     
                                                                    
                                                                        
                                                                           
                                                                         
                                                                  
   
struct fp_info
{
  Oid funcid;
  FmgrInfo flinfo;                                      
  Oid namespace;                                 
  Oid rettype;
  Oid argtypes[FUNC_MAX_ARGS];
  char fname[NAMEDATALEN];                                
};

static int16
parse_fcall_arguments(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo);
static int16
parse_fcall_arguments_20(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo);

                    
                          
   
                                                                           
                                                                          
                                                                         
                                                              
   
                                                            
                    
   
int
GetOldFunctionMessage(StringInfo buf)
{
  int32 ibuf;
  int nargs;

                             
  if (pq_getstring(buf))
  {
    return EOF;
  }
                    
  if (pq_getbytes((char *)&ibuf, 4))
  {
    return EOF;
  }
  appendBinaryStringInfo(buf, (char *)&ibuf, 4);
                           
  if (pq_getbytes((char *)&ibuf, 4))
  {
    return EOF;
  }
  appendBinaryStringInfo(buf, (char *)&ibuf, 4);
  nargs = pg_ntoh32(ibuf);
                             
  while (nargs-- > 0)
  {
    int argsize;

                 
    if (pq_getbytes((char *)&ibuf, 4))
    {
      return EOF;
    }
    appendBinaryStringInfo(buf, (char *)&ibuf, 4);
    argsize = pg_ntoh32(ibuf);
    if (argsize < -1)
    {
                                                              
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid argument size %d in function call message", argsize)));
    }
                          
    if (argsize > 0)
    {
                                  
      enlargeStringInfo(buf, argsize);
                       
      if (pq_getbytes(buf->data + buf->len, argsize))
      {
        return EOF;
      }
      buf->len += argsize;
                                                           
      buf->data[buf->len] = '\0';
    }
  }
  return 0;
}

                    
                       
   
                                                                         
                                              
                    
   
static void
SendFunctionResult(Datum retval, bool isnull, Oid rettype, int16 format)
{
  bool newstyle = (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3);
  StringInfoData buf;

  pq_beginmessage(&buf, 'V');

  if (isnull)
  {
    if (newstyle)
    {
      pq_sendint32(&buf, -1);
    }
  }
  else
  {
    if (!newstyle)
    {
      pq_sendbyte(&buf, 'G');
    }

    if (format == 0)
    {
      Oid typoutput;
      bool typisvarlena;
      char *outputstr;

      getTypeOutputInfo(rettype, &typoutput, &typisvarlena);
      outputstr = OidOutputFunctionCall(typoutput, retval);
      pq_sendcountedtext(&buf, outputstr, strlen(outputstr), false);
      pfree(outputstr);
    }
    else if (format == 1)
    {
      Oid typsend;
      bool typisvarlena;
      bytea *outputbytes;

      getTypeBinaryOutputInfo(rettype, &typsend, &typisvarlena);
      outputbytes = OidSendFunctionCall(typsend, retval);
      pq_sendint32(&buf, VARSIZE(outputbytes) - VARHDRSZ);
      pq_sendbytes(&buf, VARDATA(outputbytes), VARSIZE(outputbytes) - VARHDRSZ);
      pfree(outputbytes);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unsupported format code: %d", format)));
    }
  }

  if (!newstyle)
  {
    pq_sendbyte(&buf, '0');
  }

  pq_endmessage(&buf);
}

   
                 
   
                                                                   
                       
   
static void
fetch_fp_info(Oid func_id, struct fp_info *fip)
{
  HeapTuple func_htp;
  Form_pg_proc pp;

  Assert(fip != NULL);

     
                                                                       
                                                                        
                                                                            
                                                                            
                                                                      
                                                               
     
  MemSet(fip, 0, sizeof(struct fp_info));
  fip->funcid = InvalidOid;

  func_htp = SearchSysCache1(PROCOID, ObjectIdGetDatum(func_id));
  if (!HeapTupleIsValid(func_htp))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function with OID %u does not exist", func_id)));
  }
  pp = (Form_pg_proc)GETSTRUCT(func_htp);

                                                                   
  if (pp->prokind != PROKIND_FUNCTION || pp->proretset)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot call function %s via fastpath interface", NameStr(pp->proname))));
  }

                                                                       
  if (pp->pronargs > FUNC_MAX_ARGS)
  {
    elog(ERROR, "function %s has more than %d arguments", NameStr(pp->proname), FUNC_MAX_ARGS);
  }

  fip->namespace = pp->pronamespace;
  fip->rettype = pp->prorettype;
  memcpy(fip->argtypes, pp->proargtypes.values, pp->pronargs * sizeof(Oid));
  strlcpy(fip->fname, NameStr(pp->proname), NAMEDATALEN);

  ReleaseSysCache(func_htp);

  fmgr_info(func_id, &fip->flinfo);

     
                        
     
  fip->funcid = func_id;
}

   
                         
   
                                                                    
                                                      
   
          
                                                                     
            
   
                                                                          
                                                                    
                                                                         
                                    
   
void
HandleFunctionRequest(StringInfo msgBuf)
{
  LOCAL_FCINFO(fcinfo, FUNC_MAX_ARGS);
  Oid fid;
  AclResult aclresult;
  int16 rformat;
  Datum retval;
  struct fp_info my_fp;
  struct fp_info *fip;
  bool callit;
  bool was_logged = false;
  char msec_str[32];

     
                                                                          
                                                                     
     
  if (IsAbortedTransactionBlockState())
  {
    ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION), errmsg("current transaction is aborted, "
                                                                       "commands ignored until end of transaction block")));
  }

     
                                                                          
                                                                    
     
  PushActiveSnapshot(GetTransactionSnapshot());

     
                                        
     
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
    (void)pq_getmsgstring(msgBuf);                   
  }

  fid = (Oid)pq_getmsgint(msgBuf, 4);                   

     
                                                                         
                                        
     
  fip = &my_fp;
  fetch_fp_info(fid, fip);

                                                        
  if (log_statement == LOGSTMT_ALL)
  {
    ereport(LOG, (errmsg("fastpath function call: \"%s\" (OID %u)", fip->fname, fid)));
    was_logged = true;
  }

     
                                                                       
                                                                      
     
  aclresult = pg_namespace_aclcheck(fip->namespace, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(fip->namespace));
  }
  InvokeNamespaceSearchHook(fip->namespace, true);

  aclresult = pg_proc_aclcheck(fid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_FUNCTION, get_func_name(fid));
  }
  InvokeFunctionExecuteHook(fid);

     
                                                            
     
                                                                          
                                                                 
                                     
     
  InitFunctionCallInfoData(*fcinfo, &fip->flinfo, 0, InvalidOid, NULL, NULL);

  if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
  {
    rformat = parse_fcall_arguments(msgBuf, fip, fcinfo);
  }
  else
  {
    rformat = parse_fcall_arguments_20(msgBuf, fip, fcinfo);
  }

                                                                
  pq_getmsgend(msgBuf);

     
                                                        
     
  callit = true;
  if (fip->flinfo.fn_strict)
  {
    int i;

    for (i = 0; i < fcinfo->nargs; i++)
    {
      if (fcinfo->args[i].isnull)
      {
        callit = false;
        break;
      }
    }
  }

  if (callit)
  {
                         
    retval = FunctionCallInvoke(fcinfo);
  }
  else
  {
    fcinfo->isnull = true;
    retval = (Datum)0;
  }

                                                                        
  CHECK_FOR_INTERRUPTS();

  SendFunctionResult(retval, fcinfo->isnull, fip->rettype, rformat);

                                      
  PopActiveSnapshot();

     
                                           
     
  switch (check_log_duration(msec_str, was_logged))
  {
  case 1:
    ereport(LOG, (errmsg("duration: %s ms", msec_str)));
    break;
  case 2:
    ereport(LOG, (errmsg("duration: %s ms  fastpath function call: \"%s\" (OID %u)", msec_str, fip->fname, fid)));
    break;
  }
}

   
                                                      
   
                                                                          
                
   
static int16
parse_fcall_arguments(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo)
{
  int nargs;
  int i;
  int numAFormats;
  int16 *aformats = NULL;
  StringInfoData abuf;

                                     
  numAFormats = pq_getmsgint(msgBuf, 2);
  if (numAFormats > 0)
  {
    aformats = (int16 *)palloc(numAFormats * sizeof(int16));
    for (i = 0; i < numAFormats; i++)
    {
      aformats[i] = pq_getmsgint(msgBuf, 2);
    }
  }

  nargs = pq_getmsgint(msgBuf, 2);                     

  if (fip->flinfo.fn_nargs != nargs || nargs > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("function call message contains %d arguments but function requires %d", nargs, fip->flinfo.fn_nargs)));
  }

  fcinfo->nargs = nargs;

  if (numAFormats > 1 && numAFormats != nargs)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("function call message contains %d argument formats but %d arguments", numAFormats, nargs)));
  }

  initStringInfo(&abuf);

     
                                              
     
  for (i = 0; i < nargs; ++i)
  {
    int argsize;
    int16 aformat;

    argsize = pq_getmsgint(msgBuf, 4);
    if (argsize == -1)
    {
      fcinfo->args[i].isnull = true;
    }
    else
    {
      fcinfo->args[i].isnull = false;
      if (argsize < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid argument size %d in function call message", argsize)));
      }

                                                            
      resetStringInfo(&abuf);
      appendBinaryStringInfo(&abuf, pq_getmsgbytes(msgBuf, argsize), argsize);
    }

    if (numAFormats > 1)
    {
      aformat = aformats[i];
    }
    else if (numAFormats > 0)
    {
      aformat = aformats[0];
    }
    else
    {
      aformat = 0;                     
    }

    if (aformat == 0)
    {
      Oid typinput;
      Oid typioparam;
      char *pstring;

      getTypeInputInfo(fip->argtypes[i], &typinput, &typioparam);

         
                                                                    
                                                                     
                                                                    
                          
         
      if (argsize == -1)
      {
        pstring = NULL;
      }
      else
      {
        pstring = pg_client_to_server(abuf.data, argsize);
      }

      fcinfo->args[i].value = OidInputFunctionCall(typinput, pstring, typioparam, -1);
                                                      
      if (pstring && pstring != abuf.data)
      {
        pfree(pstring);
      }
    }
    else if (aformat == 1)
    {
      Oid typreceive;
      Oid typioparam;
      StringInfo bufptr;

                                                           
      getTypeBinaryInputInfo(fip->argtypes[i], &typreceive, &typioparam);

      if (argsize == -1)
      {
        bufptr = NULL;
      }
      else
      {
        bufptr = &abuf;
      }

      fcinfo->args[i].value = OidReceiveFunctionCall(typreceive, bufptr, typioparam, -1);

                                                     
      if (argsize != -1 && abuf.cursor != abuf.len)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("incorrect binary data format in function argument %d", i + 1)));
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unsupported format code: %d", aformat)));
    }
  }

                                 
  return (int16)pq_getmsgint(msgBuf, 2);
}

   
                                                      
   
                                                                          
                
   
static int16
parse_fcall_arguments_20(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo)
{
  int nargs;
  int i;
  StringInfoData abuf;

  nargs = pq_getmsgint(msgBuf, 4);                     

  if (fip->flinfo.fn_nargs != nargs || nargs > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("function call message contains %d arguments but function requires %d", nargs, fip->flinfo.fn_nargs)));
  }

  fcinfo->nargs = nargs;

  initStringInfo(&abuf);

     
                                                                         
                                                     
     
                                                                            
                                                                         
                                     
     
  for (i = 0; i < nargs; ++i)
  {
    int argsize;
    Oid typreceive;
    Oid typioparam;

    getTypeBinaryInputInfo(fip->argtypes[i], &typreceive, &typioparam);

    argsize = pq_getmsgint(msgBuf, 4);
    if (argsize == -1)
    {
      fcinfo->args[i].isnull = true;
      fcinfo->args[i].value = OidReceiveFunctionCall(typreceive, NULL, typioparam, -1);
      continue;
    }
    fcinfo->args[i].isnull = false;
    if (argsize < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid argument size %d in function call message", argsize)));
    }

                                                          
    resetStringInfo(&abuf);
    appendBinaryStringInfo(&abuf, pq_getmsgbytes(msgBuf, argsize), argsize);

    fcinfo->args[i].value = OidReceiveFunctionCall(typreceive, &abuf, typioparam, -1);

                                                   
    if (abuf.cursor != abuf.len)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("incorrect binary data format in function argument %d", i + 1)));
    }
  }

                                                              
  return 1;
}
