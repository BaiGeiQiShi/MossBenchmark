                                                                            
   
         
                            
   
   
                                                                         
                                                                        
   
                               
   
                                                                            
   

   
                                                                     
                                                                   
                                                                   
                                                                      
                                                                     
                                                                      
                                                                        
              
   

   
                               
   
                                                                              
                                                                       
                                                                            
                                                                         
                                                                         
                                                                            
                                                                              
                                                                            
                                                                           
                                                                            
                                                                          
                                                                           
                                                                          
                     
   
                               

#include "postgres.h"

#ifdef USE_LIBXML
#include <libxml/chvalid.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlversion.h>
#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

   
                                                                            
                                                                        
                                       
   
#if LIBXML_VERSION >= 20704
#define HAVE_XMLSTRUCTUREDERRORCONTEXT 1
#endif
#endif                 

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/namespace.h"
#include "catalog/pg_class.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "executor/spi.h"
#include "executor/tablefunc.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/execnodes.h"
#include "nodes/nodeFuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/xml.h"

                   
int xmlbinary;
int xmloption;

#ifdef USE_LIBXML

                                                 
#define ERRCXT_MAGIC 68275028

struct PgXmlErrorContext
{
  int magic;
                                                 
  PgXmlStrictness strictness;
                                                            
  bool err_occurred;
  StringInfoData err_buf;
                                                                   
  xmlStructuredErrorFunc saved_errfunc;
  void *saved_errcxt;
                                                             
  xmlExternalEntityLoader saved_entityfunc;
};

static xmlParserInputPtr
xmlPgEntityLoader(const char *URL, const char *ID, xmlParserCtxtPtr ctxt);
static void
xml_errorHandler(void *data, xmlErrorPtr error);
static void
xml_ereport_by_code(int level, int sqlcode, const char *msg, int errcode);
static void
chopStringInfoNewlines(StringInfo str);
static void
appendStringInfoLineSeparator(StringInfo str);

#ifdef USE_LIBXMLCONTEXT

static MemoryContext LibxmlContext = NULL;

static void
xml_memory_init(void);
static void *
xml_palloc(size_t size);
static void *
xml_repalloc(void *ptr, size_t size);
static void
xml_pfree(void *ptr);
static char *
xml_pstrdup(const char *string);
#endif                        

static xmlChar *
xml_text2xmlChar(text *in);
static int
parse_xml_decl(const xmlChar *str, size_t *lenp, xmlChar **version, xmlChar **encoding, int *standalone);
static bool
print_xml_decl(StringInfo buf, const xmlChar *version, pg_enc encoding, int standalone);
static bool
xml_doctype_in_content(const xmlChar *str);
static xmlDocPtr
xml_parse(text *data, XmlOptionType xmloption_arg, bool preserve_whitespace, int encoding);
static text *
xml_xmlnodetoxmltype(xmlNodePtr cur, PgXmlErrorContext *xmlerrcxt);
static int
xml_xpathobjtoxmlarray(xmlXPathObjectPtr xpathobj, ArrayBuildState *astate, PgXmlErrorContext *xmlerrcxt);
static xmlChar *
pg_xmlCharStrndup(const char *str, size_t len);
#endif                 

static void
xmldata_root_element_start(StringInfo result, const char *eltname, const char *xmlschema, const char *targetns, bool top_level);
static void
xmldata_root_element_end(StringInfo result, const char *eltname);
static StringInfo
query_to_xml_internal(const char *query, char *tablename, const char *xmlschema, bool nulls, bool tableforest, const char *targetns, bool top_level);
static const char *
map_sql_table_to_xmlschema(TupleDesc tupdesc, Oid relid, bool nulls, bool tableforest, const char *targetns);
static const char *
map_sql_schema_to_xmlschema_types(Oid nspid, List *relid_list, bool nulls, bool tableforest, const char *targetns);
static const char *
map_sql_catalog_to_xmlschema_types(List *nspid_list, bool nulls, bool tableforest, const char *targetns);
static const char *
map_sql_type_to_xml_name(Oid typeoid, int typmod);
static const char *
map_sql_typecoll_to_xmlschema_types(List *tupdesc_list);
static const char *
map_sql_type_to_xmlschema_type(Oid typeoid, int typmod);
static void
SPI_sql_row_to_xmlelement(uint64 rownum, StringInfo result, char *tablename, bool nulls, bool tableforest, const char *targetns, bool top_level);

                      
#ifdef USE_LIBXML
                                               
#define XMLTABLE_CONTEXT_MAGIC 46922182
typedef struct XmlTableBuilderData
{
  int magic;
  int natts;
  long int row_count;
  PgXmlErrorContext *xmlerrcxt;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  xmlXPathContextPtr xpathcxt;
  xmlXPathCompExprPtr xpathcomp;
  xmlXPathObjectPtr xpathobj;
  xmlXPathCompExprPtr *xpathscomp;
} XmlTableBuilderData;
#endif

static void
XmlTableInitOpaque(struct TableFuncScanState *state, int natts);
static void
XmlTableSetDocument(struct TableFuncScanState *state, Datum value);
static void
XmlTableSetNamespace(struct TableFuncScanState *state, const char *name, const char *uri);
static void
XmlTableSetRowFilter(struct TableFuncScanState *state, const char *path);
static void
XmlTableSetColumnFilter(struct TableFuncScanState *state, const char *path, int colnum);
static bool
XmlTableFetchRow(struct TableFuncScanState *state);
static Datum
XmlTableGetValue(struct TableFuncScanState *state, int colnum, Oid typid, int32 typmod, bool *isnull);
static void
XmlTableDestroyOpaque(struct TableFuncScanState *state);

const TableFuncRoutine XmlTableRoutine = {XmlTableInitOpaque, XmlTableSetDocument, XmlTableSetNamespace, XmlTableSetRowFilter, XmlTableSetColumnFilter, XmlTableFetchRow, XmlTableGetValue, XmlTableDestroyOpaque};

#define NO_XML_SUPPORT() ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unsupported XML feature"), errdetail("This functionality requires the server to be built with libxml support."), errhint("You need to rebuild PostgreSQL using --with-libxml.")))

                                   
#define NAMESPACE_XSD "http://www.w3.org/2001/XMLSchema"
#define NAMESPACE_XSI "http://www.w3.org/2001/XMLSchema-instance"
#define NAMESPACE_SQLXML "http://standards.iso.org/iso/9075/2003/sqlxml"

#ifdef USE_LIBXML

static int
xmlChar_to_encoding(const xmlChar *encoding_name)
{
  int encoding = pg_char_to_encoding((const char *)encoding_name);

  if (encoding < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid encoding name \"%s\"", (const char *)encoding_name)));
  }
  return encoding;
}
#endif

   
                                                                             
                                                         
   
                                                                    
                   
   
Datum
xml_in(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  char *s = PG_GETARG_CSTRING(0);
  xmltype *vardata;
  xmlDocPtr doc;

  vardata = (xmltype *)cstring_to_text(s);

     
                                                                         
                                       
     
  doc = xml_parse(vardata, xmloption, true, GetDatabaseEncoding());
  xmlFreeDoc(doc);

  PG_RETURN_XML_P(vardata);
#else
  NO_XML_SUPPORT();
  return 0;
#endif
}

#define PG_XML_DEFAULT_VERSION "1.0"

   
                                                                            
                                                                    
   
                                                                    
                   
   
static char *
xml_out_internal(xmltype *x, pg_enc target_encoding)
{
  char *str = text_to_cstring((text *)x);

#ifdef USE_LIBXML
  size_t len = strlen(str);
  xmlChar *version;
  int standalone;
  int res_code;

  if ((res_code = parse_xml_decl((xmlChar *)str, &len, &version, NULL, &standalone)) == 0)
  {
    StringInfoData buf;

    initStringInfo(&buf);

    if (!print_xml_decl(&buf, version, target_encoding, standalone))
    {
         
                                                                         
                                                                        
                     
         
      if (*(str + len) == '\n')
      {
        len += 1;
      }
    }
    appendStringInfoString(&buf, str + len);

    pfree(str);

    return buf.data;
  }

  xml_ereport_by_code(WARNING, ERRCODE_INTERNAL_ERROR, "could not parse XML declaration in stored value", res_code);
#endif
  return str;
}

Datum
xml_out(PG_FUNCTION_ARGS)
{
  xmltype *x = PG_GETARG_XML_P(0);

     
                                                                             
                                                                       
                                                                            
         
     
  PG_RETURN_CSTRING(xml_out_internal(x, 0));
}

Datum
xml_recv(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  xmltype *result;
  char *str;
  char *newstr;
  int nbytes;
  xmlDocPtr doc;
  xmlChar *encodingStr = NULL;
  int encoding;

     
                                                                             
                                                                        
                                                      
     
  nbytes = buf->len - buf->cursor;
  str = (char *)pq_getmsgbytes(buf, nbytes);

     
                                                                           
                                                                          
                          
     
  result = palloc(nbytes + 1 + VARHDRSZ);
  SET_VARSIZE(result, nbytes + VARHDRSZ);
  memcpy(VARDATA(result), str, nbytes);
  str = VARDATA(result);
  str[nbytes] = '\0';

  parse_xml_decl((const xmlChar *)str, NULL, NULL, &encodingStr, NULL);

     
                                                                            
                                                                           
                                                                            
                 
     
  encoding = encodingStr ? xmlChar_to_encoding(encodingStr) : PG_UTF8;

     
                                                                         
                                        
     
  doc = xml_parse(result, xmloption, true, encoding);
  xmlFreeDoc(doc);

                                                                            
  newstr = pg_any_to_server(str, nbytes, encoding);

  if (newstr != str)
  {
    pfree(result);
    result = (xmltype *)cstring_to_text(newstr);
    pfree(newstr);
  }

  PG_RETURN_XML_P(result);
#else
  NO_XML_SUPPORT();
  return 0;
#endif
}

Datum
xml_send(PG_FUNCTION_ARGS)
{
  xmltype *x = PG_GETARG_XML_P(0);
  char *outval;
  StringInfoData buf;

     
                                                                             
                                                      
     
  outval = xml_out_internal(x, pg_get_client_encoding());

  pq_begintypsend(&buf);
  pq_sendtext(&buf, outval, strlen(outval));
  pfree(outval);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

#ifdef USE_LIBXML
static void
appendStringInfoText(StringInfo str, const text *t)
{
  appendBinaryStringInfo(str, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
}
#endif

static xmltype *
stringinfo_to_xmltype(StringInfo buf)
{
  return (xmltype *)cstring_to_text_with_len(buf->data, buf->len);
}

static xmltype *
cstring_to_xmltype(const char *string)
{
  return (xmltype *)cstring_to_text(string);
}

#ifdef USE_LIBXML
static xmltype *
xmlBuffer_to_xmltype(xmlBufferPtr buf)
{
  return (xmltype *)cstring_to_text_with_len((const char *)xmlBufferContent(buf), xmlBufferLength(buf));
}
#endif

Datum
xmlcomment(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *arg = PG_GETARG_TEXT_PP(0);
  char *argdata = VARDATA_ANY(arg);
  int len = VARSIZE_ANY_EXHDR(arg);
  StringInfoData buf;
  int i;

                                                  
  for (i = 1; i < len; i++)
  {
    if (argdata[i] == '-' && argdata[i - 1] == '-')
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_XML_COMMENT), errmsg("invalid XML comment")));
    }
  }
  if (len > 0 && argdata[len - 1] == '-')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_XML_COMMENT), errmsg("invalid XML comment")));
  }

  initStringInfo(&buf);
  appendStringInfoString(&buf, "<!--");
  appendStringInfoText(&buf, arg);
  appendStringInfoString(&buf, "-->");

  PG_RETURN_XML_P(stringinfo_to_xmltype(&buf));
#else
  NO_XML_SUPPORT();
  return 0;
#endif
}

   
                                                                      
                                                                    
   
xmltype *
xmlconcat(List *args)
{
#ifdef USE_LIBXML
  int global_standalone = 1;
  xmlChar *global_version = NULL;
  bool global_version_no_value = false;
  StringInfoData buf;
  ListCell *v;

  initStringInfo(&buf);
  foreach (v, args)
  {
    xmltype *x = DatumGetXmlP(PointerGetDatum(lfirst(v)));
    size_t len;
    xmlChar *version;
    int standalone;
    char *str;

    len = VARSIZE(x) - VARHDRSZ;
    str = text_to_cstring((text *)x);

    parse_xml_decl((xmlChar *)str, &len, &version, NULL, &standalone);

    if (standalone == 0 && global_standalone == 1)
    {
      global_standalone = 0;
    }
    if (standalone < 0)
    {
      global_standalone = -1;
    }

    if (!version)
    {
      global_version_no_value = true;
    }
    else if (!global_version)
    {
      global_version = version;
    }
    else if (xmlStrcmp(version, global_version) != 0)
    {
      global_version_no_value = true;
    }

    appendStringInfoString(&buf, str + len);
    pfree(str);
  }

  if (!global_version_no_value || global_standalone >= 0)
  {
    StringInfoData buf2;

    initStringInfo(&buf2);

    print_xml_decl(&buf2, (!global_version_no_value) ? global_version : NULL, 0, global_standalone);

    appendStringInfoString(&buf2, buf.data);
    buf = buf2;
  }

  return stringinfo_to_xmltype(&buf);
#else
  NO_XML_SUPPORT();
  return NULL;
#endif
}

   
                  
   
Datum
xmlconcat2(PG_FUNCTION_ARGS)
{
  if (PG_ARGISNULL(0))
  {
    if (PG_ARGISNULL(1))
    {
      PG_RETURN_NULL();
    }
    else
    {
      PG_RETURN_XML_P(PG_GETARG_XML_P(1));
    }
  }
  else if (PG_ARGISNULL(1))
  {
    PG_RETURN_XML_P(PG_GETARG_XML_P(0));
  }
  else
  {
    PG_RETURN_XML_P(xmlconcat(list_make2(PG_GETARG_XML_P(0), PG_GETARG_XML_P(1))));
  }
}

Datum
texttoxml(PG_FUNCTION_ARGS)
{
  text *data = PG_GETARG_TEXT_PP(0);

  PG_RETURN_XML_P(xmlparse(data, xmloption, true));
}

Datum
xmltotext(PG_FUNCTION_ARGS)
{
  xmltype *data = PG_GETARG_XML_P(0);

                                        
  PG_RETURN_TEXT_P((text *)data);
}

text *
xmltotext_with_xmloption(xmltype *data, XmlOptionType xmloption_arg)
{
  if (xmloption_arg == XMLOPTION_DOCUMENT && !xml_is_document(data))
  {
    ereport(ERROR, (errcode(ERRCODE_NOT_AN_XML_DOCUMENT), errmsg("not an XML document")));
  }

                                                                  
  return (text *)data;
}

