                                                                            
   
                
                                                    
   
   
                                                                         
                                                                        
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_init_privs.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_publication_rel.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/policy.h"
#include "commands/proclang.h"
#include "commands/publicationcmds.h"
#include "commands/schemacmds.h"
#include "commands/seclabel.h"
#include "commands/sequence.h"
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteRemove.h"
#include "storage/lmgr.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                                                                             
                                                                         
                                                                         
   
typedef struct
{
  int flags;                                                      
  ObjectAddress dependee;                                            
} ObjectAddressExtra;

                                  
#define DEPFLAG_ORIGINAL 0x0001                                   
#define DEPFLAG_NORMAL 0x0002                                       
#define DEPFLAG_AUTO 0x0004                                       
#define DEPFLAG_INTERNAL 0x0008                                       
#define DEPFLAG_PARTITION 0x0010                                       
#define DEPFLAG_EXTENSION 0x0020                                       
#define DEPFLAG_REVERSE 0x0040                                        
#define DEPFLAG_IS_PART 0x0080                                   
#define DEPFLAG_SUBOBJECT 0x0100                                            

                                        
struct ObjectAddresses
{
  ObjectAddress *refs;                               
  ObjectAddressExtra *extras;                                             
  int numrefs;                                                  
  int maxrefs;                                                       
};

                                                     

                                                               
typedef struct ObjectAddressStack
{
  const ObjectAddress *object;                               
  int flags;                                                  
  struct ObjectAddressStack *next;                             
} ObjectAddressStack;

                                               
typedef struct
{
  ObjectAddress obj;                                             
  int subflags;                                                    
} ObjectAddressAndFlags;

                                     
typedef struct
{
  ObjectAddresses *addrs;                                  
  List *rtables;                                                   
} find_expr_references_context;

   
                                                                             
                              
   
static const Oid object_classes[] = {
    RelationRelationId,                                
    ProcedureRelationId,                              
    TypeRelationId,                                   
    CastRelationId,                                   
    CollationRelationId,                                   
    ConstraintRelationId,                                   
    ConversionRelationId,                                   
    AttrDefaultRelationId,                               
    LanguageRelationId,                                   
    LargeObjectRelationId,                                   
    OperatorRelationId,                                   
    OperatorClassRelationId,                             
    OperatorFamilyRelationId,                             
    AccessMethodRelationId,                         
    AccessMethodOperatorRelationId,                   
    AccessMethodProcedureRelationId,                    
    RewriteRelationId,                                   
    TriggerRelationId,                                   
    NamespaceRelationId,                                
    StatisticExtRelationId,                                    
    TSParserRelationId,                                   
    TSDictionaryRelationId,                             
    TSTemplateRelationId,                                   
    TSConfigRelationId,                                   
    AuthIdRelationId,                                 
    DatabaseRelationId,                                   
    TableSpaceRelationId,                                 
    ForeignDataWrapperRelationId,                    
    ForeignServerRelationId,                                    
    UserMappingRelationId,                                    
    DefaultAclRelationId,                               
    ExtensionRelationId,                                   
    EventTriggerRelationId,                                    
    PolicyRelationId,                                   
    PublicationRelationId,                                   
    PublicationRelRelationId,                                    
    SubscriptionRelationId,                                   
    TransformRelationId                                    
};

static void
findDependentObjects(const ObjectAddress *object, int objflags, int flags, ObjectAddressStack *stack, ObjectAddresses *targetObjects, const ObjectAddresses *pendingObjects, Relation *depRel);
static void
reportDependentObjects(const ObjectAddresses *targetObjects, DropBehavior behavior, int flags, const ObjectAddress *origObject);
static void
deleteOneObject(const ObjectAddress *object, Relation *depRel, int32 flags);
static void
doDeletion(const ObjectAddress *object, int flags);
static bool
find_expr_references_walker(Node *node, find_expr_references_context *context);
static void
eliminate_duplicate_dependencies(ObjectAddresses *addrs);
static int
object_address_comparator(const void *a, const void *b);
static void
add_object_address(ObjectClass oclass, Oid objectId, int32 subId, ObjectAddresses *addrs);
static void
add_exact_object_address_extra(const ObjectAddress *object, const ObjectAddressExtra *extra, ObjectAddresses *addrs);
static bool
object_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddresses *addrs);
static bool
stack_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddressStack *stack);
static void
DeleteInitPrivs(const ObjectAddress *object);

   
                                                                               
                        
   
