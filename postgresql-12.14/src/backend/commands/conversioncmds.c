                                                                            
   
                    
                                              
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/conversioncmds.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                     
   
ObjectAddress
CreateConversionCommand(CreateConversionStmt *stmt)
{
  Oid namespaceId;
  char *conversion_name;
  AclResult aclresult;
  int from_encoding;
  int to_encoding;
  Oid funcoid;
  const char *from_encoding_name = stmt->for_encoding_name;
  const char *to_encoding_name = stmt->to_encoding_name;
  List *func_name = stmt->func_name;
  static const Oid funcargs[] = {INT4OID, INT4OID, CSTRINGOID, INTERNALOID, INT4OID};
  char result[1];

                                                     
  namespaceId = QualifiedNameGetCreationNamespace(stmt->conversion_name, &conversion_name);

                                                         
  aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceId));
  }

                                
  from_encoding = pg_char_to_encoding(from_encoding_name);
  if (from_encoding < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("source encoding \"%s\" does not exist", from_encoding_name)));
  }

  to_encoding = pg_char_to_encoding(to_encoding_name);
  if (to_encoding < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("destination encoding \"%s\" does not exist", to_encoding_name)));
  }

     
                                                                            
                       
     
  funcoid = LookupFuncName(func_name, sizeof(funcargs) / sizeof(Oid), funcargs, false);

                                                                    
  if (get_func_rettype(funcoid) != VOIDOID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("encoding conversion function %s must return type %s", NameListToString(func_name), "void")));
  }

                                                     
  aclresult = pg_proc_aclcheck(funcoid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_FUNCTION, NameListToString(func_name));
  }

     
                                                                             
                                                                            
                                                                       
                                       
     
  OidFunctionCall5(funcoid, Int32GetDatum(from_encoding), Int32GetDatum(to_encoding), CStringGetDatum(""), CStringGetDatum(result), Int32GetDatum(0));

     
                                                                             
           
     
  return ConversionCreate(conversion_name, namespaceId, GetUserId(), from_encoding, to_encoding, funcoid, stmt->def);
}