xmltype *
xmlelement(XmlExpr *xexpr, Datum *named_argvalue, bool *named_argnull, Datum *argvalue, bool *argnull)
{
#ifdef USE_LIBXML
  xmltype *result;
  List *named_arg_strings;
  List *arg_strings;
  int i;
  ListCell *arg;
  ListCell *narg;
  PgXmlErrorContext *xmlerrcxt;
  volatile xmlBufferPtr buf = NULL;
  volatile xmlTextWriterPtr writer = NULL;

     
                                                                             
                                                                           
                                                                           
                                                                             
                                                                          
     
  named_arg_strings = NIL;
  i = 0;
  foreach (arg, xexpr->named_args)
  {
    Expr *e = (Expr *)lfirst(arg);
    char *str;

    if (named_argnull[i])
    {
      str = NULL;
    }
    else
    {
      str = map_sql_value_to_xml_value(named_argvalue[i], exprType((Node *)e), false);
    }
    named_arg_strings = lappend(named_arg_strings, str);
    i++;
  }

  arg_strings = NIL;
  i = 0;
  foreach (arg, xexpr->args)
  {
    Expr *e = (Expr *)lfirst(arg);
    char *str;

                                                           
    if (!argnull[i])
    {
      str = map_sql_value_to_xml_value(argvalue[i], exprType((Node *)e), true);
      arg_strings = lappend(arg_strings, str);
    }
    i++;
  }

  xmlerrcxt = pg_xml_init(PG_XML_STRICTNESS_ALL);

  PG_TRY();
  {
    buf = xmlBufferCreate();
    if (buf == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate xmlBuffer");
    }
    writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate xmlTextWriter");
    }

    xmlTextWriterStartElement(writer, (xmlChar *)xexpr->name);

    forboth(arg, named_arg_strings, narg, xexpr->arg_names)
    {
      char *str = (char *)lfirst(arg);
      char *argname = strVal(lfirst(narg));

      if (str)
      {
        xmlTextWriterWriteAttribute(writer, (xmlChar *)argname, (xmlChar *)str);
      }
    }

    foreach (arg, arg_strings)
    {
      char *str = (char *)lfirst(arg);

      xmlTextWriterWriteRaw(writer, (xmlChar *)str);
    }

    xmlTextWriterEndElement(writer);

                                                                 
    xmlFreeTextWriter(writer);
    writer = NULL;

    result = xmlBuffer_to_xmltype(buf);
  }
  PG_CATCH();
  {
    if (writer)
    {
      xmlFreeTextWriter(writer);
    }
    if (buf)
    {
      xmlBufferFree(buf);
    }

    pg_xml_done(xmlerrcxt, true);

    PG_RE_THROW();
  }
  PG_END_TRY();

  xmlBufferFree(buf);

  pg_xml_done(xmlerrcxt, false);

  return result;
#else
  NO_XML_SUPPORT();
  return NULL;
#endif
}

xmltype *
xmlparse(text *data, XmlOptionType xmloption_arg, bool preserve_whitespace)
{
#ifdef USE_LIBXML
  xmlDocPtr doc;

  doc = xml_parse(data, xmloption_arg, preserve_whitespace, GetDatabaseEncoding());
  xmlFreeDoc(doc);

  return (xmltype *)data;
#else
  NO_XML_SUPPORT();
  return NULL;
#endif
}

xmltype *
xmlpi(const char *target, text *arg, bool arg_is_null, bool *result_is_null)
{
#ifdef USE_LIBXML
  xmltype *result;
  StringInfoData buf;

  if (pg_strcasecmp(target, "xml") == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),             
                       errmsg("invalid XML processing instruction"), errdetail("XML processing instruction target name cannot be \"%s\".", target)));
  }

     
                                                                             
            
     
  *result_is_null = arg_is_null;
  if (*result_is_null)
  {
    return NULL;
  }

  initStringInfo(&buf);

  appendStringInfo(&buf, "<?%s", target);

  if (arg != NULL)
  {
    char *string;

    string = text_to_cstring(arg);
    if (strstr(string, "?>") != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_XML_PROCESSING_INSTRUCTION), errmsg("invalid XML processing instruction"), errdetail("XML processing instruction cannot contain \"?>\".")));
    }

    appendStringInfoChar(&buf, ' ');
    appendStringInfoString(&buf, string + strspn(string, " "));
    pfree(string);
  }
  appendStringInfoString(&buf, "?>");

  result = stringinfo_to_xmltype(&buf);
  pfree(buf.data);
  return result;
#else
  NO_XML_SUPPORT();
  return NULL;
#endif
}

xmltype *
xmlroot(xmltype *data, text *version, int standalone)
{
#ifdef USE_LIBXML
  char *str;
  size_t len;
  xmlChar *orig_version;
  int orig_standalone;
  StringInfoData buf;

  len = VARSIZE(data) - VARHDRSZ;
  str = text_to_cstring((text *)data);

  parse_xml_decl((xmlChar *)str, &len, &orig_version, NULL, &orig_standalone);

  if (version)
  {
    orig_version = xml_text2xmlChar(version);
  }
  else
  {
    orig_version = NULL;
  }

  switch (standalone)
  {
  case XML_STANDALONE_YES:
    orig_standalone = 1;
    break;
  case XML_STANDALONE_NO:
    orig_standalone = 0;
    break;
  case XML_STANDALONE_NO_VALUE:
    orig_standalone = -1;
    break;
  case XML_STANDALONE_OMITTED:
                              
    break;
  }

  initStringInfo(&buf);
  print_xml_decl(&buf, orig_version, 0, orig_standalone);
  appendStringInfoString(&buf, str + len);

  return stringinfo_to_xmltype(&buf);
#else
  NO_XML_SUPPORT();
  return NULL;
#endif
}

   
                                                                            
   
                                                                           
                                                                         
                                                                            
              
   
Datum
xmlvalidate(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("xmlvalidate is not implemented")));
  return 0;
}

bool
xml_is_document(xmltype *arg)
{
#ifdef USE_LIBXML
  bool result;
  volatile xmlDocPtr doc = NULL;
  MemoryContext ccxt = CurrentMemoryContext;

                                                                       
  PG_TRY();
  {
    doc = xml_parse((text *)arg, XMLOPTION_DOCUMENT, true, GetDatabaseEncoding());
    result = true;
  }
  PG_CATCH();
  {
    ErrorData *errdata;
    MemoryContext ecxt;

    ecxt = MemoryContextSwitchTo(ccxt);
    errdata = CopyErrorData();
    if (errdata->sqlerrcode == ERRCODE_INVALID_XML_DOCUMENT)
    {
      FlushErrorState();
      result = false;
    }
    else
    {
      MemoryContextSwitchTo(ecxt);
      PG_RE_THROW();
    }
  }
  PG_END_TRY();

  if (doc)
  {
    xmlFreeDoc(doc);
  }

  return result;
#else                      
  NO_XML_SUPPORT();
  return false;
#endif                     
}

#ifdef USE_LIBXML

   
                                                    
   
                                                                      
                                                                         
                                                                         
                                
   
                                                                             
          
   
void
pg_xml_init_library(void)
{
  static bool first_time = true;

  if (first_time)
  {
                                                

       
                                                                          
                    
       
    if (sizeof(char) != sizeof(xmlChar))
    {
      ereport(ERROR, (errmsg("could not initialize XML library"), errdetail("libxml2 has incompatible char type: sizeof(char)=%u, sizeof(xmlChar)=%u.", (int)sizeof(char), (int)sizeof(xmlChar))));
    }

#ifdef USE_LIBXMLCONTEXT
                                                   
    xml_memory_init();
#endif

                                     
    LIBXML_TEST_VERSION;

    first_time = false;
  }
}

   
                                                                          
   
                                                                      
                                                                       
                                                                   
   
                                                                          
   
                                                                             
                                                                    
   
                                                                              
                                                            
   
PgXmlErrorContext *
pg_xml_init(PgXmlStrictness strictness)
{
  PgXmlErrorContext *errcxt;
  void *new_errcxt;

                                   
  pg_xml_init_library();

                                               
  errcxt = (PgXmlErrorContext *)palloc(sizeof(PgXmlErrorContext));
  errcxt->magic = ERRCXT_MAGIC;
  errcxt->strictness = strictness;
  errcxt->err_occurred = false;
  initStringInfo(&errcxt->err_buf);

     
                                                                            
                                                                           
                                                                            
                                                                       
              
     
  errcxt->saved_errfunc = xmlStructuredError;

#ifdef HAVE_XMLSTRUCTUREDERRORCONTEXT
  errcxt->saved_errcxt = xmlStructuredErrorContext;
#else
  errcxt->saved_errcxt = xmlGenericErrorContext;
#endif

  xmlSetStructuredErrorFunc((void *)errcxt, xml_errorHandler);

     
                                                                       
                                                                             
                                                                             
                                                       
     
                                                                             
                                                                            
                                                                           
                                                                           
                                                                          
                                    
     

#ifdef HAVE_XMLSTRUCTUREDERRORCONTEXT
  new_errcxt = xmlStructuredErrorContext;
#else
  new_errcxt = xmlGenericErrorContext;
#endif

  if (new_errcxt != (void *)errcxt)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("could not set up XML error handler"),
                       errhint("This probably indicates that the version of libxml2"
                               " being used is not compatible with the libxml2"
                               " header files that PostgreSQL was built with.")));
  }

     
                                                                            
                     
     
  errcxt->saved_entityfunc = xmlGetExternalEntityLoader();
  xmlSetExternalEntityLoader(xmlPgEntityLoader);

  return errcxt;
}

   
                                                          
   
                                                                     
                             
   
                                                                      
                                       
   
void
pg_xml_done(PgXmlErrorContext *errcxt, bool isError)
{
  void *cur_errcxt;

                                                   
  Assert(errcxt->magic == ERRCXT_MAGIC);

     
                                                                            
                                                                            
                                                                    
     
  Assert(!errcxt->err_occurred || isError);

     
                                                                          
                                                                        
                
     
#ifdef HAVE_XMLSTRUCTUREDERRORCONTEXT
  cur_errcxt = xmlStructuredErrorContext;
#else
  cur_errcxt = xmlGenericErrorContext;
#endif

  if (cur_errcxt != (void *)errcxt)
  {
    elog(WARNING, "libxml error handling state is out of sync with xml.c");
  }

                                  
  xmlSetStructuredErrorFunc(errcxt->saved_errcxt, errcxt->saved_errfunc);
  xmlSetExternalEntityLoader(errcxt->saved_entityfunc);

     
                                                                          
                                                   
     
  errcxt->magic = 0;

                      
  pfree(errcxt->err_buf.data);
  pfree(errcxt);
}

   
                                                   
   
bool
pg_xml_error_occurred(PgXmlErrorContext *errcxt)
{
  return errcxt->err_occurred;
}

   
                                                                  
                                                                    
                                                                   
                                                                 
                                                                     
                     
   

#define CHECK_XML_SPACE(p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!xmlIsBlank_ch(*(p)))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      return XML_ERR_SPACE_REQUIRED;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

#define SKIP_XML_SPACE(p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  while (xmlIsBlank_ch(*(p)))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  (p)++

                                                                       
                                                 
#define PG_XMLISNAMECHAR(c) (xmlIsBaseChar_ch(c) || xmlIsIdeographicQ(c) || xmlIsDigit_ch(c) || c == '.' || c == '-' || c == '_' || c == ':' || xmlIsCombiningQ(c) || xmlIsExtender_ch(c))

                                                                           
static xmlChar *
xml_pnstrdup(const xmlChar *str, size_t len)
{
  xmlChar *result;

  result = (xmlChar *)palloc((len + 1) * sizeof(xmlChar));
  memcpy(result, str, len * sizeof(xmlChar));
  result[len] = 0;
  return result;
}

                                  
static xmlChar *
pg_xmlCharStrndup(const char *str, size_t len)
{
  xmlChar *result;

  result = (xmlChar *)palloc((len + 1) * sizeof(xmlChar));
  memcpy(result, str, len);
  result[len] = '\0';

  return result;
}

   
                                                                      
   
                                                                 
   
static char *
xml_pstrdup_and_free(xmlChar *str)
{
  char *result;

  if (str)
  {
    PG_TRY();
    {
      result = pstrdup((char *)str);
    }
    PG_CATCH();
    {
      xmlFree(str);
      PG_RE_THROW();
    }
    PG_END_TRY();
    xmlFree(str);
  }
  else
  {
    result = NULL;
  }

  return result;
}

   
                                                                     
                                                              
                                                                  
                                            
   