static void
deleteObjectsInList(ObjectAddresses *targetObjects, Relation *depRel, int flags)
{
  int i;

     
                                                             
     
  if (trackDroppedObjectsNeeded() && !(flags & PERFORM_DELETION_INTERNAL))
  {
    for (i = 0; i < targetObjects->numrefs; i++)
    {
      const ObjectAddress *thisobj = &targetObjects->refs[i];
      const ObjectAddressExtra *extra = &targetObjects->extras[i];
      bool original = false;
      bool normal = false;

      if (extra->flags & DEPFLAG_ORIGINAL)
      {
        original = true;
      }
      if (extra->flags & DEPFLAG_NORMAL)
      {
        normal = true;
      }
      if (extra->flags & DEPFLAG_REVERSE)
      {
        normal = true;
      }

      if (EventTriggerSupportsObjectClass(getObjectClass(thisobj)))
      {
        EventTriggerSQLDropAddObject(thisobj, original, normal);
      }
    }
  }

     
                                                                            
                                         
     
  for (i = 0; i < targetObjects->numrefs; i++)
  {
    ObjectAddress *thisobj = targetObjects->refs + i;
    ObjectAddressExtra *thisextra = targetObjects->extras + i;

    if ((flags & PERFORM_DELETION_SKIP_ORIGINAL) && (thisextra->flags & DEPFLAG_ORIGINAL))
    {
      continue;
    }

    deleteOneObject(thisobj, depRel, flags);
  }
}

   
                                                                      
                                                                         
                                                                           
                                                                      
                                     
   
                                                                             
                                                                             
                                                                             
                         
   
                                           
   
                                                                           
                                                                            
                                                                            
                                                                             
                                                                             
                                                                               
                                                                              
   
                                                                            
                                                                             
                                                      
   
                                                                         
   
                                                                          
                                     
   
                                                                         
                                                                          
                                                 
   
                                                                               
                                                                    
   
   
void
performDeletion(const ObjectAddress *object, DropBehavior behavior, int flags)
{
  Relation depRel;
  ObjectAddresses *targetObjects;

     
                                                                        
                                                                
     
  depRel = table_open(DependRelationId, RowExclusiveLock);

     
                                                                          
                                                              
     
  AcquireDeletionLock(object, 0);

     
                                                                      
                                                         
     
  targetObjects = new_object_addresses();

  findDependentObjects(object, DEPFLAG_ORIGINAL, flags, NULL,                  
      targetObjects, NULL,                                                           
      &depRel);

     
                                                                      
     
  reportDependentObjects(targetObjects, behavior, flags, object);

                   
  deleteObjectsInList(targetObjects, &depRel, flags);

                    
  free_object_addresses(targetObjects);

  table_close(depRel, RowExclusiveLock);
}

   
                                                                             
                    
   
                                                                               
                                                                           
                                                                            
                                     
   
void
performMultipleDeletions(const ObjectAddresses *objects, DropBehavior behavior, int flags)
{
  Relation depRel;
  ObjectAddresses *targetObjects;
  int i;

                                
  if (objects->numrefs <= 0)
  {
    return;
  }

     
                                                                        
                                                                
     
  depRel = table_open(DependRelationId, RowExclusiveLock);

     
                                                                       
                                                                      
                                                                          
                                                                            
                                                                           
                                        
     
  targetObjects = new_object_addresses();

  for (i = 0; i < objects->numrefs; i++)
  {
    const ObjectAddress *thisobj = objects->refs + i;

       
                                                                         
                                                                    
       
    AcquireDeletionLock(thisobj, flags);

    findDependentObjects(thisobj, DEPFLAG_ORIGINAL, flags, NULL,                  
        targetObjects, objects, &depRel);
  }

     
                                                                      
     
                                                                            
                                                      
     
  reportDependentObjects(targetObjects, behavior, flags, (objects->numrefs == 1 ? objects->refs : NULL));

                   
  deleteObjectsInList(targetObjects, &depRel, flags);

                    
  free_object_addresses(targetObjects);

  table_close(depRel, RowExclusiveLock);
}

   
                                                                   
   
                                                                            
                                                                       
                                                                             
                                                                          
                                                                    
   
                                                                    
                                                                         
                                                                         
                                                                          
                                                                           
                               
   
                                                                      
                        
   
                                                                       
                                                                    
                                                                           
                                                                           
                                                                     
                                                                   
                                                                     
                                                                
                                              
   
                                                                           
                                                                            
                                                                
   
static void
findDependentObjects(const ObjectAddress *object, int objflags, int flags, ObjectAddressStack *stack, ObjectAddresses *targetObjects, const ObjectAddresses *pendingObjects, Relation *depRel)
{
  ScanKeyData key[3];
  int nkeys;
  SysScanDesc scan;
  HeapTuple tup;
  ObjectAddress otherObject;
  ObjectAddress owningObject;
  ObjectAddress partitionObject;
  ObjectAddressAndFlags *dependentObjects;
  int numDependentObjects;
  int maxDependentObjects;
  ObjectAddressStack mystack;
  ObjectAddressExtra extra;

     
                                                                         
                                                                          
                                                                        
                   
     
                                                                            
                                                                       
                                                                           
                                                                         
                                                                           
                                                                          
                                                                           
                                                                           
                                    
     
  if (stack_address_present_add_flags(object, objflags, stack))
  {
    return;
  }

     
                                                                           
                                                                         
                                                 
     
                                                                        
                                                                            
                                  
     
  if (object_address_present_add_flags(object, objflags, targetObjects))
  {
    return;
  }

     
                                                                          
                                                                            
                                                                           
                                                                            
                                                                            
                                                                             
                                                                           
                                  
     
  ScanKeyInit(&key[0], Anum_pg_depend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&key[1], Anum_pg_depend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  if (object->objectSubId != 0)
  {
                                                       
    ScanKeyInit(&key[2], Anum_pg_depend_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));
    nkeys = 3;
  }
  else
  {
                                                                         
    nkeys = 2;
  }

  scan = systable_beginscan(*depRel, DependDependerIndexId, true, NULL, nkeys, key);

                                               
  memset(&owningObject, 0, sizeof(owningObject));
  memset(&partitionObject, 0, sizeof(partitionObject));

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_depend foundDep = (Form_pg_depend)GETSTRUCT(tup);

    otherObject.classId = foundDep->refclassid;
    otherObject.objectId = foundDep->refobjid;
    otherObject.objectSubId = foundDep->refobjsubid;

       
                                                                      
                                                                           
                                                                        
                                                                        
                                 
       
    if (otherObject.classId == object->classId && otherObject.objectId == object->objectId && object->objectSubId == 0)
    {
      continue;
    }

    switch (foundDep->deptype)
    {
    case DEPENDENCY_NORMAL:
    case DEPENDENCY_AUTO:
    case DEPENDENCY_AUTO_EXTENSION:
                      
      break;

    case DEPENDENCY_EXTENSION:

         
                                                                     
                                                                     
                                                                     
                                     
         
      if (flags & PERFORM_DELETION_SKIP_EXTENSIONS)
      {
        break;
      }

         
                                                              
                                                                   
                                                               
                                                                   
                                                                  
                        
         
      if (creating_extension && otherObject.classId == ExtensionRelationId && otherObject.objectId == CurrentExtensionObject)
      {
        break;
      }

                                                             
                     

    case DEPENDENCY_INTERNAL:

         
                                                               
                                                                 
                                             
         
                                                                   
                                                           
                                                                    
                                                                     
                              
         
                                                                  
                                                                 
                                                                 
                                                                    
                                                          
         
      if (stack == NULL)
      {
        if (pendingObjects && object_address_present(&otherObject, pendingObjects))
        {
          systable_endscan(scan);
                                                              
          ReleaseDeletionLock(object);
          return;
        }

           
                                                                
                                                             
                                                                
                                                               
                                                                   
                                                                  
                                                              
                       
           
        if (!OidIsValid(owningObject.classId) || foundDep->deptype == DEPENDENCY_EXTENSION)
        {
          owningObject = otherObject;
        }
        break;
      }

         
                                                                  
                                                                   
                                                                 
                                                                 
                                                                     
                                                
         
      if (stack_address_present_add_flags(&otherObject, 0, stack))
      {
        break;
      }

         
                                                             
                                                               
                        
         
                                                             
                                                               
                                                              
                                         
         
      ReleaseDeletionLock(object);
      AcquireDeletionLock(&otherObject, 0);

         
                                                                   
                                                                  
                                                            
                                            
         
      if (!systable_recheck_tuple(scan, tup))
      {
        systable_endscan(scan);
        ReleaseDeletionLock(&otherObject);
        return;
      }

         
                                                                  
                                                             
                               
         
      systable_endscan(scan);

         
                                                                   
         
                                                                 
                                                                
                                                          
         
                                                                   
                                                                    
                                                                   
         
      findDependentObjects(&otherObject, DEPFLAG_REVERSE, flags, stack, targetObjects, pendingObjects, depRel);

         
                                                             
                                                                  
                                                             
                                                                   
                                                                    
                                                                
                                                                 
         
                                                                
                                                                   
                                                                 
                                                                
                                                                 
                                                                  
         
      if (!object_address_present_add_flags(object, objflags, targetObjects))
      {
        elog(ERROR, "deletion of owning object %s failed to delete %s", getObjectDescription(&otherObject), getObjectDescription(object));
      }

                                
      return;

    case DEPENDENCY_PARTITION_PRI:

         
                                                                    
                                                                     
                                                               
         
      objflags |= DEPFLAG_IS_PART;

         
                                                              
                                                                
                                                                  
         
      partitionObject = otherObject;
      break;

    case DEPENDENCY_PARTITION_SEC:

         
                                                                     
                                                                  
         
      if (!(objflags & DEPFLAG_IS_PART))
      {
        partitionObject = otherObject;
      }

         
                                                                    
                                                                     
                                                               
         
      objflags |= DEPFLAG_IS_PART;
      break;

    case DEPENDENCY_PIN:

         
                                                                   
                                
         
      elog(ERROR, "incorrect use of PIN dependency with %s", getObjectDescription(object));
      break;
    default:
      elog(ERROR, "unrecognized dependency type '%c' for %s", foundDep->deptype, getObjectDescription(object));
      break;
    }
  }

  systable_endscan(scan);

     
                                                                         
                                                                             
                                                                          
                                          
     
  if (OidIsValid(owningObject.classId))
  {
    char *otherObjDesc;

    if (OidIsValid(partitionObject.classId))
    {
      otherObjDesc = getObjectDescription(&partitionObject);
    }
    else
    {
      otherObjDesc = getObjectDescription(&owningObject);
    }

    ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop %s because %s requires it", getObjectDescription(object), otherObjDesc), errhint("You can drop %s instead.", otherObjDesc)));
  }

     
                                                                            
                                                                 
                                                                         
                                                                       
                                                                        
                          
     
  maxDependentObjects = 128;                                   
  dependentObjects = (ObjectAddressAndFlags *)palloc(maxDependentObjects * sizeof(ObjectAddressAndFlags));
  numDependentObjects = 0;

  ScanKeyInit(&key[0], Anum_pg_depend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&key[1], Anum_pg_depend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  if (object->objectSubId != 0)
  {
    ScanKeyInit(&key[2], Anum_pg_depend_refobjsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));
    nkeys = 3;
  }
  else
  {
    nkeys = 2;
  }

  scan = systable_beginscan(*depRel, DependReferenceIndexId, true, NULL, nkeys, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_depend foundDep = (Form_pg_depend)GETSTRUCT(tup);
    int subflags;

    otherObject.classId = foundDep->classid;
    otherObject.objectId = foundDep->objid;
    otherObject.objectSubId = foundDep->objsubid;

       
                                                                           
                                                                       
                                                            
       
    if (otherObject.classId == object->classId && otherObject.objectId == object->objectId && object->objectSubId == 0)
    {
      continue;
    }

       
                                                              
       
    AcquireDeletionLock(&otherObject, 0);

       
                                                                       
                                                                         
                                                                          
                                                                       
                                                                   
       
    if (!systable_recheck_tuple(scan, tup))
    {
                                        
      ReleaseDeletionLock(&otherObject);
                                                  
      continue;
    }

       
                                                                        
                                            
       
    switch (foundDep->deptype)
    {
    case DEPENDENCY_NORMAL:
      subflags = DEPFLAG_NORMAL;
      break;
    case DEPENDENCY_AUTO:
    case DEPENDENCY_AUTO_EXTENSION:
      subflags = DEPFLAG_AUTO;
      break;
    case DEPENDENCY_INTERNAL:
      subflags = DEPFLAG_INTERNAL;
      break;
    case DEPENDENCY_PARTITION_PRI:
    case DEPENDENCY_PARTITION_SEC:
      subflags = DEPFLAG_PARTITION;
      break;
    case DEPENDENCY_EXTENSION:
      subflags = DEPFLAG_EXTENSION;
      break;
    case DEPENDENCY_PIN:

         
                                                                 
                                        
         
      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop %s because it is required by the database system", getObjectDescription(object))));
      subflags = 0;                          
      break;
    default:
      elog(ERROR, "unrecognized dependency type '%c' for %s", foundDep->deptype, getObjectDescription(object));
      subflags = 0;                          
      break;
    }

                                                
    if (numDependentObjects >= maxDependentObjects)
    {
                                   
      maxDependentObjects *= 2;
      dependentObjects = (ObjectAddressAndFlags *)repalloc(dependentObjects, maxDependentObjects * sizeof(ObjectAddressAndFlags));
    }

    dependentObjects[numDependentObjects].obj = otherObject;
    dependentObjects[numDependentObjects].subflags = subflags;
    numDependentObjects++;
  }

  systable_endscan(scan);

     
                                                                           
                                                                            
                                         
     
  if (numDependentObjects > 1)
  {
    qsort((void *)dependentObjects, numDependentObjects, sizeof(ObjectAddressAndFlags), object_address_comparator);
  }

     
                                                                           
                                                        
     
  mystack.object = object;                               
  mystack.flags = objflags;
  mystack.next = stack;

  for (int i = 0; i < numDependentObjects; i++)
  {
    ObjectAddressAndFlags *depObj = dependentObjects + i;

    findDependentObjects(&depObj->obj, depObj->subflags, flags, &mystack, targetObjects, pendingObjects, depRel);
  }

  pfree(dependentObjects);

     
                                                                            
                                                                             
                                                                       
                                                                           
                                                                           
                                 
     
  extra.flags = mystack.flags;
  if (extra.flags & DEPFLAG_IS_PART)
  {
    extra.dependee = partitionObject;
  }
  else if (stack)
  {
    extra.dependee = *stack->object;
  }
  else
  {
    memset(&extra.dependee, 0, sizeof(extra.dependee));
  }
  add_exact_object_address_extra(object, &extra, targetObjects);
}

   
                                                                            
   
                                                                     
                                                                  
                                                              
   
                                                                   
                                 
                                                 
                                                                 
                                           
   
static void
reportDependentObjects(const ObjectAddresses *targetObjects, DropBehavior behavior, int flags, const ObjectAddress *origObject)
{
  int msglevel = (flags & PERFORM_DELETION_QUIETLY) ? DEBUG2 : NOTICE;
  bool ok = true;
  StringInfoData clientdetail;
  StringInfoData logdetail;
  int numReportedClient = 0;
  int numNotReportedClient = 0;
  int i;

     
                                                                          
                                                                            
                                                                     
                               
     
                                                                            
                                                                            
                           
     
  for (i = 0; i < targetObjects->numrefs; i++)
  {
    const ObjectAddressExtra *extra = &targetObjects->extras[i];

    if ((extra->flags & DEPFLAG_IS_PART) && !(extra->flags & DEPFLAG_PARTITION))
    {
      const ObjectAddress *object = &targetObjects->refs[i];
      char *otherObjDesc = getObjectDescription(&extra->dependee);

      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop %s because %s requires it", getObjectDescription(object), otherObjDesc), errhint("You can drop %s instead.", otherObjDesc)));
    }
  }

     
                                                                             
                                                                           
               
     
                                                                      
                                                                           
                                                                             
                            
     
  if (behavior == DROP_CASCADE && msglevel < client_min_messages && (msglevel < log_min_messages || log_min_messages == LOG))
  {
    return;
  }

     
                                                                   
                                                                     
                                                                        
     
#define MAX_REPORTED_DEPS 100

  initStringInfo(&clientdetail);
  initStringInfo(&logdetail);

     
                                                                             
                                                                 
     
  for (i = targetObjects->numrefs - 1; i >= 0; i--)
  {
    const ObjectAddress *obj = &targetObjects->refs[i];
    const ObjectAddressExtra *extra = &targetObjects->extras[i];
    char *objDesc;

                                                
    if (extra->flags & DEPFLAG_ORIGINAL)
    {
      continue;
    }

                                                                          
    if (extra->flags & DEPFLAG_SUBOBJECT)
    {
      continue;
    }

    objDesc = getObjectDescription(obj);

                                                                          
    if (objDesc == NULL)
    {
      continue;
    }

       
                                                                           
                                                                        
                                                
       
    if (extra->flags & (DEPFLAG_AUTO | DEPFLAG_INTERNAL | DEPFLAG_PARTITION | DEPFLAG_EXTENSION))
    {
         
                                                                       
                                                                  
                                                                
                                         
         
      ereport(DEBUG2, (errmsg("drop auto-cascades to %s", objDesc)));
    }
    else if (behavior == DROP_RESTRICT)
    {
      char *otherDesc = getObjectDescription(&extra->dependee);

      if (otherDesc)
      {
        if (numReportedClient < MAX_REPORTED_DEPS)
        {
                                               
          if (clientdetail.len != 0)
          {
            appendStringInfoChar(&clientdetail, '\n');
          }
          appendStringInfo(&clientdetail, _("%s depends on %s"), objDesc, otherDesc);
          numReportedClient++;
        }
        else
        {
          numNotReportedClient++;
        }
                                             
        if (logdetail.len != 0)
        {
          appendStringInfoChar(&logdetail, '\n');
        }
        appendStringInfo(&logdetail, _("%s depends on %s"), objDesc, otherDesc);
        pfree(otherDesc);
      }
      else
      {
        numNotReportedClient++;
      }
      ok = false;
    }
    else
    {
      if (numReportedClient < MAX_REPORTED_DEPS)
      {
                                             
        if (clientdetail.len != 0)
        {
          appendStringInfoChar(&clientdetail, '\n');
        }
        appendStringInfo(&clientdetail, _("drop cascades to %s"), objDesc);
        numReportedClient++;
      }
      else
      {
        numNotReportedClient++;
      }
                                           
      if (logdetail.len != 0)
      {
        appendStringInfoChar(&logdetail, '\n');
      }
      appendStringInfo(&logdetail, _("drop cascades to %s"), objDesc);
    }

    pfree(objDesc);
  }

  if (numNotReportedClient > 0)
  {
    appendStringInfo(&clientdetail,
        ngettext("\nand %d other object "
                 "(see server log for list)",
            "\nand %d other objects "
            "(see server log for list)",
            numNotReportedClient),
        numNotReportedClient);
  }

  if (!ok)
  {
    if (origObject)
    {
      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop %s because other objects depend on it", getObjectDescription(origObject)), errdetail_internal("%s", clientdetail.data), errdetail_log("%s", logdetail.data), errhint("Use DROP ... CASCADE to drop the dependent objects too.")));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop desired object(s) because other objects depend on them"), errdetail_internal("%s", clientdetail.data), errdetail_log("%s", logdetail.data), errhint("Use DROP ... CASCADE to drop the dependent objects too.")));
    }
  }
  else if (numReportedClient > 1)
  {
    ereport(msglevel,
                                                             
        (errmsg_plural("drop cascades to %d other object", "drop cascades to %d other objects", numReportedClient + numNotReportedClient, numReportedClient + numNotReportedClient), errdetail_internal("%s", clientdetail.data), errdetail_log("%s", logdetail.data)));
  }
  else if (numReportedClient == 1)
  {
                                           
    ereport(msglevel, (errmsg_internal("%s", clientdetail.data)));
  }

  pfree(clientdetail.data);
  pfree(logdetail.data);
}

   
                                                                
   
                                                   
   
static void
deleteOneObject(const ObjectAddress *object, Relation *depRel, int flags)
{
  ScanKeyData key[3];
  int nkeys;
  SysScanDesc scan;
  HeapTuple tup;

                                              
  InvokeObjectDropHookArg(object->classId, object->objectId, object->objectSubId, flags);

     
                                                                            
                                                                          
                                        
     
  if (flags & PERFORM_DELETION_CONCURRENTLY)
  {
    table_close(*depRel, RowExclusiveLock);
  }

     
                                                                
     
                                                                             
                                                                           
                                                                            
                                                                        
                                                         
     
  doDeletion(object, flags);

     
                                         
     
  if (flags & PERFORM_DELETION_CONCURRENTLY)
  {
    *depRel = table_open(DependRelationId, RowExclusiveLock);
  }

     
                                                                            
                                                                  
     
                                                                            
                              
     
  ScanKeyInit(&key[0], Anum_pg_depend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&key[1], Anum_pg_depend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  if (object->objectSubId != 0)
  {
    ScanKeyInit(&key[2], Anum_pg_depend_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));
    nkeys = 3;
  }
  else
  {
    nkeys = 2;
  }

  scan = systable_beginscan(*depRel, DependDependerIndexId, true, NULL, nkeys, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    CatalogTupleDelete(*depRel, &tup->t_self);
  }

  systable_endscan(scan);

     
                                                                            
                                                    
     
  deleteSharedDependencyRecordsFor(object->classId, object->objectId, object->objectSubId);

     
                                                                            
                                                                        
                                                          
     
  DeleteComments(object->objectId, object->classId, object->objectSubId);
  DeleteSecurityLabel(object);
  DeleteInitPrivs(object);

     
                                                                           
                                        
     
  CommandCounterIncrement();

     
                     
     
}

   
                                               
   
static void
doDeletion(const ObjectAddress *object, int flags)
{
  switch (getObjectClass(object))
  {
  case OCLASS_CLASS:
  {
    char relKind = get_rel_relkind(object->objectId);

    if (relKind == RELKIND_INDEX || relKind == RELKIND_PARTITIONED_INDEX)
    {
      bool concurrent = ((flags & PERFORM_DELETION_CONCURRENTLY) != 0);
      bool concurrent_lock_mode = ((flags & PERFORM_DELETION_CONCURRENT_LOCK) != 0);

      Assert(object->objectSubId == 0);
      index_drop(object->objectId, concurrent, concurrent_lock_mode);
    }
    else
    {
      if (object->objectSubId != 0)
      {
        RemoveAttributeById(object->objectId, object->objectSubId);
      }
      else
      {
        heap_drop_with_catalog(object->objectId);
      }
    }

       
                                                              
                                
       
    if (relKind == RELKIND_SEQUENCE)
    {
      DeleteSequenceTuple(object->objectId);
    }
    break;
  }

  case OCLASS_PROC:
    RemoveFunctionById(object->objectId);
    break;

  case OCLASS_TYPE:
    RemoveTypeById(object->objectId);
    break;

  case OCLASS_CAST:
    DropCastById(object->objectId);
    break;

  case OCLASS_COLLATION:
    RemoveCollationById(object->objectId);
    break;

  case OCLASS_CONSTRAINT:
    RemoveConstraintById(object->objectId);
    break;

  case OCLASS_CONVERSION:
    RemoveConversionById(object->objectId);
    break;

  case OCLASS_DEFAULT:
    RemoveAttrDefaultById(object->objectId);
    break;

  case OCLASS_LANGUAGE:
    DropProceduralLanguageById(object->objectId);
    break;

  case OCLASS_LARGEOBJECT:
    LargeObjectDrop(object->objectId);
    break;

  case OCLASS_OPERATOR:
    RemoveOperatorById(object->objectId);
    break;

  case OCLASS_OPCLASS:
    RemoveOpClassById(object->objectId);
    break;

  case OCLASS_OPFAMILY:
    RemoveOpFamilyById(object->objectId);
    break;

  case OCLASS_AM:
    RemoveAccessMethodById(object->objectId);
    break;

  case OCLASS_AMOP:
    RemoveAmOpEntryById(object->objectId);
    break;

  case OCLASS_AMPROC:
    RemoveAmProcEntryById(object->objectId);
    break;

  case OCLASS_REWRITE:
    RemoveRewriteRuleById(object->objectId);
    break;

  case OCLASS_TRIGGER:
    RemoveTriggerById(object->objectId);
    break;

  case OCLASS_SCHEMA:
    RemoveSchemaById(object->objectId);
    break;

  case OCLASS_STATISTIC_EXT:
    RemoveStatisticsById(object->objectId);
    break;

  case OCLASS_TSPARSER:
    RemoveTSParserById(object->objectId);
    break;

  case OCLASS_TSDICT:
    RemoveTSDictionaryById(object->objectId);
    break;

  case OCLASS_TSTEMPLATE:
    RemoveTSTemplateById(object->objectId);
    break;

  case OCLASS_TSCONFIG:
    RemoveTSConfigurationById(object->objectId);
    break;

       
                                                                       
                    
       

  case OCLASS_FDW:
    RemoveForeignDataWrapperById(object->objectId);
    break;

  case OCLASS_FOREIGN_SERVER:
    RemoveForeignServerById(object->objectId);
    break;

  case OCLASS_USER_MAPPING:
    RemoveUserMappingById(object->objectId);
    break;

  case OCLASS_DEFACL:
    RemoveDefaultACLById(object->objectId);
    break;

  case OCLASS_EXTENSION:
    RemoveExtensionById(object->objectId);
    break;

  case OCLASS_EVENT_TRIGGER:
    RemoveEventTriggerById(object->objectId);
    break;

  case OCLASS_POLICY:
    RemovePolicyById(object->objectId);
    break;

  case OCLASS_PUBLICATION:
    RemovePublicationById(object->objectId);
    break;

  case OCLASS_PUBLICATION_REL:
    RemovePublicationRelById(object->objectId);
    break;

  case OCLASS_TRANSFORM:
    DropTransformById(object->objectId);
    break;

       
                                                         
       
  case OCLASS_ROLE:
  case OCLASS_DATABASE:
  case OCLASS_TBLSPACE:
  case OCLASS_SUBSCRIPTION:
    elog(ERROR, "global objects cannot be deleted by doDeletion");
    break;

       
                                                                
                                                                   
       
  }
}

   
                                                                        
   
                                                                    
                                                 
   
                                                                        
                                                                      
                                                                           
   
