                                                                            
   
             
                                                           
   
                                                                       
                       
   
                                                                         
                                                                          
                                     
   
                                                                          
                                                                         
                                                                           
                                                           
   
                                                                           
                                                                        
                                                                           
                                                                        
                                                                             
                                                                        
   
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/xact.h"
#include "catalog/namespace.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

   
                                                                         
                                                                          
                                                                      
                                                                         
                                                                    
   
                                                                             
   
typedef struct ConvProcInfo
{
  int s_encoding;                                     
  int c_encoding;
  FmgrInfo to_server_info;                                       
  FmgrInfo to_client_info;
} ConvProcInfo;

static List *ConvProcList = NIL;                           

   
                                                                       
                                             
   
static FmgrInfo *ToServerConvProc = NULL;
static FmgrInfo *ToClientConvProc = NULL;

   
                                                           
   
static const pg_enc2name *ClientEncoding = &pg_enc2name_tbl[PG_SQL_ASCII];
static const pg_enc2name *DatabaseEncoding = &pg_enc2name_tbl[PG_SQL_ASCII];
static const pg_enc2name *MessageEncoding = &pg_enc2name_tbl[PG_SQL_ASCII];

   
                                                                      
                                                                             
                                                                          
                                                               
   
static bool backend_startup_complete = false;
static int pending_client_encoding = PG_SQL_ASCII;

                        
static char *
perform_default_encoding_conversion(const char *src, int len, bool is_client_to_server);
static int
cliplen(const char *str, int len, int limit);

   
                                                                        
                                                                              
   
                                                                             
                      
   
                                                                           
   
int
PrepareClientEncoding(int encoding)
{
  int current_server_encoding;
  ListCell *lc;

  if (!PG_VALID_FE_ENCODING(encoding))
  {
    return -1;
  }

                                                         
  if (!backend_startup_complete)
  {
    return 0;
  }

  current_server_encoding = GetDatabaseEncoding();

     
                                                          
     
  if (current_server_encoding == encoding || current_server_encoding == PG_SQL_ASCII || encoding == PG_SQL_ASCII)
  {
    return 0;
  }

  if (IsTransactionState())
  {
       
                                                                         
                                                                           
                                                                          
                      
       
    Oid to_server_proc, to_client_proc;
    ConvProcInfo *convinfo;
    MemoryContext oldcontext;

    to_server_proc = FindDefaultConversionProc(encoding, current_server_encoding);
    if (!OidIsValid(to_server_proc))
    {
      return -1;
    }
    to_client_proc = FindDefaultConversionProc(current_server_encoding, encoding);
    if (!OidIsValid(to_client_proc))
    {
      return -1;
    }

       
                                                                        
       
    convinfo = (ConvProcInfo *)MemoryContextAlloc(TopMemoryContext, sizeof(ConvProcInfo));
    convinfo->s_encoding = current_server_encoding;
    convinfo->c_encoding = encoding;
    fmgr_info_cxt(to_server_proc, &convinfo->to_server_info, TopMemoryContext);
    fmgr_info_cxt(to_client_proc, &convinfo->to_client_info, TopMemoryContext);

                                         
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    ConvProcList = lcons(convinfo, ConvProcList);
    MemoryContextSwitchTo(oldcontext);

       
                                                                        
                                                                         
       

    return 0;              
  }
  else
  {
       
                                                                       
                                                                    
                                                                           
                                                              
                                                                         
                           
       
    foreach (lc, ConvProcList)
    {
      ConvProcInfo *oldinfo = (ConvProcInfo *)lfirst(lc);

      if (oldinfo->s_encoding == current_server_encoding && oldinfo->c_encoding == encoding)
      {
        return 0;
      }
    }

    return -1;                               
  }
}

   
                                                                               
                                                                               
   
                                                                           
   