static int
parse_xml_decl(const xmlChar *str, size_t *lenp, xmlChar **version, xmlChar **encoding, int *standalone)
{
  const xmlChar *p;
  const xmlChar *save_p;
  size_t len;
  int utf8char;
  int utf8len;

     
                                                                           
                                                                       
                                                                            
                         
     
  pg_xml_init_library();

                                                    
  if (version)
  {
    *version = NULL;
  }
  if (encoding)
  {
    *encoding = NULL;
  }
  if (standalone)
  {
    *standalone = -1;
  }

  p = str;

  if (xmlStrncmp(p, (xmlChar *)"<?xml", 5) != 0)
  {
    goto finished;
  }

     
                                                                        
                                                                             
              
     
                                                                           
                                                                  
     
  utf8len = strnlen((const char *)(p + 5), MAX_MULTIBYTE_CHAR_LEN);
  utf8char = xmlGetUTF8Char(p + 5, &utf8len);
  if (PG_XMLISNAMECHAR(utf8char))
  {
    goto finished;
  }

  p += 5;

               
  CHECK_XML_SPACE(p);
  SKIP_XML_SPACE(p);
  if (xmlStrncmp(p, (xmlChar *)"version", 7) != 0)
  {
    return XML_ERR_VERSION_MISSING;
  }
  p += 7;
  SKIP_XML_SPACE(p);
  if (*p != '=')
  {
    return XML_ERR_VERSION_MISSING;
  }
  p += 1;
  SKIP_XML_SPACE(p);

  if (*p == '\'' || *p == '"')
  {
    const xmlChar *q;

    q = xmlStrchr(p + 1, *p);
    if (!q)
    {
      return XML_ERR_VERSION_MISSING;
    }

    if (version)
    {
      *version = xml_pnstrdup(p + 1, q - p - 1);
    }
    p = q + 1;
  }
  else
  {
    return XML_ERR_VERSION_MISSING;
  }

                
  save_p = p;
  SKIP_XML_SPACE(p);
  if (xmlStrncmp(p, (xmlChar *)"encoding", 8) == 0)
  {
    CHECK_XML_SPACE(save_p);
    p += 8;
    SKIP_XML_SPACE(p);
    if (*p != '=')
    {
      return XML_ERR_MISSING_ENCODING;
    }
    p += 1;
    SKIP_XML_SPACE(p);

    if (*p == '\'' || *p == '"')
    {
      const xmlChar *q;

      q = xmlStrchr(p + 1, *p);
      if (!q)
      {
        return XML_ERR_MISSING_ENCODING;
      }

      if (encoding)
      {
        *encoding = xml_pnstrdup(p + 1, q - p - 1);
      }
      p = q + 1;
    }
    else
    {
      return XML_ERR_MISSING_ENCODING;
    }
  }
  else
  {
    p = save_p;
  }

                  
  save_p = p;
  SKIP_XML_SPACE(p);
  if (xmlStrncmp(p, (xmlChar *)"standalone", 10) == 0)
  {
    CHECK_XML_SPACE(save_p);
    p += 10;
    SKIP_XML_SPACE(p);
    if (*p != '=')
    {
      return XML_ERR_STANDALONE_VALUE;
    }
    p += 1;
    SKIP_XML_SPACE(p);
    if (xmlStrncmp(p, (xmlChar *)"'yes'", 5) == 0 || xmlStrncmp(p, (xmlChar *)"\"yes\"", 5) == 0)
    {
      if (standalone)
      {
        *standalone = 1;
      }
      p += 5;
    }
    else if (xmlStrncmp(p, (xmlChar *)"'no'", 4) == 0 || xmlStrncmp(p, (xmlChar *)"\"no\"", 4) == 0)
    {
      if (standalone)
      {
        *standalone = 0;
      }
      p += 4;
    }
    else
    {
      return XML_ERR_STANDALONE_VALUE;
    }
  }
  else
  {
    p = save_p;
  }

  SKIP_XML_SPACE(p);
  if (xmlStrncmp(p, (xmlChar *)"?>", 2) != 0)
  {
    return XML_ERR_XMLDECL_NOT_FINISHED;
  }
  p += 2;

finished:
  len = p - str;

  for (p = str; p < str + len; p++)
  {
    if (*p > 127)
    {
      return XML_ERR_INVALID_CHAR;
    }
  }

  if (lenp)
  {
    *lenp = len;
  }

  return XML_ERR_OK;
}

   
                                                                       
                                                                    
                                                         
   
                                                                       
                                                                       
                                                                      
                                                                      
                                                           
                                                               
                                                                     
                                                       
   
static bool
print_xml_decl(StringInfo buf, const xmlChar *version, pg_enc encoding, int standalone)
{
  if ((version && strcmp((const char *)version, PG_XML_DEFAULT_VERSION) != 0) || (encoding && encoding != PG_UTF8) || standalone != -1)
  {
    appendStringInfoString(buf, "<?xml");

    if (version)
    {
      appendStringInfo(buf, " version=\"%s\"", version);
    }
    else
    {
      appendStringInfo(buf, " version=\"%s\"", PG_XML_DEFAULT_VERSION);
    }

    if (encoding && encoding != PG_UTF8)
    {
         
                                                                       
                                                         
         
      appendStringInfo(buf, " encoding=\"%s\"", pg_encoding_to_char(encoding));
    }

    if (standalone == 1)
    {
      appendStringInfoString(buf, " standalone=\"yes\"");
    }
    else if (standalone == 0)
    {
      appendStringInfoString(buf, " standalone=\"no\"");
    }
    appendStringInfoString(buf, "?>");

    return true;
  }
  else
  {
    return false;
  }
}

   
                                                                         
   
                                                                      
                                                                             
                                                                            
                                                                     
                                                                           
                                                                           
                                                                              
                
   
                                                                             
                                                                            
                                                                             
                                                                        
   
                                                                              
                                                                            
                                                                         
                                                                               
                                                                           
                                                                         
                                                                             
                                                                    
   
                                                                          
                                                        
   
static bool
xml_doctype_in_content(const xmlChar *str)
{
  const xmlChar *p = str;

  for (;;)
  {
    const xmlChar *e;

    SKIP_XML_SPACE(p);
    if (*p != '<')
    {
      return false;
    }
    p++;

    if (*p == '!')
    {
      p++;

                                                   
      if (xmlStrncmp(p, (xmlChar *)"DOCTYPE", 7) == 0)
      {
        return true;
      }

                                                  
      if (xmlStrncmp(p, (xmlChar *)"--", 2) != 0)
      {
        return false;
      }
                                                            
      p = xmlStrstr(p + 2, (xmlChar *)"--");
      if (!p || p[2] != '>')
      {
        return false;
      }
                                                   
      p += 3;
      continue;
    }

                                                                
    if (*p != '?')
    {
      return false;
    }
    p++;

                                                                 
    e = xmlStrstr(p, (xmlChar *)"?>");
    if (!e)
    {
      return false;
    }

                                        
    p = e + 2;
  }
}

   
                                                     
   
                                                                   
                                            
   
                                                                    
                                         
   
static xmlDocPtr
xml_parse(text *data, XmlOptionType xmloption_arg, bool preserve_whitespace, int encoding)
{
  int32 len;
  xmlChar *string;
  xmlChar *utf8string;
  PgXmlErrorContext *xmlerrcxt;
  volatile xmlParserCtxtPtr ctxt = NULL;
  volatile xmlDocPtr doc = NULL;

  len = VARSIZE_ANY_EXHDR(data);                           
  string = xml_text2xmlChar(data);

  utf8string = pg_do_encoding_conversion(string, len, encoding, PG_UTF8);

                                      
  xmlerrcxt = pg_xml_init(PG_XML_STRICTNESS_WELLFORMED);

                                                       
  PG_TRY();
  {
    bool parse_as_document = false;
    int res_code;
    size_t count = 0;
    xmlChar *version = NULL;
    int standalone = 0;

    xmlInitParser();

    ctxt = xmlNewParserCtxt();
    if (ctxt == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate parser context");
    }

                                                        
    if (xmloption_arg == XMLOPTION_DOCUMENT)
    {
      parse_as_document = true;
    }
    else
    {
                                                           
      res_code = parse_xml_decl(utf8string, &count, &version, NULL, &standalone);
      if (res_code != 0)
      {
        xml_ereport_by_code(ERROR, ERRCODE_INVALID_XML_CONTENT, "invalid XML content: invalid XML declaration", res_code);
      }

                                       
      if (xml_doctype_in_content(utf8string + count))
      {
        parse_as_document = true;
      }
    }

    if (parse_as_document)
    {
         
                                                      
                                                                     
                                                                      
                                                                         
                    
         
      doc = xmlCtxtReadDoc(ctxt, utf8string, NULL, "UTF-8", XML_PARSE_NOENT | XML_PARSE_DTDATTR | (preserve_whitespace ? 0 : XML_PARSE_NOBLANKS));
      if (doc == NULL || xmlerrcxt->err_occurred)
      {
                                                                     
        if (xmloption_arg == XMLOPTION_DOCUMENT)
        {
          xml_ereport(xmlerrcxt, ERROR, ERRCODE_INVALID_XML_DOCUMENT, "invalid XML document");
        }
        else
        {
          xml_ereport(xmlerrcxt, ERROR, ERRCODE_INVALID_XML_CONTENT, "invalid XML content");
        }
      }
    }
    else
    {
      doc = xmlNewDoc(version);
      Assert(doc->encoding == NULL);
      doc->encoding = xmlStrdup((const xmlChar *)"UTF-8");
      doc->standalone = standalone;

                               
      if (*(utf8string + count))
      {
        res_code = xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, utf8string + count, NULL);
        if (res_code != 0 || xmlerrcxt->err_occurred)
        {
          xml_ereport(xmlerrcxt, ERROR, ERRCODE_INVALID_XML_CONTENT, "invalid XML content");
        }
      }
    }
  }
  PG_CATCH();
  {
    if (doc != NULL)
    {
      xmlFreeDoc(doc);
    }
    if (ctxt != NULL)
    {
      xmlFreeParserCtxt(ctxt);
    }

    pg_xml_done(xmlerrcxt, true);

    PG_RE_THROW();
  }
  PG_END_TRY();

  xmlFreeParserCtxt(ctxt);

  pg_xml_done(xmlerrcxt, false);

  return doc;
}

   
                              
   
static xmlChar *
xml_text2xmlChar(text *in)
{
  return (xmlChar *)text_to_cstring(in);
}

#ifdef USE_LIBXMLCONTEXT

   
                                                                        
                                                      
   
static void
xml_memory_init(void)
{
                                                  
  if (LibxmlContext == NULL)
  {
    LibxmlContext = AllocSetContextCreate(TopMemoryContext, "Libxml context", ALLOCSET_DEFAULT_SIZES);
  }

                                                      
  xmlMemSetup(xml_pfree, xml_palloc, xml_repalloc, xml_pstrdup);
}

   
                                            
   
static void *
xml_palloc(size_t size)
{
  return MemoryContextAlloc(LibxmlContext, size);
}

static void *
xml_repalloc(void *ptr, size_t size)
{
  return repalloc(ptr, size);
}

static void
xml_pfree(void *ptr)
{
                                                                     
  if (ptr)
  {
    pfree(ptr);
  }
}

static char *
xml_pstrdup(const char *string)
{
  return MemoryContextStrdup(LibxmlContext, string);
}
#endif                        

   
                                                         
   
                                                                              
                                                                              
           
   
                                                                        
                                                                          
                                                                   
   
static xmlParserInputPtr
xmlPgEntityLoader(const char *URL, const char *ID, xmlParserCtxtPtr ctxt)
{
  return xmlNewStringInputStream(ctxt, (const xmlChar *)"");
}

   
                                               
   
                                                                            
                                                                           
           
   
                                                                         
                                                                         
   
void
xml_ereport(PgXmlErrorContext *errcxt, int level, int sqlcode, const char *msg)
{
  char *detail;

                                                                
  if (errcxt->magic != ERRCXT_MAGIC)
  {
    elog(ERROR, "xml_ereport called with invalid PgXmlErrorContext");
  }

                                                            
  errcxt->err_occurred = false;

                                                            
  if (errcxt->err_buf.len > 0)
  {
    detail = errcxt->err_buf.data;
  }
  else
  {
    detail = NULL;
  }

  ereport(level, (errcode(sqlcode), errmsg_internal("%s", msg), detail ? errdetail_internal("%s", detail) : 0));
}

   
                                                
   
static void
xml_errorHandler(void *data, xmlErrorPtr error)
{
  PgXmlErrorContext *xmlerrcxt = (PgXmlErrorContext *)data;
  xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)error->ctxt;
  xmlParserInputPtr input = (ctxt != NULL) ? ctxt->input : NULL;
  xmlNodePtr node = error->node;
  const xmlChar *name = (node != NULL && node->type == XML_ELEMENT_NODE) ? node->name : NULL;
  int domain = error->domain;
  int level = error->level;
  StringInfo errorBuf;

     
                                                               
     
                                                                            
                                                          
     
  if (xmlerrcxt->magic != ERRCXT_MAGIC)
  {
    elog(FATAL, "xml_errorHandler called with invalid PgXmlErrorContext");
  }

               
                                                           
                                                                           
                                                                      
                                                    
                                             
               
     
  switch (error->code)
  {
  case XML_WAR_NS_URI:
    level = XML_ERR_ERROR;
    domain = XML_FROM_NAMESPACE;
    break;

  case XML_ERR_NS_DECL_ERROR:
  case XML_WAR_NS_URI_RELATIVE:
  case XML_WAR_NS_COLUMN:
  case XML_NS_ERR_XML_NAMESPACE:
  case XML_NS_ERR_UNDEFINED_NAMESPACE:
  case XML_NS_ERR_QNAME:
  case XML_NS_ERR_ATTRIBUTE_REDEFINED:
  case XML_NS_ERR_EMPTY:
    domain = XML_FROM_NAMESPACE;
    break;
  }

                                                 
  switch (domain)
  {
  case XML_FROM_PARSER:
  case XML_FROM_NONE:
  case XML_FROM_MEMORY:
  case XML_FROM_IO:

       
                                                                   
                                                                  
       
    if (error->code == XML_WAR_UNDECLARED_ENTITY)
    {
      return;
    }

                                                                   
    break;

  default:
                                                          
    if (xmlerrcxt->strictness == PG_XML_STRICTNESS_WELLFORMED)
    {
      return;
    }
    break;
  }

                                         
  errorBuf = makeStringInfo();

  if (error->line > 0)
  {
    appendStringInfo(errorBuf, "line %d: ", error->line);
  }
  if (name != NULL)
  {
    appendStringInfo(errorBuf, "element %s: ", name);
  }
  if (error->message != NULL)
  {
    appendStringInfoString(errorBuf, error->message);
  }
  else
  {
    appendStringInfoString(errorBuf, "(no message provided)");
  }

     
                                             
     
                                                                          
                                                                 
                                                                        
     
                                                                           
                                                                       
                                                               
     
  if (input != NULL)
  {
    xmlGenericErrorFunc errFuncSaved = xmlGenericError;
    void *errCtxSaved = xmlGenericErrorContext;

    xmlSetGenericErrorFunc((void *)errorBuf, (xmlGenericErrorFunc)appendStringInfo);

                                             
    appendStringInfoLineSeparator(errorBuf);

    xmlParserPrintFileContext(input);

                                    
    xmlSetGenericErrorFunc(errCtxSaved, errFuncSaved);
  }

                                                    
  chopStringInfoNewlines(errorBuf);

     
                                                                             
                                                                           
                                                                             
                                                                        
                                                                             
                                                           
     
  if (xmlerrcxt->strictness == PG_XML_STRICTNESS_LEGACY)
  {
    appendStringInfoLineSeparator(&xmlerrcxt->err_buf);
    appendStringInfoString(&xmlerrcxt->err_buf, errorBuf->data);

    pfree(errorBuf->data);
    pfree(errorBuf);
    return;
  }

     
                                                                             
                                                                          
                         
     
                                                                             
                                
     
  if (level >= XML_ERR_ERROR)
  {
    appendStringInfoLineSeparator(&xmlerrcxt->err_buf);
    appendStringInfoString(&xmlerrcxt->err_buf, errorBuf->data);

    xmlerrcxt->err_occurred = true;
  }
  else if (level >= XML_ERR_WARNING)
  {
    ereport(WARNING, (errmsg_internal("%s", errorBuf->data)));
  }
  else
  {
    ereport(NOTICE, (errmsg_internal("%s", errorBuf->data)));
  }

  pfree(errorBuf->data);
  pfree(errorBuf);
}

   
                                                                     
                                                                  
                                                                   
                                                                      
                           
   
static void
xml_ereport_by_code(int level, int sqlcode, const char *msg, int code)
{
  const char *det;

  switch (code)
  {
  case XML_ERR_INVALID_CHAR:
    det = gettext_noop("Invalid character value.");
    break;
  case XML_ERR_SPACE_REQUIRED:
    det = gettext_noop("Space required.");
    break;
  case XML_ERR_STANDALONE_VALUE:
    det = gettext_noop("standalone accepts only 'yes' or 'no'.");
    break;
  case XML_ERR_VERSION_MISSING:
    det = gettext_noop("Malformed declaration: missing version.");
    break;
  case XML_ERR_MISSING_ENCODING:
    det = gettext_noop("Missing encoding in text declaration.");
    break;
  case XML_ERR_XMLDECL_NOT_FINISHED:
    det = gettext_noop("Parsing XML declaration: '?>' expected.");
    break;
  default:
    det = gettext_noop("Unrecognized libxml error code: %d.");
    break;
  }

  ereport(level, (errcode(sqlcode), errmsg_internal("%s", msg), errdetail(det, code)));
}

   
                                                         
   
static void
chopStringInfoNewlines(StringInfo str)
{
  while (str->len > 0 && str->data[str->len - 1] == '\n')
  {
    str->data[--str->len] = '\0';
  }
}

   
                                                                  
   
static void
appendStringInfoLineSeparator(StringInfo str)
{
  chopStringInfoNewlines(str);
  if (str->len > 0)
  {
    appendStringInfoChar(str, '\n');
  }
}

   
                                                                           
   
static pg_wchar
sqlchar_to_unicode(const char *s)
{
  char *utf8string;
  pg_wchar ret[2];                                   

                                                    
  utf8string = pg_server_to_any(s, pg_mblen(s), PG_UTF8);

  pg_encoding_mb2wchar_with_len(PG_UTF8, utf8string, ret, pg_encoding_mblen(PG_UTF8, utf8string));

  if (utf8string != s)
  {
    pfree(utf8string);
  }

  return ret[0];
}

