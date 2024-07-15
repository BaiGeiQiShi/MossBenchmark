                                                                            
   
              
                                      
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_class.h"
#include "catalog/pg_proc.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static void
does_not_exist_skipping(ObjectType objtype, Node *object);
static bool
owningrel_does_not_exist_skipping(List *object, const char **msg, char **name);
static bool
schema_does_not_exist_skipping(List *object, const char **msg, char **name);
static bool
type_in_list_does_not_exist_skipping(List *typenames, const char **msg, char **name);

   
                             
   
                                                                             
                                                                           
                         
   
                                                                      
                                                                           
                                                  
   
void
RemoveObjects(DropStmt *stmt)
{
  ObjectAddresses *objects;
  ListCell *cell1;

  objects = new_object_addresses();

  foreach (cell1, stmt->objects)
  {
    ObjectAddress address;
    Node *object = lfirst(cell1);
    Relation relation = NULL;
    Oid namespaceId;

                                              
    address = get_object_address(stmt->removeType, object, &relation, AccessExclusiveLock, stmt->missing_ok);

       
                                                                         
                                                          
                                                      
       
    if (!OidIsValid(address.objectId))
    {
      Assert(stmt->missing_ok);
      does_not_exist_skipping(stmt->removeType, object);
      continue;
    }

       
                                                                          
                                                                          
                                                        
       
    if (stmt->removeType == OBJECT_FUNCTION)
    {
      if (get_func_prokind(address.objectId) == PROKIND_AGGREGATE)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an aggregate function", NameListToString(castNode(ObjectWithArgs, object)->objname)), errhint("Use DROP AGGREGATE to drop aggregate functions.")));
      }
    }

                            
    namespaceId = get_object_namespace(&address);
    if (!OidIsValid(namespaceId) || !pg_namespace_ownercheck(namespaceId, GetUserId()))
    {
      check_object_ownership(GetUserId(), stmt->removeType, address, object, relation);
    }

       
                                                                    
                    
       
    if (OidIsValid(namespaceId) && isTempNamespace(namespaceId))
    {
      MyXactFlags |= XACT_FLAGS_ACCESSEDTEMPNAMESPACE;
    }

                                                                           
    if (relation)
    {
      table_close(relation, NoLock);
    }

    add_exact_object_address(&address, objects);
  }

                                   
  performMultipleDeletions(objects, stmt->behavior, 0);

  free_object_addresses(objects);
}

   
                                     
                                 
   
                                                                             
                                                                              
                                                                              
                                                                           
                                                                          
   
static bool
owningrel_does_not_exist_skipping(List *object, const char **msg, char **name)
{
  List *parent_object;
  RangeVar *parent_rel;

  parent_object = list_truncate(list_copy(object), list_length(object) - 1);

  if (schema_does_not_exist_skipping(parent_object, msg, name))
  {
    return true;
  }

  parent_rel = makeRangeVarFromNameList(parent_object);

  if (!OidIsValid(RangeVarGetRelid(parent_rel, NoLock, true)))
  {
    *msg = gettext_noop("relation \"%s\" does not exist, skipping");
    *name = NameListToString(parent_object);

    return true;
  }

  return false;
}

   
                                  
                                 
   
                                                                          
                                                                              
                                                                            
                                                                           
                                                                       
                                           
   
static bool
schema_does_not_exist_skipping(List *object, const char **msg, char **name)
{
  RangeVar *rel;

  rel = makeRangeVarFromNameList(object);

  if (rel->schemaname != NULL && !OidIsValid(LookupNamespaceNoError(rel->schemaname)))
  {
    *msg = gettext_noop("schema \"%s\" does not exist, skipping");
    *name = rel->schemaname;

    return true;
  }

  return false;
}

   
                                        
                                 
   
                                                                             
                                                                               
                                                                           
                                                                              
                                                                         
                                  
   
                                           
   
static bool
type_in_list_does_not_exist_skipping(List *typenames, const char **msg, char **name)
{
  ListCell *l;

  foreach (l, typenames)
  {
    TypeName *typeName = lfirst_node(TypeName, l);

    if (typeName != NULL)
    {
      if (!OidIsValid(LookupTypeNameOid(NULL, typeName, true)))
      {
                                                 
        if (schema_does_not_exist_skipping(typeName->names, msg, name))
        {
          return true;
        }

        *msg = gettext_noop("type \"%s\" does not exist, skipping");
        *name = TypeNameToString(typeName);

        return true;
      }
    }
  }

  return false;
}

   
                           
                                 
   
                                                                         
                                                                              
                                                                     
   