void
AcquireDeletionLock(const ObjectAddress *object, int flags)
{
  if (object->classId == RelationRelationId)
  {
       
                                                                         
                                                                          
                                                                      
             
       
    if (flags & PERFORM_DELETION_CONCURRENTLY)
    {
      LockRelationOid(object->objectId, ShareUpdateExclusiveLock);
    }
    else
    {
      LockRelationOid(object->objectId, AccessExclusiveLock);
    }
  }
  else
  {
                                                                 
    LockDatabaseObject(object->classId, object->objectId, 0, AccessExclusiveLock);
  }
}

   
                                                         
   
                                     
   
void
ReleaseDeletionLock(const ObjectAddress *object)
{
  if (object->classId == RelationRelationId)
  {
    UnlockRelationOid(object->objectId, AccessExclusiveLock);
  }
  else
  {
                                                                 
    UnlockDatabaseObject(object->classId, object->objectId, 0, AccessExclusiveLock);
  }
}

   
                                                         
   
                                                                           
        
   
                                                                        
                                                                      
                                                                     
                                               
   
                                                                             
                                                    
   
void
recordDependencyOnExpr(const ObjectAddress *depender, Node *expr, List *rtable, DependencyType behavior)
{
  find_expr_references_context context;

  context.addrs = new_object_addresses();

                                                         
  context.rtables = list_make1(rtable);

                                                          
  find_expr_references_walker(expr, &context);

                             
  eliminate_duplicate_dependencies(context.addrs);

                      
  recordMultipleDependencies(depender, context.addrs->refs, context.addrs->numrefs, behavior);

  free_object_addresses(context.addrs);
}

   
                                                                  
   
                                                                      
                                                                       
                                                                     
                                                                     
                                                                           
                                                                         
                                          
   
                                                                       
                                                                               
                                                                        
                                                                           
                                                             
   
void
recordDependencyOnSingleRelExpr(const ObjectAddress *depender, Node *expr, Oid relId, DependencyType behavior, DependencyType self_behavior, bool reverse_self)
{
  find_expr_references_context context;
  RangeTblEntry rte;

  context.addrs = new_object_addresses();

                                                               
  MemSet(&rte, 0, sizeof(rte));
  rte.type = T_RangeTblEntry;
  rte.rtekind = RTE_RELATION;
  rte.relid = relId;
  rte.relkind = RELKIND_RELATION;                                 
  rte.rellockmode = AccessShareLock;

  context.rtables = list_make1(list_make1(&rte));

                                                          
  find_expr_references_walker(expr, &context);

                             
  eliminate_duplicate_dependencies(context.addrs);

                                               
  if ((behavior != self_behavior || reverse_self) && context.addrs->numrefs > 0)
  {
    ObjectAddresses *self_addrs;
    ObjectAddress *outobj;
    int oldref, outrefs;

    self_addrs = new_object_addresses();

    outobj = context.addrs->refs;
    outrefs = 0;
    for (oldref = 0; oldref < context.addrs->numrefs; oldref++)
    {
      ObjectAddress *thisobj = context.addrs->refs + oldref;

      if (thisobj->classId == RelationRelationId && thisobj->objectId == relId)
      {
                                           
        add_exact_object_address(thisobj, self_addrs);
      }
      else
      {
                                      
        *outobj = *thisobj;
        outobj++;
        outrefs++;
      }
    }
    context.addrs->numrefs = outrefs;

                                                                     
    if (!reverse_self)
    {
      recordMultipleDependencies(depender, self_addrs->refs, self_addrs->numrefs, self_behavior);
    }
    else
    {
                                                                       
      int selfref;

      for (selfref = 0; selfref < self_addrs->numrefs; selfref++)
      {
        ObjectAddress *thisobj = self_addrs->refs + selfref;

        recordDependencyOn(thisobj, depender, self_behavior);
      }
    }

    free_object_addresses(self_addrs);
  }

                                        
  recordMultipleDependencies(depender, context.addrs->refs, context.addrs->numrefs, behavior);

  free_object_addresses(context.addrs);
}

   
                                                                
   
                                                                            
                                                                              
                                                                       
                                                                         
                                                     
   
                                                                              
                                                                            
                                                                               
                                                                             
                                                                           
                                                                          
   
                                                                              
                                                                
   
static bool
find_expr_references_walker(Node *node, find_expr_references_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;
    List *rtable;
    RangeTblEntry *rte;

                                                              
    if (var->varlevelsup >= list_length(context->rtables))
    {
      elog(ERROR, "invalid varlevelsup %d", var->varlevelsup);
    }
    rtable = (List *)list_nth(context->rtables, var->varlevelsup);
    if (var->varno <= 0 || var->varno > list_length(rtable))
    {
      elog(ERROR, "invalid varno %d", var->varno);
    }
    rte = rt_fetch(var->varno, rtable);

       
                                                                      
                                                                      
                                                                      
                                                                  
                                                                        
                                                                         
                                                                       
                                                                      
                                                                  
       
    if (var->varattno == InvalidAttrNumber)
    {
      return false;
    }
    if (rte->rtekind == RTE_RELATION)
    {
                                                           
      add_object_address(OCLASS_CLASS, rte->relid, var->varattno, context->addrs);
    }
    else if (rte->rtekind == RTE_JOIN)
    {
                                                                    
      List *save_rtables;

                                                                 
      save_rtables = context->rtables;
      context->rtables = list_copy_tail(context->rtables, var->varlevelsup);
      if (var->varattno <= 0 || var->varattno > list_length(rte->joinaliasvars))
      {
        elog(ERROR, "invalid varattno %d", var->varattno);
      }
      find_expr_references_walker((Node *)list_nth(rte->joinaliasvars, var->varattno - 1), context);
      list_free(context->rtables);
      context->rtables = save_rtables;
    }
    return false;
  }
  else if (IsA(node, Const))
  {
    Const *con = (Const *)node;
    Oid objoid;

                                                           
    add_object_address(OCLASS_TYPE, con->consttype, 0, context->addrs);

       
                                                                    
                                                                           
                                                                       
                                                                           
       
    if (OidIsValid(con->constcollid) && con->constcollid != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, con->constcollid, 0, context->addrs);
    }

       
                                                                      
                                                                     
                                                                       
                                                              
       
    if (!con->constisnull)
    {
      switch (con->consttype)
      {
      case REGPROCOID:
      case REGPROCEDUREOID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(PROCOID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_PROC, objoid, 0, context->addrs);
        }
        break;
      case REGOPEROID:
      case REGOPERATOROID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(OPEROID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_OPERATOR, objoid, 0, context->addrs);
        }
        break;
      case REGCLASSOID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(RELOID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_CLASS, objoid, 0, context->addrs);
        }
        break;
      case REGTYPEOID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(TYPEOID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_TYPE, objoid, 0, context->addrs);
        }
        break;
      case REGCONFIGOID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(TSCONFIGOID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_TSCONFIG, objoid, 0, context->addrs);
        }
        break;
      case REGDICTIONARYOID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(TSDICTOID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_TSDICT, objoid, 0, context->addrs);
        }
        break;

      case REGNAMESPACEOID:
        objoid = DatumGetObjectId(con->constvalue);
        if (SearchSysCacheExists1(NAMESPACEOID, ObjectIdGetDatum(objoid)))
        {
          add_object_address(OCLASS_SCHEMA, objoid, 0, context->addrs);
        }
        break;

           
                                                               
                                                                  
           
      case REGROLEOID:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("constant of the type %s cannot be used here", "regrole")));
        break;
      }
    }
    return false;
  }
  else if (IsA(node, Param))
  {
    Param *param = (Param *)node;

                                                             
    add_object_address(OCLASS_TYPE, param->paramtype, 0, context->addrs);
                                               
    if (OidIsValid(param->paramcollid) && param->paramcollid != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, param->paramcollid, 0, context->addrs);
    }
  }
  else if (IsA(node, FuncExpr))
  {
    FuncExpr *funcexpr = (FuncExpr *)node;

    add_object_address(OCLASS_PROC, funcexpr->funcid, 0, context->addrs);
                                           
  }
  else if (IsA(node, OpExpr))
  {
    OpExpr *opexpr = (OpExpr *)node;

    add_object_address(OCLASS_OPERATOR, opexpr->opno, 0, context->addrs);
                                           
  }
  else if (IsA(node, DistinctExpr))
  {
    DistinctExpr *distinctexpr = (DistinctExpr *)node;

    add_object_address(OCLASS_OPERATOR, distinctexpr->opno, 0, context->addrs);
                                           
  }
  else if (IsA(node, NullIfExpr))
  {
    NullIfExpr *nullifexpr = (NullIfExpr *)node;

    add_object_address(OCLASS_OPERATOR, nullifexpr->opno, 0, context->addrs);
                                           
  }
  else if (IsA(node, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *opexpr = (ScalarArrayOpExpr *)node;

    add_object_address(OCLASS_OPERATOR, opexpr->opno, 0, context->addrs);
                                           
  }
  else if (IsA(node, Aggref))
  {
    Aggref *aggref = (Aggref *)node;

    add_object_address(OCLASS_PROC, aggref->aggfnoid, 0, context->addrs);
                                           
  }
  else if (IsA(node, WindowFunc))
  {
    WindowFunc *wfunc = (WindowFunc *)node;

    add_object_address(OCLASS_PROC, wfunc->winfnoid, 0, context->addrs);
                                           
  }
  else if (IsA(node, SubPlan))
  {
                                                          
    elog(ERROR, "already-planned subqueries not supported");
  }
  else if (IsA(node, FieldSelect))
  {
    FieldSelect *fselect = (FieldSelect *)node;
    Oid argtype = getBaseType(exprType((Node *)fselect->arg));
    Oid reltype = get_typ_typrelid(argtype);

       
                                                                         
                                                                       
                                                                        
                                                                          
                                                                      
                                                                    
                                        
       
    if (OidIsValid(reltype))
    {
      add_object_address(OCLASS_CLASS, reltype, fselect->fieldnum, context->addrs);
    }
    else
    {
      add_object_address(OCLASS_TYPE, fselect->resulttype, 0, context->addrs);
    }
                                                                     
    if (OidIsValid(fselect->resultcollid) && fselect->resultcollid != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, fselect->resultcollid, 0, context->addrs);
    }
  }
  else if (IsA(node, FieldStore))
  {
    FieldStore *fstore = (FieldStore *)node;
    Oid reltype = get_typ_typrelid(fstore->resulttype);

                                                                       
    if (OidIsValid(reltype))
    {
      ListCell *l;

      foreach (l, fstore->fieldnums)
      {
        add_object_address(OCLASS_CLASS, reltype, lfirst_int(l), context->addrs);
      }
    }
    else
    {
      add_object_address(OCLASS_TYPE, fstore->resulttype, 0, context->addrs);
    }
  }
  else if (IsA(node, RelabelType))
  {
    RelabelType *relab = (RelabelType *)node;

                                                                       
    add_object_address(OCLASS_TYPE, relab->resulttype, 0, context->addrs);
                                                                     
    if (OidIsValid(relab->resultcollid) && relab->resultcollid != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, relab->resultcollid, 0, context->addrs);
    }
  }
  else if (IsA(node, CoerceViaIO))
  {
    CoerceViaIO *iocoerce = (CoerceViaIO *)node;

                                                                    
    add_object_address(OCLASS_TYPE, iocoerce->resulttype, 0, context->addrs);
                                                                     
    if (OidIsValid(iocoerce->resultcollid) && iocoerce->resultcollid != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, iocoerce->resultcollid, 0, context->addrs);
    }
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
    ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;

                                  
    add_object_address(OCLASS_TYPE, acoerce->resulttype, 0, context->addrs);
                                                                     
    if (OidIsValid(acoerce->resultcollid) && acoerce->resultcollid != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, acoerce->resultcollid, 0, context->addrs);
    }
                                           
  }
  else if (IsA(node, ConvertRowtypeExpr))
  {
    ConvertRowtypeExpr *cvt = (ConvertRowtypeExpr *)node;

                                                                       
    add_object_address(OCLASS_TYPE, cvt->resulttype, 0, context->addrs);
  }
  else if (IsA(node, CollateExpr))
  {
    CollateExpr *coll = (CollateExpr *)node;

    add_object_address(OCLASS_COLLATION, coll->collOid, 0, context->addrs);
  }
  else if (IsA(node, RowExpr))
  {
    RowExpr *rowexpr = (RowExpr *)node;

    add_object_address(OCLASS_TYPE, rowexpr->row_typeid, 0, context->addrs);
  }
  else if (IsA(node, RowCompareExpr))
  {
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;
    ListCell *l;

    foreach (l, rcexpr->opnos)
    {
      add_object_address(OCLASS_OPERATOR, lfirst_oid(l), 0, context->addrs);
    }
    foreach (l, rcexpr->opfamilies)
    {
      add_object_address(OCLASS_OPFAMILY, lfirst_oid(l), 0, context->addrs);
    }
                                           
  }
  else if (IsA(node, CoerceToDomain))
  {
    CoerceToDomain *cd = (CoerceToDomain *)node;

    add_object_address(OCLASS_TYPE, cd->resulttype, 0, context->addrs);
  }
  else if (IsA(node, NextValueExpr))
  {
    NextValueExpr *nve = (NextValueExpr *)node;

    add_object_address(OCLASS_CLASS, nve->seqid, 0, context->addrs);
  }
  else if (IsA(node, OnConflictExpr))
  {
    OnConflictExpr *onconflict = (OnConflictExpr *)node;

    if (OidIsValid(onconflict->constraint))
    {
      add_object_address(OCLASS_CONSTRAINT, onconflict->constraint, 0, context->addrs);
    }
                                           
  }
  else if (IsA(node, SortGroupClause))
  {
    SortGroupClause *sgc = (SortGroupClause *)node;

    add_object_address(OCLASS_OPERATOR, sgc->eqop, 0, context->addrs);
    if (OidIsValid(sgc->sortop))
    {
      add_object_address(OCLASS_OPERATOR, sgc->sortop, 0, context->addrs);
    }
    return false;
  }
  else if (IsA(node, WindowClause))
  {
    WindowClause *wc = (WindowClause *)node;

    if (OidIsValid(wc->startInRangeFunc))
    {
      add_object_address(OCLASS_PROC, wc->startInRangeFunc, 0, context->addrs);
    }
    if (OidIsValid(wc->endInRangeFunc))
    {
      add_object_address(OCLASS_PROC, wc->endInRangeFunc, 0, context->addrs);
    }
    if (OidIsValid(wc->inRangeColl) && wc->inRangeColl != DEFAULT_COLLATION_OID)
    {
      add_object_address(OCLASS_COLLATION, wc->inRangeColl, 0, context->addrs);
    }
                                              
  }
  else if (IsA(node, Query))
  {
                                                                       
    Query *query = (Query *)node;
    ListCell *lc;
    bool result;

       
                                                                        
                          
       
                                                                         
                                                                       
                                         
       
                                                                  
                                                                     
                                                                         
                                                                          
                                                                 
       
    foreach (lc, query->rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

      switch (rte->rtekind)
      {
      case RTE_RELATION:
        add_object_address(OCLASS_CLASS, rte->relid, 0, context->addrs);
        break;
      default:
        break;
      }
    }

       
                                                                          
                                                                         
                                                                          
                                                                         
                                    
       
    if (query->commandType == CMD_INSERT || query->commandType == CMD_UPDATE)
    {
      RangeTblEntry *rte;

      if (query->resultRelation <= 0 || query->resultRelation > list_length(query->rtable))
      {
        elog(ERROR, "invalid resultRelation %d", query->resultRelation);
      }
      rte = rt_fetch(query->resultRelation, query->rtable);
      if (rte->rtekind == RTE_RELATION)
      {
        foreach (lc, query->targetList)
        {
          TargetEntry *tle = (TargetEntry *)lfirst(lc);

          if (tle->resjunk)
          {
            continue;                              
          }
          add_object_address(OCLASS_CLASS, rte->relid, tle->resno, context->addrs);
        }
      }
    }

       
                                                                        
       
    foreach (lc, query->constraintDeps)
    {
      add_object_address(OCLASS_CONSTRAINT, lfirst_oid(lc), 0, context->addrs);
    }

                                       
    context->rtables = lcons(query->rtable, context->rtables);
    result = query_tree_walker(query, find_expr_references_walker, (void *)context, QTW_IGNORE_JOINALIASES | QTW_EXAMINE_SORTGROUP);
    context->rtables = list_delete_first(context->rtables);
    return result;
  }
  else if (IsA(node, SetOperationStmt))
  {
    SetOperationStmt *setop = (SetOperationStmt *)node;

                                                                     
    find_expr_references_walker((Node *)setop->groupClauses, context);
                                             
  }
  else if (IsA(node, RangeTblFunction))
  {
    RangeTblFunction *rtfunc = (RangeTblFunction *)node;
    ListCell *ct;

       
                                                                  
                                                                           
                                                    
       
    foreach (ct, rtfunc->funccoltypes)
    {
      add_object_address(OCLASS_TYPE, lfirst_oid(ct), 0, context->addrs);
    }
    foreach (ct, rtfunc->funccolcollations)
    {
      Oid collid = lfirst_oid(ct);

      if (OidIsValid(collid) && collid != DEFAULT_COLLATION_OID)
      {
        add_object_address(OCLASS_COLLATION, collid, 0, context->addrs);
      }
    }
  }
  else if (IsA(node, TableFunc))
  {
    TableFunc *tf = (TableFunc *)node;
    ListCell *ct;

       
                                                                        
       
    foreach (ct, tf->coltypes)
    {
      add_object_address(OCLASS_TYPE, lfirst_oid(ct), 0, context->addrs);
    }
    foreach (ct, tf->colcollations)
    {
      Oid collid = lfirst_oid(ct);

      if (OidIsValid(collid) && collid != DEFAULT_COLLATION_OID)
      {
        add_object_address(OCLASS_COLLATION, collid, 0, context->addrs);
      }
    }
  }
  else if (IsA(node, TableSampleClause))
  {
    TableSampleClause *tsc = (TableSampleClause *)node;

    add_object_address(OCLASS_PROC, tsc->tsmhandler, 0, context->addrs);
                                           
  }

  return expression_tree_walker(node, find_expr_references_walker, (void *)context);
}

   
                                                                      
   
static void
eliminate_duplicate_dependencies(ObjectAddresses *addrs)
{
  ObjectAddress *priorobj;
  int oldref, newrefs;

     
                                                                            
                                                                       
             
     
  Assert(!addrs->extras);

  if (addrs->numrefs <= 1)
  {
    return;                    
  }

                                                     
  qsort((void *)addrs->refs, addrs->numrefs, sizeof(ObjectAddress), object_address_comparator);

                   
  priorobj = addrs->refs;
  newrefs = 1;
  for (oldref = 1; oldref < addrs->numrefs; oldref++)
  {
    ObjectAddress *thisobj = addrs->refs + oldref;

    if (priorobj->classId == thisobj->classId && priorobj->objectId == thisobj->objectId)
    {
      if (priorobj->objectSubId == thisobj->objectSubId)
      {
        continue;                                 
      }

         
                                                                       
                                                                      
                                                                     
                                                                         
                                   
         
      if (priorobj->objectSubId == 0)
      {
                                            
        priorobj->objectSubId = thisobj->objectSubId;
        continue;
      }
    }
                                                     
    priorobj++;
    *priorobj = *thisobj;
    newrefs++;
  }

  addrs->numrefs = newrefs;
}

   
                                            
   
static int
object_address_comparator(const void *a, const void *b)
{
  const ObjectAddress *obja = (const ObjectAddress *)a;
  const ObjectAddress *objb = (const ObjectAddress *)b;

     
                                                                             
                                                                           
                               
     
  if (obja->objectId > objb->objectId)
  {
    return -1;
  }
  if (obja->objectId < objb->objectId)
  {
    return 1;
  }

     
                                                                         
                                                         
     
  if (obja->classId < objb->classId)
  {
    return -1;
  }
  if (obja->classId > objb->classId)
  {
    return 1;
  }

     
                                 
     
                                                                            
                                                                          
                                                          
     
  if ((unsigned int)obja->objectSubId < (unsigned int)objb->objectSubId)
  {
    return -1;
  }
  if ((unsigned int)obja->objectSubId > (unsigned int)objb->objectSubId)
  {
    return 1;
  }
  return 0;
}

   
                                                                     
   
                                                             
   
ObjectAddresses *
new_object_addresses(void)
{
  ObjectAddresses *addrs;

  addrs = palloc(sizeof(ObjectAddresses));

  addrs->numrefs = 0;
  addrs->maxrefs = 32;
  addrs->refs = (ObjectAddress *)palloc(addrs->maxrefs * sizeof(ObjectAddress));
  addrs->extras = NULL;                          

  return addrs;
}

   
                                             
   
                                                                             
                   
   