static bool
is_valid_xml_namefirst(pg_wchar c)
{
                            
  return (xmlIsBaseCharQ(c) || xmlIsIdeographicQ(c) || c == '_' || c == ':');
}

static bool
is_valid_xml_namechar(pg_wchar c)
{
                                                                         
  return (xmlIsBaseCharQ(c) || xmlIsIdeographicQ(c) || xmlIsDigitQ(c) || c == '.' || c == '-' || c == '_' || c == ':' || xmlIsCombiningQ(c) || xmlIsExtenderQ(c));
}
#endif                 

   
                                                                 
   
char *
map_sql_identifier_to_xml_name(const char *ident, bool fully_escaped, bool escape_period)
{
#ifdef USE_LIBXML
  StringInfoData buf;
  const char *p;

     
                                                                        
              
     
  Assert(fully_escaped || !escape_period);

  initStringInfo(&buf);

  for (p = ident; *p; p += pg_mblen(p))
  {
    if (*p == ':' && (p == ident || fully_escaped))
    {
      appendStringInfoString(&buf, "_x003A_");
    }
    else if (*p == '_' && *(p + 1) == 'x')
    {
      appendStringInfoString(&buf, "_x005F_");
    }
    else if (fully_escaped && p == ident && pg_strncasecmp(p, "xml", 3) == 0)
    {
      if (*p == 'x')
      {
        appendStringInfoString(&buf, "_x0078_");
      }
      else
      {
        appendStringInfoString(&buf, "_x0058_");
      }
    }
    else if (escape_period && *p == '.')
    {
      appendStringInfoString(&buf, "_x002E_");
    }
    else
    {
      pg_wchar u = sqlchar_to_unicode(p);

      if ((p == ident) ? !is_valid_xml_namefirst(u) : !is_valid_xml_namechar(u))
      {
        appendStringInfo(&buf, "_x%04X_", (unsigned int)u);
      }
      else
      {
        appendBinaryStringInfo(&buf, p, pg_mblen(p));
      }
    }
  }

  return buf.data;
#else                      
  NO_XML_SUPPORT();
  return NULL;
#endif                     
}

   
                                                             
   
static char *
unicode_to_sqlchar(pg_wchar c)
{
  char utf8string[8];                                  
  char *result;

  memset(utf8string, 0, sizeof(utf8string));
  unicode_to_utf8(c, (unsigned char *)utf8string);

  result = pg_any_to_server(utf8string, strlen(utf8string), PG_UTF8);
                                                  
  if (result == utf8string)
  {
    result = pstrdup(result);
  }
  return result;
}

   
                                                                 
   
char *
map_xml_name_to_sql_identifier(const char *name)
{
  StringInfoData buf;
  const char *p;

  initStringInfo(&buf);

  for (p = name; *p; p += pg_mblen(p))
  {
    if (*p == '_' && *(p + 1) == 'x' && isxdigit((unsigned char)*(p + 2)) && isxdigit((unsigned char)*(p + 3)) && isxdigit((unsigned char)*(p + 4)) && isxdigit((unsigned char)*(p + 5)) && *(p + 6) == '_')
    {
      unsigned int u;

      sscanf(p + 2, "%X", &u);
      appendStringInfoString(&buf, unicode_to_sqlchar(u));
      p += 6;
    }
    else
    {
      appendBinaryStringInfo(&buf, p, pg_mblen(p));
    }
  }

  return buf.data;
}

   
                                                             
   
                                                                      
                                                                      
                                                                         
                                                                     
                                                              
                                                              
   
char *
map_sql_value_to_xml_value(Datum value, Oid type, bool xml_escape_strings)
{
  if (type_is_array_domain(type))
  {
    ArrayType *array;
    Oid elmtype;
    int16 elmlen;
    bool elmbyval;
    char elmalign;
    int num_elems;
    Datum *elem_values;
    bool *elem_nulls;
    StringInfoData buf;
    int i;

    array = DatumGetArrayTypeP(value);
    elmtype = ARR_ELEMTYPE(array);
    get_typlenbyvalalign(elmtype, &elmlen, &elmbyval, &elmalign);

    deconstruct_array(array, elmtype, elmlen, elmbyval, elmalign, &elem_values, &elem_nulls, &num_elems);

    initStringInfo(&buf);

    for (i = 0; i < num_elems; i++)
    {
      if (elem_nulls[i])
      {
        continue;
      }
      appendStringInfoString(&buf, "<element>");
      appendStringInfoString(&buf, map_sql_value_to_xml_value(elem_values[i], elmtype, true));
      appendStringInfoString(&buf, "</element>");
    }

    pfree(elem_values);
    pfree(elem_nulls);

    return buf.data;
  }
  else
  {
    Oid typeOut;
    bool isvarlena;
    char *str;

       
                                                                           
                                                  
       
    type = getBaseType(type);

       
                                                  
       
    switch (type)
    {
    case BOOLOID:
      if (DatumGetBool(value))
      {
        return "true";
      }
      else
      {
        return "false";
      }

    case DATEOID:
    {
      DateADT date;
      struct pg_tm tm;
      char buf[MAXDATELEN + 1];

      date = DatumGetDateADT(value);
                                               
      if (DATE_NOT_FINITE(date))
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range"), errdetail("XML does not support infinite date values.")));
      }
      j2date(date + POSTGRES_EPOCH_JDATE, &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday));
      EncodeDateOnly(&tm, USE_XSD_DATES, buf);

      return pstrdup(buf);
    }

    case TIMESTAMPOID:
    {
      Timestamp timestamp;
      struct pg_tm tm;
      fsec_t fsec;
      char buf[MAXDATELEN + 1];

      timestamp = DatumGetTimestamp(value);

                                               
      if (TIMESTAMP_NOT_FINITE(timestamp))
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range"), errdetail("XML does not support infinite timestamp values.")));
      }
      else if (timestamp2tm(timestamp, NULL, &tm, &fsec, NULL, NULL) == 0)
      {
        EncodeDateTime(&tm, fsec, false, 0, NULL, USE_XSD_DATES, buf);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }

      return pstrdup(buf);
    }

    case TIMESTAMPTZOID:
    {
      TimestampTz timestamp;
      struct pg_tm tm;
      int tz;
      fsec_t fsec;
      const char *tzn = NULL;
      char buf[MAXDATELEN + 1];

      timestamp = DatumGetTimestamp(value);

                                               
      if (TIMESTAMP_NOT_FINITE(timestamp))
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range"), errdetail("XML does not support infinite timestamp values.")));
      }
      else if (timestamp2tm(timestamp, &tz, &tm, &fsec, &tzn, NULL) == 0)
      {
        EncodeDateTime(&tm, fsec, true, tz, tzn, USE_XSD_DATES, buf);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }

      return pstrdup(buf);
    }

#ifdef USE_LIBXML
    case BYTEAOID:
    {
      bytea *bstr = DatumGetByteaPP(value);
      PgXmlErrorContext *xmlerrcxt;
      volatile xmlBufferPtr buf = NULL;
      volatile xmlTextWriterPtr writer = NULL;
      char *result;

      xmlerrcxt = pg_xml_init(PG_XML_STRICTNESS_ALL);

      PG_TRY();
      {
        buf = xmlBufferCreate();
        if (buf == NULL || xmlerrcxt->err_occurred)
        {
          xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate xmlBuffer");
        }
        writer = xmlNewTextWriterMemory(buf, 0);
        if (writer == NULL || xmlerrcxt->err_occurred)
        {
          xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate xmlTextWriter");
        }

        if (xmlbinary == XMLBINARY_BASE64)
        {
          xmlTextWriterWriteBase64(writer, VARDATA_ANY(bstr), 0, VARSIZE_ANY_EXHDR(bstr));
        }
        else
        {
          xmlTextWriterWriteBinHex(writer, VARDATA_ANY(bstr), 0, VARSIZE_ANY_EXHDR(bstr));
        }

                                                                 
        xmlFreeTextWriter(writer);
        writer = NULL;

        result = pstrdup((const char *)xmlBufferContent(buf));
      }
      PG_CATCH();
      {
        if (writer)
        {
          xmlFreeTextWriter(writer);
        }
        if (buf)
        {
          xmlBufferFree(buf);
        }

        pg_xml_done(xmlerrcxt, true);

        PG_RE_THROW();
      }
      PG_END_TRY();

      xmlBufferFree(buf);

      pg_xml_done(xmlerrcxt, false);

      return result;
    }
#endif                 
    }

       
                                                                 
       
    getTypeOutputInfo(type, &typeOut, &isvarlena);
    str = OidOutputFunctionCall(typeOut, value);

                                                                    
    if (type == XMLOID || !xml_escape_strings)
    {
      return str;
    }

                                                           
    return escape_xml(str);
  }
}

   
                                                                
   
                              
   
                                                      
   
char *
escape_xml(const char *str)
{
  StringInfoData buf;
  const char *p;

  initStringInfo(&buf);
  for (p = str; *p; p++)
  {
    switch (*p)
    {
    case '&':
      appendStringInfoString(&buf, "&amp;");
      break;
    case '<':
      appendStringInfoString(&buf, "&lt;");
      break;
    case '>':
      appendStringInfoString(&buf, "&gt;");
      break;
    case '\r':
      appendStringInfoString(&buf, "&#x0d;");
      break;
    default:
      appendStringInfoCharMacro(&buf, *p);
      break;
    }
  }
  return buf.data;
}

static char *
_SPI_strdup(const char *s)
{
  size_t len = strlen(s) + 1;
  char *ret = SPI_palloc(len);

  memcpy(ret, s, len);
  return ret;
}

   
                                
   
                                                                       
                                                                 
                                                                
                                                                    
                                                            
                                                                       
                  
   
                                         
   
                                                                      
                                                                     
                                                                   
   
                                                                   
                                                                   
                                                                
                                                                   
                                                                     
                                                                       
                                                               
                   
   
                                                                  
                                          
   
                                                                       
                                                                    
                                                                    
                                                                    
                                                                     
   

   
                                                                    
           
   

   
                                                                      
                                          
   
static List *
query_to_oid_list(const char *query)
{
  uint64 i;
  List *list = NIL;

  SPI_execute(query, true, 0);

  for (i = 0; i < SPI_processed; i++)
  {
    Datum oid;
    bool isnull;

    oid = SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1, &isnull);
    if (!isnull)
    {
      list = lappend_oid(list, DatumGetObjectId(oid));
    }
  }

  return list;
}

static List *
schema_get_xml_visible_tables(Oid nspid)
{
  StringInfoData query;

  initStringInfo(&query);
  appendStringInfo(&query,
      "SELECT oid FROM pg_catalog.pg_class"
      " WHERE relnamespace = %u AND relkind IN (" CppAsString2(RELKIND_RELATION) "," CppAsString2(RELKIND_MATVIEW) "," CppAsString2(RELKIND_VIEW) ")"
                                                                                                                                                  " AND pg_catalog.has_table_privilege (oid, 'SELECT')"
                                                                                                                                                  " ORDER BY relname;",
      nspid);

  return query_to_oid_list(query.data);
}

   
                                                                      
            
   
#define XML_VISIBLE_SCHEMAS_EXCLUDE "(nspname ~ '^pg_' OR nspname = 'information_schema')"

#define XML_VISIBLE_SCHEMAS "SELECT oid FROM pg_catalog.pg_namespace WHERE pg_catalog.has_schema_privilege (oid, 'USAGE') AND NOT " XML_VISIBLE_SCHEMAS_EXCLUDE

static List *
database_get_xml_visible_schemas(void)
{
  return query_to_oid_list(XML_VISIBLE_SCHEMAS " ORDER BY nspname;");
}

static List *
database_get_xml_visible_tables(void)
{
                                                      
  return query_to_oid_list("SELECT oid FROM pg_catalog.pg_class"
                           " WHERE relkind IN (" CppAsString2(RELKIND_RELATION) "," CppAsString2(RELKIND_MATVIEW) "," CppAsString2(RELKIND_VIEW) ")"
                                                                                                                                                 " AND pg_catalog.has_table_privilege(pg_class.oid, 'SELECT')"
                                                                                                                                                 " AND relnamespace IN (" XML_VISIBLE_SCHEMAS ");");
}

   
                                                                     
                 
   

static StringInfo
table_to_xml_internal(Oid relid, const char *xmlschema, bool nulls, bool tableforest, const char *targetns, bool top_level)
{
  StringInfoData query;

  initStringInfo(&query);
  appendStringInfo(&query, "SELECT * FROM %s", DatumGetCString(DirectFunctionCall1(regclassout, ObjectIdGetDatum(relid))));
  return query_to_xml_internal(query.data, get_rel_name(relid), xmlschema, nulls, tableforest, targetns, top_level);
}

Datum
table_to_xml(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));

  PG_RETURN_XML_P(stringinfo_to_xmltype(table_to_xml_internal(relid, NULL, nulls, tableforest, targetns, true)));
}