int
SetClientEncoding(int encoding)
{
  int current_server_encoding;
  bool found;
  ListCell *lc;
  ListCell *prev;
  ListCell *next;

  if (!PG_VALID_FE_ENCODING(encoding))
  {
    return -1;
  }

                                                         
  if (!backend_startup_complete)
  {
    pending_client_encoding = encoding;
    return 0;
  }

  current_server_encoding = GetDatabaseEncoding();

     
                                                          
     
  if (current_server_encoding == encoding || current_server_encoding == PG_SQL_ASCII || encoding == PG_SQL_ASCII)
  {
    ClientEncoding = &pg_enc2name_tbl[encoding];
    ToServerConvProc = NULL;
    ToClientConvProc = NULL;
    return 0;
  }

     
                                                           
                                                                       
                                                                             
                  
     
  found = false;
  prev = NULL;
  for (lc = list_head(ConvProcList); lc; lc = next)
  {
    ConvProcInfo *convinfo = (ConvProcInfo *)lfirst(lc);

    next = lnext(lc);

    if (convinfo->s_encoding == current_server_encoding && convinfo->c_encoding == encoding)
    {
      if (!found)
      {
                                           
        ClientEncoding = &pg_enc2name_tbl[encoding];
        ToServerConvProc = &convinfo->to_server_info;
        ToClientConvProc = &convinfo->to_client_info;
        found = true;
      }
      else
      {
                                         
        ConvProcList = list_delete_cell(ConvProcList, lc, prev);
        pfree(convinfo);
        continue;                           
      }
    }

    prev = lc;
  }

  if (found)
  {
    return 0;              
  }
  else
  {
    return -1;                               
  }
}

   
                                           
                                                            
   