static void
does_not_exist_skipping(ObjectType objtype, Node *object)
{
  const char *msg = NULL;
  char *name = NULL;
  char *args = NULL;

  switch (objtype)
  {
  case OBJECT_ACCESS_METHOD:
    msg = gettext_noop("access method \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_TYPE:
  case OBJECT_DOMAIN:
  {
    TypeName *typ = castNode(TypeName, object);

    if (!schema_does_not_exist_skipping(typ->names, &msg, &name))
    {
      msg = gettext_noop("type \"%s\" does not exist, skipping");
      name = TypeNameToString(typ);
    }
  }
  break;
  case OBJECT_COLLATION:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("collation \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_CONVERSION:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("conversion \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_SCHEMA:
    msg = gettext_noop("schema \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_STATISTIC_EXT:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("statistics object \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_TSPARSER:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("text search parser \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_TSDICTIONARY:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("text search dictionary \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_TSTEMPLATE:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("text search template \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_TSCONFIGURATION:
    if (!schema_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("text search configuration \"%s\" does not exist, skipping");
      name = NameListToString(castNode(List, object));
    }
    break;
  case OBJECT_EXTENSION:
    msg = gettext_noop("extension \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_FUNCTION:
  {
    ObjectWithArgs *owa = castNode(ObjectWithArgs, object);

    if (!schema_does_not_exist_skipping(owa->objname, &msg, &name) && !type_in_list_does_not_exist_skipping(owa->objargs, &msg, &name))
    {
      msg = gettext_noop("function %s(%s) does not exist, skipping");
      name = NameListToString(owa->objname);
      args = TypeNameListToString(owa->objargs);
    }
    break;
  }
  case OBJECT_PROCEDURE:
  {
    ObjectWithArgs *owa = castNode(ObjectWithArgs, object);

    if (!schema_does_not_exist_skipping(owa->objname, &msg, &name) && !type_in_list_does_not_exist_skipping(owa->objargs, &msg, &name))
    {
      msg = gettext_noop("procedure %s(%s) does not exist, skipping");
      name = NameListToString(owa->objname);
      args = TypeNameListToString(owa->objargs);
    }
    break;
  }
  case OBJECT_ROUTINE:
  {
    ObjectWithArgs *owa = castNode(ObjectWithArgs, object);

    if (!schema_does_not_exist_skipping(owa->objname, &msg, &name) && !type_in_list_does_not_exist_skipping(owa->objargs, &msg, &name))
    {
      msg = gettext_noop("routine %s(%s) does not exist, skipping");
      name = NameListToString(owa->objname);
      args = TypeNameListToString(owa->objargs);
    }
    break;
  }
  case OBJECT_AGGREGATE:
  {
    ObjectWithArgs *owa = castNode(ObjectWithArgs, object);

    if (!schema_does_not_exist_skipping(owa->objname, &msg, &name) && !type_in_list_does_not_exist_skipping(owa->objargs, &msg, &name))
    {
      msg = gettext_noop("aggregate %s(%s) does not exist, skipping");
      name = NameListToString(owa->objname);
      args = TypeNameListToString(owa->objargs);
    }
    break;
  }
  case OBJECT_OPERATOR:
  {
    ObjectWithArgs *owa = castNode(ObjectWithArgs, object);

    if (!schema_does_not_exist_skipping(owa->objname, &msg, &name) && !type_in_list_does_not_exist_skipping(owa->objargs, &msg, &name))
    {
      msg = gettext_noop("operator %s does not exist, skipping");
      name = NameListToString(owa->objname);
    }
    break;
  }
  case OBJECT_LANGUAGE:
    msg = gettext_noop("language \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_CAST:
  {
    if (!type_in_list_does_not_exist_skipping(list_make1(linitial(castNode(List, object))), &msg, &name) && !type_in_list_does_not_exist_skipping(list_make1(lsecond(castNode(List, object))), &msg, &name))
    {
                                  
      msg = gettext_noop("cast from type %s to type %s does not exist, skipping");
      name = TypeNameToString(linitial_node(TypeName, castNode(List, object)));
      args = TypeNameToString(lsecond_node(TypeName, castNode(List, object)));
    }
  }
  break;
  case OBJECT_TRANSFORM:
    if (!type_in_list_does_not_exist_skipping(list_make1(linitial(castNode(List, object))), &msg, &name))
    {
      msg = gettext_noop("transform for type %s language \"%s\" does not exist, skipping");
      name = TypeNameToString(linitial_node(TypeName, castNode(List, object)));
      args = strVal(lsecond(castNode(List, object)));
    }
    break;
  case OBJECT_TRIGGER:
    if (!owningrel_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("trigger \"%s\" for relation \"%s\" does not exist, skipping");
      name = strVal(llast(castNode(List, object)));
      args = NameListToString(list_truncate(list_copy(castNode(List, object)), list_length(castNode(List, object)) - 1));
    }
    break;
  case OBJECT_POLICY:
    if (!owningrel_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("policy \"%s\" for relation \"%s\" does not exist, skipping");
      name = strVal(llast(castNode(List, object)));
      args = NameListToString(list_truncate(list_copy(castNode(List, object)), list_length(castNode(List, object)) - 1));
    }
    break;
  case OBJECT_EVENT_TRIGGER:
    msg = gettext_noop("event trigger \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_RULE:
    if (!owningrel_does_not_exist_skipping(castNode(List, object), &msg, &name))
    {
      msg = gettext_noop("rule \"%s\" for relation \"%s\" does not exist, skipping");
      name = strVal(llast(castNode(List, object)));
      args = NameListToString(list_truncate(list_copy(castNode(List, object)), list_length(castNode(List, object)) - 1));
    }
    break;
  case OBJECT_FDW:
    msg = gettext_noop("foreign-data wrapper \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_FOREIGN_SERVER:
    msg = gettext_noop("server \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  case OBJECT_OPCLASS:
  {
    List *opcname = list_copy_tail(castNode(List, object), 1);

    if (!schema_does_not_exist_skipping(opcname, &msg, &name))
    {
      msg = gettext_noop("operator class \"%s\" does not exist for access method \"%s\", skipping");
      name = NameListToString(opcname);
      args = strVal(linitial(castNode(List, object)));
    }
  }
  break;
  case OBJECT_OPFAMILY:
  {
    List *opfname = list_copy_tail(castNode(List, object), 1);

    if (!schema_does_not_exist_skipping(opfname, &msg, &name))
    {
      msg = gettext_noop("operator family \"%s\" does not exist for access method \"%s\", skipping");
      name = NameListToString(opfname);
      args = strVal(linitial(castNode(List, object)));
    }
  }
  break;
  case OBJECT_PUBLICATION:
    msg = gettext_noop("publication \"%s\" does not exist, skipping");
    name = strVal((Value *)object);
    break;
  default:
    elog(ERROR, "unrecognized object type: %d", (int)objtype);
    break;
  }

  if (!args)
  {
    ereport(NOTICE, (errmsg(msg, name)));
  }
  else
  {
    ereport(NOTICE, (errmsg(msg, name, args)));
  }
}