Datum
query_to_xml(PG_FUNCTION_ARGS)
{
  char *query = text_to_cstring(PG_GETARG_TEXT_PP(0));
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));

  PG_RETURN_XML_P(stringinfo_to_xmltype(query_to_xml_internal(query, NULL, NULL, nulls, tableforest, targetns, true)));
}

Datum
cursor_to_xml(PG_FUNCTION_ARGS)
{
  char *name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  int32 count = PG_GETARG_INT32(1);
  bool nulls = PG_GETARG_BOOL(2);
  bool tableforest = PG_GETARG_BOOL(3);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(4));

  StringInfoData result;
  Portal portal;
  uint64 i;

  initStringInfo(&result);

  if (!tableforest)
  {
    xmldata_root_element_start(&result, "table", NULL, targetns, true);
    appendStringInfoChar(&result, '\n');
  }

  SPI_connect();
  portal = SPI_cursor_find(name);
  if (portal == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("cursor \"%s\" does not exist", name)));
  }

  SPI_cursor_fetch(portal, true, count);
  for (i = 0; i < SPI_processed; i++)
  {
    SPI_sql_row_to_xmlelement(i, &result, NULL, nulls, tableforest, targetns, true);
  }

  SPI_finish();

  if (!tableforest)
  {
    xmldata_root_element_end(&result, "table");
  }

  PG_RETURN_XML_P(stringinfo_to_xmltype(&result));
}

   
                                                              
   
                                                                   
                                                                       
                                                                       
                                                                 
                                                                       
                                                                       
                                                                     
                  
   
static void
xmldata_root_element_start(StringInfo result, const char *eltname, const char *xmlschema, const char *targetns, bool top_level)
{
                                                             
  Assert(top_level || !xmlschema);

  appendStringInfo(result, "<%s", eltname);
  if (top_level)
  {
    appendStringInfoString(result, " xmlns:xsi=\"" NAMESPACE_XSI "\"");
    if (strlen(targetns) > 0)
    {
      appendStringInfo(result, " xmlns=\"%s\"", targetns);
    }
  }
  if (xmlschema)
  {
                               
    if (strlen(targetns) > 0)
    {
      appendStringInfo(result, " xsi:schemaLocation=\"%s #\"", targetns);
    }
    else
    {
      appendStringInfoString(result, " xsi:noNamespaceSchemaLocation=\"#\"");
    }
  }
  appendStringInfoString(result, ">\n");
}

static void
xmldata_root_element_end(StringInfo result, const char *eltname)
{
  appendStringInfo(result, "</%s>\n", eltname);
}

static StringInfo
query_to_xml_internal(const char *query, char *tablename, const char *xmlschema, bool nulls, bool tableforest, const char *targetns, bool top_level)
{
  StringInfo result;
  char *xmltn;
  uint64 i;

  if (tablename)
  {
    xmltn = map_sql_identifier_to_xml_name(tablename, true, false);
  }
  else
  {
    xmltn = "table";
  }

  result = makeStringInfo();

  SPI_connect();
  if (SPI_execute(query, true, 0) != SPI_OK_SELECT)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("invalid query")));
  }

  if (!tableforest)
  {
    xmldata_root_element_start(result, xmltn, xmlschema, targetns, top_level);
    appendStringInfoChar(result, '\n');
  }

  if (xmlschema)
  {
    appendStringInfo(result, "%s\n\n", xmlschema);
  }

  for (i = 0; i < SPI_processed; i++)
  {
    SPI_sql_row_to_xmlelement(i, result, tablename, nulls, tableforest, targetns, top_level);
  }

  if (!tableforest)
  {
    xmldata_root_element_end(result, xmltn);
  }

  SPI_finish();

  return result;
}

Datum
table_to_xmlschema(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));
  const char *result;
  Relation rel;

  rel = table_open(relid, AccessShareLock);
  result = map_sql_table_to_xmlschema(rel->rd_att, relid, nulls, tableforest, targetns);
  table_close(rel, NoLock);

  PG_RETURN_XML_P(cstring_to_xmltype(result));
}

Datum
query_to_xmlschema(PG_FUNCTION_ARGS)
{
  char *query = text_to_cstring(PG_GETARG_TEXT_PP(0));
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));
  const char *result;
  SPIPlanPtr plan;
  Portal portal;

  SPI_connect();

  if ((plan = SPI_prepare(query, 0, NULL)) == NULL)
  {
    elog(ERROR, "SPI_prepare(\"%s\") failed", query);
  }

  if ((portal = SPI_cursor_open(NULL, plan, NULL, NULL, true)) == NULL)
  {
    elog(ERROR, "SPI_cursor_open(\"%s\") failed", query);
  }

  result = _SPI_strdup(map_sql_table_to_xmlschema(portal->tupDesc, InvalidOid, nulls, tableforest, targetns));
  SPI_cursor_close(portal);
  SPI_finish();

  PG_RETURN_XML_P(cstring_to_xmltype(result));
}

Datum
cursor_to_xmlschema(PG_FUNCTION_ARGS)
{
  char *name = text_to_cstring(PG_GETARG_TEXT_PP(0));
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));
  const char *xmlschema;
  Portal portal;

  SPI_connect();
  portal = SPI_cursor_find(name);
  if (portal == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("cursor \"%s\" does not exist", name)));
  }

  xmlschema = _SPI_strdup(map_sql_table_to_xmlschema(portal->tupDesc, InvalidOid, nulls, tableforest, targetns));
  SPI_finish();

  PG_RETURN_XML_P(cstring_to_xmltype(xmlschema));
}

Datum
table_to_xml_and_xmlschema(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));
  Relation rel;
  const char *xmlschema;

  rel = table_open(relid, AccessShareLock);
  xmlschema = map_sql_table_to_xmlschema(rel->rd_att, relid, nulls, tableforest, targetns);
  table_close(rel, NoLock);

  PG_RETURN_XML_P(stringinfo_to_xmltype(table_to_xml_internal(relid, xmlschema, nulls, tableforest, targetns, true)));
}

Datum
query_to_xml_and_xmlschema(PG_FUNCTION_ARGS)
{
  char *query = text_to_cstring(PG_GETARG_TEXT_PP(0));
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));

  const char *xmlschema;
  SPIPlanPtr plan;
  Portal portal;

  SPI_connect();

  if ((plan = SPI_prepare(query, 0, NULL)) == NULL)
  {
    elog(ERROR, "SPI_prepare(\"%s\") failed", query);
  }

  if ((portal = SPI_cursor_open(NULL, plan, NULL, NULL, true)) == NULL)
  {
    elog(ERROR, "SPI_cursor_open(\"%s\") failed", query);
  }

  xmlschema = _SPI_strdup(map_sql_table_to_xmlschema(portal->tupDesc, InvalidOid, nulls, tableforest, targetns));
  SPI_cursor_close(portal);
  SPI_finish();

  PG_RETURN_XML_P(stringinfo_to_xmltype(query_to_xml_internal(query, NULL, xmlschema, nulls, tableforest, targetns, true)));
}

   
                                                                      
                        
   

static StringInfo
schema_to_xml_internal(Oid nspid, const char *xmlschema, bool nulls, bool tableforest, const char *targetns, bool top_level)
{
  StringInfo result;
  char *xmlsn;
  List *relid_list;
  ListCell *cell;

  xmlsn = map_sql_identifier_to_xml_name(get_namespace_name(nspid), true, false);
  result = makeStringInfo();

  xmldata_root_element_start(result, xmlsn, xmlschema, targetns, top_level);
  appendStringInfoChar(result, '\n');

  if (xmlschema)
  {
    appendStringInfo(result, "%s\n\n", xmlschema);
  }

  SPI_connect();

  relid_list = schema_get_xml_visible_tables(nspid);

  foreach (cell, relid_list)
  {
    Oid relid = lfirst_oid(cell);
    StringInfo subres;

    subres = table_to_xml_internal(relid, NULL, nulls, tableforest, targetns, false);

    appendStringInfoString(result, subres->data);
    appendStringInfoChar(result, '\n');
  }

  SPI_finish();

  xmldata_root_element_end(result, xmlsn);

  return result;
}

Datum
schema_to_xml(PG_FUNCTION_ARGS)
{
  Name name = PG_GETARG_NAME(0);
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));

  char *schemaname;
  Oid nspid;

  schemaname = NameStr(*name);
  nspid = LookupExplicitNamespace(schemaname, false);

  PG_RETURN_XML_P(stringinfo_to_xmltype(schema_to_xml_internal(nspid, NULL, nulls, tableforest, targetns, true)));
}

   
                                                                         
   
static void
xsd_schema_element_start(StringInfo result, const char *targetns)
{
  appendStringInfoString(result, "<xsd:schema\n"
                                 "    xmlns:xsd=\"" NAMESPACE_XSD "\"");
  if (strlen(targetns) > 0)
  {
    appendStringInfo(result,
        "\n"
        "    targetNamespace=\"%s\"\n"
        "    elementFormDefault=\"qualified\"",
        targetns);
  }
  appendStringInfoString(result, ">\n\n");
}

static void
xsd_schema_element_end(StringInfo result)
{
  appendStringInfoString(result, "</xsd:schema>");
}

static StringInfo
schema_to_xmlschema_internal(const char *schemaname, bool nulls, bool tableforest, const char *targetns)
{
  Oid nspid;
  List *relid_list;
  List *tupdesc_list;
  ListCell *cell;
  StringInfo result;

  result = makeStringInfo();

  nspid = LookupExplicitNamespace(schemaname, false);

  xsd_schema_element_start(result, targetns);

  SPI_connect();

  relid_list = schema_get_xml_visible_tables(nspid);

  tupdesc_list = NIL;
  foreach (cell, relid_list)
  {
    Relation rel;

    rel = table_open(lfirst_oid(cell), AccessShareLock);
    tupdesc_list = lappend(tupdesc_list, CreateTupleDescCopy(rel->rd_att));
    table_close(rel, NoLock);
  }

  appendStringInfoString(result, map_sql_typecoll_to_xmlschema_types(tupdesc_list));

  appendStringInfoString(result, map_sql_schema_to_xmlschema_types(nspid, relid_list, nulls, tableforest, targetns));

  xsd_schema_element_end(result);

  SPI_finish();

  return result;
}

Datum
schema_to_xmlschema(PG_FUNCTION_ARGS)
{
  Name name = PG_GETARG_NAME(0);
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));

  PG_RETURN_XML_P(stringinfo_to_xmltype(schema_to_xmlschema_internal(NameStr(*name), nulls, tableforest, targetns)));
}

Datum
schema_to_xml_and_xmlschema(PG_FUNCTION_ARGS)
{
  Name name = PG_GETARG_NAME(0);
  bool nulls = PG_GETARG_BOOL(1);
  bool tableforest = PG_GETARG_BOOL(2);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(3));
  char *schemaname;
  Oid nspid;
  StringInfo xmlschema;

  schemaname = NameStr(*name);
  nspid = LookupExplicitNamespace(schemaname, false);

  xmlschema = schema_to_xmlschema_internal(schemaname, nulls, tableforest, targetns);

  PG_RETURN_XML_P(stringinfo_to_xmltype(schema_to_xml_internal(nspid, xmlschema->data, nulls, tableforest, targetns, true)));
}

   
                                                                        
                        
   

static StringInfo
database_to_xml_internal(const char *xmlschema, bool nulls, bool tableforest, const char *targetns)
{
  StringInfo result;
  List *nspid_list;
  ListCell *cell;
  char *xmlcn;

  xmlcn = map_sql_identifier_to_xml_name(get_database_name(MyDatabaseId), true, false);
  result = makeStringInfo();

  xmldata_root_element_start(result, xmlcn, xmlschema, targetns, true);
  appendStringInfoChar(result, '\n');

  if (xmlschema)
  {
    appendStringInfo(result, "%s\n\n", xmlschema);
  }

  SPI_connect();

  nspid_list = database_get_xml_visible_schemas();

  foreach (cell, nspid_list)
  {
    Oid nspid = lfirst_oid(cell);
    StringInfo subres;

    subres = schema_to_xml_internal(nspid, NULL, nulls, tableforest, targetns, false);

    appendStringInfoString(result, subres->data);
    appendStringInfoChar(result, '\n');
  }

  SPI_finish();

  xmldata_root_element_end(result, xmlcn);

  return result;
}

Datum
database_to_xml(PG_FUNCTION_ARGS)
{
  bool nulls = PG_GETARG_BOOL(0);
  bool tableforest = PG_GETARG_BOOL(1);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(2));

  PG_RETURN_XML_P(stringinfo_to_xmltype(database_to_xml_internal(NULL, nulls, tableforest, targetns)));
}

static StringInfo
database_to_xmlschema_internal(bool nulls, bool tableforest, const char *targetns)
{
  List *relid_list;
  List *nspid_list;
  List *tupdesc_list;
  ListCell *cell;
  StringInfo result;

  result = makeStringInfo();

  xsd_schema_element_start(result, targetns);

  SPI_connect();

  relid_list = database_get_xml_visible_tables();
  nspid_list = database_get_xml_visible_schemas();

  tupdesc_list = NIL;
  foreach (cell, relid_list)
  {
    Relation rel;

    rel = table_open(lfirst_oid(cell), AccessShareLock);
    tupdesc_list = lappend(tupdesc_list, CreateTupleDescCopy(rel->rd_att));
    table_close(rel, NoLock);
  }

  appendStringInfoString(result, map_sql_typecoll_to_xmlschema_types(tupdesc_list));

  appendStringInfoString(result, map_sql_catalog_to_xmlschema_types(nspid_list, nulls, tableforest, targetns));

  xsd_schema_element_end(result);

  SPI_finish();

  return result;
}

Datum
database_to_xmlschema(PG_FUNCTION_ARGS)
{
  bool nulls = PG_GETARG_BOOL(0);
  bool tableforest = PG_GETARG_BOOL(1);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(2));

  PG_RETURN_XML_P(stringinfo_to_xmltype(database_to_xmlschema_internal(nulls, tableforest, targetns)));
}

Datum
database_to_xml_and_xmlschema(PG_FUNCTION_ARGS)
{
  bool nulls = PG_GETARG_BOOL(0);
  bool tableforest = PG_GETARG_BOOL(1);
  const char *targetns = text_to_cstring(PG_GETARG_TEXT_PP(2));
  StringInfo xmlschema;

  xmlschema = database_to_xmlschema_internal(nulls, tableforest, targetns);

  PG_RETURN_XML_P(stringinfo_to_xmltype(database_to_xml_internal(xmlschema->data, nulls, tableforest, targetns)));
}

   
                                                                      
        
   