void
InitializeClientEncoding(void)
{
  Assert(!backend_startup_complete);
  backend_startup_complete = true;

  if (PrepareClientEncoding(pending_client_encoding) < 0 || SetClientEncoding(pending_client_encoding) < 0)
  {
       
                                                                         
                               
       
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("conversion between %s and %s is not supported", pg_enc2name_tbl[pending_client_encoding].name, GetDatabaseEncodingName())));
  }
}

   
                                       
   
int
pg_get_client_encoding(void)
{
  return ClientEncoding->encoding;
}

   
                                            
   
const char *
pg_get_client_encoding_name(void)
{
  return ClientEncoding->name;
}

   
                                                          
   
                                                                            
   
unsigned char *
pg_do_encoding_conversion(unsigned char *src, int len, int src_encoding, int dest_encoding)
{
  unsigned char *result;
  Oid proc;

  if (len <= 0)
  {
    return src;                                   
  }

  if (src_encoding == dest_encoding)
  {
    return src;                                           
  }

  if (dest_encoding == PG_SQL_ASCII)
  {
    return src;                                       
  }

  if (src_encoding == PG_SQL_ASCII)
  {
                                                                    
    (void)pg_verify_mbstr(dest_encoding, (const char *)src, len, false);
    return src;
  }

  if (!IsTransactionState())                       
  {
    elog(ERROR, "cannot perform encoding conversion outside a transaction");
  }

  proc = FindDefaultConversionProc(src_encoding, dest_encoding);
  if (!OidIsValid(proc))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("default conversion function for encoding \"%s\" to \"%s\" does not exist", pg_encoding_to_char(src_encoding), pg_encoding_to_char(dest_encoding))));
  }

     
                                                                           
     
                                                                         
                                                                            
                                                                           
                                                                             
                                                                        
     
  if ((Size)len >= (MaxAllocHugeSize / (Size)MAX_CONVERSION_GROWTH))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of memory"), errdetail("String of %d bytes is too long for encoding conversion.", len)));
  }

  result = (unsigned char *)MemoryContextAllocHuge(CurrentMemoryContext, (Size)len * MAX_CONVERSION_GROWTH + 1);

  OidFunctionCall5(proc, Int32GetDatum(src_encoding), Int32GetDatum(dest_encoding), CStringGetDatum(src), CStringGetDatum(result), Int32GetDatum(len));

     
                                                                          
                                                                        
                                                                         
     
  if (len > 1000000)
  {
    Size resultlen = strlen((char *)result);

    if (resultlen >= MaxAllocSize)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of memory"), errdetail("String of %d bytes is too long for encoding conversion.", len)));
    }

    result = (unsigned char *)repalloc(result, resultlen + 1);
  }

  return result;
}

   
                                                        
                                
   
                                                       
Datum
pg_convert_to(PG_FUNCTION_ARGS)
{
  Datum string = PG_GETARG_DATUM(0);
  Datum dest_encoding_name = PG_GETARG_DATUM(1);
  Datum src_encoding_name = DirectFunctionCall1(namein, CStringGetDatum(DatabaseEncoding->name));
  Datum result;

     
                                                                          
                                                                        
                                                     
     
  result = DirectFunctionCall3(pg_convert, string, src_encoding_name, dest_encoding_name);

  PG_RETURN_DATUM(result);
}

   
                                                               
                                
   
                                                         
Datum
pg_convert_from(PG_FUNCTION_ARGS)
{
  Datum string = PG_GETARG_DATUM(0);
  Datum src_encoding_name = PG_GETARG_DATUM(1);
  Datum dest_encoding_name = DirectFunctionCall1(namein, CStringGetDatum(DatabaseEncoding->name));
  Datum result;

  result = DirectFunctionCall3(pg_convert, string, src_encoding_name, dest_encoding_name);

     
                                                                             
                                                                 
                                                                           
                                                                         
                                                             
     
  PG_RETURN_DATUM(result);
}

   
                                                   
   
                                                                                
   
Datum
pg_convert(PG_FUNCTION_ARGS)
{
  bytea *string = PG_GETARG_BYTEA_PP(0);
  char *src_encoding_name = NameStr(*PG_GETARG_NAME(1));
  int src_encoding = pg_char_to_encoding(src_encoding_name);
  char *dest_encoding_name = NameStr(*PG_GETARG_NAME(2));
  int dest_encoding = pg_char_to_encoding(dest_encoding_name);
  const char *src_str;
  char *dest_str;
  bytea *retval;
  int len;

  if (src_encoding < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid source encoding name \"%s\"", src_encoding_name)));
  }
  if (dest_encoding < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid destination encoding name \"%s\"", dest_encoding_name)));
  }

                                             
  len = VARSIZE_ANY_EXHDR(string);
  src_str = VARDATA_ANY(string);
  pg_verify_mbstr_len(src_encoding, src_str, len, false);

                          
  dest_str = (char *)pg_do_encoding_conversion((unsigned char *)unconstify(char *, src_str), len, src_encoding, dest_encoding);

                                                  
  if (dest_str != src_str)
  {
    len = strlen(dest_str);
  }

     
                                      
     
  retval = (bytea *)palloc(len + VARHDRSZ);
  SET_VARSIZE(retval, len + VARHDRSZ);
  memcpy(VARDATA(retval), dest_str, len);

  if (dest_str != src_str)
  {
    pfree(dest_str);
  }

                                               
  PG_FREE_IF_COPY(string, 0);

  PG_RETURN_BYTEA_P(retval);
}

   
                                                                    
                                                              
             
   
                                                      
   
Datum
length_in_encoding(PG_FUNCTION_ARGS)
{
  bytea *string = PG_GETARG_BYTEA_PP(0);
  char *src_encoding_name = NameStr(*PG_GETARG_NAME(1));
  int src_encoding = pg_char_to_encoding(src_encoding_name);
  const char *src_str;
  int len;
  int retval;

  if (src_encoding < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid encoding name \"%s\"", src_encoding_name)));
  }

  len = VARSIZE_ANY_EXHDR(string);
  src_str = VARDATA_ANY(string);

  retval = pg_verify_mbstr_len(src_encoding, src_str, len, false);

  PG_RETURN_INT32(retval);
}

   
                                                                     
   
                                                                 
   
Datum
pg_encoding_max_length_sql(PG_FUNCTION_ARGS)
{
  int encoding = PG_GETARG_INT32(0);

  if (PG_VALID_ENCODING(encoding))
  {
    PG_RETURN_INT32(pg_wchar_table[encoding].maxmblen);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                               
   
                                                                            
   
char *
pg_client_to_server(const char *s, int len)
{
  return pg_any_to_server(s, len, ClientEncoding->encoding);
}

   
                                            
   
                                                                            
   
                                                                            
                                                                            
                                                                          
                                 
   
char *
pg_any_to_server(const char *s, int len, int encoding)
{
  if (len <= 0)
  {
    return unconstify(char *, s);                                   
  }

  if (encoding == DatabaseEncoding->encoding || encoding == PG_SQL_ASCII)
  {
       
                                                                     
       
    (void)pg_verify_mbstr(DatabaseEncoding->encoding, s, len, false);
    return unconstify(char *, s);
  }

  if (DatabaseEncoding->encoding == PG_SQL_ASCII)
  {
       
                                                                       
                                                                          
                                                                           
                                                                          
                                                                           
                                                                         
                                                                   
       
    if (PG_VALID_BE_ENCODING(encoding))
    {
      (void)pg_verify_mbstr(encoding, s, len, false);
    }
    else
    {
      int i;

      for (i = 0; i < len; i++)
      {
        if (s[i] == '\0' || IS_HIGHBIT_SET(s[i]))
        {
          ereport(ERROR, (errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE), errmsg("invalid byte value for encoding \"%s\": 0x%02x", pg_enc2name_tbl[PG_SQL_ASCII].name, (unsigned char)s[i])));
        }
      }
    }
    return unconstify(char *, s);
  }

                                                          
  if (encoding == ClientEncoding->encoding)
  {
    return perform_default_encoding_conversion(s, len, true);
  }

                                                           
  return (char *)pg_do_encoding_conversion((unsigned char *)unconstify(char *, s), len, encoding, DatabaseEncoding->encoding);
}

   
                                               
   
                                                                            
   
char *
pg_server_to_client(const char *s, int len)
{
  return pg_server_to_any(s, len, ClientEncoding->encoding);
}

   
                                            
   
                                                                            
   
char *
pg_server_to_any(const char *s, int len, int encoding)
{
  if (len <= 0)
  {
    return unconstify(char *, s);                                   
  }

  if (encoding == DatabaseEncoding->encoding || encoding == PG_SQL_ASCII)
  {
    return unconstify(char *, s);                           
  }

  if (DatabaseEncoding->encoding == PG_SQL_ASCII)
  {
                                                                    
    (void)pg_verify_mbstr(encoding, s, len, false);
    return unconstify(char *, s);
  }

                                                          
  if (encoding == ClientEncoding->encoding)
  {
    return perform_default_encoding_conversion(s, len, false);
  }

                                                           
  return (char *)pg_do_encoding_conversion((unsigned char *)unconstify(char *, s), len, DatabaseEncoding->encoding, encoding);
}

   
                                                                    
                                                                     
                                                                   
                                                    
   
static char *
perform_default_encoding_conversion(const char *src, int len, bool is_client_to_server)
{
  char *result;
  int src_encoding, dest_encoding;
  FmgrInfo *flinfo;

  if (is_client_to_server)
  {
    src_encoding = ClientEncoding->encoding;
    dest_encoding = DatabaseEncoding->encoding;
    flinfo = ToServerConvProc;
  }
  else
  {
    src_encoding = DatabaseEncoding->encoding;
    dest_encoding = ClientEncoding->encoding;
    flinfo = ToClientConvProc;
  }

  if (flinfo == NULL)
  {
    return unconstify(char *, src);
  }

     
                                                                           
                                                
     
  if ((Size)len >= (MaxAllocHugeSize / (Size)MAX_CONVERSION_GROWTH))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of memory"), errdetail("String of %d bytes is too long for encoding conversion.", len)));
  }

  result = (char *)MemoryContextAllocHuge(CurrentMemoryContext, (Size)len * MAX_CONVERSION_GROWTH + 1);

  FunctionCall5(flinfo, Int32GetDatum(src_encoding), Int32GetDatum(dest_encoding), CStringGetDatum(src), CStringGetDatum(result), Int32GetDatum(len));

     
                                                                     
                                
     
  if (len > 1000000)
  {
    Size resultlen = strlen(result);

    if (resultlen >= MaxAllocSize)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of memory"), errdetail("String of %d bytes is too long for encoding conversion.", len)));
    }

    result = (char *)repalloc(result, resultlen + 1);
  }

  return result;
}

                                           
int
pg_mb2wchar(const char *from, pg_wchar *to)
{
  return pg_wchar_table[DatabaseEncoding->encoding].mb2wchar_with_len((const unsigned char *)from, to, strlen(from));
}

                                                                 
int
pg_mb2wchar_with_len(const char *from, pg_wchar *to, int len)
{
  return pg_wchar_table[DatabaseEncoding->encoding].mb2wchar_with_len((const unsigned char *)from, to, len);
}

                             
int
pg_encoding_mb2wchar_with_len(int encoding, const char *from, pg_wchar *to, int len)
{
  return pg_wchar_table[encoding].mb2wchar_with_len((const unsigned char *)from, to, len);
}

                                           
int
pg_wchar2mb(const pg_wchar *from, char *to)
{
  return pg_wchar_table[DatabaseEncoding->encoding].wchar2mb_with_len(from, (unsigned char *)to, pg_wchar_strlen(from));
}

                                                                 
int
pg_wchar2mb_with_len(const pg_wchar *from, char *to, int len)
{
  return pg_wchar_table[DatabaseEncoding->encoding].wchar2mb_with_len(from, (unsigned char *)to, len);
}

                             
int
pg_encoding_wchar2mb_with_len(int encoding, const pg_wchar *from, char *to, int len)
{
  return pg_wchar_table[encoding].wchar2mb_with_len(from, (unsigned char *)to, len);
}

                                                      
int
pg_mblen(const char *mbstr)
{
  return pg_wchar_table[DatabaseEncoding->encoding].mblen((const unsigned char *)mbstr);
}

                                                         
int
pg_dsplen(const char *mbstr)
{
  return pg_wchar_table[DatabaseEncoding->encoding].dsplen((const unsigned char *)mbstr);
}

                                                                  
int
pg_mbstrlen(const char *mbstr)
{
  int len = 0;

                                             
  if (pg_database_encoding_max_length() == 1)
  {
    return strlen(mbstr);
  }

  while (*mbstr)
  {
    mbstr += pg_mblen(mbstr);
    len++;
  }
  return len;
}

                                                                
                                     
   
int
pg_mbstrlen_with_len(const char *mbstr, int limit)
{
  int len = 0;

                                             
  if (pg_database_encoding_max_length() == 1)
  {
    return limit;
  }

  while (limit > 0 && *mbstr)
  {
    int l = pg_mblen(mbstr);

    limit -= l;
    mbstr += l;
    len++;
  }
  return len;
}

   
                                                 
                                     
                                 
                                                              
   