static void
add_object_address(ObjectClass oclass, Oid objectId, int32 subId, ObjectAddresses *addrs)
{
  ObjectAddress *item;

     
                                                                            
     
  StaticAssertStmt(lengthof(object_classes) == LAST_OCLASS + 1, "object_classes[] must cover all ObjectClasses");

                               
  if (addrs->numrefs >= addrs->maxrefs)
  {
    addrs->maxrefs *= 2;
    addrs->refs = (ObjectAddress *)repalloc(addrs->refs, addrs->maxrefs * sizeof(ObjectAddress));
    Assert(!addrs->extras);
  }
                        
  item = addrs->refs + addrs->numrefs;
  item->classId = object_classes[oclass];
  item->objectId = objectId;
  item->objectSubId = subId;
  addrs->numrefs++;
}

   
                                             
   
                                        
   
void
add_exact_object_address(const ObjectAddress *object, ObjectAddresses *addrs)
{
  ObjectAddress *item;

                               
  if (addrs->numrefs >= addrs->maxrefs)
  {
    addrs->maxrefs *= 2;
    addrs->refs = (ObjectAddress *)repalloc(addrs->refs, addrs->maxrefs * sizeof(ObjectAddress));
    Assert(!addrs->extras);
  }
                        
  item = addrs->refs + addrs->numrefs;
  *item = *object;
  addrs->numrefs++;
}

   
                                             
   
                                                                          
   
static void
add_exact_object_address_extra(const ObjectAddress *object, const ObjectAddressExtra *extra, ObjectAddresses *addrs)
{
  ObjectAddress *item;
  ObjectAddressExtra *itemextra;

                                          
  if (!addrs->extras)
  {
    addrs->extras = (ObjectAddressExtra *)palloc(addrs->maxrefs * sizeof(ObjectAddressExtra));
  }

                               
  if (addrs->numrefs >= addrs->maxrefs)
  {
    addrs->maxrefs *= 2;
    addrs->refs = (ObjectAddress *)repalloc(addrs->refs, addrs->maxrefs * sizeof(ObjectAddress));
    addrs->extras = (ObjectAddressExtra *)repalloc(addrs->extras, addrs->maxrefs * sizeof(ObjectAddressExtra));
  }
                        
  item = addrs->refs + addrs->numrefs;
  *item = *object;
  itemextra = addrs->extras + addrs->numrefs;
  *itemextra = *extra;
  addrs->numrefs++;
}

   
                                                                  
   
                                                                             
   
bool
object_address_present(const ObjectAddress *object, const ObjectAddresses *addrs)
{
  int i;

  for (i = addrs->numrefs - 1; i >= 0; i--)
  {
    const ObjectAddress *thisobj = addrs->refs + i;

    if (object->classId == thisobj->classId && object->objectId == thisobj->objectId)
    {
      if (object->objectSubId == thisobj->objectSubId || thisobj->objectSubId == 0)
      {
        return true;
      }
    }
  }

  return false;
}

   
                                                                         
                                                            
   