static char *
map_multipart_sql_identifier_to_xml_name(const char *a, const char *b, const char *c, const char *d)
{
  StringInfoData result;

  initStringInfo(&result);

  if (a)
  {
    appendStringInfoString(&result, map_sql_identifier_to_xml_name(a, true, true));
  }
  if (b)
  {
    appendStringInfo(&result, ".%s", map_sql_identifier_to_xml_name(b, true, true));
  }
  if (c)
  {
    appendStringInfo(&result, ".%s", map_sql_identifier_to_xml_name(c, true, true));
  }
  if (d)
  {
    appendStringInfo(&result, ".%s", map_sql_identifier_to_xml_name(d, true, true));
  }

  return result.data;
}

   
                                                                
                 
   
                                                                       
        
   
static const char *
map_sql_table_to_xmlschema(TupleDesc tupdesc, Oid relid, bool nulls, bool tableforest, const char *targetns)
{
  int i;
  char *xmltn;
  char *tabletypename;
  char *rowtypename;
  StringInfoData result;

  initStringInfo(&result);

  if (OidIsValid(relid))
  {
    HeapTuple tuple;
    Form_pg_class reltuple;

    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", relid);
    }
    reltuple = (Form_pg_class)GETSTRUCT(tuple);

    xmltn = map_sql_identifier_to_xml_name(NameStr(reltuple->relname), true, false);

    tabletypename = map_multipart_sql_identifier_to_xml_name("TableType", get_database_name(MyDatabaseId), get_namespace_name(reltuple->relnamespace), NameStr(reltuple->relname));

    rowtypename = map_multipart_sql_identifier_to_xml_name("RowType", get_database_name(MyDatabaseId), get_namespace_name(reltuple->relnamespace), NameStr(reltuple->relname));

    ReleaseSysCache(tuple);
  }
  else
  {
    if (tableforest)
    {
      xmltn = "row";
    }
    else
    {
      xmltn = "table";
    }

    tabletypename = "TableType";
    rowtypename = "RowType";
  }

  xsd_schema_element_start(&result, targetns);

  appendStringInfoString(&result, map_sql_typecoll_to_xmlschema_types(list_make1(tupdesc)));

  appendStringInfo(&result,
      "<xsd:complexType name=\"%s\">\n"
      "  <xsd:sequence>\n",
      rowtypename);

  for (i = 0; i < tupdesc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

    if (att->attisdropped)
    {
      continue;
    }
    appendStringInfo(&result, "    <xsd:element name=\"%s\" type=\"%s\"%s></xsd:element>\n", map_sql_identifier_to_xml_name(NameStr(att->attname), true, false), map_sql_type_to_xml_name(att->atttypid, -1), nulls ? " nillable=\"true\"" : " minOccurs=\"0\"");
  }

  appendStringInfoString(&result, "  </xsd:sequence>\n"
                                  "</xsd:complexType>\n\n");

  if (!tableforest)
  {
    appendStringInfo(&result,
        "<xsd:complexType name=\"%s\">\n"
        "  <xsd:sequence>\n"
        "    <xsd:element name=\"row\" type=\"%s\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>\n"
        "  </xsd:sequence>\n"
        "</xsd:complexType>\n\n",
        tabletypename, rowtypename);

    appendStringInfo(&result, "<xsd:element name=\"%s\" type=\"%s\"/>\n\n", xmltn, tabletypename);
  }
  else
  {
    appendStringInfo(&result, "<xsd:element name=\"%s\" type=\"%s\"/>\n\n", xmltn, rowtypename);
  }

  xsd_schema_element_end(&result);

  return result.data;
}

   
                                                                
                 
   
static const char *
map_sql_schema_to_xmlschema_types(Oid nspid, List *relid_list, bool nulls, bool tableforest, const char *targetns)
{
  char *dbname;
  char *nspname;
  char *xmlsn;
  char *schematypename;
  StringInfoData result;
  ListCell *cell;

  dbname = get_database_name(MyDatabaseId);
  nspname = get_namespace_name(nspid);

  initStringInfo(&result);

  xmlsn = map_sql_identifier_to_xml_name(nspname, true, false);

  schematypename = map_multipart_sql_identifier_to_xml_name("SchemaType", dbname, nspname, NULL);

  appendStringInfo(&result, "<xsd:complexType name=\"%s\">\n", schematypename);
  if (!tableforest)
  {
    appendStringInfoString(&result, "  <xsd:all>\n");
  }
  else
  {
    appendStringInfoString(&result, "  <xsd:sequence>\n");
  }

  foreach (cell, relid_list)
  {
    Oid relid = lfirst_oid(cell);
    char *relname = get_rel_name(relid);
    char *xmltn = map_sql_identifier_to_xml_name(relname, true, false);
    char *tabletypename = map_multipart_sql_identifier_to_xml_name(tableforest ? "RowType" : "TableType", dbname, nspname, relname);

    if (!tableforest)
    {
      appendStringInfo(&result, "    <xsd:element name=\"%s\" type=\"%s\"/>\n", xmltn, tabletypename);
    }
    else
    {
      appendStringInfo(&result, "    <xsd:element name=\"%s\" type=\"%s\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>\n", xmltn, tabletypename);
    }
  }

  if (!tableforest)
  {
    appendStringInfoString(&result, "  </xsd:all>\n");
  }
  else
  {
    appendStringInfoString(&result, "  </xsd:sequence>\n");
  }
  appendStringInfoString(&result, "</xsd:complexType>\n\n");

  appendStringInfo(&result, "<xsd:element name=\"%s\" type=\"%s\"/>\n\n", xmlsn, schematypename);

  return result.data;
}

   
                                                                 
                 
   
static const char *
map_sql_catalog_to_xmlschema_types(List *nspid_list, bool nulls, bool tableforest, const char *targetns)
{
  char *dbname;
  char *xmlcn;
  char *catalogtypename;
  StringInfoData result;
  ListCell *cell;

  dbname = get_database_name(MyDatabaseId);

  initStringInfo(&result);

  xmlcn = map_sql_identifier_to_xml_name(dbname, true, false);

  catalogtypename = map_multipart_sql_identifier_to_xml_name("CatalogType", dbname, NULL, NULL);

  appendStringInfo(&result, "<xsd:complexType name=\"%s\">\n", catalogtypename);
  appendStringInfoString(&result, "  <xsd:all>\n");

  foreach (cell, nspid_list)
  {
    Oid nspid = lfirst_oid(cell);
    char *nspname = get_namespace_name(nspid);
    char *xmlsn = map_sql_identifier_to_xml_name(nspname, true, false);
    char *schematypename = map_multipart_sql_identifier_to_xml_name("SchemaType", dbname, nspname, NULL);

    appendStringInfo(&result, "    <xsd:element name=\"%s\" type=\"%s\"/>\n", xmlsn, schematypename);
  }

  appendStringInfoString(&result, "  </xsd:all>\n");
  appendStringInfoString(&result, "</xsd:complexType>\n\n");

  appendStringInfo(&result, "<xsd:element name=\"%s\" type=\"%s\"/>\n\n", xmlcn, catalogtypename);

  return result.data;
}

   
                                                                      
   
static const char *
map_sql_type_to_xml_name(Oid typeoid, int typmod)
{
  StringInfoData result;

  initStringInfo(&result);

  switch (typeoid)
  {
  case BPCHAROID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "CHAR");
    }
    else
    {
      appendStringInfo(&result, "CHAR_%d", typmod - VARHDRSZ);
    }
    break;
  case VARCHAROID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "VARCHAR");
    }
    else
    {
      appendStringInfo(&result, "VARCHAR_%d", typmod - VARHDRSZ);
    }
    break;
  case NUMERICOID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "NUMERIC");
    }
    else
    {
      appendStringInfo(&result, "NUMERIC_%d_%d", ((typmod - VARHDRSZ) >> 16) & 0xffff, (typmod - VARHDRSZ) & 0xffff);
    }
    break;
  case INT4OID:
    appendStringInfoString(&result, "INTEGER");
    break;
  case INT2OID:
    appendStringInfoString(&result, "SMALLINT");
    break;
  case INT8OID:
    appendStringInfoString(&result, "BIGINT");
    break;
  case FLOAT4OID:
    appendStringInfoString(&result, "REAL");
    break;
  case FLOAT8OID:
    appendStringInfoString(&result, "DOUBLE");
    break;
  case BOOLOID:
    appendStringInfoString(&result, "BOOLEAN");
    break;
  case TIMEOID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "TIME");
    }
    else
    {
      appendStringInfo(&result, "TIME_%d", typmod);
    }
    break;
  case TIMETZOID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "TIME_WTZ");
    }
    else
    {
      appendStringInfo(&result, "TIME_WTZ_%d", typmod);
    }
    break;
  case TIMESTAMPOID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "TIMESTAMP");
    }
    else
    {
      appendStringInfo(&result, "TIMESTAMP_%d", typmod);
    }
    break;
  case TIMESTAMPTZOID:
    if (typmod == -1)
    {
      appendStringInfoString(&result, "TIMESTAMP_WTZ");
    }
    else
    {
      appendStringInfo(&result, "TIMESTAMP_WTZ_%d", typmod);
    }
    break;
  case DATEOID:
    appendStringInfoString(&result, "DATE");
    break;
  case XMLOID:
    appendStringInfoString(&result, "XML");
    break;
  default:
  {
    HeapTuple tuple;
    Form_pg_type typtuple;

    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for type %u", typeoid);
    }
    typtuple = (Form_pg_type)GETSTRUCT(tuple);

    appendStringInfoString(&result, map_multipart_sql_identifier_to_xml_name((typtuple->typtype == TYPTYPE_DOMAIN) ? "Domain" : "UDT", get_database_name(MyDatabaseId), get_namespace_name(typtuple->typnamespace), NameStr(typtuple->typname)));

    ReleaseSysCache(tuple);
  }
  }

  return result.data;
}

   
                                                                    
                             
   
static const char *
map_sql_typecoll_to_xmlschema_types(List *tupdesc_list)
{
  List *uniquetypes = NIL;
  int i;
  StringInfoData result;
  ListCell *cell0;

                                                              
  foreach (cell0, tupdesc_list)
  {
    TupleDesc tupdesc = (TupleDesc)lfirst(cell0);

    for (i = 0; i < tupdesc->natts; i++)
    {
      Form_pg_attribute att = TupleDescAttr(tupdesc, i);

      if (att->attisdropped)
      {
        continue;
      }
      uniquetypes = list_append_unique_oid(uniquetypes, att->atttypid);
    }
  }

                                 
  foreach (cell0, uniquetypes)
  {
    Oid typid = lfirst_oid(cell0);
    Oid basetypid = getBaseType(typid);

    if (basetypid != typid)
    {
      uniquetypes = list_append_unique_oid(uniquetypes, basetypid);
    }
  }

                               
  initStringInfo(&result);

  foreach (cell0, uniquetypes)
  {
    appendStringInfo(&result, "%s\n", map_sql_type_to_xmlschema_type(lfirst_oid(cell0), -1));
  }

  return result.data;
}

   
                                                             
                                      
   
                                                                   
                                                                      
                                                
   
static const char *
map_sql_type_to_xmlschema_type(Oid typeoid, int typmod)
{
  StringInfoData result;
  const char *typename = map_sql_type_to_xml_name(typeoid, typmod);

  initStringInfo(&result);

  if (typeoid == XMLOID)
  {
    appendStringInfoString(&result, "<xsd:complexType mixed=\"true\">\n"
                                    "  <xsd:sequence>\n"
                                    "    <xsd:any name=\"element\" minOccurs=\"0\" maxOccurs=\"unbounded\" processContents=\"skip\"/>\n"
                                    "  </xsd:sequence>\n"
                                    "</xsd:complexType>\n");
  }
  else
  {
    appendStringInfo(&result, "<xsd:simpleType name=\"%s\">\n", typename);

    switch (typeoid)
    {
    case BPCHAROID:
    case VARCHAROID:
    case TEXTOID:
      appendStringInfoString(&result, "  <xsd:restriction base=\"xsd:string\">\n");
      if (typmod != -1)
      {
        appendStringInfo(&result, "    <xsd:maxLength value=\"%d\"/>\n", typmod - VARHDRSZ);
      }
      appendStringInfoString(&result, "  </xsd:restriction>\n");
      break;

    case BYTEAOID:
      appendStringInfo(&result,
          "  <xsd:restriction base=\"xsd:%s\">\n"
          "  </xsd:restriction>\n",
          xmlbinary == XMLBINARY_BASE64 ? "base64Binary" : "hexBinary");
      break;

    case NUMERICOID:
      if (typmod != -1)
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:decimal\">\n"
            "    <xsd:totalDigits value=\"%d\"/>\n"
            "    <xsd:fractionDigits value=\"%d\"/>\n"
            "  </xsd:restriction>\n",
            ((typmod - VARHDRSZ) >> 16) & 0xffff, (typmod - VARHDRSZ) & 0xffff);
      }
      break;

    case INT2OID:
      appendStringInfo(&result,
          "  <xsd:restriction base=\"xsd:short\">\n"
          "    <xsd:maxInclusive value=\"%d\"/>\n"
          "    <xsd:minInclusive value=\"%d\"/>\n"
          "  </xsd:restriction>\n",
          SHRT_MAX, SHRT_MIN);
      break;

    case INT4OID:
      appendStringInfo(&result,
          "  <xsd:restriction base=\"xsd:int\">\n"
          "    <xsd:maxInclusive value=\"%d\"/>\n"
          "    <xsd:minInclusive value=\"%d\"/>\n"
          "  </xsd:restriction>\n",
          INT_MAX, INT_MIN);
      break;

    case INT8OID:
      appendStringInfo(&result,
          "  <xsd:restriction base=\"xsd:long\">\n"
          "    <xsd:maxInclusive value=\"" INT64_FORMAT "\"/>\n"
          "    <xsd:minInclusive value=\"" INT64_FORMAT "\"/>\n"
          "  </xsd:restriction>\n",
          (((uint64)1) << (sizeof(int64) * 8 - 1)) - 1, (((uint64)1) << (sizeof(int64) * 8 - 1)));
      break;

    case FLOAT4OID:
      appendStringInfoString(&result, "  <xsd:restriction base=\"xsd:float\"></xsd:restriction>\n");
      break;

    case FLOAT8OID:
      appendStringInfoString(&result, "  <xsd:restriction base=\"xsd:double\"></xsd:restriction>\n");
      break;

    case BOOLOID:
      appendStringInfoString(&result, "  <xsd:restriction base=\"xsd:boolean\"></xsd:restriction>\n");
      break;

    case TIMEOID:
    case TIMETZOID:
    {
      const char *tz = (typeoid == TIMETZOID ? "(\\+|-)\\p{Nd}{2}:\\p{Nd}{2}" : "");

      if (typmod == -1)
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:time\">\n"
            "    <xsd:pattern value=\"\\p{Nd}{2}:\\p{Nd}{2}:\\p{Nd}{2}(.\\p{Nd}+)?%s\"/>\n"
            "  </xsd:restriction>\n",
            tz);
      }
      else if (typmod == 0)
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:time\">\n"
            "    <xsd:pattern value=\"\\p{Nd}{2}:\\p{Nd}{2}:\\p{Nd}{2}%s\"/>\n"
            "  </xsd:restriction>\n",
            tz);
      }
      else
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:time\">\n"
            "    <xsd:pattern value=\"\\p{Nd}{2}:\\p{Nd}{2}:\\p{Nd}{2}.\\p{Nd}{%d}%s\"/>\n"
            "  </xsd:restriction>\n",
            typmod - VARHDRSZ, tz);
      }
      break;
    }

    case TIMESTAMPOID:
    case TIMESTAMPTZOID:
    {
      const char *tz = (typeoid == TIMESTAMPTZOID ? "(\\+|-)\\p{Nd}{2}:\\p{Nd}{2}" : "");

      if (typmod == -1)
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:dateTime\">\n"
            "    <xsd:pattern value=\"\\p{Nd}{4}-\\p{Nd}{2}-\\p{Nd}{2}T\\p{Nd}{2}:\\p{Nd}{2}:\\p{Nd}{2}(.\\p{Nd}+)?%s\"/>\n"
            "  </xsd:restriction>\n",
            tz);
      }
      else if (typmod == 0)
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:dateTime\">\n"
            "    <xsd:pattern value=\"\\p{Nd}{4}-\\p{Nd}{2}-\\p{Nd}{2}T\\p{Nd}{2}:\\p{Nd}{2}:\\p{Nd}{2}%s\"/>\n"
            "  </xsd:restriction>\n",
            tz);
      }
      else
      {
        appendStringInfo(&result,
            "  <xsd:restriction base=\"xsd:dateTime\">\n"
            "    <xsd:pattern value=\"\\p{Nd}{4}-\\p{Nd}{2}-\\p{Nd}{2}T\\p{Nd}{2}:\\p{Nd}{2}:\\p{Nd}{2}.\\p{Nd}{%d}%s\"/>\n"
            "  </xsd:restriction>\n",
            typmod - VARHDRSZ, tz);
      }
      break;
    }

    case DATEOID:
      appendStringInfoString(&result, "  <xsd:restriction base=\"xsd:date\">\n"
                                      "    <xsd:pattern value=\"\\p{Nd}{4}-\\p{Nd}{2}-\\p{Nd}{2}\"/>\n"
                                      "  </xsd:restriction>\n");
      break;

    default:
      if (get_typtype(typeoid) == TYPTYPE_DOMAIN)
      {
        Oid base_typeoid;
        int32 base_typmod = -1;

        base_typeoid = getBaseTypeAndTypmod(typeoid, &base_typmod);

        appendStringInfo(&result, "  <xsd:restriction base=\"%s\"/>\n", map_sql_type_to_xml_name(base_typeoid, base_typmod));
      }
      break;
    }
    appendStringInfoString(&result, "</xsd:simpleType>\n");
  }

  return result.data;
}

   
                                                                    
                                                    
   