int
pg_mbcliplen(const char *mbstr, int len, int limit)
{
  return pg_encoding_mbcliplen(DatabaseEncoding->encoding, mbstr, len, limit);
}

   
                                        
   
int
pg_encoding_mbcliplen(int encoding, const char *mbstr, int len, int limit)
{
  mblen_converter mblen_fn;
  int clen = 0;
  int l;

                                             
  if (pg_encoding_max_length(encoding) == 1)
  {
    return cliplen(mbstr, len, limit);
  }

  mblen_fn = pg_wchar_table[encoding].mblen;

  while (len > 0 && *mbstr)
  {
    l = (*mblen_fn)((const unsigned char *)mbstr);
    if ((clen + l) > limit)
    {
      break;
    }
    clen += l;
    if (clen == limit)
    {
      break;
    }
    len -= l;
    mbstr += l;
  }
  return clen;
}

   
                                                                    
                                          
   
int
pg_mbcharcliplen(const char *mbstr, int len, int limit)
{
  int clen = 0;
  int nch = 0;
  int l;

                                             
  if (pg_database_encoding_max_length() == 1)
  {
    return cliplen(mbstr, len, limit);
  }

  while (len > 0 && *mbstr)
  {
    l = pg_mblen(mbstr);
    nch++;
    if (nch > limit)
    {
      break;
    }
    clen += l;
    len -= l;
    mbstr += l;
  }
  return clen;
}

                                            
static int
cliplen(const char *str, int len, int limit)
{
  int l = 0;

  len = Min(len, limit);
  while (l < len && str[l])
  {
    l++;
  }
  return l;
}