static bool
object_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddresses *addrs)
{
  bool result = false;
  int i;

  for (i = addrs->numrefs - 1; i >= 0; i--)
  {
    ObjectAddress *thisobj = addrs->refs + i;

    if (object->classId == thisobj->classId && object->objectId == thisobj->objectId)
    {
      if (object->objectSubId == thisobj->objectSubId)
      {
        ObjectAddressExtra *thisextra = addrs->extras + i;

        thisextra->flags |= flags;
        result = true;
      }
      else if (thisobj->objectSubId == 0)
      {
           
                                                                  
                                                                      
                                                                      
                                                                    
                                      
           
        result = true;
      }
      else if (object->objectSubId == 0)
      {
           
                                                                       
                                                                  
                                                                      
                                                                    
           
                                                                      
                                                                 
                                                                      
                                                                      
                                                                     
                                                                    
                                                                     
                            
           
                                                                       
                                                                     
                                                                    
                                                                 
           
                                                                     
                                                                     
                                                             
           
        ObjectAddressExtra *thisextra = addrs->extras + i;

        if (flags)
        {
          thisextra->flags |= (flags | DEPFLAG_SUBOBJECT);
        }
      }
    }
  }

  return result;
}

   
                                                             
   
static bool
stack_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddressStack *stack)
{
  bool result = false;
  ObjectAddressStack *stackptr;

  for (stackptr = stack; stackptr; stackptr = stackptr->next)
  {
    const ObjectAddress *thisobj = stackptr->object;

    if (object->classId == thisobj->classId && object->objectId == thisobj->objectId)
    {
      if (object->objectSubId == thisobj->objectSubId)
      {
        stackptr->flags |= flags;
        result = true;
      }
      else if (thisobj->objectSubId == 0)
      {
           
                                                                      
                                                                 
                                                                     
                                                                  
           
        result = true;
      }
      else if (object->objectSubId == 0)
      {
           
                                                                       
                                                                   
                                                                 
           
        if (flags)
        {
          stackptr->flags |= (flags | DEPFLAG_SUBOBJECT);
        }
      }
    }
  }

  return result;
}

   
                                                                           
                            
   