static void
SPI_sql_row_to_xmlelement(uint64 rownum, StringInfo result, char *tablename, bool nulls, bool tableforest, const char *targetns, bool top_level)
{
  int i;
  char *xmltn;

  if (tablename)
  {
    xmltn = map_sql_identifier_to_xml_name(tablename, true, false);
  }
  else
  {
    if (tableforest)
    {
      xmltn = "row";
    }
    else
    {
      xmltn = "table";
    }
  }

  if (tableforest)
  {
    xmldata_root_element_start(result, xmltn, NULL, targetns, top_level);
  }
  else
  {
    appendStringInfoString(result, "<row>\n");
  }

  for (i = 1; i <= SPI_tuptable->tupdesc->natts; i++)
  {
    char *colname;
    Datum colval;
    bool isnull;

    colname = map_sql_identifier_to_xml_name(SPI_fname(SPI_tuptable->tupdesc, i), true, false);
    colval = SPI_getbinval(SPI_tuptable->vals[rownum], SPI_tuptable->tupdesc, i, &isnull);
    if (isnull)
    {
      if (nulls)
      {
        appendStringInfo(result, "  <%s xsi:nil=\"true\"/>\n", colname);
      }
    }
    else
    {
      appendStringInfo(result, "  <%s>%s</%s>\n", colname, map_sql_value_to_xml_value(colval, SPI_gettypeid(SPI_tuptable->tupdesc, i), true), colname);
    }
  }

  if (tableforest)
  {
    xmldata_root_element_end(result, xmltn);
    appendStringInfoChar(result, '\n');
  }
  else
  {
    appendStringInfoString(result, "</row>\n\n");
  }
}

   
                           
   

#ifdef USE_LIBXML

   
                             
   
                                                                              
                           
   
static text *
xml_xmlnodetoxmltype(xmlNodePtr cur, PgXmlErrorContext *xmlerrcxt)
{
  xmltype *result;

  if (cur->type != XML_ATTRIBUTE_NODE && cur->type != XML_TEXT_NODE)
  {
    void (*volatile nodefree)(xmlNodePtr) = NULL;
    volatile xmlBufferPtr buf = NULL;
    volatile xmlNodePtr cur_copy = NULL;

    PG_TRY();
    {
      int bytes;

      buf = xmlBufferCreate();
      if (buf == NULL || xmlerrcxt->err_occurred)
      {
        xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate xmlBuffer");
      }

         
                                                                        
                                                                  
                                                                     
                                                                         
                                
         
                                                                   
                                                                   
                                                                   
                                                                    
         
      cur_copy = xmlCopyNode(cur, 1);
      if (cur_copy == NULL || xmlerrcxt->err_occurred)
      {
        xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not copy node");
      }
      nodefree = (cur_copy->type == XML_DOCUMENT_NODE) ? (void (*)(xmlNodePtr))xmlFreeDoc : xmlFreeNode;

      bytes = xmlNodeDump(buf, NULL, cur_copy, 0, 0);
      if (bytes == -1 || xmlerrcxt->err_occurred)
      {
        xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not dump node");
      }

      result = xmlBuffer_to_xmltype(buf);
    }
    PG_CATCH();
    {
      if (nodefree)
      {
        nodefree(cur_copy);
      }
      if (buf)
      {
        xmlBufferFree(buf);
      }
      PG_RE_THROW();
    }
    PG_END_TRY();

    if (nodefree)
    {
      nodefree(cur_copy);
    }
    xmlBufferFree(buf);
  }
  else
  {
    xmlChar *str;

    str = xmlXPathCastNodeToString(cur);
    PG_TRY();
    {
                                                                      
      char *escaped = escape_xml((char *)str);

      result = (xmltype *)cstring_to_text(escaped);
      pfree(escaped);
    }
    PG_CATCH();
    {
      xmlFree(str);
      PG_RE_THROW();
    }
    PG_END_TRY();
    xmlFree(str);
  }

  return result;
}

   
                                                                              
                                                                          
                                                        
   
                                                                            
                                                    
   
                                                                    
                                                                            
                                                                           
   
static int
xml_xpathobjtoxmlarray(xmlXPathObjectPtr xpathobj, ArrayBuildState *astate, PgXmlErrorContext *xmlerrcxt)
{
  int result = 0;
  Datum datum;
  Oid datumtype;
  char *result_str;

  switch (xpathobj->type)
  {
  case XPATH_NODESET:
    if (xpathobj->nodesetval != NULL)
    {
      result = xpathobj->nodesetval->nodeNr;
      if (astate != NULL)
      {
        int i;

        for (i = 0; i < result; i++)
        {
          datum = PointerGetDatum(xml_xmlnodetoxmltype(xpathobj->nodesetval->nodeTab[i], xmlerrcxt));
          (void)accumArrayResult(astate, datum, false, XMLOID, CurrentMemoryContext);
        }
      }
    }
    return result;

  case XPATH_BOOLEAN:
    if (astate == NULL)
    {
      return 1;
    }
    datum = BoolGetDatum(xpathobj->boolval);
    datumtype = BOOLOID;
    break;

  case XPATH_NUMBER:
    if (astate == NULL)
    {
      return 1;
    }
    datum = Float8GetDatum(xpathobj->floatval);
    datumtype = FLOAT8OID;
    break;

  case XPATH_STRING:
    if (astate == NULL)
    {
      return 1;
    }
    datum = CStringGetDatum((char *)xpathobj->stringval);
    datumtype = CSTRINGOID;
    break;

  default:
    elog(ERROR, "xpath expression result type %d is unsupported", xpathobj->type);
    return 0;                          
  }

                                          
  result_str = map_sql_value_to_xml_value(datum, datumtype, true);
  datum = PointerGetDatum(cstring_to_xmltype(result_str));
  (void)accumArrayResult(astate, datum, false, XMLOID, CurrentMemoryContext);
  return 1;
}

   
                                           
   
                                                                     
                                                                       
                                                  
   
                                                                 
                                                                    
                               
   
static void
xpath_internal(text *xpath_expr_text, xmltype *data, ArrayType *namespaces, int *res_nitems, ArrayBuildState *astate)
{
  PgXmlErrorContext *xmlerrcxt;
  volatile xmlParserCtxtPtr ctxt = NULL;
  volatile xmlDocPtr doc = NULL;
  volatile xmlXPathContextPtr xpathctx = NULL;
  volatile xmlXPathCompExprPtr xpathcomp = NULL;
  volatile xmlXPathObjectPtr xpathobj = NULL;
  char *datastr;
  int32 len;
  int32 xpath_len;
  xmlChar *string;
  xmlChar *xpath_expr;
  size_t xmldecl_len = 0;
  int i;
  int ndim;
  Datum *ns_names_uris;
  bool *ns_names_uris_nulls;
  int ns_count;

     
                                                                           
                                                                        
                                                                            
                                                                          
                                                                        
                                                               
                              
     
  ndim = namespaces ? ARR_NDIM(namespaces) : 0;
  if (ndim != 0)
  {
    int *dims;

    dims = ARR_DIMS(namespaces);

    if (ndim != 2 || dims[1] != 2)
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("invalid array for XML namespace mapping"), errdetail("The array must be two-dimensional with length of the second axis equal to 2.")));
    }

    Assert(ARR_ELEMTYPE(namespaces) == TEXTOID);

    deconstruct_array(namespaces, TEXTOID, -1, false, 'i', &ns_names_uris, &ns_names_uris_nulls, &ns_count);

    Assert((ns_count % 2) == 0);                    
    ns_count /= 2;                                     
  }
  else
  {
    ns_names_uris = NULL;
    ns_names_uris_nulls = NULL;
    ns_count = 0;
  }

  datastr = VARDATA(data);
  len = VARSIZE(data) - VARHDRSZ;
  xpath_len = VARSIZE_ANY_EXHDR(xpath_expr_text);
  if (xpath_len == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("empty XPath expression")));
  }

  string = pg_xmlCharStrndup(datastr, len);
  xpath_expr = pg_xmlCharStrndup(VARDATA_ANY(xpath_expr_text), xpath_len);

     
                                                                      
                                                                 
                                                                       
                                                                        
                                                              
     
  if (GetDatabaseEncoding() == PG_UTF8)
  {
    parse_xml_decl(string, &xmldecl_len, NULL, NULL, NULL);
  }

  xmlerrcxt = pg_xml_init(PG_XML_STRICTNESS_ALL);

  PG_TRY();
  {
    xmlInitParser();

       
                                                                         
                                       
       
    ctxt = xmlNewParserCtxt();
    if (ctxt == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate parser context");
    }
    doc = xmlCtxtReadMemory(ctxt, (char *)string + xmldecl_len, len - xmldecl_len, NULL, NULL, 0);
    if (doc == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_INVALID_XML_DOCUMENT, "could not parse XML document");
    }
    xpathctx = xmlXPathNewContext(doc);
    if (xpathctx == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate XPath context");
    }
    xpathctx->node = (xmlNodePtr)doc;

                                     
    if (ns_count > 0)
    {
      for (i = 0; i < ns_count; i++)
      {
        char *ns_name;
        char *ns_uri;

        if (ns_names_uris_nulls[i * 2] || ns_names_uris_nulls[i * 2 + 1])
        {
          ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("neither namespace name nor URI may be null")));
        }
        ns_name = TextDatumGetCString(ns_names_uris[i * 2]);
        ns_uri = TextDatumGetCString(ns_names_uris[i * 2 + 1]);
        if (xmlXPathRegisterNs(xpathctx, (xmlChar *)ns_name, (xmlChar *)ns_uri) != 0)
        {
          ereport(ERROR,                                   
              (errmsg("could not register XML namespace with name \"%s\" and URI \"%s\"", ns_name, ns_uri)));
        }
      }
    }

    xpathcomp = xmlXPathCompile(xpath_expr);
    if (xpathcomp == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_INTERNAL_ERROR, "invalid XPath expression");
    }

       
                                                  
                                                                           
                                                                          
                                                                         
                 
       
    xpathobj = xmlXPathCompiledEval(xpathcomp, xpathctx);
    if (xpathobj == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_INTERNAL_ERROR, "could not create XPath object");
    }

       
                                         
       
    if (res_nitems != NULL)
    {
      *res_nitems = xml_xpathobjtoxmlarray(xpathobj, astate, xmlerrcxt);
    }
    else
    {
      (void)xml_xpathobjtoxmlarray(xpathobj, astate, xmlerrcxt);
    }
  }
  PG_CATCH();
  {
    if (xpathobj)
    {
      xmlXPathFreeObject(xpathobj);
    }
    if (xpathcomp)
    {
      xmlXPathFreeCompExpr(xpathcomp);
    }
    if (xpathctx)
    {
      xmlXPathFreeContext(xpathctx);
    }
    if (doc)
    {
      xmlFreeDoc(doc);
    }
    if (ctxt)
    {
      xmlFreeParserCtxt(ctxt);
    }

    pg_xml_done(xmlerrcxt, true);

    PG_RE_THROW();
  }
  PG_END_TRY();

  xmlXPathFreeObject(xpathobj);
  xmlXPathFreeCompExpr(xpathcomp);
  xmlXPathFreeContext(xpathctx);
  xmlFreeDoc(doc);
  xmlFreeParserCtxt(ctxt);

  pg_xml_done(xmlerrcxt, false);
}
#endif                 

   
                                                             
   
                                                                      
                                                                     
                                                    
   
Datum
xpath(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *xpath_expr_text = PG_GETARG_TEXT_PP(0);
  xmltype *data = PG_GETARG_XML_P(1);
  ArrayType *namespaces = PG_GETARG_ARRAYTYPE_P(2);
  ArrayBuildState *astate;

  astate = initArrayResult(XMLOID, CurrentMemoryContext, true);
  xpath_internal(xpath_expr_text, data, namespaces, NULL, astate);
  PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
#else
  NO_XML_SUPPORT();
  return 0;
#endif
}

   
                                                                 
                                                 
   