void
SetDatabaseEncoding(int encoding)
{
  if (!PG_VALID_BE_ENCODING(encoding))
  {
    elog(ERROR, "invalid database encoding: %d", encoding);
  }

  DatabaseEncoding = &pg_enc2name_tbl[encoding];
  Assert(DatabaseEncoding->encoding == encoding);
}

void
SetMessageEncoding(int encoding)
{
                                               
  Assert(PG_VALID_ENCODING(encoding));

  MessageEncoding = &pg_enc2name_tbl[encoding];
  Assert(MessageEncoding->encoding == encoding);
}

#ifdef ENABLE_NLS
   
                                                                              
                                                                               
                                                        
   
static bool
raw_pg_bind_textdomain_codeset(const char *domainname, int encoding)
{
  bool elog_ok = (CurrentMemoryContext != NULL);
  int i;

  for (i = 0; pg_enc2gettext_tbl[i].name != NULL; i++)
  {
    if (pg_enc2gettext_tbl[i].encoding == encoding)
    {
      if (bind_textdomain_codeset(domainname, pg_enc2gettext_tbl[i].name) != NULL)
      {
        return true;
      }

      if (elog_ok)
      {
        elog(LOG, "bind_textdomain_codeset failed");
      }
      else
      {
        write_stderr("bind_textdomain_codeset failed");
      }

      break;
    }
  }

  return false;
}

   
                                                                              
                                                                              
                                                           
   
                                                                           
                                                                              
                                                                              
                                                                              
                                                                            
                                 
   
                                                                          
                                                                             
                                                                            
                                                                       
   
                                                                  
   
int
pg_bind_textdomain_codeset(const char *domainname)
{
  bool elog_ok = (CurrentMemoryContext != NULL);
  int encoding = GetDatabaseEncoding();
  int new_msgenc;

#ifndef WIN32
  const char *ctype = setlocale(LC_CTYPE, NULL);

  if (pg_strcasecmp(ctype, "C") == 0 || pg_strcasecmp(ctype, "POSIX") == 0)
#endif
    if (encoding != PG_SQL_ASCII && raw_pg_bind_textdomain_codeset(domainname, encoding))
    {
      return encoding;
    }

  new_msgenc = pg_get_encoding_from_locale(NULL, elog_ok);
  if (new_msgenc < 0)
  {
    new_msgenc = PG_SQL_ASCII;
  }

#ifdef WIN32
  if (!raw_pg_bind_textdomain_codeset(domainname, new_msgenc))
  {
                                                             
    return GetMessageEncoding();
  }
#endif

  return new_msgenc;
}
#endif

   
                                                                          
                                                                            
                                                
   
int
GetDatabaseEncoding(void)
{
  return DatabaseEncoding->encoding;
}