void
record_object_address_dependencies(const ObjectAddress *depender, ObjectAddresses *referenced, DependencyType behavior)
{
  eliminate_duplicate_dependencies(referenced);
  recordMultipleDependencies(depender, referenced->refs, referenced->numrefs, behavior);
}

   
                                               
   
                                                                              
                                                                              
                                                                              
                                                                             
   
void
sort_object_addresses(ObjectAddresses *addrs)
{
  if (addrs->numrefs > 1)
  {
    qsort((void *)addrs->refs, addrs->numrefs, sizeof(ObjectAddress), object_address_comparator);
  }
}

   
                                                     
   
void
free_object_addresses(ObjectAddresses *addrs)
{
  pfree(addrs->refs);
  if (addrs->extras)
  {
    pfree(addrs->extras);
  }
  pfree(addrs);
}

   
                                                                      
   
                                                                             
                                                                              
   
ObjectClass
getObjectClass(const ObjectAddress *object)
{
                                                          
  if (object->classId != RelationRelationId && object->objectSubId != 0)
  {
    elog(ERROR, "invalid non-zero objectSubId for object class %u", object->classId);
  }

  switch (object->classId)
  {
  case RelationRelationId:
                                       
    return OCLASS_CLASS;

  case ProcedureRelationId:
    return OCLASS_PROC;

  case TypeRelationId:
    return OCLASS_TYPE;

  case CastRelationId:
    return OCLASS_CAST;

  case CollationRelationId:
    return OCLASS_COLLATION;

  case ConstraintRelationId:
    return OCLASS_CONSTRAINT;

  case ConversionRelationId:
    return OCLASS_CONVERSION;

  case AttrDefaultRelationId:
    return OCLASS_DEFAULT;

  case LanguageRelationId:
    return OCLASS_LANGUAGE;

  case LargeObjectRelationId:
    return OCLASS_LARGEOBJECT;

  case OperatorRelationId:
    return OCLASS_OPERATOR;

  case OperatorClassRelationId:
    return OCLASS_OPCLASS;

  case OperatorFamilyRelationId:
    return OCLASS_OPFAMILY;

  case AccessMethodRelationId:
    return OCLASS_AM;

  case AccessMethodOperatorRelationId:
    return OCLASS_AMOP;

  case AccessMethodProcedureRelationId:
    return OCLASS_AMPROC;

  case RewriteRelationId:
    return OCLASS_REWRITE;

  case TriggerRelationId:
    return OCLASS_TRIGGER;

  case NamespaceRelationId:
    return OCLASS_SCHEMA;

  case StatisticExtRelationId:
    return OCLASS_STATISTIC_EXT;

  case TSParserRelationId:
    return OCLASS_TSPARSER;

  case TSDictionaryRelationId:
    return OCLASS_TSDICT;

  case TSTemplateRelationId:
    return OCLASS_TSTEMPLATE;

  case TSConfigRelationId:
    return OCLASS_TSCONFIG;

  case AuthIdRelationId:
    return OCLASS_ROLE;

  case DatabaseRelationId:
    return OCLASS_DATABASE;

  case TableSpaceRelationId:
    return OCLASS_TBLSPACE;

  case ForeignDataWrapperRelationId:
    return OCLASS_FDW;

  case ForeignServerRelationId:
    return OCLASS_FOREIGN_SERVER;

  case UserMappingRelationId:
    return OCLASS_USER_MAPPING;

  case DefaultAclRelationId:
    return OCLASS_DEFACL;

  case ExtensionRelationId:
    return OCLASS_EXTENSION;

  case EventTriggerRelationId:
    return OCLASS_EVENT_TRIGGER;

  case PolicyRelationId:
    return OCLASS_POLICY;

  case PublicationRelationId:
    return OCLASS_PUBLICATION;

  case PublicationRelRelationId:
    return OCLASS_PUBLICATION_REL;

  case SubscriptionRelationId:
    return OCLASS_SUBSCRIPTION;

  case TransformRelationId:
    return OCLASS_TRANSFORM;
  }

                          
  elog(ERROR, "unrecognized object class: %u", object->classId);
  return OCLASS_CLASS;                          
}

   
                                            
   
static void
DeleteInitPrivs(const ObjectAddress *object)
{
  Relation relation;
  ScanKeyData key[3];
  SysScanDesc scan;
  HeapTuple oldtuple;

  relation = table_open(InitPrivsRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_init_privs_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  ScanKeyInit(&key[1], Anum_pg_init_privs_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&key[2], Anum_pg_init_privs_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));

  scan = systable_beginscan(relation, InitPrivsObjIndexId, true, NULL, 3, key);

  while (HeapTupleIsValid(oldtuple = systable_getnext(scan)))
  {
    CatalogTupleDelete(relation, &oldtuple->t_self);
  }

  systable_endscan(scan);

  table_close(relation, RowExclusiveLock);
}