Datum
xmlexists(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *xpath_expr_text = PG_GETARG_TEXT_PP(0);
  xmltype *data = PG_GETARG_XML_P(1);
  int res_nitems;

  xpath_internal(xpath_expr_text, data, NULL, &res_nitems, NULL);

  PG_RETURN_BOOL(res_nitems > 0);
#else
  NO_XML_SUPPORT();
  return 0;
#endif
}

   
                                                                 
                                                              
                                                                      
   
Datum
xpath_exists(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *xpath_expr_text = PG_GETARG_TEXT_PP(0);
  xmltype *data = PG_GETARG_XML_P(1);
  ArrayType *namespaces = PG_GETARG_ARRAYTYPE_P(2);
  int res_nitems;

  xpath_internal(xpath_expr_text, data, namespaces, &res_nitems, NULL);

  PG_RETURN_BOOL(res_nitems > 0);
#else
  NO_XML_SUPPORT();
  return 0;
#endif
}

   
                                           
   

#ifdef USE_LIBXML
static bool
wellformed_xml(text *data, XmlOptionType xmloption_arg)
{
  bool result;
  volatile xmlDocPtr doc = NULL;

                                                        
  PG_TRY();
  {
    doc = xml_parse(data, xmloption_arg, true, GetDatabaseEncoding());
    result = true;
  }
  PG_CATCH();
  {
    FlushErrorState();
    result = false;
  }
  PG_END_TRY();

  if (doc)
  {
    xmlFreeDoc(doc);
  }

  return result;
}
#endif

Datum
xml_is_well_formed(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *data = PG_GETARG_TEXT_PP(0);

  PG_RETURN_BOOL(wellformed_xml(data, xmloption));
#else
  NO_XML_SUPPORT();
  return 0;
#endif                     
}

Datum
xml_is_well_formed_document(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *data = PG_GETARG_TEXT_PP(0);

  PG_RETURN_BOOL(wellformed_xml(data, XMLOPTION_DOCUMENT));
#else
  NO_XML_SUPPORT();
  return 0;
#endif                     
}

Datum
xml_is_well_formed_content(PG_FUNCTION_ARGS)
{
#ifdef USE_LIBXML
  text *data = PG_GETARG_TEXT_PP(0);

  PG_RETURN_BOOL(wellformed_xml(data, XMLOPTION_CONTENT));
#else
  NO_XML_SUPPORT();
  return 0;
#endif                     
}

   
                                  
   
   
#ifdef USE_LIBXML

   
                                                                           
                 
   
static inline XmlTableBuilderData *
GetXmlTableBuilderPrivateData(TableFuncScanState *state, const char *fname)
{
  XmlTableBuilderData *result;

  if (!IsA(state, TableFuncScanState))
  {
    elog(ERROR, "%s called with invalid TableFuncScanState", fname);
  }
  result = (XmlTableBuilderData *)state->opaque;
  if (result->magic != XMLTABLE_CONTEXT_MAGIC)
  {
    elog(ERROR, "%s called with invalid TableFuncScanState", fname);
  }

  return result;
}
#endif

   
                      
                                                                          
                    
   
                                                                 
                                                                      
                                                                          
                                                                             
                                                                   
   
static void
XmlTableInitOpaque(TableFuncScanState *state, int natts)
{
#ifdef USE_LIBXML
  volatile xmlParserCtxtPtr ctxt = NULL;
  XmlTableBuilderData *xtCxt;
  PgXmlErrorContext *xmlerrcxt;

  xtCxt = palloc0(sizeof(XmlTableBuilderData));
  xtCxt->magic = XMLTABLE_CONTEXT_MAGIC;
  xtCxt->natts = natts;
  xtCxt->xpathscomp = palloc0(sizeof(xmlXPathCompExprPtr) * natts);

  xmlerrcxt = pg_xml_init(PG_XML_STRICTNESS_ALL);

  PG_TRY();
  {
    xmlInitParser();

    ctxt = xmlNewParserCtxt();
    if (ctxt == NULL || xmlerrcxt->err_occurred)
    {
      xml_ereport(xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate parser context");
    }
  }
  PG_CATCH();
  {
    if (ctxt != NULL)
    {
      xmlFreeParserCtxt(ctxt);
    }

    pg_xml_done(xmlerrcxt, true);

    PG_RE_THROW();
  }
  PG_END_TRY();

  xtCxt->xmlerrcxt = xmlerrcxt;
  xtCxt->ctxt = ctxt;

  state->opaque = xtCxt;
#else
  NO_XML_SUPPORT();
#endif                     
}

   
                       
                               
   
static void
XmlTableSetDocument(TableFuncScanState *state, Datum value)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;
  xmltype *xmlval = DatumGetXmlP(value);
  char *str;
  xmlChar *xstr;
  int length;
  volatile xmlDocPtr doc = NULL;
  volatile xmlXPathContextPtr xpathcxt = NULL;

  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableSetDocument");

     
                                                                            
                         
     
  str = xml_out_internal(xmlval, 0);

  length = strlen(str);
  xstr = pg_xmlCharStrndup(str, length);

  PG_TRY();
  {
    doc = xmlCtxtReadMemory(xtCxt->ctxt, (char *)xstr, length, NULL, NULL, 0);
    if (doc == NULL || xtCxt->xmlerrcxt->err_occurred)
    {
      xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_INVALID_XML_DOCUMENT, "could not parse XML document");
    }
    xpathcxt = xmlXPathNewContext(doc);
    if (xpathcxt == NULL || xtCxt->xmlerrcxt->err_occurred)
    {
      xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_OUT_OF_MEMORY, "could not allocate XPath context");
    }
    xpathcxt->node = (xmlNodePtr)doc;
  }
  PG_CATCH();
  {
    if (xpathcxt != NULL)
    {
      xmlXPathFreeContext(xpathcxt);
    }
    if (doc != NULL)
    {
      xmlFreeDoc(doc);
    }

    PG_RE_THROW();
  }
  PG_END_TRY();

  xtCxt->doc = doc;
  xtCxt->xpathcxt = xpathcxt;
#else
  NO_XML_SUPPORT();
#endif                     
}

   
                        
                                
   
static void
XmlTableSetNamespace(TableFuncScanState *state, const char *name, const char *uri)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;

  if (name == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DEFAULT namespace is not supported")));
  }
  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableSetNamespace");

  if (xmlXPathRegisterNs(xtCxt->xpathcxt, pg_xmlCharStrndup(name, strlen(name)), pg_xmlCharStrndup(uri, strlen(uri))))
  {
    xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_DATA_EXCEPTION, "could not set XML namespace");
  }
#else
  NO_XML_SUPPORT();
#endif                     
}

   
                        
                                             
   
static void
XmlTableSetRowFilter(TableFuncScanState *state, const char *path)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;
  xmlChar *xstr;

  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableSetRowFilter");

  if (*path == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("row path filter must not be empty string")));
  }

  xstr = pg_xmlCharStrndup(path, strlen(path));

  xtCxt->xpathcomp = xmlXPathCompile(xstr);
  if (xtCxt->xpathcomp == NULL || xtCxt->xmlerrcxt->err_occurred)
  {
    xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_SYNTAX_ERROR, "invalid XPath expression");
  }
#else
  NO_XML_SUPPORT();
#endif                     
}

   
                           
                                                                      
   
static void
XmlTableSetColumnFilter(TableFuncScanState *state, const char *path, int colnum)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;
  xmlChar *xstr;

  AssertArg(PointerIsValid(path));

  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableSetColumnFilter");

  if (*path == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("column path filter must not be empty string")));
  }

  xstr = pg_xmlCharStrndup(path, strlen(path));

  xtCxt->xpathscomp[colnum] = xmlXPathCompile(xstr);
  if (xtCxt->xpathscomp[colnum] == NULL || xtCxt->xmlerrcxt->err_occurred)
  {
    xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_DATA_EXCEPTION, "invalid XPath expression");
  }
#else
  NO_XML_SUPPORT();
#endif                     
}

   
                    
                                                                  
                                                                      
   
static bool
XmlTableFetchRow(TableFuncScanState *state)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;

  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableFetchRow");

     
                                                                             
                                                                       
                                                                          
                                                                         
                                               
     
  xmlSetStructuredErrorFunc((void *)xtCxt->xmlerrcxt, xml_errorHandler);

  if (xtCxt->xpathobj == NULL)
  {
    xtCxt->xpathobj = xmlXPathCompiledEval(xtCxt->xpathcomp, xtCxt->xpathcxt);
    if (xtCxt->xpathobj == NULL || xtCxt->xmlerrcxt->err_occurred)
    {
      xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_INTERNAL_ERROR, "could not create XPath object");
    }

    xtCxt->row_count = 0;
  }

  if (xtCxt->xpathobj->type == XPATH_NODESET)
  {
    if (xtCxt->xpathobj->nodesetval != NULL)
    {
      if (xtCxt->row_count++ < xtCxt->xpathobj->nodesetval->nodeNr)
      {
        return true;
      }
    }
  }

  return false;
#else
  NO_XML_SUPPORT();
  return false;
#endif                     
}

   
                    
                                                                         
                                                                    
   
                                                                          
           
   
static Datum
XmlTableGetValue(TableFuncScanState *state, int colnum, Oid typid, int32 typmod, bool *isnull)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;
  Datum result = (Datum)0;
  xmlNodePtr cur;
  char *cstr = NULL;
  volatile xmlXPathObjectPtr xpathobj = NULL;

  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableGetValue");

  Assert(xtCxt->xpathobj && xtCxt->xpathobj->type == XPATH_NODESET && xtCxt->xpathobj->nodesetval != NULL);

                                                          
  xmlSetStructuredErrorFunc((void *)xtCxt->xmlerrcxt, xml_errorHandler);

  *isnull = false;

  cur = xtCxt->xpathobj->nodesetval->nodeTab[xtCxt->row_count - 1];

  Assert(xtCxt->xpathscomp[colnum] != NULL);

  PG_TRY();
  {
                                                              
    xtCxt->xpathcxt->node = cur;

                              
    xpathobj = xmlXPathCompiledEval(xtCxt->xpathscomp[colnum], xtCxt->xpathcxt);
    if (xpathobj == NULL || xtCxt->xmlerrcxt->err_occurred)
    {
      xml_ereport(xtCxt->xmlerrcxt, ERROR, ERRCODE_INTERNAL_ERROR, "could not create XPath object");
    }

       
                                                                       
                                                                           
                                                                         
                                                                          
                                  
       
    if (xpathobj->type == XPATH_NODESET)
    {
      int count = 0;

      if (xpathobj->nodesetval != NULL)
      {
        count = xpathobj->nodesetval->nodeNr;
      }

      if (xpathobj->nodesetval == NULL || count == 0)
      {
        *isnull = true;
      }
      else
      {
        if (typid == XMLOID)
        {
          text *textstr;
          StringInfoData str;

                                             
          initStringInfo(&str);
          for (int i = 0; i < count; i++)
          {
            textstr = xml_xmlnodetoxmltype(xpathobj->nodesetval->nodeTab[i], xtCxt->xmlerrcxt);

            appendStringInfoText(&str, textstr);
          }
          cstr = str.data;
        }
        else
        {
          xmlChar *str;

          if (count > 1)
          {
            ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("more than one value returned by column XPath expression")));
          }

          str = xmlXPathCastNodeSetToString(xpathobj->nodesetval);
          cstr = str ? xml_pstrdup_and_free(str) : "";
        }
      }
    }
    else if (xpathobj->type == XPATH_STRING)
    {
                                                             
      if (typid == XMLOID)
      {
        cstr = escape_xml((char *)xpathobj->stringval);
      }
      else
      {
        cstr = (char *)xpathobj->stringval;
      }
    }
    else if (xpathobj->type == XPATH_BOOLEAN)
    {
      char typcategory;
      bool typispreferred;
      xmlChar *str;

                                                          
      get_type_category_preferred(typid, &typcategory, &typispreferred);

      if (typcategory != TYPCATEGORY_NUMERIC)
      {
        str = xmlXPathCastBooleanToString(xpathobj->boolval);
      }
      else
      {
        str = xmlXPathCastNumberToString(xmlXPathCastBooleanToNumber(xpathobj->boolval));
      }

      cstr = xml_pstrdup_and_free(str);
    }
    else if (xpathobj->type == XPATH_NUMBER)
    {
      xmlChar *str;

      str = xmlXPathCastNumberToString(xpathobj->floatval);
      cstr = xml_pstrdup_and_free(str);
    }
    else
    {
      elog(ERROR, "unexpected XPath object type %u", xpathobj->type);
    }

       
                                                                          
                     
       
    Assert(cstr || *isnull);

    if (!*isnull)
    {
      result = InputFunctionCall(&state->in_functions[colnum], cstr, state->typioparams[colnum], typmod);
    }
  }
  PG_CATCH();
  {
    if (xpathobj != NULL)
    {
      xmlXPathFreeObject(xpathobj);
    }
    PG_RE_THROW();
  }
  PG_END_TRY();

  xmlXPathFreeObject(xpathobj);

  return result;
#else
  NO_XML_SUPPORT();
  return 0;
#endif                     
}

   
                         
                                  
   
static void
XmlTableDestroyOpaque(TableFuncScanState *state)
{
#ifdef USE_LIBXML
  XmlTableBuilderData *xtCxt;

  xtCxt = GetXmlTableBuilderPrivateData(state, "XmlTableDestroyOpaque");

                                                          
  xmlSetStructuredErrorFunc((void *)xtCxt->xmlerrcxt, xml_errorHandler);

  if (xtCxt->xpathscomp != NULL)
  {
    int i;

    for (i = 0; i < xtCxt->natts; i++)
    {
      if (xtCxt->xpathscomp[i] != NULL)
      {
        xmlXPathFreeCompExpr(xtCxt->xpathscomp[i]);
      }
    }
  }

  if (xtCxt->xpathobj != NULL)
  {
    xmlXPathFreeObject(xtCxt->xpathobj);
  }
  if (xtCxt->xpathcomp != NULL)
  {
    xmlXPathFreeCompExpr(xtCxt->xpathcomp);
  }
  if (xtCxt->xpathcxt != NULL)
  {
    xmlXPathFreeContext(xtCxt->xpathcxt);
  }
  if (xtCxt->doc != NULL)
  {
    xmlFreeDoc(xtCxt->doc);
  }
  if (xtCxt->ctxt != NULL)
  {
    xmlFreeParserCtxt(xtCxt->ctxt);
  }

  pg_xml_done(xtCxt->xmlerrcxt, true);

                         
  xtCxt->magic = 0;
  state->opaque = NULL;

#else
  NO_XML_SUPPORT();
#endif                     
}