const char *
GetDatabaseEncodingName(void)
{
  return DatabaseEncoding->name;
}

Datum
getdatabaseencoding(PG_FUNCTION_ARGS)
{
  return DirectFunctionCall1(namein, CStringGetDatum(DatabaseEncoding->name));
}

Datum
pg_client_encoding(PG_FUNCTION_ARGS)
{
  return DirectFunctionCall1(namein, CStringGetDatum(ClientEncoding->name));
}

   
                                                                        
                                                                            
                                                                           
                            
   
int
GetMessageEncoding(void)
{
  return MessageEncoding->encoding;
}

#ifdef WIN32
   
                                                                      
                                                                  
                                                                               
                                                                               
   
WCHAR *
pgwin32_message_to_UTF16(const char *str, int len, int *utf16len)
{
  int msgenc = GetMessageEncoding();
  WCHAR *utf16;
  int dstlen;
  UINT codepage;

  if (msgenc == PG_SQL_ASCII)
  {
                                                                  
    return NULL;
  }

  codepage = pg_enc2name_tbl[msgenc].codepage;

     
                                                                            
                                                                             
                                                              
     
  if (codepage != 0)
  {
    utf16 = (WCHAR *)palloc(sizeof(WCHAR) * (len + 1));
    dstlen = MultiByteToWideChar(codepage, 0, str, len, utf16, len);
    utf16[dstlen] = (WCHAR)0;
  }
  else
  {
    char *utf8;

       
                                                                       
                                                            
       
    if (IsTransactionState())
    {
      utf8 = (char *)pg_do_encoding_conversion((unsigned char *)str, len, msgenc, PG_UTF8);
      if (utf8 != str)
      {
        len = strlen(utf8);
      }
    }
    else
    {
      utf8 = (char *)str;
    }

    utf16 = (WCHAR *)palloc(sizeof(WCHAR) * (len + 1));
    dstlen = MultiByteToWideChar(CP_UTF8, 0, utf8, len, utf16, len);
    utf16[dstlen] = (WCHAR)0;

    if (utf8 != str)
    {
      pfree(utf8);
    }
  }

  if (dstlen == 0 && len > 0)
  {
    pfree(utf16);
    return NULL;            
  }

  if (utf16len)
  {
    *utf16len = dstlen;
  }
  return utf16;
}

#endif
